#include "CompressorProcessor.h"

//==============================================================================
CompressorProcessor::CompressorProcessor()
{
    // Initialize with default values from Figma
    thresholdDb = -19.4f;
    ratio = 1.81f;
    attackMs = 6.3f;
    releaseMs = 163.7f;
    makeupDb = 0.0f;
    sidechainHPF = 90.0f;
    stereoLink = 100.0f;
    compType = CLASSIC;
    msMode = STEREO;
}

void CompressorProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    
    // Prepare sidechain filters
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, sidechainHPF);
    sidechainFilterL.coefficients = coeffs;
    sidechainFilterR.coefficients = coeffs;
    
    // Prepare lookahead buffer
    lookaheadBuffer.setSize(spec.numChannels, lookaheadSamples);
    
    // Calculate attack/release coefficients
    updateParameters();
    
    reset();
}

void CompressorProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;
    
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    if (numChannels < 2)
    {
        // Mono processing
        processStereo(buffer);
        return;
    }
    
    switch (msMode)
    {
        case STEREO:
        case MID:
        case SIDE:
        case M_TO_S:
        case S_TO_M:
            processMidSide(buffer);
            break;
        default:
            processStereo(buffer);
            break;
    }
}

void CompressorProcessor::reset()
{
    envelopeL = 0.0f;
    envelopeR = 0.0f;
    envelopeM = 0.0f;
    envelopeS = 0.0f;
    currentGR = 0.0f;
    inputLevel = -60.0f;
    outputLevel = -60.0f;
    
    sidechainFilterL.reset();
    sidechainFilterR.reset();
    lookaheadBuffer.clear();
}

//==============================================================================
void CompressorProcessor::setThreshold(float thresholdDb)
{
    this->thresholdDb = thresholdDb;
}

void CompressorProcessor::setRatio(float ratio)
{
    this->ratio = juce::jmax(1.0f, ratio);
}

void CompressorProcessor::setAttack(float attackMs)
{
    this->attackMs = attackMs;
    updateParameters();
}

void CompressorProcessor::setRelease(float releaseMs)
{
    this->releaseMs = releaseMs;
    updateParameters();
}

void CompressorProcessor::setMakeup(float makeupDb)
{
    this->makeupDb = makeupDb;
}

void CompressorProcessor::setSidechainHPF(float freqHz)
{
    sidechainHPF = freqHz;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, sidechainHPF);
    sidechainFilterL.coefficients = coeffs;
    sidechainFilterR.coefficients = coeffs;
}

void CompressorProcessor::setStereoLink(float linkPercent)
{
    stereoLink = linkPercent;
}

void CompressorProcessor::setCompType(CompType type)
{
    compType = type;
    
    // Adjust knee based on type
    switch (compType)
    {
        case CLASSIC:  kneeWidth = 6.0f; break;
        case MODERN:   kneeWidth = 3.0f; break;
        case VINTAGE:  kneeWidth = 10.0f; break;
        default:       kneeWidth = 6.0f; break;
    }
}

void CompressorProcessor::setMSMode(MSMode mode)
{
    msMode = mode;
}

//==============================================================================
void CompressorProcessor::updateParameters()
{
    // Convert attack/release times to coefficients
    attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * spec.sampleRate));
    releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * spec.sampleRate));
}

void CompressorProcessor::processStereo(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    
    float peakL = 0.0f;
    float peakR = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Get input samples
        float inL = left[i];
        float inR = right[i];
        
        // Update input level (RMS)
        inputLevel = 0.999f * inputLevel + 0.001f * (std::abs(inL) + std::abs(inR)) * 0.5f;
        
        // Sidechain filtering
        float scL = sidechainFilterL.processSample(inL);
        float scR = sidechainFilterR.processSample(inR);
        
        // Calculate envelope (peak detection)
        float envL = std::abs(scL);
        float envR = std::abs(scR);
        
        // Apply attack/release
        envelopeL = (envL > envelopeL) ? 
                   attackCoeff * envelopeL + (1.0f - attackCoeff) * envL :
                   releaseCoeff * envelopeL + (1.0f - releaseCoeff) * envL;
        
        envelopeR = (envR > envelopeR) ?
                   attackCoeff * envelopeR + (1.0f - attackCoeff) * envR :
                   releaseCoeff * envelopeR + (1.0f - releaseCoeff) * envR;
        
        // Convert to dB
        float dbL = 20.0f * std::log10(envelopeL + 1e-6f);
        float dbR = 20.0f * std::log10(envelopeR + 1e-6f);
        
        // Stereo linking
        float linkedDb = (stereoLink * 0.01f) * (dbL + dbR) * 0.5f + 
                        (1.0f - stereoLink * 0.01f) * dbL;
        
        // Calculate gain reduction
        float gr = calculateGainReduction(linkedDb, thresholdDb, ratio, kneeWidth);
        
        // Apply makeup gain
        float makeupGain = std::pow(10.0f, makeupDb / 20.0f);
        
        // Apply gain reduction with makeup
        float gain = std::pow(10.0f, (gr + makeupDb) / 20.0f);
        
        left[i] *= gain;
        right[i] *= gain;
        
        // Update gain reduction meter
        currentGR = 0.995f * currentGR + 0.005f * gr;
        
        // Update output level
        outputLevel = 0.999f * outputLevel + 0.001f * (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
        
        // Track peaks for display
        peakL = std::max(peakL, std::abs(left[i]));
        peakR = std::max(peakR, std::abs(right[i]));
    }
}

void CompressorProcessor::processMidSide(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    
    // Temporary mid/side buffers
    juce::HeapBlock<float> mid(numSamples);
    juce::HeapBlock<float> side(numSamples);
    
    // Encode to Mid/Side
    for (int i = 0; i < numSamples; ++i)
    {
        mid[i] = (left[i] + right[i]) * 0.5f;
        side[i] = (left[i] - right[i]) * 0.5f;
    }
    
    // Process based on MS mode
    float grMid = 0.0f;
    float grSide = 0.0f;
    
    switch (msMode)
    {
        case MID:
            // Only compress mid
            for (int i = 0; i < numSamples; ++i)
            {
                float db = 20.0f * std::log10(std::abs(mid[i]) + 1e-6f);
                grMid = calculateGainReduction(db, thresholdDb, ratio, kneeWidth);
                float gain = std::pow(10.0f, (grMid + makeupDb) / 20.0f);
                mid[i] *= gain;
            }
            break;
            
        case SIDE:
            // Only compress side
            for (int i = 0; i < numSamples; ++i)
            {
                float db = 20.0f * std::log10(std::abs(side[i]) + 1e-6f);
                grSide = calculateGainReduction(db, thresholdDb, ratio, kneeWidth);
                float gain = std::pow(10.0f, (grSide + makeupDb) / 20.0f);
                side[i] *= gain;
            }
            break;
            
        case M_TO_S:
            // Use mid to control side compression
            for (int i = 0; i < numSamples; ++i)
            {
                float dbMid = 20.0f * std::log10(std::abs(mid[i]) + 1e-6f);
                grMid = calculateGainReduction(dbMid, thresholdDb, ratio, kneeWidth);
                float gain = std::pow(10.0f, (grMid + makeupDb) / 20.0f);
                side[i] *= gain;
            }
            break;
            
        case S_TO_M:
            // Use side to control mid compression
            for (int i = 0; i < numSamples; ++i)
            {
                float dbSide = 20.0f * std::log10(std::abs(side[i]) + 1e-6f);
                grSide = calculateGainReduction(dbSide, thresholdDb, ratio, kneeWidth);
                float gain = std::pow(10.0f, (grSide + makeupDb) / 20.0f);
                mid[i] *= gain;
            }
            break;
            
        case STEREO:
        default:
            // Compress both independently
            for (int i = 0; i < numSamples; ++i)
            {
                float dbMid = 20.0f * std::log10(std::abs(mid[i]) + 1e-6f);
                float dbSide = 20.0f * std::log10(std::abs(side[i]) + 1e-6f);
                
                grMid = calculateGainReduction(dbMid, thresholdDb, ratio, kneeWidth);
                grSide = calculateGainReduction(dbSide, thresholdDb, ratio, kneeWidth);
                
                float gainMid = std::pow(10.0f, (grMid + makeupDb) / 20.0f);
                float gainSide = std::pow(10.0f, (grSide + makeupDb) / 20.0f);
                
                mid[i] *= gainMid;
                side[i] *= gainSide;
            }
            break;
    }
    
    // Decode back to Left/Right
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = mid[i] + side[i];
        right[i] = mid[i] - side[i];
        
        // Update gain reduction
        currentGR = 0.995f * currentGR + 0.005f * std::max(grMid, grSide);
        
        // Update levels
        inputLevel = 0.999f * inputLevel + 0.001f * (std::abs(mid[i]) + std::abs(side[i])) * 0.5f;
        outputLevel = 0.999f * outputLevel + 0.001f * (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
    }
}

float CompressorProcessor::calculateGainReduction(float levelDb, float thresholdDb, float ratio, float kneeWidth)
{
    if (levelDb <= thresholdDb - kneeWidth * 0.5f)
    {
        // Below knee - no compression
        return 0.0f;
    }
    else if (levelDb >= thresholdDb + kneeWidth * 0.5f)
    {
        // Above knee - full compression
        return (thresholdDb - levelDb) * (1.0f - 1.0f / ratio);
    }
    else
    {
        // In knee region - smooth transition
        float x = levelDb - (thresholdDb - kneeWidth * 0.5f);
        float knee = x / kneeWidth;
        float curve = 1.0f - 1.0f / ratio;
        return -kneeWidth * 0.5f * curve * knee * knee;
    }
}
