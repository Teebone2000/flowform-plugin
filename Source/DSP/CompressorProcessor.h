#pragma once

#include <JuceHeader.h>

//==============================================================================
class CompressorProcessor
{
public:
    enum CompType
    {
        CLASSIC = 0,
        MODERN,
        VINTAGE,
        NUM_TYPES
    };
    
    enum MSMode
    {
        STEREO = 0,
        MID,
        SIDE,
        M_TO_S,
        S_TO_M,
        NUM_MS_MODES
    };
    
    CompressorProcessor();
    ~CompressorProcessor() = default;
    
    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    //==============================================================================
    void setEnabled(bool enabled) { this->enabled = enabled; }
    void setSolo(bool solo) { this->solo = solo; }
    void setDelta(bool delta) { this->delta = delta; }
    
    void setThreshold(float thresholdDb);
    void setRatio(float ratio);
    void setAttack(float attackMs);
    void setRelease(float releaseMs);
    void setMakeup(float makeupDb);
    void setSidechainHPF(float freqHz);
    void setStereoLink(float linkPercent);
    
    void setCompType(CompType type);
    void setMSMode(MSMode mode);
    
    //==============================================================================
    float getGainReduction() const { return currentGR; }
    float getInputLevel() const { return inputLevel; }
    float getOutputLevel() const { return outputLevel; }
    
private:
    //==============================================================================
    void updateParameters();
    void processStereo(juce::AudioBuffer<float>& buffer);
    void processMidSide(juce::AudioBuffer<float>& buffer);
    
    float calculateGainReduction(float level, float threshold, float ratio, float kneeWidth);
    void applyGainReduction(juce::AudioBuffer<float>& buffer, float gr);
    
    //==============================================================================
    juce::dsp::ProcessSpec spec;
    
    // Parameters
    bool enabled = true;
    bool solo = false;
    bool delta = false;
    
    float thresholdDb = -19.4f;
    float ratio = 1.81f;
    float attackMs = 6.3f;
    float releaseMs = 163.7f;
    float makeupDb = 0.0f;
    float sidechainHPF = 90.0f;
    float stereoLink = 100.0f;
    
    CompType compType = CLASSIC;
    MSMode msMode = STEREO;
    
    // State variables
    float currentGR = 0.0f;
    float inputLevel = -60.0f;
    float outputLevel = -60.0f;
    
    // Envelope followers (2 channels max)
    std::array<float, 2> envelope { 0.0f, 0.0f };
    
    // Sidechain filters
    juce::dsp::IIR::Filter<float> scFilter[2];
    
    // Attack/release coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // Knee width (dB)
    float kneeWidth = 6.0f;
    
    // Lookahead (samples) for lookahead compression
    static constexpr int lookaheadSamples = 32;
    juce::AudioBuffer<float> lookaheadBuffer;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorProcessor)
};
