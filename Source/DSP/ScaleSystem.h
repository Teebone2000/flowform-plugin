#pragma once

#include <JuceHeader.h>

//==============================================================================
class ScaleSystem
{
public:
    enum ScaleMode
    {
        ISO,        // ISO 266:1997 (standard)
        MIX,        // Mix engineer (musical)
        MASTER,     // Mastering (extended)
        KEY_MODE,   // Musical key-based
        CUSTOM      // User-defined
    };
    
    enum MusicalKey
    {
        C, C_SHARP, D, D_SHARP, E, F, F_SHARP, G, G_SHARP, A, A_SHARP, B,
        NUM_KEYS
    };
    
    ScaleSystem();
    ~ScaleSystem() = default;
    
    //==============================================================================
    void prepare(double sampleRate);
    
    //==============================================================================
    void setScaleMode(ScaleMode mode);
    void setMusicalKey(MusicalKey key);
    void setCustomScale(const std::vector<float>& frequencies);
    
    //==============================================================================
    // Convert frequency to musical position based on current scale
    float frequencyToPosition(float freqHz) const;
    
    // Get crossover frequencies for current scale
    std::vector<float> getCrossoverFrequencies(int numBands) const;
    
    // Get Q values for filters based on scale
    float getQForFrequency(float freqHz) const;
    
    //==============================================================================
    // Psychoacoustic smoothing
    float applySmoothing(float freqHz, float value) const;
    
private:
    //==============================================================================
    ScaleMode currentMode = ISO;
    MusicalKey currentKey = C;
    
    // Custom scale frequencies
    std::vector<float> customScaleFreqs;
    
    // Scale definitions
    struct ScaleDefinition
    {
        std::vector<float> referenceFrequencies;
        std::map<float, float> qValues; // frequency -> Q mapping
        bool isMusical = false;
    };
    
    std::map<ScaleMode, ScaleDefinition> scaleDefinitions;
    
    //==============================================================================
    void initializeScaleDefinitions();
    
    // ISO standard frequencies
    std::vector<float> getISOFrequencies() const;
    
    // Mix engineer frequencies (musically relevant)
    std::vector<float> getMixFrequencies() const;
    
    // Mastering frequencies (extended range)
    std::vector<float> getMasterFrequencies() const;
    
    // Key-based frequencies
    std::vector<float> getKeyFrequencies(MusicalKey key) const;
    
    //==============================================================================
    // Psychoacoustic functions
    float calculateCriticalBandwidth(float freqHz) const;
    float calculateERB(float freqHz) const; // Equivalent Rectangular Bandwidth
    float calculateBarkScale(float freqHz) const;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaleSystem)
};
