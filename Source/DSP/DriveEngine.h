#pragma once

#include <JuceHeader.h>

//==============================================================================
class DriveEngine
{
public:
    enum Algorithm
    {
        TUBE = 0,
        TAPE,
        SOLID_STATE,
        TRANSFORMER,
        DIGITAL,
        TRANSISTOR,
        NUM_ALGORITHMS
    };
    
    DriveEngine();
    ~DriveEngine() = default;
    
    //==============================================================================
    void prepare(double sampleRate);
    void process(juce::AudioBuffer<float>& buffer, int algorithm, float drive);
    void reset();
    
    //==============================================================================
    void setSampleRate(double sampleRate) { this->sampleRate = sampleRate; }
    void setDrive(float drive) { currentDrive = drive; }
    void setAlgorithm(int algorithm) { currentAlgorithm = algorithm; }
    
    //==============================================================================
    // Advanced tone shaping
    void setVoiceTilt(float tilt); // -50 to +50
    void setVoiceBias(float bias); // -50 to +50
    void setEQBias(float bias);    // EQ curve bias
    void setEQFlip(bool flip);     // Invert EQ curve
    void setEQMult(float mult);    // Scale EQ curve
    
    //==============================================================================
    // Modular transformer simulation (6 engines in series)
    struct TransformerStage
    {
        float saturation = 0.0f;
        float hysteresis = 0.0f;
        float frequencyResponse = 1.0f;
        float phaseShift = 0.0f;
        bool enabled = true;
    };
    
    void configureTransformerStages(const std::array<TransformerStage, 6>& stages);
    
private:
    //==============================================================================
    double sampleRate = 44100.0;
    float currentDrive = 0.0f;
    int currentAlgorithm = TUBE;
    
    // Tone shaping parameters
    float voiceTilt = 0.0f;
    float voiceBias = 0.0f;
    float eqBias = 0.0f;
    bool eqFlip = false;
    float eqMult = 1.0f;
    
    // Transformer stages
    std::array<TransformerStage, 6> transformerStages;
    
    // State for filters and memory
    std::array<float, 2> dcOffset{0.0f, 0.0f};
    std::array<float, 2> lastSample{0.0f, 0.0f};
    
    //==============================================================================
    // Algorithm implementations
    float processTube(float x, float drive);
    float processTape(float x, float drive);
    float processSolidState(float x, float drive);
    float processTransformer(float x, float drive);
    float processDigital(float x, float drive);
    float processTransistor(float x, float drive);
    
    // Helper functions
    float applyToneShaping(float x, int channel);
    float applyTransformerChain(float x, int channel);
    
    // Nonlinear functions
    float tanhSoftClip(float x);
    float asymmetricClip(float x);
    float diodeClipping(float x);
    float magneticHysteresis(float x, float& state);
    
    // Filter for tone shaping
    class ToneFilter
    {
    public:
        void prepare(double sampleRate);
        float process(float x);
        void setTilt(float tilt); // -1 to 1
        void setBias(float bias); // -1 to 1
        
    private:
        double sampleRate = 44100.0;
        float tilt = 0.0f;
        float bias = 0.0f;
        
        // State variables for filters
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
        
        void updateCoefficients();
    };
    
    std::array<ToneFilter, 2> toneFilters;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriveEngine)
};
