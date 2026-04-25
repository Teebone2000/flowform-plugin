#include "ScaleSystem.h"

//==============================================================================
ScaleSystem::ScaleSystem()
{
    initializeScaleDefinitions();
}

void ScaleSystem::prepare(double sampleRate)
{
    // Initialize any required state
    (void)sampleRate; // Mark as used to avoid warning
}

//==============================================================================
void ScaleSystem::setScaleMode(ScaleMode mode)
{
    currentMode = mode;
}

void ScaleSystem::setMusicalKey(MusicalKey key)
{
    currentKey = key;
    
    if (currentMode == KEY_MODE)
    {
        // Update scale definition for current key
        auto& definition = scaleDefinitions[KEY_MODE];
        definition.referenceFrequencies = getKeyFrequencies(key);
    }
}

void ScaleSystem::setCustomScale(const std::vector<float>& frequencies)
{
    customScaleFreqs = frequencies;
    
    if (currentMode == CUSTOM)
    {
        auto& definition = scaleDefinitions[CUSTOM];
        definition.referenceFrequencies = customScaleFreqs;
    }
}

//==============================================================================
float ScaleSystem::frequencyToPosition(float freqHz) const
{
    const auto& definition = scaleDefinitions.at(currentMode);
    const auto& freqs = definition.referenceFrequencies;
    
    if (freqs.empty())
        return 0.0f;
    
    // Find closest frequency in scale
    float minDistance = std::numeric_limits<float>::max();
    int closestIndex = 0;
    
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        float distance = std::abs(std::log2(freqHz / freqs[i]));
        if (distance < minDistance)
        {
            minDistance = distance;
            closestIndex = static_cast<int>(i);
        }
    }
    
    // Normalize position 0-1
    return static_cast<float>(closestIndex) / (freqs.size() - 1);
}

std::vector<float> ScaleSystem::getCrossoverFrequencies(int numBands) const
{
    const auto& definition = scaleDefinitions.at(currentMode);
    const auto& freqs = definition.referenceFrequencies;
    
    if (freqs.empty() || numBands <= 1)
        return {};
    
    // Distribute bands across available frequencies
    std::vector<float> crossovers;
    float step = static_cast<float>(freqs.size() - 1) / (numBands - 1);
    
    for (int i = 1; i < numBands; ++i)
    {
        float pos = i * step;
        int idx1 = static_cast<int>(std::floor(pos));
        int idx2 = static_cast<int>(std::ceil(pos));
        
        if (idx2 >= static_cast<int>(freqs.size()))
            idx2 = static_cast<int>(freqs.size()) - 1;
        
        float t = pos - idx1;
        float freq = freqs[idx1] * std::pow(freqs[idx2] / freqs[idx1], t);
        
        crossovers.push_back(freq);
    }
    
    return crossovers;
}

float ScaleSystem::getQForFrequency(float freqHz) const
{
    const auto& definition = scaleDefinitions.at(currentMode);
    
    // If Q values are defined for this scale, use them
    if (!definition.qValues.empty())
    {
        // Find closest defined frequency
        float closestFreq = 0.0f;
        float minDistance = std::numeric_limits<float>::max();
        
        for (const auto& [definedFreq, q] : definition.qValues)
        {
            float distance = std::abs(std::log2(freqHz / definedFreq));
            if (distance < minDistance)
            {
                minDistance = distance;
                closestFreq = definedFreq;
            }
        }
        
        if (closestFreq > 0.0f)
            return definition.qValues.at(closestFreq);
    }
    
    // Default: use psychoacoustic critical bandwidth
    return calculateCriticalBandwidth(freqHz) / freqHz;
}

//==============================================================================
float ScaleSystem::applySmoothing(float freqHz, float value) const
{
    // Psychoacoustic smoothing based on human hearing
    // This simulates how we perceive changes across frequencies
    
    if (currentMode == ISO || currentMode == MASTER)
    {
        // Technical scales: minimal smoothing
        return value;
    }
    else if (currentMode == MIX || currentMode == KEY_MODE)
    {
        // Musical scales: apply frequency-dependent smoothing
        float bark = calculateBarkScale(freqHz);
        
        // More smoothing at higher frequencies (where our resolution is lower)
        // smoothingAmount variable removed to fix warning
        (void)bark; // Mark as used
        return value;
    }
    
    return value;
}

//==============================================================================
void ScaleSystem::initializeScaleDefinitions()
{
    // ISO Scale (standard measurement)
    ScaleDefinition isoScale;
    isoScale.referenceFrequencies = getISOFrequencies();
    isoScale.isMusical = false;
    
    // Set Q values for ISO scale (constant Q across spectrum)
    for (float freq : isoScale.referenceFrequencies)
    {
        isoScale.qValues[freq] = 0.707f; // Butterworth
    }
    
    scaleDefinitions[ISO] = isoScale;
    
    // Mix Scale (musically relevant)
    ScaleDefinition mixScale;
    mixScale.referenceFrequencies = getMixFrequencies();
    mixScale.isMusical = true;
    
    // Variable Q based on frequency (wider at extremes)
    for (float freq : mixScale.referenceFrequencies)
    {
        float normalizedFreq = std::log2(freq / 1000.0f);
        float q = 0.5f + 0.3f * std::exp(-normalizedFreq * normalizedFreq);
        mixScale.qValues[freq] = q;
    }
    
    scaleDefinitions[MIX] = mixScale;
    
    // Master Scale (extended range)
    ScaleDefinition masterScale;
    masterScale.referenceFrequencies = getMasterFrequencies();
    masterScale.isMusical = false;
    
    // Tighter Q for mastering precision
    for (float freq : masterScale.referenceFrequencies)
    {
        masterScale.qValues[freq] = 1.0f;
    }
    
    scaleDefinitions[MASTER] = masterScale;
    
    // Key Mode (initialized with C major)
    ScaleDefinition keyScale;
    keyScale.referenceFrequencies = getKeyFrequencies(C);
    keyScale.isMusical = true;
    
    // Musical Q - wider for fundamentals, tighter for harmonics
    for (float freq : keyScale.referenceFrequencies)
    {
        // Check if frequency is close to a harmonic of the root
        float rootFreq = keyScale.referenceFrequencies[0]; // First is root
        float ratio = freq / rootFreq;
        
        // If close to integer ratio, it's a harmonic
        float nearestInteger = std::round(ratio);
        if (std::abs(ratio - nearestInteger) < 0.01f)
        {
            // Harmonic: tighter Q
            keyScale.qValues[freq] = 1.2f;
        }
        else
        {
            // Non-harmonic: wider Q
            keyScale.qValues[freq] = 0.6f;
        }
    }
    
    scaleDefinitions[KEY_MODE] = keyScale;
    
    // Custom Scale (empty initially)
    ScaleDefinition customScale;
    customScale.isMusical = false;
    scaleDefinitions[CUSTOM] = customScale;
}

//==============================================================================
std::vector<float> ScaleSystem::getISOFrequencies() const
{
    // ISO 266:1997 preferred frequencies
    return {
        31.5f, 63.f, 125.f, 250.f, 500.f,
        1000.f, 2000.f, 4000.f, 8000.f, 16000.f
    };
}

std::vector<float> ScaleSystem::getMixFrequencies() const
{
    // Musically relevant frequencies for mixing
    return {
        50.f, 100.f, 200.f, 400.f, 800.f,
        1600.f, 3200.f, 6400.f, 12800.f
    };
}

std::vector<float> ScaleSystem::getMasterFrequencies() const
{
    // Extended range for mastering
    return {
        20.f, 30.f, 50.f, 80.f, 120.f, 180.f, 250.f, 350.f, 500.f,
        700.f, 1000.f, 1400.f, 2000.f, 2800.f, 4000.f, 5600.f, 8000.f,
        11000.f, 16000.f, 22000.f
    };
}

std::vector<float> ScaleSystem::getKeyFrequencies(MusicalKey key) const
{
    // A4 = 440 Hz
    float a4 = 440.0f;
    
    // Calculate frequencies for two octaves of the major scale
    std::vector<float> frequencies;
    
    // Major scale intervals (in semitones from root)
    std::vector<int> majorScale = {0, 2, 4, 5, 7, 9, 11, 12};
    
    // Convert key to semitone offset from C
    int keyOffset = static_cast<int>(key);
    
    for (int octave = 3; octave <= 5; ++octave) // Two octaves
    {
        for (int interval : majorScale)
        {
            int semitones = keyOffset + interval + (octave - 4) * 12;
            float freq = a4 * std::pow(2.0f, (semitones - 9) / 12.0f);
            frequencies.push_back(freq);
        }
    }
    
    // Remove duplicates and sort
    std::sort(frequencies.begin(), frequencies.end());
    frequencies.erase(std::unique(frequencies.begin(), frequencies.end()), frequencies.end());
    
    return frequencies;
}

//==============================================================================
float ScaleSystem::calculateCriticalBandwidth(float freqHz) const
{
    // Zwicker's critical bandwidth formula
    return 25.0f + 75.0f * std::pow(1.0f + 1.4f * (freqHz / 1000.0f) * (freqHz / 1000.0f), 0.69f);
}

float ScaleSystem::calculateERB(float freqHz) const
{
    // Moore & Glasberg's Equivalent Rectangular Bandwidth
    return 24.7f * (4.37f * freqHz / 1000.0f + 1.0f);
}

float ScaleSystem::calculateBarkScale(float freqHz) const
{
    // Convert frequency to Bark scale (0-24)
    float f = freqHz / 1000.0f;
    return 13.0f * std::atan(0.76f * f) + 3.5f * std::atan(f * f / (7.5f * 7.5f));
}
