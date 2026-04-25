#include "ProportionalQFilter.h"

//==============================================================================
ProportionalQFilter::ProportionalQFilter()
{
    // Default values
    frequency = 1000.0f;
    q = 0.707f;
    gain = 0.0f;
    filterType = LowPass;
    
    qMin = 0.1f;
    qMax = 10.0f;
    qScale = 0.5f;
}

void ProportionalQFilter::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
    
    // Initialize state for 2 channels (stereo)
    states.resize(2);
    reset();
    
    updateCoefficients();
}

void ProportionalQFilter::process(juce::AudioBuffer<float>& buffer, int channel)
{
    const auto numSamples = buffer.getNumSamples();
    
    if (channel < 0 || channel >= buffer.getNumChannels())
        return;
    
    auto* samples = buffer.getWritePointer(channel);
    
    for (int i = 0; i < numSamples; ++i)
    {
        samples[i] = processSample(samples[i], channel);
    }
}

float ProportionalQFilter::processSample(float x, int channel)
{
    if (channel < 0 || channel >= states.size())
        return x;
    
    auto& state = states[channel];
    
    // Direct Form II transposed
    float y = (b0 * x + state.x1) / a0;
    state.x1 = b1 * x + state.y1 - a1 * y;
    state.y1 = b2 * x + state.y2 - a2 * y;
    state.y2 = b2 * x - a2 * y;
    
    return y;
}

void ProportionalQFilter::reset()
{
    for (auto& state : states)
    {
        state.x1 = state.x2 = 0.0f;
        state.y1 = state.y2 = 0.0f;
    }
}

//==============================================================================
void ProportionalQFilter::setFrequency(float freqHz)
{
    frequency = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.5f), freqHz);
    updateCoefficients();
}

void ProportionalQFilter::setQ(float qValue)
{
    q = juce::jlimit(0.1f, 10.0f, qValue);
    updateCoefficients();
}

void ProportionalQFilter::setGain(float gainDb)
{
    gain = gainDb;
    updateCoefficients();
}

void ProportionalQFilter::setFilterType(FilterType type)
{
    filterType = type;
    updateCoefficients();
}

void ProportionalQFilter::setQScale(float scale)
{
    qScale = juce::jlimit(0.0f, 1.0f, scale);
    
    // Calculate Q based on scale
    q = qMin + (qMax - qMin) * qScale;
    updateCoefficients();
}

//==============================================================================
void ProportionalQFilter::updateCoefficients()
{
    // Calculate normalized frequency
    float w0 = 2.0f * juce::MathConstants<float>::pi * frequency / static_cast<float>(sampleRate);
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / (2.0f * q);
    
    // Calculate A for shelving/peaking filters
    float A = std::pow(10.0f, gain / 40.0f);
    float sqrtA = std::sqrt(A);
    
    // Reset coefficients
    b0 = b1 = b2 = a0 = a1 = a2 = 0.0f;
    
    switch (filterType)
    {
        case LowPass:
            b0 = (1.0f - cosw0) / 2.0f;
            b1 = 1.0f - cosw0;
            b2 = (1.0f - cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
            
        case HighPass:
            b0 = (1.0f + cosw0) / 2.0f;
            b1 = -(1.0f + cosw0);
            b2 = (1.0f + cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
            
        case BandPass:
            b0 = sinw0 / 2.0f;
            b1 = 0.0f;
            b2 = -sinw0 / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
            
        case Notch:
            b0 = 1.0f;
            b1 = -2.0f * cosw0;
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
            
        case LowShelf:
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
            a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
            break;
            
        case HighShelf:
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
            a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
            break;
            
        case Peak:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosw0;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha / A;
            break;
    }
    
    // Normalize by a0
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;
    a0 = 1.0f;
}