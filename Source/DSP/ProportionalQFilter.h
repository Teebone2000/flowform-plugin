#pragma once

#include <JuceHeader.h>

//==============================================================================
class ProportionalQFilter
{
public:
    enum FilterType
    {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        LowShelf,
        HighShelf,
        Peak
    };
    
    ProportionalQFilter();
    ~ProportionalQFilter() = default;
    
    //==============================================================================
    void prepare(double sampleRate);
    void process(juce::AudioBuffer<float>& buffer, int channel = 0);
    float processSample(float x, int channel = 0);
    void reset();
    
    //==============================================================================
    void setFrequency(float freqHz);
    void setQ(float qValue);
    void setGain(float gainDb);
    void setFilterType(FilterType type);
    
    // Advanced Q scaling
    void setQMin(float qMin) { this->qMin = qMin; }
    void setQMax(float qMax) { this->qMax = qMax; }
    void setQScale(float scale); // 0-1 for proportional Q
    
    //==============================================================================
    float getFrequency() const { return frequency; }
    float getQ() const { return q; }
    float getGain() const { return gain; }
    
private:
    //==============================================================================
    void updateCoefficients();
    
    //==============================================================================
    double sampleRate = 44100.0;
    
    // Filter parameters
    float frequency = 1000.0f;
    float q = 0.707f;
    float gain = 0.0f;
    FilterType filterType = LowPass;
    
    // Q scaling parameters
    float qMin = 0.1f;
    float qMax = 10.0f;
    float qScale = 0.5f;
    
    // State variables (per channel)
    struct FilterState
    {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };
    
    std::vector<FilterState> states;
    
    // Coefficients
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProportionalQFilter)
};