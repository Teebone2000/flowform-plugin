#pragma once

#include <JuceHeader.h>

//==============================================================================
class LimiterProcessor
{
public:
    LimiterProcessor();
    ~LimiterProcessor() = default;
    
    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    //==============================================================================
    void setEnabled(bool enabled) { this->enabled = enabled; }
    void setSolo(bool solo) { this->solo = solo; }
    void setDelta(bool delta) { this->delta = delta; }
    
    void setThreshold(float thresholdDb);
    void setGain(float gainDb);
    void setAttack(float attackMs);
    void setCeiling(float ceilingDb);
    void setRelease(float releaseMs);
    
    //==============================================================================
    float getGainReduction() const { return currentGR; }
    float getInputLevel() const { return inputLevel; }
    float getOutputLevel() const { return outputLevel; }
    
private:
    //==============================================================================
    void updateParameters();
    void applyLookahead();
    
    float calculateGainReduction(float peak);
    
    //==============================================================================
    juce::dsp::ProcessSpec spec;
    
    // Parameters
    bool enabled = true;
    bool solo = false;
    bool delta = false;
    
    float thresholdDb = -8.0f;
    float gainDb = 3.0f;
    float attackMs = 1.5f;
    float ceilingDb = 0.0f;
    float releaseMs = 59.0f;
    
    // State variables
    float currentGR = 0.0f;
    float inputLevel = -60.0f;
    float outputLevel = -60.0f;
    
    // Envelope followers
    float envelope = 0.0f;
    
    // Attack/release coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // Lookahead delay
    static constexpr int lookaheadSamples = 64;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> lookaheadDelay;
    
    // True peak detection
    std::array<float, 4> oversampleBuffer;
    juce::dsp::Oversampling<float> oversampler;
    
    // Ceiling limiter (brick wall)
    float ceilingGain = 1.0f;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterProcessor)
};
