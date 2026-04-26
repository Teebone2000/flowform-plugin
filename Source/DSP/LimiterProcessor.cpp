#include "LimiterProcessor.h"
#include <new>

LimiterProcessor::LimiterProcessor()
{
    thresholdDb = -8.0f;
    gainDb = 3.0f;
    attackMs = 1.5f;
    ceilingDb = 0.0f;
    releaseMs = 59.0f;
    ceilingGain = 1.0f;
}

void LimiterProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;

    lookaheadDelay.prepare (spec);
    lookaheadDelay.setMaximumDelayInSamples (lookaheadSamples);
    lookaheadDelay.setDelay (lookaheadSamples);

    new (&oversampler) juce::dsp::Oversampling<float>(
        spec.numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler.initProcessing (spec.maximumBlockSize);

    updateParameters();
    reset();
}

void LimiterProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;

    const auto n = buffer.getNumSamples();
    const auto ch = buffer.getNumChannels();

    // Input gain stage
    float inputGain = std::pow (10.0f, gainDb / 20.0f);
    buffer.applyGain (inputGain);

    // Track peak envelope (stereo linked via max of both channels)
    auto* writeL = buffer.getWritePointer (0);
    auto* writeR = buffer.getWritePointer (ch > 1 ? 1 : 0);

    for (int i = 0; i < n; ++i)
    {
        // Lookahead: write current, read delayed sample
        float delayedL = lookaheadDelay.popSample (0, 0.0f);
        lookaheadDelay.pushSample (0, writeL[i]);

        float delayedR = 0.0f;
        if (ch > 1)
        {
            delayedR = lookaheadDelay.popSample (1, 0.0f);
            lookaheadDelay.pushSample (1, writeR[i]);
        }

        // Stereo-linked peak detection — use MAX of both channels
        float peak = std::max (std::abs (delayedL), ch > 1 ? std::abs (delayedR) : 0.0f);
        float targetGR = calculateGainReduction (peak);

        // Correct envelope logic:
        // Attack = target is MORE gain reduction (more negative) than current
        // Release = target is LESS gain reduction than current
        if (targetGR < currentGR)
            currentGR = attackCoeff * currentGR + (1.0f - attackCoeff) * targetGR;
        else
            currentGR = releaseCoeff * currentGR + (1.0f - releaseCoeff) * targetGR;

        // Apply same gain to both channels (linked limiter)
        float gain = std::pow (10.0f, currentGR / 20.0f);
        writeL[i] *= gain;
        if (ch > 1) writeR[i] *= gain;

        // Brick-wall ceiling
        if (std::abs (writeL[i]) > ceilingGain)
            writeL[i] = std::copysign (ceilingGain, writeL[i]);
        if (ch > 1 && std::abs (writeR[i]) > ceilingGain)
            writeR[i] = std::copysign (ceilingGain, writeR[i]);

        // Metering
        inputLevel = 0.999f * inputLevel + 0.001f * peak;
        float outPeak = std::max (std::abs (writeL[i]), ch > 1 ? std::abs (writeR[i]) : 0.0f);
        outputLevel = 0.999f * outputLevel + 0.001f * outPeak;
    }
}

void LimiterProcessor::reset()
{
    envelope = 0.0f;
    currentGR = 0.0f;
    inputLevel = -60.0f;
    outputLevel = -60.0f;
    lookaheadDelay.reset();
    oversampler.reset();
}

void LimiterProcessor::setThreshold(float t) { thresholdDb = t; }
void LimiterProcessor::setGain(float g)      { gainDb = g; }

void LimiterProcessor::setAttack(float ms)
{
    attackMs = ms;
    updateParameters();
}

void LimiterProcessor::setCeiling(float db)
{
    ceilingDb = db;
    ceilingGain = std::pow (10.0f, db / 20.0f);
}

void LimiterProcessor::setRelease(float ms)
{
    releaseMs = ms;
    updateParameters();
}

void LimiterProcessor::updateParameters()
{
    attackCoeff  = std::exp (-1.0f / (attackMs  * 0.001f * (float) spec.sampleRate));
    releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * (float) spec.sampleRate));
}

float LimiterProcessor::calculateGainReduction(float peak)
{
    if (peak <= 0.0f) return 0.0f;

    float peakDb = 20.0f * std::log10 (peak + 1e-12f);
    if (peakDb <= thresholdDb) return 0.0f;

    // Infinite ratio: any overshoot gets pulled back to threshold
    return thresholdDb - peakDb;
}
