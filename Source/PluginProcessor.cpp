#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static juce::String bnd (int b, const juce::String& p) { return ParamIDs::band (b, p); }

juce::AudioProcessorValueTreeState::ParameterLayout FlowFormAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using namespace juce;

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (id, name, r, def));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (id, name, def));
    };
    auto addC = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (id, name, choices, def));
    };

    // Input
    addF (ParamIDs::inTrimDb,  "Trim (dB)",     { -12.0f, 12.0f, 0.01f }, 0.0f);
    addF (ParamIDs::inHPFHz,   "HPF (Hz)",      { 20.0f, 500.0f, 0.01f, 0.35f }, 20.0f);
    addF (ParamIDs::inLPFHz,   "LPF (Hz)",      { 1000.0f, 20000.0f, 0.01f, 0.35f }, 20000.0f);
    addF (ParamIDs::inVoice,   "Voice",         { -50.0f, 50.0f, 0.1f }, 0.0f);
    addF (ParamIDs::inBias,    "Voice Bias",    { -50.0f, 50.0f, 0.1f }, 0.0f);
    addB (ParamIDs::inMono,    "Mono",          false);
    addB (ParamIDs::inPolarity,"Polarity",      false);
    addB (ParamIDs::inDelta,   "Input Delta",   false);
    addB (ParamIDs::inComp,    "Input Comp",    false);

    // Compressor
    addF (ParamIDs::compSC,     "SC HPF (Hz)",  { 20.0f, 500.0f, 0.01f, 0.35f }, 90.0f);
    addF (ParamIDs::compThresh, "Threshold (dB)",{ -40.0f, 0.0f, 0.01f }, -19.4f);
    addF (ParamIDs::compRatio,  "Ratio",         { 1.0f, 20.0f, 0.01f }, 1.81f);
    addF (ParamIDs::compAttack, "Attack (ms)",   { 0.1f, 100.0f, 0.01f, 0.35f }, 6.3f);
    addF (ParamIDs::compRelease,"Release (ms)",  { 10.0f, 1000.0f, 0.01f, 0.35f }, 163.7f);
    addF (ParamIDs::compMakeup, "Makeup (dB)",   { -12.0f, 12.0f, 0.01f }, 0.0f);
    addF (ParamIDs::compStereo, "Stereo Link",   { 0.0f, 1.0f, 0.001f }, 1.0f);
    addC (ParamIDs::compMS,    "M/S Mode",       { "Stereo", "Mid", "Side", "M>S", "S>M" }, 0);
    addC (ParamIDs::compType,  "Comp Type",      { "Classic", "Modern", "Vintage" }, 0);
    addB (ParamIDs::compOn,    "Comp On",        true);
    addB (ParamIDs::compSolo,  "Comp Solo",      false);
    addB (ParamIDs::compDelta, "Comp Delta",     false);

    // Saturation crossovers
    addF (ParamIDs::x1Hz, "Low Split (Hz)",  { 50.0f, 1000.0f, 0.01f, 0.35f }, 250.0f);
    addF (ParamIDs::x2Hz, "Mid Split (Hz)",  { 500.0f, 5000.0f, 0.01f, 0.35f }, 2000.0f);
    addF (ParamIDs::x3Hz, "High Split (Hz)", { 2000.0f, 16000.0f, 0.01f, 0.35f }, 8000.0f);
    addF (ParamIDs::satMix, "Sat Mix", { 0.0f, 1.0f, 0.001f }, 1.0f);
    addB (ParamIDs::satOn,   "Sat On",   true);
    addB (ParamIDs::satSolo, "Sat Solo", false);
    addB (ParamIDs::satDelta,"Sat Delta", false);

    // 4 saturation bands (per blueprint: Tube, Tape, Solid-State, Transformer)
    static const char* algoNames[] = { "Tube", "Tape", "Solid-State", "Transformer" };
    static float defDrives[] = { 22.9f, 22.0f, 23.1f, 18.0f };
    for (int b = 0; b < 4; ++b)
    {
        addF (bnd (b, ParamIDs::driveDb).toRawUTF8(), ("Band " + String(b) + " Drive").toRawUTF8(),
              { 0.0f, 100.0f, 0.1f }, defDrives[b]);
        addF (bnd (b, ParamIDs::mix).toRawUTF8(), ("Band " + String(b) + " Mix").toRawUTF8(),
              { 0.0f, 1.0f, 0.001f }, 1.0f);
        addF (bnd (b, ParamIDs::msFocus).toRawUTF8(), ("Band " + String(b) + " M/S").toRawUTF8(),
              { -100.0f, 100.0f, 0.1f }, 0.0f);
        addC (bnd (b, ParamIDs::algo).toRawUTF8(), ("Band " + String(b) + " Algo").toRawUTF8(),
              { "Tube", "Tape", "Solid-State", "Transformer" }, 0);
        addB (bnd (b, ParamIDs::on).toRawUTF8(),    ("Band " + String(b) + " On").toRawUTF8(), true);
        addB (bnd (b, ParamIDs::solo).toRawUTF8(),  ("Band " + String(b) + " Solo").toRawUTF8(), false);
        addB (bnd (b, ParamIDs::delta).toRawUTF8(), ("Band " + String(b) + " Delta").toRawUTF8(), false);
    }

    // Limiter
    addF (ParamIDs::limitThresh,  "Lim Thresh (dB)", { -40.0f, 0.0f, 0.01f }, -8.0f);
    addF (ParamIDs::limitGain,    "Lim Gain (dB)",   { -12.0f, 12.0f, 0.01f }, 3.0f);
    addF (ParamIDs::limitAttack,  "Lim Attack (ms)", { 0.01f, 10.0f, 0.001f, 0.35f }, 1.5f);
    addF (ParamIDs::limitCeiling, "Lim Ceiling (dB)",{ -3.0f, 0.0f, 0.01f }, 0.0f);
    addF (ParamIDs::limitRelease, "Lim Release (ms)",{ 10.0f, 500.0f, 0.1f, 0.35f }, 59.0f);
    addB (ParamIDs::limitOn,    "Lim On",    true);
    addB (ParamIDs::limitSolo,  "Lim Solo",  false);
    addB (ParamIDs::limitDelta, "Lim Delta", false);

    // Master
    addF (ParamIDs::masterMTrim,    "M Trim (dB)",   { -24.0f, 0.0f, 0.01f }, -15.0f);
    addF (ParamIDs::masterHarmonics,"Harmonics",     { 0.0f, 100.0f, 0.1f }, 24.0f);
    addF (ParamIDs::masterShape,    "Shape",         { 0.0f, 100.0f, 0.1f }, 18.0f);
    addF (ParamIDs::masterDepth,    "Depth",         { 0.0f, 100.0f, 0.1f }, 24.0f);
    addF (ParamIDs::masterMix,      "Master Mix",    { 0.0f, 1.0f, 0.001f }, 1.0f);
    addF (ParamIDs::masterOutTrim,  "Out Trim (dB)", { -24.0f, 24.0f, 0.01f }, -19.0f);
    addB (ParamIDs::masterOn,    "Master On",    true);
    addB (ParamIDs::masterSolo,  "Master Solo",  false);
    addB (ParamIDs::masterDelta, "Master Delta", false);

    // Clipper
    addF (ParamIDs::clipDrive,    "Clip Drive",      { 0.0f, 100.0f, 0.1f }, 18.0f);
    addF (ParamIDs::clipSoftness, "Clip Softness",   { 0.0f, 100.0f, 0.1f }, 50.0f);
    addF (ParamIDs::clipLink,     "Clip Link",       { 0.0f, 100.0f, 0.1f }, 18.0f);
    addB (ParamIDs::clipOn,    "Clip On",    true);
    addB (ParamIDs::clipSolo,  "Clip Solo",  false);
    addB (ParamIDs::clipDelta, "Clip Delta", false);

    // Global
    addB (ParamIDs::bypass,    "Hard Bypass",   false);
    addB (ParamIDs::deltaGlob, "Global Delta",  false);
    addB (ParamIDs::compGlob,  "Global Comp",   false);
    addC (ParamIDs::oversample,"Oversample",    { "1x", "2x", "4x" }, 0);

    return layout;
}

//==============================================================================
FlowFormAudioProcessor::FlowFormAudioProcessor()
: AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                   .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
, apvts (*this, nullptr, juce::Identifier("PARAMS"), createParameterLayout())
{
}

bool FlowFormAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void FlowFormAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    inEnv.prepare (sampleRate);
    inPeak.prepare (sampleRate);

    juce::dsp::ProcessSpec inSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    inEnv.prepare (sampleRate);
    inPeak.prepare (sampleRate);
    compressor.prepare (inSpec);
    compEnv.prepare (sampleRate);
    driveEngine.prepare (sampleRate);
    harmonics.prepare (inSpec);
    limiter.prepare (inSpec);
    clipper.prepare (inSpec);
    compEnv.prepare (sampleRate);

    for (auto& sb : satBands)
        sb.env.prepare (sampleRate);

    driveEngine.prepare (sampleRate);
    limiter.prepare (inSpec);
    clipper.prepare (inSpec);

    scopeFifo.reset();
}

void FlowFormAudioProcessor::releaseResources() {}

//==============================================================================
void FlowFormAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto* inL  = buffer.getReadPointer  (0);
    auto* inR  = buffer.getReadPointer  (1);
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    // Read all params
    auto getF = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto getB = [&] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    const float inTrimDb  = getF (ParamIDs::inTrimDb);
    const float inHPFHz   = getF (ParamIDs::inHPFHz);
    const float inLPFHz   = getF (ParamIDs::inLPFHz);
    const float inVoice   = getF (ParamIDs::inVoice);
    const float inBias    = getF (ParamIDs::inBias);
    const bool  inMono    = getB (ParamIDs::inMono);
    const bool  inPolarity = getB (ParamIDs::inPolarity);
    const bool  inDelta   = getB (ParamIDs::inDelta);
    const bool  inComp    = getB (ParamIDs::inComp);

    const float compSC     = getF (ParamIDs::compSC);
    const float compThresh = getF (ParamIDs::compThresh);
    const float compRatio  = getF (ParamIDs::compRatio);
    const float compAttack = getF (ParamIDs::compAttack);
    const float compRelease= getF (ParamIDs::compRelease);
    const float compMakeup = getF (ParamIDs::compMakeup);
    const float compStereo = getF (ParamIDs::compStereo);
    const int   compMS     = (int) getF (ParamIDs::compMS);
    const int   compType   = (int) getF (ParamIDs::compType);
    const bool  compOn     = getB (ParamIDs::compOn);
    const bool  compSolo   = getB (ParamIDs::compSolo);
    const bool  compDelta  = getB (ParamIDs::compDelta);

    const float x1 = getF (ParamIDs::x1Hz);
    const float x2 = getF (ParamIDs::x2Hz);
    const float x3 = getF (ParamIDs::x3Hz);
    const float satMixVal = getF (ParamIDs::satMix);
    const bool  satOn     = getB (ParamIDs::satOn);
    const bool  satSolo   = getB (ParamIDs::satSolo);
    const bool  satDelta  = getB (ParamIDs::satDelta);

    const float limitThresh  = getF (ParamIDs::limitThresh);
    const float limitGain    = getF (ParamIDs::limitGain);
    const float limitAttack  = getF (ParamIDs::limitAttack);
    const float limitCeiling = getF (ParamIDs::limitCeiling);
    const float limitRelease = getF (ParamIDs::limitRelease);
    const bool  limitOn     = getB (ParamIDs::limitOn);
    const bool  limitSolo   = getB (ParamIDs::limitSolo);
    const bool  limitDelta  = getB (ParamIDs::limitDelta);

    const float masterMTrim    = getF (ParamIDs::masterMTrim);
    const float masterHarmonics= getF (ParamIDs::masterHarmonics);
    const float masterShape    = getF (ParamIDs::masterShape);
    const float masterDepth    = getF (ParamIDs::masterDepth);
    const float masterMixVal   = getF (ParamIDs::masterMix);
    const float masterOutTrim  = getF (ParamIDs::masterOutTrim);
    const bool  masterOn      = getB (ParamIDs::masterOn);
    const bool  masterSolo    = getB (ParamIDs::masterSolo);
    const bool  masterDelta   = getB (ParamIDs::masterDelta);

    const float clipDrive    = getF (ParamIDs::clipDrive);
    const float clipSoftness = getF (ParamIDs::clipSoftness);
    const float clipLink     = getF (ParamIDs::clipLink);
    const bool  clipOn       = getB (ParamIDs::clipOn);
    const bool  clipSolo     = getB (ParamIDs::clipSolo);
    const bool  clipDelta    = getB (ParamIDs::clipDelta);

    hardBypass = getB (ParamIDs::bypass);
    const bool gDelta = getB (ParamIDs::deltaGlob);
    const bool gComp  = getB (ParamIDs::compGlob);
    oversampleFactor = 1; // TODO: implement properly

    // Determine audition
    auto checkAudition = [&] (bool solo, bool delta) -> int
    {
        if (delta) return 2;
        if (solo)  return 1;
        return 0;
    };

    int modes[NUM_SECTIONS] =
    {
        inDelta ? 2 : 0,
        checkAudition (compSolo, compDelta),
        checkAudition (satSolo, satDelta),
        checkAudition (limitSolo, limitDelta),
        checkAudition (masterSolo, masterDelta),
        checkAudition  (clipSolo, clipDelta),
    };

    auditionMode = 0;
    auditionSection = -1;
    for (int i = 0; i < NUM_SECTIONS; ++i)
        if (modes[i] > 0) { auditionMode = modes[i]; auditionSection = i; break; }

    // Audio processing
    float inGain = flowform::dbToLin (inTrimDb);

    // Oversampled buffers
    int osN = n;
    // For now we process at native rate
    // const float* procL = inL; const float* procR = inR;
    // TODO: oversampling

    float outBufL = 0.0f, outBufR = 0.0f;
    float dryBufL = 0.0f, dryBufR = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float L = inL[i] * inGain;
        float R = inR[i] * inGain;

        // Input processing
        if (inMono) R = L;
        if (inPolarity) { L = -L; R = -R; }

        // Meter input
        inLevelL = L;
        inLevelR = R;
        inEnv.process (L, R);
        inPeak.process (L, R);

        // Dry capture (before any processing, for delta)
        dryBufL = L;
        dryBufR = R;

        // ===== COMPRESSOR =====
        float compOutL = L, compOutR = R;
        if (compOn)
        {
            // TODO: proper compressor process
            // For now, simple level detection + reduction
            float env = std::sqrt ((L * L + R * R) * 0.5f);
            float envDb = flowform::linToDb (env + 1e-8f);
            float gr = 0.0f;
            if (envDb > compThresh)
                gr = (envDb - compThresh) / compRatio;
            float gainRed = flowform::dbToLin (-gr);
            compOutL = L * gainRed;
            compOutR = R * gainRed;
            float makeG = flowform::dbToLin (compMakeup);
            compOutL *= makeG;
            compOutR *= makeG;
            compGR = gr;
        }
        L = compOutL; R = compOutR;
        compEnv.process (L, R);

        // ===== SATURATION (4-band) =====
        // Simplified: just apply drive engine to full range for now
        // Full crossover implementation later
        if (satOn)
        {
            float satL = L, satR = R;
            for (int b = 0; b < numSatBands; ++b)
            {
                float drive = getF (bnd (b, ParamIDs::driveDb).toRawUTF8());
                float mix   = getF (bnd (b, ParamIDs::mix).toRawUTF8());
                float msFoc = getF (bnd (b, ParamIDs::msFocus).toRawUTF8());
                int   algo  = (int) getF (bnd (b, ParamIDs::algo).toRawUTF8());
                bool  bandOn = getB (bnd (b, ParamIDs::on).toRawUTF8());
                if (!bandOn) continue;

                // Drive range 0-100 in blueprint maps to dB range
                float driveDb = drive * 0.36f; // 0-100 → 0-36dB
                satBands[b].driveLin = flowform::dbToLin (driveDb);
                satBands[b].mixVal = mix;
                satBands[b].algo = algo;

                // Apply sat per sample (simplified — full crossover later)
                float dLin = satBands[b].driveLin;
                float xL = satL * dLin;
                float xR = satR * dLin;

                // Apply via DriveEngine
                // For now: simple tanh per band
                float yL = flowform::fastTanh (xL);
                float yR = flowform::fastTanh (xR);

                satL = satL + (yL - satL) * mix;
                satR = satR + (yR - satR) * mix;
            }
            L = satL; R = satR;
        }

        // ===== LIMITER =====
        if (limitOn)
        {
            float env = std::max (std::abs (L), std::abs (R));
            float envDb = flowform::linToDb (env + 1e-8f);
            float gr = 0.0f;
            if (envDb > limitThresh)
                gr = envDb - limitThresh;
            float gainRed = flowform::dbToLin (-gr);
            float ceilingGain = flowform::dbToLin (limitCeiling);
            float preGain = flowform::dbToLin (limitGain);
            L *= preGain * gainRed * ceilingGain;
            R *= preGain * gainRed * ceilingGain;
        }

        // ===== MASTER (Harmonics) =====
        if (masterOn)
        {
            // Simple harmonic saturation
            float hAmount = masterHarmonics * 0.01f;
            float shp = masterShape * 0.01f;
            float dpt = masterDepth * 0.01f;
            float mTrim = flowform::dbToLin (masterMTrim);
            float oTrim = flowform::dbToLin (masterOutTrim);
            float mixM = masterMixVal;

            float xL = L * mTrim;
            float xR = R * mTrim;

            float hL = xL + hAmount * (xL * xL * 0.5f + xL * xL * xL * shp) * dpt;
            float hR = xR + hAmount * (xR * xR * 0.5f + xR * xR * xR * shp) * dpt;

            L = (L * (1.0f - mixM) + hL * mixM) * oTrim;
            R = (R * (1.0f - mixM) + hR * mixM) * oTrim;
        }

        // ===== CLIPPER =====
        if (clipOn)
        {
            float dDrive = clipDrive * 0.01f * 3.0f; // 0-100 → 0-3x gain
            float soft = 1.0f - clipSoftness * 0.009f; // 0-100 → 1.0 down to 0.1
            soft = std::max (0.1f, soft);

            L *= dDrive;
            R *= dDrive;

            L = flowform::fastTanh (L / soft) * soft;
            R = flowform::fastTanh (R / soft) * soft;
        }

        // ===== DELTA / SOLO / MIX =====
        if (auditionMode == 2) // delta
        {
            // Output is dry minus wet
            outBufL = dryBufL - L;
            outBufR = dryBufR - R;
        }
        else if (auditionMode == 1) // solo
        {
            outBufL = L;
            outBufR = R;
        }
        else
        {
            outBufL = L;
            outBufR = R;
        }

        // Hard bypass
        if (hardBypass)
        {
            outBufL = inL[i];
            outBufR = inR[i];
        }

        outL[i] = outBufL;
        outR[i] = outBufR;

        // Output metering
        outLevelL = outBufL;
        outLevelR = outBufR;
        outPeak.process (outBufL, outBufR);
    }

    // LUFS
    lufsMeter.process (outL, outR, n);
    lufsIntegrated = lufsMeter.getIntegrated();
    lufsShortTerm  = lufsMeter.getShortTerm();
    lufsMaxMomentary = lufsMeter.getMaxMomentary();

    // Scope
    scopeFifo.push (inL, outL, nullptr, nullptr, n);
}

//==============================================================================
void FlowFormAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FlowFormAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FlowFormAudioProcessor::createEditor()
{
    return new FlowFormAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlowFormAudioProcessor();
}
