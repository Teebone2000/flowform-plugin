#include "MeterProcessor.h"

namespace flowform
{

//==============================================================================
void LufsMeter::reset()
{
    integratedLUFS = -70.0f;
    shortTermLUFS = -70.0f;
    maxMomentary = -70.0f;
    hpL = hpR = 0.0f;
    shelvL = shelvR = 0.0f;
    momentarySum = 0.0f;
    momentaryCount = 0;
    shortTermSum = 0.0f;
    shortTermCount = 0;
}

void LufsMeter::process (const float* L, const float* R, int n)
{
    // K-weighting: 2nd-order HPF at 38Hz + shelving filter
    const float a0_hp = 0.9982f;
    const float b1_hp = -0.9982f;
    // Simplified shelving at 1.5kHz (pre-filter from ITU-R BS.1770-4)
    const float a0_sh = 0.9996f;
    const float b1_sh = -0.9996f;

    for (int i = 0; i < n; ++i)
    {
        // HPF
        float hpOutL = L[i] * a0_hp - b1_hp * hpL;
        hpL = L[i];
        float hpOutR = R[i] * a0_hp - b1_hp * hpR;
        hpR = R[i];

        // Shelving
        float shOutL = hpOutL * a0_sh - b1_sh * shelvL;
        shelvL = hpOutL;
        float shOutR = hpOutR * a0_sh - b1_sh * shelvR;
        shelvR = hpOutR;

        float sq = shOutL * shOutL + shOutR * shOutR;

        // Momentary (400ms blocks)
        momentarySum += sq;
        momentaryCount++;
        if (momentaryCount >= momentaryBlock)
        {
            float lufs = -0.691f + 10.0f * std::log10 (momentarySum / momentaryCount);
            if (lufs < -70.0f) lufs = -70.0f;
            maxMomentary = std::max (maxMomentary, lufs);
            momentarySum = 0.0f;
            momentaryCount = 0;
        }

        // Short-term (3s blocks)
        shortTermSum += sq;
        shortTermCount++;
        if (shortTermCount >= shortTermBlock)
        {
            shortTermLUFS = -0.691f + 10.0f * std::log10 (shortTermSum / shortTermCount);
            if (shortTermLUFS < -70.0f) shortTermLUFS = -70.0f;
            shortTermSum = 0.0f;
            shortTermCount = 0;
        }
    }
}

//==============================================================================
void PeakMeter::process (float L, float R)
{
    peakL = std::abs (L);
    peakR = std::abs (R);

    if (peakL > heldL)
    {
        heldL = peakL;
        decayCount = 0;
    }
    if (peakR > heldR)
    {
        heldR = peakR;
        decayCount = 0;
    }

    decayCount++;
    if (decayCount > holdSamples)
    {
        heldL *= 0.99f;
        heldR *= 0.99f;
        if (heldL < peakL) heldL = peakL;
        if (heldR < peakR) heldR = peakR;
    }
}

//==============================================================================
void Oversampler::prepare (double sampleRate, int maxB)
{
    sr = sampleRate;
    maxBlock = maxB;
    bufL.resize ((size_t) maxBlock * 4);
    bufR.resize ((size_t) maxBlock * 4);
    reset();
}

void Oversampler::reset()
{
    zL = 0.0f;
    zR = 0.0f;
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
}

int Oversampler::process (const float* inL, const float* inR, float* outL, float* outR, int n, int factor)
{
    // Simple linear upsampler — 2x zero-stuff + linear interpolation
    if (factor <= 1) { memcpy (outL, inL, (size_t) n * sizeof (float)); memcpy (outR, inR, (size_t) n * sizeof (float)); return n; }

    int outN = n * factor;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < factor; ++j)
        {
            int idx = i * factor + j;
            float t = (float) j / (float) factor;
            float a = (i > 0 ? inL[i-1] : zL);
            float b = inL[i];
            bufL[(size_t) idx] = a + (b - a) * t;
            a = (i > 0 ? inR[i-1] : zR);
            b = inR[i];
            bufR[(size_t) idx] = a + (b - a) * t;
        }
    }

    zL = inL[n-1];
    zR = inR[n-1];

    memcpy (outL, bufL.data(), (size_t) outN * sizeof (float));
    memcpy (outR, bufR.data(), (size_t) outN * sizeof (float));
    return outN;
}

} // namespace flowform
