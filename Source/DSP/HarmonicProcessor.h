#pragma once

#include <JuceHeader.h>

//==============================================================================
class HarmonicProcessor
{
public:
    enum HarmonicType
    {
        EVEN = 0,
        ODD,
        OCTAVE,
        FIFTH,
        MIXED,
        NUM_TYPES
    };
    
    HarmonicProcessor();
    ~HarmonicProcessor() = default;
    
    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    //==============================================================================
    void setMTrim(float mTrimDb);
    void setHarmonics(float amount); // 0-100
    void setShape(float shape);      // 0-100
    void setDepth(float depth);      // 0-100
    void setGlobalMix(float mix);    // 0-100%
    void setOutputTrim(float trimDb);
    
    //==============================================================================
    float getOutputLevel() const { return outputLevel; }
    
private:
    //==============================================================================
    void generateHarmonics(float* samples, int numSamples, int channel);
    void updateHarmonicWeights();
    
    // Harmonic generation methods
    float addEvenHarmonics(float x, float amount);
    float addOddHarmonics(float x, float amount);
    float addOctaveHarmonics(float x, float amount);
    float addFifthHarmonics(float x, float amount);
    float addMixedHarmonics(float x, float amount);
    
    // Wave shaping
    float waveshape(float x, float shape);
    
    //==============================================================================
    juce::dsp::ProcessSpec spec;
    
    // Parameters
    float mTrimDb = -15.0f;
    float harmonics = 24.0f;  // 0-100
    float shape = 18.0f;      // 0-100
    float depth = 24.0f;      // 0-100
    float globalMix = 100.0f; // 0-100%
    float outputTrimDb = -19.0f;
    
    // Derived parameters
    float mTrimGain = 1.0f;
    float outputTrimGain = 1.0f;
    float mixWet = 1.0f;
    float mixDry = 0.0f;
    
    // Harmonic weights
    std::array<float, 8> harmonicWeights; // Up to 8th harmonic
    
    // Harmonic type
    HarmonicType harmonicType = MIXED;
    
    // State variables
    float outputLevel = -60.0f;
    
    // DC blocking
    std::array<float, 2> dcState{0.0f, 0.0f};
    
    // Anti-aliasing filter
    juce::dsp::IIR::Filter<float> antiAliasFilterL;
    juce::dsp::IIR::Filter<float> antiAliasFilterR;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicProcessor)
};
