#include "InputProcessor.h"

InputProcessor::InputProcessor()
{
    trimDb = 0.0f;
    loPassFreq = 20000.0f;
    hiPassFreq = 20.0f;
    voice = 0.0f;
    voiceBias = 0.0f;
}

void InputProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;

    trimGain = std::pow (10.0f, trimDb / 20.0f);
    updateFilters();

    voiceFilter.prepare (spec.sampleRate);
    dryBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
    reset();
}

void InputProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const auto n = buffer.getNumSamples();
    const auto ch = buffer.getNumChannels();
    if (ch < 1) return;

    if (delta) dryBuffer.makeCopyOf (buffer, true);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = ch > 1 ? buffer.getWritePointer (1) : outL;

    // Meter + trim + filters all in one pass
    auto& hpL = hpFilter[0];
    auto& hpR = hpFilter[ch > 1 ? 1 : 0];
    auto& lpL = lpFilter[0];
    auto& lpR = lpFilter[ch > 1 ? 1 : 0];

    for (int i = 0; i < n; ++i)
    {
        float xL = outL[i] * trimGain;
        float xR = outR[i] * trimGain;

        // Filters
        xL = hpL.processSample (xL);
        xL = lpL.processSample (xL);
        if (ch > 1)
        {
            xR = hpR.processSample (xR);
            xR = lpR.processSample (xR);
        }

        // Polarity
        if (invertPolarity) { xL = -xL; xR = -xR; }

        // Mono sum
        if (mono)
        {
            float m = (xL + xR) * 0.5f;
            xL = m; xR = m;
        }

        outL[i] = xL;
        if (ch > 1) outR[i] = xR;

        // Metering
        inputLevelL  = 0.999f * inputLevelL  + 0.001f * std::abs (outL[i]);
        inputLevelR  = 0.999f * inputLevelR  + 0.001f * std::abs (outR[i]);
        outputLevelL = inputLevelL;
        outputLevelR = inputLevelR;
    }

    // Delta mode
    if (delta)
    {
        auto* dryL = dryBuffer.getReadPointer (0);
        auto* dryR = dryBuffer.getReadPointer (ch > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            outL[i] = outL[i] - dryL[i];
            if (ch > 1) outR[i] = outR[i] - dryR[i];
        }
    }
}

void InputProcessor::reset()
{
    inputLevelL = -60.0f;
    inputLevelR = -60.0f;
    outputLevelL = -60.0f;
    outputLevelR = -60.0f;

    for (auto& f : hpFilter) f.reset();
    for (auto& f : lpFilter) f.reset();
    dryBuffer.clear();
}

void InputProcessor::setTrim(float db)       { trimDb = db; trimGain = std::pow (10.0f, db / 20.0f); }
void InputProcessor::setLoPass(float f)      { loPassFreq = f; updateFilters(); }
void InputProcessor::setHiPass(float f)      { hiPassFreq = f; updateFilters(); }
void InputProcessor::setVoice(float v)        { voice = juce::jlimit (-50.0f, 50.0f, v); }
void InputProcessor::setVoiceBias(float b)    { voiceBias = juce::jlimit (-50.0f, 50.0f, b); }
void InputProcessor::setMono(bool m)          { mono = m; }
void InputProcessor::setPolarity(bool p)      { invertPolarity = p; }
void InputProcessor::setDelta(bool d)         { delta = d; }
void InputProcessor::setCompensate(bool c)    { compensate = c; compensationGain = c ? 0.5f : 1.0f; }

void InputProcessor::updateFilters()
{
    const float nyquist = (float) spec.sampleRate * 0.48f;

    float lpHz = juce::jmin (loPassFreq, nyquist);
    auto lpC = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, lpHz);
    lpFilter[0].coefficients = lpC;
    lpFilter[1].coefficients = lpC;

    float hpHz = juce::jmin (hiPassFreq, nyquist);
    auto hpC = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, hpHz);
    hpFilter[0].coefficients = hpC;
    hpFilter[1].coefficients = hpC;
}

void InputProcessor::VoiceFilter::prepare(double) {}
void InputProcessor::VoiceFilter::process(float* left, float* right, int n, float tilt, float bias)
{
    float t = tilt / 50.0f;
    float b = bias / 50.0f;
    float g = 1.0f + t * 0.1f + b * 0.05f;
    for (int i = 0; i < n; ++i) { left[i] *= g; right[i] *= g; }
}
void InputProcessor::VoiceFilter::updateCoefficients() {}
