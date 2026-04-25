#pragma once

#include <JuceHeader.h>

//==============================================================================
class InputProcessor
{
public:
    InputProcessor();
    ~InputProcessor() = default;
    
    //==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    //==============================================================================
    void setTrim(float trimDb);
    void setLoPass(float freqHz);
    void setHiPass(float freqHz);
    void setVoice(float voice);      // -50 to +50
    void setVoiceBias(float bias);   // -50 to +50
    
    void setMono(bool mono);
    void setPolarity(bool invert);
    void setDelta(bool delta);
    void setCompensate(bool compensate);
    
    //==============================================================================
    float getInputLevelL() const { return inputLevelL; }
    float getInputLevelR() const { return inputLevelR; }
    float getOutputLevelL() const { return outputLevelL; }
    float getOutputLevelR() const { return outputLevelR; }
    
private:
    //==============================================================================
    void updateFilters();
    void processVoiceTilt(float* left, float* right, int numSamples);
    
    //==============================================================================
    juce::dsp::ProcessSpec spec;
    
    // Parameters
    float trimDb = 0.0f;
    float loPassFreq = 20000.0f;
    float hiPassFreq = 20.0f;
    float voice = 0.0f;     // -50 to +50
    float voiceBias = 0.0f; // -50 to +50
    
    bool mono = false;
    bool invertPolarity = false;
    bool delta = false;
    bool compensate = false;
    
    // Derived parameters
    float trimGain = 1.0f;
    float compensationGain = 1.0f;
    
    // Filters
    juce::dsp::IIR::Filter<float> loPassFilterL;
    juce::dsp::IIR::Filter<float> loPassFilterR;
    juce::dsp::IIR::Filter<float> hiPassFilterL;
    juce::dsp::IIR::Filter<float> hiPassFilterR;
    
    // Voice tilt filter (shelving EQ)
    struct VoiceFilter
    {
        void prepare(double sampleRate);
        void process(float* left, float* right, int numSamples, float tilt, float bias);
        
    private:
        double sampleRate = 44100.0;
        float tilt = 0.0f;
        float bias = 0.0f;
        
        // State for each channel
        struct ChannelState
        {
            float x1 = 0.0f, x2 = 0.0f;
            float y1 = 0.0f, y2 = 0.0f;
        };
        
        ChannelState stateL, stateR;
        
        void updateCoefficients();
    };
    
    VoiceFilter voiceFilter;
    
    // State variables
    float inputLevelL = -60.0f;
    float inputLevelR = -60.0f;
    float outputLevelL = -60.0f;
    float outputLevelR = -60.0f;
    
    // Delta mode buffer (stores dry signal)
    juce::AudioBuffer<float> dryBuffer;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputProcessor)
};
