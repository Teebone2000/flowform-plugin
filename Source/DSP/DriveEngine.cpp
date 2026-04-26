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

void DriveEngine::prepare(double sampleRate)
{
    this->sampleRate = (float) sampleRate;
    for (auto& filter : toneFilters) filter.prepare (sampleRate);
    reset();
}

void DriveEngine::reset()
{
    dcOffset.fill (0.0f);
    lastSample.fill (0.0f);
    for (auto& t : toneFilters) t.reset();
    lrState[0] = lrState[1] = 0.0f;
}

// ── New: process a single band with crossover split ─────────────────────
// Splits the dry signal into band `bandIdx`, applies saturation, writes to wet buffer.
// The split uses simple 2-pole state-variable filters at x1, x2, x3.
void DriveEngine::processBand (juce::AudioBuffer<float>& wet,
                               const juce::AudioBuffer<float>& dry,
                               int bandIdx,
                               float x1Hz, float x2Hz, float x3Hz,
                               int algo, float driveDb, float mix,
                               int numChannels, int numSamples)
{
    if (numChannels < 1) return;

    auto* wetL = wet.getWritePointer (0);
    auto* wetR = numChannels > 1 ? wet.getWritePointer (1) : wetL;
    auto* dryL = dry.getReadPointer (0);
    auto* dryR = numChannels > 1 ? dry.getReadPointer (1) : dryL;

    float driveLin = std::pow (10.0f, driveDb / 20.0f);
    float mixScale = mix;  // 0..1

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = dryL[i];
        float inR = dryR[i];

        // Simple LR4-style crossovers using 1-pole IIR cascaded
        // We use a state-variable approach: each band = bp at (x1,x2) or lp/hp
        float bandL = 0.0f, bandR = 0.0f;

        // Crossover frequencies — clamp sanely
        float fLow  = juce::jlimit (10.0f, 20000.0f, x1Hz);
        float fMid  = juce::jlimit (10.0f, 20000.0f, x2Hz);
        float fHigh = juce::jlimit (10.0f, 20000.0f, x3Hz);

        float dt = 1.0f / (sampleRate + 1e-12f);

        // Per-channel crossover filtering
        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        {
            float in = (ch == 0) ? inL : inR;
            float& lp1 = lpState[ch][0];
            float& lp2 = lpState[ch][1];
            float& hp1 = hpState[ch][0];
            float& hp2 = hpState[ch][1];
            float& midLP = midLpState[ch];
            float& midHP = midHpState[ch];
            float& hiLP = hiLpState[ch];

            // Band 0 (LOW): LP at fLow
            float aLow = dt / (1.0f / (2.0f * 3.14159f * fLow) + dt);
            lp1 = lp1 + aLow * (in - lp1);  // 1-pole LP
            lp2 = lp2 + aLow * (lp1 - lp2); // cascade for 2-pole

            // Band 1 (LOW-MID): BP at [fLow, fMid] = HP first then LP
            float aHP = dt / (1.0f / (2.0f * 3.14159f * fLow) + dt);
            midHP = midHP + aHP * (in - midHP); // 1-pole HP
            float hpOut = in - midHP;
            float aMidLP = dt / (1.0f / (2.0f * 3.14159f * fMid) + dt);
            midLP = midLP + aMidLP * (hpOut - midLP); // LP cascade
            float midOut = midLP;

            // Band 2 (HIGH-MID): BP at [fMid, fHigh]
            float aMidHP = dt / (1.0f / (2.0f * 3.14159f * fMid) + dt);
            float& hpMid = hpMidState[ch];
            hpMid = hpMid + aMidHP * (in - hpMid);
            float hpMidOut = in - hpMid;
            float aHiLP  = dt / (1.0f / (2.0f * 3.14159f * fHigh) + dt);
            hiLP = hiLP + aHiLP * (hpMidOut - hiLP);

            // Band 3 (HIGH): HP at fHigh
            float aHiHP = dt / (1.0f / (2.0f * 3.14159f * fHigh) + dt);
            float& hp3 = hpHiState[ch];
            hp3 = hp3 + aHiHP * (in - hp3);
            float hiOut = in - hp3;

            float sel;
            switch (bandIdx)
            {
                case 0: sel = lp2;        break;  // LOW
                case 1: sel = midOut;     break;  // LO-MID
                case 2: sel = hiLP;       break;  // HI-MID (midHP->LP at fHigh)
                case 3: sel = hiOut;      break;  // HIGH
                default: sel = 0.0f;       break;
            }

            // Now apply saturation algorithm to band signal
            float sat = sel;
            sat = applySaturation (sat, algo, driveLin);

            // Mix wet with dry per-band
            sat = sel * (1.0f - mixScale) + sat * mixScale;

            if (ch == 0) bandL = sat;
            else         bandR = sat;
        }

        // Accumulate into wet buffer (summed across bands)
        wetL[i] += bandL;
        wetR[i] += bandR;
    }
}

// ── Core saturation functions ──────────────────────────────────────────
float DriveEngine::applySaturation (float x, int algo, float drive)
{
    switch (algo)
    {
        case 0: x = processTube (x, drive);         break;
        case 1: x = processTape (x, drive);         break;
        case 2: x = processSolidState (x, drive);   break;
        case 3: x = processTransformer (x, drive);  break;
        default: break;
    }
    return x;
}

// ── Legacy process (kept for API compat, now acts on full signal) ─────
void DriveEngine::process(juce::AudioBuffer<float>& buffer, int algorithm, float drive)
{
    currentAlgorithm = algorithm;
    currentDrive = drive;
    if (std::abs (drive) < 0.001f) return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            samples[i] = applySaturation (samples[i], algorithm, drive);
    }
}

// ── Saturation algorithm implementations ────────────────────────────────
float DriveEngine::processTube(float x, float drive)
{
    float gain = 1.0f + drive * 0.1f;
    x *= gain;

    if (x > 0.0f)
        x = tanhSoftClip (x * 1.2f) * 0.833f;
    else
        x = tanhSoftClip (x * 0.8f) * 1.25f;

    // Even harmonics
    float evenHarm = x * x * 0.1f * (drive / (100.0f + 1e-6f));
    x += evenHarm;

    // DC block
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

    float hysteresis = transformerStages[0].hysteresis * 0.1f;
    float state = lastSample[0];
    float threshold = 0.1f;
    if (std::abs (x - state) > threshold)
    {
        if (x > state) x += hysteresis * 0.05f;
        else           x -= hysteresis * 0.05f;
    }

    if (x > 0.0f)
        x = tanhSoftClip (x);
    else
        x = tanhSoftClip (x * 0.7f) * 1.428f;

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
    x = x / (1.0f + x * x * 0.5f);
    return x;
}

float DriveEngine::processDigital(float x, float drive)
{
    float gain = 1.0f + drive * 0.2f;
    x *= gain;
    float limit = 0.95f;
    x = juce::jlimit (-limit, limit, x);
    float folded = std::sin (x * 3.14159f) * 0.1f * (drive / (100.0f + 1e-6f));
    x += folded;
    return x;
}

float DriveEngine::processTransistor(float x, float drive)
{
    float gain = 1.0f + drive * 0.25f;
    x *= gain;
    x = asymmetricClip (x);
    if (x > 0.0f) x = std::atan (x * 2.0f) * 0.5f;
    else          x = std::atan (x * 1.5f) * 0.666f;
    return x;
}

// ── Helper functions ──────────────────────────────────────────────────
float DriveEngine::tanhSoftClip(float x)
{
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2 + x4);
}

float DriveEngine::asymmetricClip(float x)
{
    if (x > 0.0f) return std::tanh (x * 0.8f) * 1.25f;
    else          return std::tanh (x * 1.2f) * 0.833f;
}

float DriveEngine::diodeClipping(float x)
{
    if (x > 0.0f) return 1.0f - std::exp (-x * 1.5f);
    else          return -1.0f + std::exp (x * 1.5f);
}

float DriveEngine::magneticHysteresis(float x, float& state)
{
    float delta = x - state;
    float hysteresis = 0.05f;
    if (delta > hysteresis)       state = x - hysteresis;
    else if (delta < -hysteresis) state = x + hysteresis;
    return state;
}

// ── ToneFilter ─────────────────────────────────────────────────────────
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
    y *= (1.0f + bias * 0.1f);
    return y;
}

void DriveEngine::ToneFilter::setTilt(float t) { tilt = t; }
void DriveEngine::ToneFilter::setBias(float b) { bias = b; }
void DriveEngine::ToneFilter::updateCoefficients() {}
