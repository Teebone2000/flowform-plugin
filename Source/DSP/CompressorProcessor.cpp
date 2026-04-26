#include "CompressorProcessor.h"

CompressorProcessor::CompressorProcessor()
{
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

    // Per-channel SC HPF filters
    auto scCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, sidechainHPF);
    for (auto& f : scFilter) f.coefficients = scCoeffs;

    lookaheadBuffer.setSize ((int) spec.numChannels, lookaheadSamples);
    updateParameters();
    reset();
}

void CompressorProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;

    if (buffer.getNumChannels() < 1) return;

    if (msMode == STEREO || buffer.getNumChannels() < 2)
        processStereo (buffer);
    else
        processMidSide (buffer);
}

void CompressorProcessor::reset()
{
    envelope.fill (0.0f);
    currentGR = 0.0f;
    inputLevel = -60.0f;
    outputLevel = -60.0f;
    scFilter[0].reset();
    scFilter[1].reset();
    lookaheadBuffer.clear();
}

void CompressorProcessor::setThreshold(float t) { thresholdDb = t; }
void CompressorProcessor::setRatio(float r)     { ratio = juce::jmax (1.0f, r); }

void CompressorProcessor::setAttack(float ms)
{
    attackMs = ms;
    updateParameters();
}

void CompressorProcessor::setRelease(float ms)
{
    releaseMs = ms;
    updateParameters();
}

void CompressorProcessor::setMakeup(float db)       { makeupDb = db; }

void CompressorProcessor::setSidechainHPF(float f)
{
    sidechainHPF = f;
    auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, f);
    scFilter[0].coefficients = c;
    scFilter[1].coefficients = c;
}

void CompressorProcessor::setStereoLink(float p)    { stereoLink = p; }

void CompressorProcessor::setCompType(CompType t)
{
    compType = t;
    switch (compType)
    {
        case CLASSIC: kneeWidth = 6.0f;  break;
        case MODERN:  kneeWidth = 3.0f;  break;
        case VINTAGE: kneeWidth = 10.0f; break;
        default:      kneeWidth = 6.0f;  break;
    }
}

void CompressorProcessor::setMSMode(MSMode m) { msMode = m; }

void CompressorProcessor::updateParameters()
{
    float atkS = attackMs * 0.001f;
    float relS = releaseMs * 0.001f;
    attackCoeff  = std::exp (-1.0f / (atkS * (float) spec.sampleRate));
    releaseCoeff = std::exp (-1.0f / (relS * (float) spec.sampleRate));
}

// ── Stereo compressor with envelope follower ─────────────────────────────
void CompressorProcessor::processStereo(juce::AudioBuffer<float>& buffer)
{
    const auto n = buffer.getNumSamples();
    const bool isStereo = buffer.getNumChannels() > 1;

    auto* L = buffer.getWritePointer (0);
    auto* R = isStereo ? buffer.getWritePointer (1) : L;

    // makeupLin was here
    float linkFactor = stereoLink * 0.01f;

    for (int i = 0; i < n; ++i)
    {
        float inL = L[i];
        float inR = R[i];

        // Sidechain HPF
        float scL = scFilter[0].processSample (inL);
        float scR = isStereo ? scFilter[1].processSample (inR) : scL;

        // Peak detector envelope (per channel)
        float absL = std::abs (scL);
        float absR = std::abs (scR);

        float envL = (absL > envelope[0]) ? attackCoeff * envelope[0] + (1.0f - attackCoeff) * absL
                                          : releaseCoeff * envelope[0] + (1.0f - releaseCoeff) * absL;
        float envR = (absR > envelope[1]) ? attackCoeff * envelope[1] + (1.0f - attackCoeff) * absR
                                          : releaseCoeff * envelope[1] + (1.0f - releaseCoeff) * absR;
        envelope[0] = envL;
        envelope[1] = envR;

        // Convert to dB
        float envLin = envL * (1.0f - linkFactor) + (envL + envR) * 0.5f * linkFactor;
        float envDb = 20.0f * std::log10 (envLin + 1e-12f);

        // Gain reduction in dB
        float grDb = calculateGainReduction (envDb, thresholdDb, ratio, kneeWidth);

        // Apply gain + makeup
        float gainLin = std::pow (10.0f, (grDb + makeupDb) / 20.0f);
        L[i] *= gainLin;
        if (isStereo) R[i] *= gainLin;

        // Metering
        currentGR = 0.995f * currentGR + 0.005f * grDb;
    }
}

// ── M/S processing ──────────────────────────────────────────────────────
void CompressorProcessor::processMidSide(juce::AudioBuffer<float>& buffer)
{
    const auto n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    juce::HeapBlock<float> mid (n), side (n);

    // Encode M/S
    for (int i = 0; i < n; ++i)
    {
        mid[i]  = (L[i] + R[i]) * 0.5f;
        side[i] = (L[i] - R[i]) * 0.5f;
    }

        auto compressChannel = [&] (float* buf, size_t chIdx, int nSmp)
    {
        for (int i = 0; i < nSmp; ++i)
        {
            float absV = std::abs (buf[i]);
            float env = (absV > envelope[chIdx]) ? attackCoeff * envelope[chIdx] + (1.0f - attackCoeff) * absV
                                                 : releaseCoeff * envelope[chIdx] + (1.0f - releaseCoeff) * absV;
            envelope[chIdx] = env;
            float envDb = 20.0f * std::log10 (env + 1e-12f);
            float grDb = calculateGainReduction (envDb, thresholdDb, ratio, kneeWidth);
            buf[i] *= std::pow (10.0f, (grDb + makeupDb) / 20.0f);
            currentGR = 0.995f * currentGR + 0.005f * grDb;
        }
    };

    switch (msMode)
    {
        case MID:   compressChannel (mid,  0, n); break;
        case SIDE:  compressChannel (side, 1, n); break;
        case M_TO_S:
        {
            // Mid controls side compression
            for (int i = 0; i < n; ++i)
            {
                float absM = std::abs (mid[i]);
                float envM = (absM > envelope[0]) ? attackCoeff * envelope[0] + (1.0f - attackCoeff) * absM
                                                  : releaseCoeff * envelope[0] + (1.0f - releaseCoeff) * absM;
                envelope[0] = envM;
                float gr = calculateGainReduction (20.0f * std::log10 (envM + 1e-12f), thresholdDb, ratio, kneeWidth);
                float g = std::pow (10.0f, (gr + makeupDb) / 20.0f);
                side[i] *= g;
            }
            break;
        }
        case S_TO_M:
        {
            for (int i = 0; i < n; ++i)
            {
                float absS = std::abs (side[i]);
                float envS = (absS > envelope[1]) ? attackCoeff * envelope[1] + (1.0f - attackCoeff) * absS
                                                  : releaseCoeff * envelope[1] + (1.0f - releaseCoeff) * absS;
                envelope[1] = envS;
                float gr = calculateGainReduction (20.0f * std::log10 (envS + 1e-12f), thresholdDb, ratio, kneeWidth);
                float g = std::pow (10.0f, (gr + makeupDb) / 20.0f);
                mid[i] *= g;
            }
            break;
        }
        default: break;
    }

    // Decode back
    for (int i = 0; i < n; ++i)
    {
        L[i] = mid[i] + side[i];
        R[i] = mid[i] - side[i];
    }
}

// ── Gain reduction calculation ──────────────────────────────────────────
float CompressorProcessor::calculateGainReduction(float lvlDb, float thrDb, float rat, float knee)
{
    if (lvlDb <= thrDb - knee * 0.5f)
        return 0.0f;

    if (lvlDb >= thrDb + knee * 0.5f)
        return (thrDb - lvlDb) * (1.0f - 1.0f / rat);

    // Knee region: quadratic transition
    float x = lvlDb - (thrDb - knee * 0.5f);
    float t = x / knee;
    float curve = 1.0f - 1.0f / rat;
    return -knee * 0.5f * curve * t * t;
}
