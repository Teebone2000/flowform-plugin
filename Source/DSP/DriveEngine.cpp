#include "DriveEngine.h"

//==============================================================================
DriveEngine::DriveEngine()
{
    // Initialize transformer stages
    for (auto& stage : transformerStages)
    {
        stage.enabled = true;
        stage.saturation = 0.5f;
        stage.hysteresis = 0.2f;
        stage.frequencyResponse = 1.0f;
        stage.phaseShift = 0.0f;
    }
}

void DriveEngine::prepare(double sampleRate)
{
    this->sampleRate = sampleRate;
    
    // Prepare tone filters
    for (auto& filter : toneFilters)
    {
        filter.prepare(sampleRate);
    }
    
    // Reset state
    reset();
}

void DriveEngine::process(juce::AudioBuffer<float>& buffer, int algorithm, float drive)
{
    currentAlgorithm = algorithm;
    currentDrive = drive;
    
    // Pass through clean if no drive
    if (std::abs (drive) < 0.001f)
        return;
    
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float x = samples[i];
            
            // Apply tone shaping
            x = applyToneShaping(x, ch);
            
            // Apply drive based on algorithm
            switch (currentAlgorithm)
            {
                case TUBE:        x = processTube(x, currentDrive); break;
                case TAPE:        x = processTape(x, currentDrive); break;
                case SOLID_STATE: x = processSolidState(x, currentDrive); break;
                case TRANSFORMER: x = processTransformer(x, currentDrive); break;
                case DIGITAL:     x = processDigital(x, currentDrive); break;
                case TRANSISTOR:  x = processTransistor(x, currentDrive); break;
                default:          x = processTube(x, currentDrive); break;
            }
            
            // Apply transformer chain for all algorithms (adds character)
            x = applyTransformerChain(x, ch);
            
            samples[i] = x;
        }
    }
}

void DriveEngine::reset()
{
    dcOffset.fill(0.0f);
    lastSample.fill(0.0f);
    
    for (auto& filter : toneFilters)
    {
        // Reset filter state
    }
}

//==============================================================================
void DriveEngine::setVoiceTilt(float tilt)
{
    voiceTilt = juce::jlimit(-50.0f, 50.0f, tilt) / 50.0f; // Normalize to -1..1
    
    for (auto& filter : toneFilters)
    {
        filter.setTilt(voiceTilt);
    }
}

void DriveEngine::setVoiceBias(float bias)
{
    voiceBias = juce::jlimit(-50.0f, 50.0f, bias) / 50.0f; // Normalize to -1..1
    
    for (auto& filter : toneFilters)
    {
        filter.setBias(voiceBias);
    }
}

void DriveEngine::setEQBias(float bias)
{
    eqBias = bias;
}

void DriveEngine::setEQFlip(bool flip)
{
    eqFlip = flip;
}

void DriveEngine::setEQMult(float mult)
{
    eqMult = mult;
}

void DriveEngine::configureTransformerStages(const std::array<TransformerStage, 6>& stages)
{
    transformerStages = stages;
}

//==============================================================================
float DriveEngine::processTube(float x, float drive)
{
    // Tube saturation simulation
    float gain = 1.0f + drive * 0.1f;
    x *= gain;
    
    // Soft asymmetric clipping
    if (x > 0.0f)
        x = tanhSoftClip(x * 1.2f) * 0.833f; // Slightly different for positive
    else
        x = tanhSoftClip(x * 0.8f) * 1.25f; // Different for negative
    
    // Add even harmonics characteristic of tubes
    float evenHarm = x * x * 0.1f * (drive / 100.0f);
    x += evenHarm;
    
    // DC blocking
    float dc = dcOffset[0];
    dc = 0.999f * dc + 0.001f * x;
    x -= dc;
    dcOffset[0] = dc;
    
    return x;
}

float DriveEngine::processTape(float x, float drive)
{
    // Tape saturation with hysteresis
    float gain = 1.0f + drive * 0.05f;
    x *= gain;
    
    // Magnetic hysteresis simulation
    float hysteresis = transformerStages[0].hysteresis * 0.1f;
    float state = lastSample[0];
    
    // Simple hysteresis model
    float threshold = 0.1f;
    if (std::abs(x - state) > threshold)
    {
        // Hysteresis effect
        if (x > state)
            x += hysteresis * 0.05f;
        else
            x -= hysteresis * 0.05f;
    }
    
    // Asymmetric tape saturation
    if (x > 0.0f)
        x = tanhSoftClip(x);
    else
        x = tanhSoftClip(x * 0.7f) * 1.428f; // More compression on negative
    
    // Add tape compression (soft knee)
    float compression = 0.5f + 0.5f * (drive / 100.0f);
    x = x / (1.0f + std::abs(x) * compression);
    
    lastSample[0] = x;
    
    return x;
}

float DriveEngine::processSolidState(float x, float drive)
{
    // Solid-state transistor saturation
    float gain = 1.0f + drive * 0.15f;
    x *= gain;
    
    // Diode-like clipping
    x = diodeClipping(x);
    
    // Add odd harmonics
    float x3 = x * x * x;
    x += x3 * 0.05f * (drive / 100.0f);
    
    // Harder clipping characteristic
    float limit = 0.8f;
    if (x > limit) x = limit + (x - limit) * 0.3f;
    if (x < -limit) x = -limit + (x + limit) * 0.3f;
    
    return x;
}

float DriveEngine::processTransformer(float x, float drive)
{
    // Transformer saturation with multiple stages
    x = applyTransformerChain(x, 0);
    
    // Additional saturation
    float gain = 1.0f + drive * 0.08f;
    x *= gain;
    
    // Core saturation curve
    x = x / (1.0f + x * x * 0.5f);
    
    return x;
}

float DriveEngine::processDigital(float x, float drive)
{
    // Digital clipping with aliasing prevention
    float gain = 1.0f + drive * 0.2f;
    x *= gain;
    
    // Hard clip with oversampling consideration
    float limit = 0.95f;
    if (x > limit) x = limit;
    if (x < -limit) x = -limit;
    
    // Add digital distortion harmonics
    float folded = std::sin(x * 3.14159f) * 0.1f * (drive / 100.0f);
    x += folded;
    
    return x;
}

float DriveEngine::processTransistor(float x, float drive)
{
    // Transistor fuzz/distortion
    float gain = 1.0f + drive * 0.25f;
    x *= gain;
    
    // Asymmetric transistor clipping
    x = asymmetricClip(x);
    
    // Add transistor characteristic
    if (x > 0.0f)
        x = std::atan(x * 2.0f) * 0.5f;
    else
        x = std::atan(x * 1.5f) * 0.666f;
    
    return x;
}

//==============================================================================
float DriveEngine::applyToneShaping(float x, int channel)
{
    // Apply voice tilt and bias
    x = toneFilters[channel].process(x);
    
    // Apply EQ bias
    if (eqBias != 0.0f)
    {
        float biasFactor = 1.0f + eqBias * 0.01f;
        x *= biasFactor;
    }
    
    // Apply EQ flip
    if (eqFlip)
    {
        // Spectral inversion (simplified)
        x = -x * 0.7f; // Not true spectral inversion but gives a sense of it
    }
    
    // Apply EQ multiplication
    x *= eqMult;
    
    return x;
}

float DriveEngine::applyTransformerChain(float x, int channel)
{
    float result = x;
    
    for (int stage = 0; stage < 6; ++stage)
    {
        if (!transformerStages[stage].enabled)
            continue;
        
        float saturation = transformerStages[stage].saturation;
        float freqResponse = transformerStages[stage].frequencyResponse;
        
        // Apply frequency response (simplified)
        result *= freqResponse;
        
        // Apply stage saturation
        if (saturation > 0.0f)
        {
            float satGain = 1.0f + saturation;
            result *= satGain;
            result = tanhSoftClip(result);
            result /= satGain; // Compensate gain
        }
        
        // Apply hysteresis from this stage
        if (transformerStages[stage].hysteresis > 0.0f)
        {
            static float hysteresisState[6][2] = {{0}};
            result = magneticHysteresis(result, hysteresisState[stage][channel]);
        }
        
        // Apply phase shift (simplified)
        if (transformerStages[stage].phaseShift != 0.0f)
        {
            // Simple all-pass-like effect
            float phase = transformerStages[stage].phaseShift * 0.1f;
            float delayed = lastSample[channel];
            result = result * (1.0f - phase) + delayed * phase;
            lastSample[channel] = result;
        }
    }
    
    return result;
}

//==============================================================================
float DriveEngine::tanhSoftClip(float x)
{
    // Fast tanh approximation
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2 + x4);
}

float DriveEngine::asymmetricClip(float x)
{
    // Asymmetric clipping
    if (x > 0.0f)
        return std::tanh(x * 0.8f) * 1.25f;
    else
        return std::tanh(x * 1.2f) * 0.833f;
}

float DriveEngine::diodeClipping(float x)
{
    // Diode-like exponential clipping
    if (x > 0.0f)
        return 1.0f - std::exp(-x * 1.5f);
    else
        return -1.0f + std::exp(x * 1.5f);
}

float DriveEngine::magneticHysteresis(float x, float& state)
{
    // Simple hysteresis model
    float delta = x - state;
    float hysteresis = 0.05f; // Base hysteresis amount
    
    if (delta > hysteresis)
        state = x - hysteresis;
    else if (delta < -hysteresis)
        state = x + hysteresis;
    else
        state = state; // Stay in hysteresis zone
    
    return state;
}

//==============================================================================
// ToneFilter implementation
void DriveEngine::ToneFilter::prepare(double sr)
{
    sampleRate = sr;
    updateCoefficients();
}

float DriveEngine::ToneFilter::process(float x)
{
    // Simple tilt EQ: boost highs if tilt > 0, boost lows if tilt < 0
    
    // Update filter coefficients based on tilt
    updateCoefficients();
    
    // Simple shelving filter implementation
    float omega = 2.0f * 3.14159f * 1000.0f / sampleRate; // 1kHz center
    float alpha = std::sin(omega) / (2.0f * 0.707f); // Q = 0.707
    
    // Calculate shelving filter coefficients
    float A = std::pow(10.0f, tilt * 0.25f); // Gain based on tilt
    float sqrtA = std::sqrt(A);
    
    float b0 = A * ((A + 1.0f) + (A - 1.0f) * std::cos(omega) + 2.0f * sqrtA * alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * std::cos(omega));
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * std::cos(omega) - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * std::cos(omega) + 2.0f * sqrtA * alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * std::cos(omega));
    float a2 = (A + 1.0f) - (A - 1.0f) * std::cos(omega) - 2.0f * sqrtA * alpha;
    
    // Normalize
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;
    
    // Process with bias
    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    
    // Update state
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    
    // Apply bias (simple gain)
    y *= (1.0f + bias * 0.1f);
    
    return y;
}

void DriveEngine::ToneFilter::setTilt(float t)
{
    tilt = t;
}

void DriveEngine::ToneFilter::setBias(float b)
{
    bias = b;
}

void DriveEngine::ToneFilter::updateCoefficients()
{
    // Coefficients are calculated in process()
}
