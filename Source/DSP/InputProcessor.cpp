#include "InputProcessor.h"

//==============================================================================
InputProcessor::InputProcessor()
{
    // Default values from Figma
    trimDb = 0.0f;
    loPassFreq = 20000.0f;
    hiPassFreq = 20.0f;
    voice = 0.0f;
    voiceBias = 0.0f;
}

void InputProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    
    // Update gains
    trimGain = std::pow(10.0f, trimDb / 20.0f);
    
    // Prepare filters
    updateFilters();
    
    // Prepare voice filter
    voiceFilter.prepare(spec.sampleRate);
    
    // Prepare dry buffer for delta mode
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    
    reset();
}

void InputProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    if (numChannels < 2)
        return;
    
    // Store dry signal for delta mode
    if (delta)
    {
        dryBuffer.makeCopyOf(buffer, true);
    }
    
    // Update input levels
    const auto* leftIn = buffer.getReadPointer(0);
    const auto* rightIn = buffer.getReadPointer(1);
    
    for (int i = 0; i < numSamples; ++i)
    {
        inputLevelL = 0.999f * inputLevelL + 0.001f * std::abs(leftIn[i]);
        inputLevelR = 0.999f * inputLevelR + 0.001f * std::abs(rightIn[i]);
    }
    
    // Apply trim gain
    buffer.applyGain(trimGain);
    
    // Apply compensation gain if enabled
    if (compensate)
    {
        buffer.applyGain(compensationGain);
    }
    
    // Apply filters
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        
        // Apply high-pass filter
        for (int i = 0; i < numSamples; ++i)
        {
            samples[i] = hiPassFilterL.processSample(samples[i]);
        }
        
        // Apply low-pass filter
        for (int i = 0; i < numSamples; ++i)
        {
            samples[i] = loPassFilterL.processSample(samples[i]);
        }
    }
    
    // Apply mono sum if enabled
    if (mono)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = (left[i] + right[i]) * 0.5f;
            left[i] = mono;
            right[i] = mono;
        }
    }
    
    // Apply voice tilt and bias
    if (voice != 0.0f || voiceBias != 0.0f)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        voiceFilter.process(left, right, numSamples, voice, voiceBias);
    }
    
    // Apply polarity inversion
    if (invertPolarity)
    {
        buffer.applyGain(-1.0f);
    }
    
    // Apply delta mode (output difference)
    if (delta)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            
            for (int i = 0; i < numSamples; ++i)
            {
                // Output difference between processed and dry
                wet[i] = wet[i] - dry[i];
            }
        }
    }
    
    // Update output levels
    const auto* leftOut = buffer.getReadPointer(0);
    const auto* rightOut = buffer.getReadPointer(1);
    
    for (int i = 0; i < numSamples; ++i)
    {
        outputLevelL = 0.999f * outputLevelL + 0.001f * std::abs(leftOut[i]);
        outputLevelR = 0.999f * outputLevelR + 0.001f * std::abs(rightOut[i]);
    }
}

void InputProcessor::reset()
{
    inputLevelL = -60.0f;
    inputLevelR = -60.0f;
    outputLevelL = -60.0f;
    outputLevelR = -60.0f;
    
    loPassFilterL.reset();
    loPassFilterR.reset();
    hiPassFilterL.reset();
    hiPassFilterR.reset();
    
    dryBuffer.clear();
}

//==============================================================================
void InputProcessor::setTrim(float trimDb)
{
    this->trimDb = trimDb;
    trimGain = std::pow(10.0f, trimDb / 20.0f);
}

void InputProcessor::setLoPass(float freqHz)
{
    loPassFreq = freqHz;
    updateFilters();
}

void InputProcessor::setHiPass(float freqHz)
{
    hiPassFreq = freqHz;
    updateFilters();
}

void InputProcessor::setVoice(float v)
{
    voice = juce::jlimit(-50.0f, 50.0f, v);
}

void InputProcessor::setVoiceBias(float bias)
{
    voiceBias = juce::jlimit(-50.0f, 50.0f, bias);
}

void InputProcessor::setMono(bool m)
{
    mono = m;
}

void InputProcessor::setPolarity(bool invert)
{
    invertPolarity = invert;
}

void InputProcessor::setDelta(bool d)
{
    delta = d;
}

void InputProcessor::setCompensate(bool comp)
{
    compensate = comp;
    // In a real implementation, you'd calculate compensation gain based on processing
    compensationGain = compensate ? 0.5f : 1.0f; // Simplified
}

//==============================================================================
void InputProcessor::updateFilters()
{
    // Update low-pass filter coefficients
    if (loPassFreq < spec.sampleRate * 0.5f)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, loPassFreq);
        loPassFilterL.coefficients = coeffs;
        loPassFilterR.coefficients = coeffs;
    }
    else
    {
        // If frequency is at or above Nyquist, bypass
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, spec.sampleRate * 0.49f);
        loPassFilterL.coefficients = coeffs;
        loPassFilterR.coefficients = coeffs;
    }
    
    // Update high-pass filter coefficients
    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, hiPassFreq);
    hiPassFilterL.coefficients = hpfCoeffs;
    hiPassFilterR.coefficients = hpfCoeffs;
}

void InputProcessor::VoiceFilter::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
}

void InputProcessor::VoiceFilter::process(float* left, float* right, int numSamples, float tilt, float bias)
{
    this->tilt = tilt / 50.0f; // Normalize to -1..1
    this->bias = bias / 50.0f; // Normalize to -1..1
    
    updateCoefficients();
    
    // Simple tilt filter implementation
    // Positive tilt = boost highs, cut lows
    // Negative tilt = boost lows, cut highs
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Process left channel
        float x = left[i];
        float y = 0.0f;
        
        // Simple shelving filter (placeholder)
        // In production, implement proper filters
        
        left[i] = x * (1.0f + tilt * 0.1f + bias * 0.05f);
        
        // Process right channel similarly
        x = right[i];
        right[i] = x * (1.0f + tilt * 0.1f + bias * 0.05f);
    }
}

void InputProcessor::VoiceFilter::updateCoefficients()
{
    // Coefficient calculation would go here
    // This is simplified - actual implementation would have proper filter design
}
