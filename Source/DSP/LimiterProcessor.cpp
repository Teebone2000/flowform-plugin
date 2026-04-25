#include "LimiterProcessor.h"
#include <new>

//==============================================================================
LimiterProcessor::LimiterProcessor()
{
    // Default values from Figma
    thresholdDb = -8.0f;
    gainDb = 3.0f;
    attackMs = 1.5f;
    ceilingDb = 0.0f;
    releaseMs = 59.0f;
}

void LimiterProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    
    // Prepare lookahead delay
    lookaheadDelay.prepare(spec);
    lookaheadDelay.setMaximumDelayInSamples(lookaheadSamples);
    lookaheadDelay.setDelay(static_cast<float>(lookaheadSamples));
    
    // Prepare oversampler for true peak detection (2x)
    new (&oversampler) juce::dsp::Oversampling<float>(
        spec.numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    );
    oversampler.initProcessing(spec.maximumBlockSize);
    
    // Calculate coefficients
    updateParameters();
    
    reset();
}

void LimiterProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;
    
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    // Apply input gain
    float inputGain = std::pow(10.0f, gainDb / 20.0f);
    buffer.applyGain(inputGain);
    
    // Update input level
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* samples = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float absSample = std::abs(samples[i]);
            inputLevel = 0.999f * inputLevel + 0.001f * absSample;
        }
    }
    
    // Process through lookahead delay
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Write to delay line
            lookaheadDelay.pushSample(ch, samples[i]);
            
            // Read delayed sample (with lookahead)
            float delayed = lookaheadDelay.popSample(ch);
            
            // Calculate gain reduction based on delayed sample
            float peak = std::abs(delayed);
            float targetGainReduction = calculateGainReduction(peak);
            
            // Smooth gain reduction with attack/release
            if (targetGainReduction < currentGR)
            {
                // Attack
                currentGR = attackCoeff * currentGR + (1.0f - attackCoeff) * targetGainReduction;
            }
            else
            {
                // Release
                currentGR = releaseCoeff * currentGR + (1.0f - releaseCoeff) * targetGainReduction;
            }
            
            // Apply gain reduction to current sample
            float gain = std::pow(10.0f, currentGR / 20.0f);
            samples[i] *= gain;
            
            // Apply ceiling (brick wall)
            if (std::abs(samples[i]) > ceilingGain)
            {
                samples[i] = std::copysign(ceilingGain, samples[i]);
            }
            
            // Update output level
            outputLevel = 0.999f * outputLevel + 0.001f * std::abs(samples[i]);
        }
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

//==============================================================================
void LimiterProcessor::setThreshold(float thresholdDb)
{
    this->thresholdDb = thresholdDb;
}

void LimiterProcessor::setGain(float gainDb)
{
    this->gainDb = gainDb;
}

void LimiterProcessor::setAttack(float attackMs)
{
    this->attackMs = attackMs;
    updateParameters();
}

void LimiterProcessor::setCeiling(float ceilingDb)
{
    this->ceilingDb = ceilingDb;
    ceilingGain = std::pow(10.0f, ceilingDb / 20.0f);
}

void LimiterProcessor::setRelease(float releaseMs)
{
    this->releaseMs = releaseMs;
    updateParameters();
}

//==============================================================================
void LimiterProcessor::updateParameters()
{
    // Convert attack/release times to coefficients
    // Limiter uses faster times than compressor
    attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * spec.sampleRate));
    releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * spec.sampleRate));
}

float LimiterProcessor::calculateGainReduction(float peak)
{
    if (peak <= 0.0f)
        return 0.0f;
    
    float peakDb = 20.0f * std::log10(peak);
    float thresholdDb = this->thresholdDb;
    
    if (peakDb <= thresholdDb)
    {
        // Below threshold - no limiting
        return 0.0f;
    }
    else
    {
        // Above threshold - hard limit
        // For a limiter, ratio is essentially infinite
        return thresholdDb - peakDb;
    }
}
