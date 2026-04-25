#include "ClipperProcessor.h"
#include <new> // Added as per instruction

//==============================================================================
ClipperProcessor::ClipperProcessor()
{
    // Default values from Figma
    drive = 18.0f;
    softness = 50.0f;
    link = 18.0f;
}

void ClipperProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    
    // Prepare oversampler (2x for anti-aliasing)
    {
        // Explicitly destroy any existing instance, then reconstruct in place
        oversampler.~Oversampling();
        new (&oversampler) juce::dsp::Oversampling<float>(
            static_cast<size_t>(spec.numChannels),
            1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
        );
        oversampler.initProcessing(static_cast<size_t>(spec.maximumBlockSize));
    }
    
    // Prepare DC blocking filters (high-pass at 20Hz)
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);
    dcFilterL.coefficients = coeffs;
    dcFilterR.coefficients = coeffs;
    
    reset();
}

void ClipperProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;
    
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    // Apply drive
    float driveGain = 1.0f + drive * 0.02f; // 0-100 maps to 1-3x gain
    buffer.applyGain(driveGain);
    
    // Convert softness (0-100) to knee parameter
    float knee = softness * 0.01f; // 0-1
    
    // Convert link (0-100) to stereo linking amount
    float linkAmount = link * 0.01f; // 0-1
    
    // Process samples
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        auto& dcFilter = (ch == 0) ? dcFilterL : dcFilterR;
        
        float prevSample = 0.0f;
        
        for (int i = 0; i < numSamples; ++i)
        {
            float x = samples[i];
            
            // Apply stereo linking (simplified)
            if (ch == 1 && linkAmount > 0.0f) // Right channel
            {
                // Blend with left channel based on link amount
                float linked = samples[i] * (1.0f - linkAmount) + prevSample * linkAmount;
                x = linked;
            }
            
            // Apply clipping based on type
            float threshold = 0.9f; // Base threshold
            
            switch (clipType)
            {
                case SOFT:
                    x = softClip(x, threshold, knee);
                    break;
                    
                case HARD:
                    x = hardClip(x, threshold);
                    break;
                    
                case FOLDING:
                    x = foldbackClip(x, threshold);
                    break;
                    
                default:
                    x = softClip(x, threshold, knee);
                    break;
            }
            
            // Remove DC offset
            x = dcFilter.processSample(x);
            
            samples[i] = x;
            
            // Store for linking
            if (ch == 0) // Left channel
            {
                prevSample = x;
            }
            
            // Update output level
            outputLevel = 0.999f * outputLevel + 0.001f * std::abs(x);
        }
    }
}

void ClipperProcessor::reset()
{
    outputLevel = -60.0f;
    
    // Reset oversampler if it has been initialised
    oversampler.reset();
    dcFilterL.reset();
    dcFilterR.reset();
}

//==============================================================================
void ClipperProcessor::setDrive(float d)
{
    drive = juce::jlimit(0.0f, 100.0f, d);
}

void ClipperProcessor::setSoftness(float s)
{
    softness = juce::jlimit(0.0f, 100.0f, s);
}

void ClipperProcessor::setLink(float l)
{
    link = juce::jlimit(0.0f, 100.0f, l);
}

//==============================================================================
float ClipperProcessor::softClip(float x, float threshold, float softness)
{
    float absX = std::abs(x);
    
    if (absX <= threshold)
    {
        // Below threshold - no clipping
        return x;
    }
    else
    {
        // Above threshold - soft knee
        float overshoot = absX - threshold;
        float knee = softness * 0.5f; // Adjust knee based on softness
        
        // Cubic soft knee
        float reduction = overshoot / (1.0f + overshoot * knee);
        
        return std::copysign(threshold + reduction, x);
    }
}

float ClipperProcessor::hardClip(float x, float threshold)
{
    if (x > threshold)
        return threshold;
    else if (x < -threshold)
        return -threshold;
    else
        return x;
}

float ClipperProcessor::foldbackClip(float x, float threshold)
{
    float absX = std::abs(x);
    
    if (absX <= threshold)
    {
        // Below threshold - no folding
        return x;
    }
    else
    {
        // Fold back
        float folded = 2.0f * threshold - absX;
        
        // If still above threshold, fold again (can create interesting harmonics)
        while (folded > threshold)
        {
            folded = 2.0f * threshold - folded;
        }
        
        return std::copysign(folded, x);
    }
}

