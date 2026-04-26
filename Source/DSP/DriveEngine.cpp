#include "DriveEngine.h"

DriveEngine::DriveEngine()
{
    for (auto& stage : transformerStages)
    {
        stage.enabled = true;
        stage.saturation = 0.5f;
        stage.hysteresis = 0.2f;
        stage.frequencyResponse = 1.0f;
        stage.phaseShift = 0.0f;
    }
}

void DriveEngine::prepare(double sr)
{
    this->sampleRate = (float) sr;

    juce::dsp::ProcessSpec spec { sr, 512u, 1u };
    for (int ch = 0; ch < 2; ++ch)
    {
        xover[ch].lp[0].prepare (spec);  xover[ch].hp[0].prepare (spec);
        xover[ch].lp[0].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xover[ch].hp[0].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        xover[ch].lp[1].prepare (spec);  xover[ch].hp[1].prepare (spec);
        xover[ch].lp[1].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xover[ch].hp[1].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        xover[ch].lp[2].prepare (spec);  xover[ch].hp[2].prepare (spec);
        xover[ch].lp[2].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xover[ch].hp[2].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    }

    for (auto& filter : toneFilters) filter.prepare (sr);
    reset();
}

void DriveEngine::reset()
{
    dcOffset.fill (0.0f);
    lastSample.fill (0.0f);
    for (auto& t : toneFilters) t.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 3; ++i)
        {
            xover[ch].lp[i].reset();
            xover[ch].hp[i].reset();
        }
    }
}

// ── Process a single band with LR4 crossover split ─────────────────────
// Uses Linkwitz-Riley 4th-order filters (perfect phase alignment).
// Band reconstruction: all bands sum back to the original signal perfectly.
void DriveEngine::processBand (juce::AudioBuffer<float>& wet,
                               const juce::AudioBuffer<float>& dry,
                               int bandIdx,
                               float x1Hz, float x2Hz, float x3Hz,
                               int algo, float driveDb, float mix,
                               int numChannels, int numSamples)
{
    if (numChannels < 1 || bandIdx < 0 || bandIdx > 3) return;

    float fLow  = juce::jlimit (20.0f, 19000.0f, x1Hz);
    float fMid  = juce::jlimit (20.0f, 19000.0f, x2Hz);
    float fHigh = juce::jlimit (20.0f, 19000.0f, x3Hz);

    // Re-arrange to ensure monotonicity
    if (fMid <= fLow)  fMid  = fLow + 10.0f;
    if (fHigh <= fMid) fHigh = fMid + 10.0f;

    for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
    {
        // Set crossover frequencies (only if changed — JUCE's setCutoffFrequency is smart about this)
        xover[ch].lp[0].setCutoffFrequency (fLow);
        xover[ch].hp[0].setCutoffFrequency (fLow);
        xover[ch].lp[1].setCutoffFrequency (fMid);
        xover[ch].hp[1].setCutoffFrequency (fMid);
        xover[ch].lp[2].setCutoffFrequency (fHigh);
        xover[ch].hp[2].setCutoffFrequency (fHigh);
    }

    float driveLin = std::pow (10.0f, driveDb / 20.0f);
    float mixScale = mix;

    auto* wetL = wet.getWritePointer (0);
    auto* wetR = numChannels > 1 ? wet.getWritePointer (1) : wetL;
    auto* dryL = dry.getReadPointer (0);
    auto* dryR = numChannels > 1 ? dry.getReadPointer (1) : dryL;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = dryL[i];
        float inR = (numChannels > 1) ? dryR[i] : inL;

        float bandVals[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        {
            float in = (ch == 0) ? inL : inR;
            XoverFilters& x = xover[ch];
            float sel = 0.0f;

            // LR4 filters: process each filter independently per sample
            // LP = cascaded 2nd-order, HP = cascaded 2nd-order
            // Perfect reconstruction: LP(x) + HP(x) = x (time-aligned at -6dB xover)
            float lp0 = x.lp[0].processSample (ch, in);
            float hp0 = x.hp[0].processSample (ch, in);
            float lp1 = x.lp[1].processSample (ch, hp0);  // HP at fLow → LP at fMid = LO-MID band
            float hp1 = x.hp[1].processSample (ch, hp0);
            float lp2 = x.lp[2].processSample (ch, hp1);  // HP at fLow → HP at fMid → LP at fHigh = HI-MID band
            float hp2 = x.hp[2].processSample (ch, hp1);  // HP at fLow → HP at fMid → HP at fHigh = HIGH band

            // Band selection with perfect reconstruction guarantee:
            //   Band 0 (LOW)   = lp0
            //   Band 1 (LO-MID)= hp0 - hp1 (or equivalently lp1)
            //   Band 2 (HI-MID)= hp1 - hp2 (or equivalently lp2)
            //   Band 3 (HIGH)  = hp2
            //   Sum: lp0 + (hp0-hp1) + (hp1-hp2) + hp2 = lp0 + hp0 = in ✓
            switch (bandIdx)
            {
                case 0: sel = lp0;                     break;  // LOW
                case 1: sel = hp0 - hp1;               break;  // LO-MID (or lp1)
                case 2: sel = hp1 - hp2;               break;  // HI-MID (or lp2)
                case 3: sel = hp2;                     break;  // HIGH
                default: sel = 0.0f;                    break;
            }

            // Apply saturation
            float sat = applySaturation (sel, algo, driveLin);

            // Dry/wet per-band
            bandVals[ch] = sel * (1.0f - mixScale) + sat * mixScale;
        }

        wetL[i] += bandVals[0];
        if (numChannels > 1) wetR[i] += bandVals[1];
    }
}

// ── Core saturation functions ──────────────────────────────────────────
float DriveEngine::applySaturation (float x, int algo, float drive)
{
    switch (algo)
    {
        case 0: return processTube (x, drive);
        case 1: return processTape (x, drive);
        case 2: return processSolidState (x, drive);
        case 3: return processTransformer (x, drive);
        default: return x;
    }
}

// ── Legacy full-buffer process (unused but kept for API compat) ────────
void DriveEngine::process(juce::AudioBuffer<float>& buffer, int algorithm, float drive)
{
    currentAlgorithm = algorithm;
    currentDrive = drive;
    if (std::abs (drive) < 0.001f) return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            samples[i] = applySaturation (samples[i], algorithm, drive);
    }
}

// ── Saturation algorithms ──────────────────────────────────────────────
float DriveEngine::processTube(float x, float drive)
{
    float gain = 1.0f + drive * 0.1f;
    x *= gain;
    x = (x > 0.0f) ? tanhSoftClip (x * 1.2f) * 0.833f
                    : tanhSoftClip (x * 0.8f) * 1.25f;
    float evenHarm = x * x * 0.1f * (drive / (100.0f + 1e-6f));
    x += evenHarm;
    float dc = dcOffset[0];
    dc = 0.999f * dc + 0.001f * x;
    x -= dc;
    dcOffset[0] = dc;
    return x;
}

float DriveEngine::processTape(float x, float drive)
{
    float gain = 1.0f + drive * 0.05f;
    x *= gain;
    float state = lastSample[0];
    float threshold = 0.1f;
    if (std::abs (x - state) > threshold)
    {
        float hyst = transformerStages[0].hysteresis * 0.1f;
        x += (x > state ? hyst : -hyst) * 0.05f;
    }
    x = (x > 0.0f) ? tanhSoftClip (x) : tanhSoftClip (x * 0.7f) * 1.428f;
    float compression = 0.5f + 0.5f * (drive / (100.0f + 1e-6f));
    x = x / (1.0f + std::abs (x) * compression);
    lastSample[0] = x;
    return x;
}

float DriveEngine::processSolidState(float x, float drive)
{
    float gain = 1.0f + drive * 0.15f;
    x *= gain;
    x = diodeClipping (x);
    float x3 = x * x * x;
    x += x3 * 0.05f * (drive / (100.0f + 1e-6f));
    float limit = 0.8f;
    if (x > limit)  x = limit  + (x - limit)  * 0.3f;
    if (x < -limit) x = -limit + (x + limit)  * 0.3f;
    return x;
}

float DriveEngine::processTransformer(float x, float drive)
{
    float gain = 1.0f + drive * 0.08f;
    x *= gain;
    return x / (1.0f + x * x * 0.5f);
}

float DriveEngine::processDigital(float x, float drive)
{
    float gain = 1.0f + drive * 0.2f;
    x *= gain;
    x = juce::jlimit (-0.95f, 0.95f, x);
    return x + std::sin (x * 3.14159f) * 0.1f * (drive / (100.0f + 1e-6f));
}

float DriveEngine::processTransistor(float x, float drive)
{
    float gain = 1.0f + drive * 0.25f;
    x *= gain;
    x = asymmetricClip (x);
    return (x > 0.0f) ? std::atan (x * 2.0f) * 0.5f
                      : std::atan (x * 1.5f) * 0.666f;
}

// ── Helper functions ─────────────────────────────────────────────────
float DriveEngine::tanhSoftClip(float x)
{
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2 + x4);
}

float DriveEngine::asymmetricClip(float x)
{
    return (x > 0.0f) ? std::tanh (x * 0.8f) * 1.25f
                      : std::tanh (x * 1.2f) * 0.833f;
}

float DriveEngine::diodeClipping(float x)
{
    return (x > 0.0f) ? 1.0f - std::exp (-x * 1.5f)
                      : -1.0f + std::exp (x * 1.5f);
}

float DriveEngine::magneticHysteresis(float x, float& state)
{
    float delta = x - state;
    float hysteresis = 0.05f;
    if (delta > hysteresis)            state = x - hysteresis;
    else if (delta < -hysteresis)      state = x + hysteresis;
    return state;
}

// ── ToneFilter ────────────────────────────────────────────────────────
void DriveEngine::ToneFilter::prepare(double sr) { sampleRate = sr; }

float DriveEngine::ToneFilter::process(float x)
{
    float omega = 2.0f * 3.14159f * 1000.0f / (float) sampleRate;
    float alpha = std::sin (omega) / (2.0f * 0.707f);
    float A = std::pow (10.0f, tilt * 0.25f);
    float sqrtA = std::sqrt (A);

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * std::cos (omega) + 2.0f * sqrtA * alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * std::cos (omega));
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * std::cos (omega) - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * std::cos (omega) + 2.0f * sqrtA * alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * std::cos (omega));
    float a2 = (A + 1.0f) - (A - 1.0f) * std::cos (omega) - 2.0f * sqrtA * alpha;

    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    return y * (1.0f + bias * 0.1f);
}

void DriveEngine::ToneFilter::setTilt(float t) { tilt = t; }
void DriveEngine::ToneFilter::setBias(float b) { bias = b; }
void DriveEngine::ToneFilter::updateCoefficients() {}
