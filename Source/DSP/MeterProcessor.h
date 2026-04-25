#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>

namespace flowform
{

//==============================================================================
// Fast math utilities
inline float dbToLin (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
inline float linToDb (float lin) noexcept { return juce::jlimit (-120.0f, 24.0f, 20.0f * std::log10 (std::max (1e-8f, std::abs (lin)))); }

inline float fastTanh (float x) noexcept
{
    if (x < -3.0f) return -1.0f;
    if (x >  3.0f) return  1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

//==============================================================================
// Envelope follower for real-time metering
struct EnvelopeFollower
{
    float zL = 0.0f, zR = 0.0f;
    float attackMs = 1.0f, releaseMs = 200.0f;
    double sr = 44100.0;

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { zL = 0.0f; zR = 0.0f; }
    void setTimes (float aMs, float rMs) { attackMs = aMs; releaseMs = rMs; }

    void process (float L, float R)
    {
        float a = std::exp (-1000.0f / (attackMs * (float) sr));
        float r = std::exp (-1000.0f / (releaseMs * (float) sr));
        float envL = std::abs (L);
        float envR = std::abs (R);
        zL = envL > zL ? a * zL + (1.0f - a) * envL : r * zL + (1.0f - r) * envL;
        zR = envR > zR ? a * zR + (1.0f - a) * envR : r * zR + (1.0f - r) * envR;
    }

    float getLevelL() const noexcept { return zL; }
    float getLevelR() const noexcept { return zR; }
    float getLevel()  const noexcept { return (zL + zR) * 0.5f; }
};

//==============================================================================
// Simple LUFS meter (short-term, uses K-weighting filter)
struct LufsMeter
{
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset();

    void process (const float* L, const float* R, int n);

    float getIntegrated() const noexcept { return integratedLUFS; }
    float getShortTerm()  const noexcept { return shortTermLUFS; }
    float getMaxMomentary() const noexcept { return maxMomentary; }

private:
    double sr = 44100.0;
    float integratedLUFS = -70.0f;
    float shortTermLUFS = -70.0f;
    float maxMomentary = -70.0f;

    // Pre-filter state (K-weighting)
    float hpL = 0.0f, hpR = 0.0f;
    float shelvL = 0.0f, shelvR = 0.0f;

    // RMS accumulation (400ms blocks for momentary, 3s for short-term)
    float momentarySum = 0.0f;
    int momentaryCount = 0;
    float shortTermSum = 0.0f;
    int shortTermCount = 0;

    static constexpr int momentaryBlock = (int) (0.4 * 44100);
    static constexpr int shortTermBlock = (int) (3.0 * 44100);
};

//==============================================================================
// Peak level capture for meters
struct PeakMeter
{
    float peakL = 0.0f, peakR = 0.0f;
    float heldL = 0.0f, heldR = 0.0f;
    float holdTime = 2.0f;
    int holdSamples = 0;

    void prepare (double sampleRate) { sr = sampleRate; holdSamples = (int) (holdTime * sr); reset(); }
    void reset() { peakL = 0.0f; peakR = 0.0f; heldL = 0.0f; heldR = 0.0f; decayCount = 0; }
    void process (float L, float R);

    float getPeakL() const noexcept { return std::max (std::abs (peakL), std::abs (heldL)); }
    float getPeakR() const noexcept { return std::max (std::abs (peakR), std::abs (heldR)); }

private:
    double sr = 44100.0;
    int decayCount = 0;
};

//==============================================================================
// Oversampling (simple 2x)
struct Oversampler
{
    void prepare (double sampleRate, int maxBlock);
    void reset();

    int process (const float* inL, const float* inR, float* outL, float* outR, int n, int factor);

private:
    double sr = 44100.0;
    int maxBlock = 0;
    // Linear interpolation halfband for now - full polyphase later
    std::vector<float> bufL, bufR;
    float zL = 0.0f, zR = 0.0f;
};

} // namespace flowform
