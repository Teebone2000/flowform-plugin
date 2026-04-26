#include "HarmonicProcessor.h"

//==============================================================================
HarmonicProcessor::HarmonicProcessor()
{
    // Default values from Figma
    mTrimDb = -15.0f;
    harmonics = 24.0f;
    shape = 18.0f;
    depth = 24.0f;
    globalMix = 100.0f;
    outputTrimDb = -19.0f;
    
    updateHarmonicWeights();
}

void HarmonicProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    
    mTrimGain = std::pow (10.0f, mTrimDb / 20.0f);
    outputTrimGain = std::pow (10.0f, outputTrimDb / 20.0f);
    mixWet = globalMix;
    mixDry = 1.0f - mixWet;
    
    float nyquist = (float) spec.sampleRate * 0.5f;
    float cutoff = nyquist * 0.5f;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, cutoff);
    antiAliasFilterL.coefficients = coeffs;
    antiAliasFilterR.coefficients = coeffs;
    
    reset();
}

void HarmonicProcessor::process(juce::AudioBuffer<float>& buffer)
{
    // Pass through if no harmonics active
    if (harmonics < 0.5f && globalMix < 0.5f)
        return;
    
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    // Store dry signal for mixing
    juce::AudioBuffer<float> dryBuffer(numChannels, numSamples);
    dryBuffer.makeCopyOf(buffer);
    
    // Apply M Trim gain
    buffer.applyGain(mTrimGain);
    
    // Process each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        generateHarmonics(buffer.getWritePointer(ch), numSamples, ch);
    }
    
    // Depth controls the waveshaping character, not a separate gain stage.
    // The actual waveshaping intensity is modulated by 'shape' and 'harmonics'.
    juce::ignoreUnused (depth);
    
    // Apply anti-aliasing filter
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& filter = (ch == 0) ? antiAliasFilterL : antiAliasFilterR;
        
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.getWritePointer(ch)[i] = filter.processSample(buffer.getWritePointer(ch)[i]);
        }
    }
    
    // Apply global mix (dry/wet)
    if (mixWet < 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            
            for (int i = 0; i < numSamples; ++i)
            {
                wet[i] = wet[i] * mixWet + dry[i] * mixDry;
            }
        }
    }
    
    // Apply output trim
    buffer.applyGain(outputTrimGain);
    
    // Update output level
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* samples = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            outputLevel = 0.999f * outputLevel + 0.001f * std::abs(samples[i]);
        }
    }
}

void HarmonicProcessor::reset()
{
    outputLevel = -60.0f;
    dcState.fill(0.0f);
    
    antiAliasFilterL.reset();
    antiAliasFilterR.reset();
}

//==============================================================================
void HarmonicProcessor::setMTrim(float mTrimDb)
{
    this->mTrimDb = mTrimDb;
    mTrimGain = std::pow(10.0f, mTrimDb / 20.0f);
}

void HarmonicProcessor::setHarmonics(float amount)
{
    harmonics = juce::jlimit(0.0f, 100.0f, amount);
    updateHarmonicWeights();
}

void HarmonicProcessor::setShape(float s)
{
    shape = juce::jlimit(0.0f, 100.0f, s);
}

void HarmonicProcessor::setDepth(float d)
{
    depth = juce::jlimit(0.0f, 100.0f, d);
}

void HarmonicProcessor::setGlobalMix(float mix)
{
    // Input is 0..1 from editor; store as fraction
    globalMix = juce::jlimit (0.0f, 1.0f, mix);
    mixWet = globalMix;
    mixDry = 1.0f - mixWet;
}

void HarmonicProcessor::setOutputTrim(float trimDb)
{
    outputTrimDb = trimDb;
    outputTrimGain = std::pow(10.0f, outputTrimDb / 20.0f);
}

//==============================================================================
void HarmonicProcessor::generateHarmonics(float* samples, int numSamples, int channel)
{
    float harmonicAmount = harmonics * 0.01f;
    float shapeAmount = shape * 0.01f;
    
    // DC blocking
    float dc = dcState[channel];
    float dcCoeff = 0.999f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float x = samples[i];
        
        // Remove DC
        x -= dc;
        dc = dcCoeff * dc + (1.0f - dcCoeff) * samples[i];
        
        // Apply waveshaping for harmonic generation
        x = waveshape(x, shapeAmount);
        
        // Add specific harmonics based on type
        switch (harmonicType)
        {
            case EVEN:
                x = addEvenHarmonics(x, harmonicAmount);
                break;
                
            case ODD:
                x = addOddHarmonics(x, harmonicAmount);
                break;
                
            case OCTAVE:
                x = addOctaveHarmonics(x, harmonicAmount);
                break;
                
            case FIFTH:
                x = addFifthHarmonics(x, harmonicAmount);
                break;
                
            case MIXED:
            default:
                x = addMixedHarmonics(x, harmonicAmount);
                break;
        }
        
        // Apply harmonic weights
        float weighted = 0.0f;
        for (int h = 0; h < 8; ++h)
        {
            // This is simplified - actual harmonic synthesis would be more complex
            float harmonic = std::sin(x * static_cast<float>(h + 1));
            weighted += harmonic * harmonicWeights[h];
        }
        
        // Blend with original
        x = x * (1.0f - harmonicAmount) + weighted * harmonicAmount;
        
        samples[i] = x;
    }
    
    dcState[channel] = dc;
}

void HarmonicProcessor::updateHarmonicWeights()
{
    // Generate harmonic weights based on current settings
    // This creates different harmonic spectra
    
    float base = harmonics * 0.01f;
    
    switch (harmonicType)
    {
        case EVEN:
            // Even harmonics only (2nd, 4th, 6th, 8th)
            harmonicWeights = {0.0f, base, 0.0f, base * 0.5f, 0.0f, base * 0.25f, 0.0f, base * 0.125f};
            break;
            
        case ODD:
            // Odd harmonics only (3rd, 5th, 7th)
            harmonicWeights = {0.0f, 0.0f, base, 0.0f, base * 0.5f, 0.0f, base * 0.25f, 0.0f};
            break;
            
        case OCTAVE:
            // Octave harmonics (2nd, 4th, 8th)
            harmonicWeights = {0.0f, base, 0.0f, base * 0.7f, 0.0f, 0.0f, 0.0f, base * 0.3f};
            break;
            
        case FIFTH:
            // Fifth harmonics (3rd, 6th)
            harmonicWeights = {0.0f, 0.0f, base, 0.0f, 0.0f, base * 0.5f, 0.0f, 0.0f};
            break;
            
        case MIXED:
        default:
            // Mixed harmonics (full spectrum)
            harmonicWeights = {base * 0.1f, base * 0.8f, base * 0.6f, base * 0.4f, 
                              base * 0.3f, base * 0.2f, base * 0.1f, base * 0.05f};
            break;
    }
}

float HarmonicProcessor::addEvenHarmonics(float x, float amount)
{
    // Generate even harmonics (2nd, 4th, 6th, 8th)
    float x2 = x * x * amount * 0.5f;
    float x4 = x2 * x2 * amount * 0.25f;
    float x6 = x2 * x2 * x2 * amount * 0.125f;
    
    return x + x2 - x4 + x6;
}

float HarmonicProcessor::addOddHarmonics(float x, float amount)
{
    // Generate odd harmonics (3rd, 5th, 7th)
    float x3 = x * x * x * amount * 0.3f;
    float x5 = x3 * x * x * amount * 0.2f;
    float x7 = x5 * x * x * amount * 0.1f;
    
    return x + x3 - x5 + x7;
}

float HarmonicProcessor::addOctaveHarmonics(float x, float amount)
{
    // Octave up (2nd harmonic)
    float octave = std::sin(x * 2.0f * 3.14159f) * amount * 0.3f;
    
    // Two octaves up (4th harmonic)
    float twoOctaves = std::sin(x * 4.0f * 3.14159f) * amount * 0.1f;
    
    return x + octave + twoOctaves;
}

float HarmonicProcessor::addFifthHarmonics(float x, float amount)
{
    // Perfect fifth (3:2 ratio)
    float fifth = std::sin(x * 1.5f * 3.14159f) * amount * 0.4f;
    
    // Octave+fifth (3rd harmonic)
    float octaveFifth = std::sin(x * 3.0f * 3.14159f) * amount * 0.2f;
    
    return x + fifth + octaveFifth;
}

float HarmonicProcessor::addMixedHarmonics(float x, float amount)
{
    // Full harmonic series
    float harmonic = 0.0f;
    
    for (int i = 1; i <= 8; ++i)
    {
        float weight = harmonicWeights[i - 1];
        harmonic += std::sin(x * static_cast<float>(i) * 3.14159f) * weight;
    }
    
    return x * (1.0f - amount) + harmonic * amount;
}

float HarmonicProcessor::waveshape(float x, float shape)
{
    // Waveshaping function that adds harmonics
    // Shape parameter controls the character
    
    float shaped = x;
    
    // Asymmetric shaping for more character
    if (x > 0.0f)
    {
        // Positive half
        shaped = std::atan(x * (1.0f + shape)) / std::atan(1.0f + shape);
    }
    else
    {
        // Negative half (different curve)
        shaped = std::atan(x * (1.0f + shape * 0.5f)) / std::atan(1.0f + shape * 0.5f);
    }
    
    // Add some polynomial shaping
    float poly = x * x * x * shape * 0.1f;
    shaped += poly;
    
    return shaped;
}
