#include "ClipperProcessor.h"
#include <new>

ClipperProcessor::ClipperProcessor()
{
    drive = 18.0f;
    softness = 50.0f;
    link = 18.0f;
}

void ClipperProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;

    {
        oversampler.~Oversampling();
        new (&oversampler) juce::dsp::Oversampling<float>(
            (size_t) spec.numChannels, 1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler.initProcessing ((size_t) spec.maximumBlockSize);
    }

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0f);
    dcFilterL.coefficients = coeffs;
    dcFilterR.coefficients = coeffs;

    reset();
}

void ClipperProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;

    const auto n = buffer.getNumSamples();
    const auto ch = buffer.getNumChannels();

    float driveGain = 1.0f + drive * 0.02f;
    float knee = softness * 0.01f;
    float linkAmt = link * 0.01f;

    // Stereo-linked envelope: the control signal is the max of both channels
    float env = 0.0f; // smoothed envelope follower for linking

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (ch > 1 ? 1 : 0);

    for (int i = 0; i < n; ++i)
    {
        float inL = outL[i] * driveGain;
        float inR = (ch > 1 ? outR[i] : inL) * driveGain;

        // Build linked gain reduction envelope
        float absMax = std::max (std::abs (inL), ch > 1 ? std::abs (inR) : 0.0f);
        env = linkAmt * absMax + (1.0f - linkAmt) * env; // smoothed

        // Reduce both channels by linked envelope if link > 0
        float gainReduction = 1.0f;
        if (linkAmt > 0.001f && env > 0.9f)
            gainReduction = 0.9f / (env + 1e-12f);

        float xL = inL * gainReduction;
        // Clip
        float threshold = 0.9f;
        switch (clipType)
        {
            case SOFT:   xL = softClip (xL, threshold, knee); break;
            case HARD:   xL = hardClip (xL, threshold);       break;
            case FOLDING:xL = foldbackClip (xL, threshold);   break;
            default:     xL = softClip (xL, threshold, knee); break;
        }
        xL = dcFilterL.processSample (xL);
        outL[i] = xL;

        if (ch > 1)
        {
            float xR = inR * gainReduction;
            switch (clipType)
            {
                case SOFT:   xR = softClip (xR, threshold, knee); break;
                case HARD:   xR = hardClip (xR, threshold);       break;
                case FOLDING:xR = foldbackClip (xR, threshold);   break;
                default:     xR = softClip (xR, threshold, knee); break;
            }
            xR = dcFilterR.processSample (xR);
            outR[i] = xR;
        }

        outputLevel = 0.999f * outputLevel + 0.001f * std::abs (outL[i]);
    }
}

void ClipperProcessor::reset()
{
    outputLevel = -60.0f;
    oversampler.reset();
    dcFilterL.reset();
    dcFilterR.reset();
}

void ClipperProcessor::setDrive(float d)     { drive = juce::jlimit (0.0f, 100.0f, d); }
void ClipperProcessor::setSoftness(float s)  { softness = juce::jlimit (0.0f, 100.0f, s); }
void ClipperProcessor::setLink(float l)      { link = juce::jlimit (0.0f, 100.0f, l); }

float ClipperProcessor::softClip(float x, float threshold, float knee)
{
    float absX = std::abs (x);
    if (absX <= threshold) return x;

    float overshoot = absX - threshold;
    float reduction = overshoot / (1.0f + overshoot * knee);
    return std::copysign (threshold + reduction, x);
}

float ClipperProcessor::hardClip(float x, float threshold)
{
    return juce::jlimit (-threshold, threshold, x);
}

float ClipperProcessor::foldbackClip(float x, float threshold)
{
    float absX = std::abs (x);
    if (absX <= threshold) return x;

    float folded = 2.0f * threshold - absX;
    while (std::abs (folded) > threshold)
        folded = 2.0f * threshold - std::abs (folded);

    return std::copysign (folded, x);
}
