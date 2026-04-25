#pragma once

#include <JuceHeader.h>

//==============================================================================
class ClipperProcessor
{
public:
    enum ClipType
    {
        SOFT = 0,
        HARD,
        FOLDING,
        NUM_TYPES
    };
    
    ClipperProcessor();
    ~ClipperProcessor() = default;
    
    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    //==============================================================================
    void setEnabled(bool enabled) { this->enabled = enabled; }
    void setSolo(bool solo) { this->solo = solo; }
    void setDelta(bool delta) { this->delta = delta; }
    
    void setDrive(float drive);
    void setSoftness(float softness);
    void setLink(float link);
    
    void setClipType(ClipType type) { clipType = type; }
    
    //==============================================================================
    float getOutputLevel() const { return outputLevel; }
    
private:
    //==============================================================================
    float processSample(float x, float drive, float softness);
    float softClip(float x, float threshold, float softness);
    float hardClip(float x, float threshold);
    float foldbackClip(float x, float threshold);
    
    //==============================================================================
    juce::dsp::ProcessSpec spec;
    
    // Parameters
    bool enabled = true;
    bool solo = false;
    bool delta = false;
    
    float drive = 18.0f;
    float softness = 50.0f;
    float link = 18.0f;
    
    ClipType clipType = SOFT;
    
    // State variables
    float outputLevel = -60.0f;
    
    // Oversampling for anti-aliasing
    juce::dsp::Oversampling<float> oversampler;
    
    // DC blocking filter
    juce::dsp::IIR::Filter<float> dcFilterL;
    juce::dsp::IIR::Filter<float> dcFilterR;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipperProcessor)
};
