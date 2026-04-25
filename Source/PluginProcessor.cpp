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
    addF (ParamIDs::compThresh, "Threshold (dB)",{ -40.0f, 0.0f, 0.01f }, 0.0f);
    addF (ParamIDs::compRatio,  "Ratio",         { 1.0f, 20.0f, 0.01f }, 1.0f);
    addF (ParamIDs::compAttack, "Attack (ms)",   { 0.1f, 100.0f, 0.01f, 0.35f }, 1.0f);
    addF (ParamIDs::compRelease,"Release (ms)",  { 10.0f, 1000.0f, 0.01f, 0.35f }, 80.0f);
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
    static float defDrives[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int b = 0; b < 4; ++b)
    {
        addF (bnd (b, ParamIDs::driveDb).toRawUTF8(), ("Band " + String(b) + " Drive").toRawUTF8(),
              { 0.0f, 100.0f, 0.1f }, defDrives[b]);
        addF (bnd (b, ParamIDs::mix).toRawUTF8(), ("Band " + String(b) + " Mix").toRawUTF8(),
              { 0.0f, 1.0f, 0.001f }, 1.0f);
        addF (bnd (b, ParamIDs::msFocus).toRawUTF8(), ("Band " + String(b) + " M/S").toRawUTF8(),
              { -100.0f, 100.0f, 0.1f }, 0.0f);
        addC (bnd (b, ParamIDs::algo).toRawUTF8(), ("Band " + String(b) + " Algo").toRawUTF8(),
              { "Tube", "Tape", "Solid-State", "Transformer" }, 3);
        addB (bnd (b, ParamIDs::on).toRawUTF8(),    ("Band " + String(b) + " On").toRawUTF8(), true);
        addB (bnd (b, ParamIDs::solo).toRawUTF8(),  ("Band " + String(b) + " Solo").toRawUTF8(), false);
        addB (bnd (b, ParamIDs::delta).toRawUTF8(), ("Band " + String(b) + " Delta").toRawUTF8(), false);
    }

    // Limiter
    addF (ParamIDs::limitThresh,  "Lim Thresh (dB)", { -40.0f, 0.0f, 0.01f }, 0.0f);
    addF (ParamIDs::limitGain,    "Lim Gain (dB)",   { -12.0f, 12.0f, 0.01f }, 0.0f);
    addF (ParamIDs::limitAttack,  "Lim Attack (ms)", { 0.01f, 10.0f, 0.001f, 0.35f }, 0.1f);
    addF (ParamIDs::limitCeiling, "Lim Ceiling (dB)",{ -3.0f, 0.0f, 0.01f }, 0.0f);
    addF (ParamIDs::limitRelease, "Lim Release (ms)",{ 10.0f, 500.0f, 0.1f, 0.35f }, 150.0f);
    addB (ParamIDs::limitOn,    "Lim On",    true);
    addB (ParamIDs::limitSolo,  "Lim Solo",  false);
    addB (ParamIDs::limitDelta, "Lim Delta", false);

    // Master
    addF (ParamIDs::masterMTrim,    "M Trim (dB)",   { -24.0f, 0.0f, 0.01f }, -15.0f);
    addF (ParamIDs::masterHarmonics,"Harmonics",     { 0.0f, 100.0f, 0.1f }, 0.0f);
    addF (ParamIDs::masterShape,    "Shape",         { 0.0f, 100.0f, 0.1f }, 0.0f);
    addF (ParamIDs::masterDepth,    "Depth",         { 0.0f, 100.0f, 0.1f }, 0.0f);
    addF (ParamIDs::masterMix,      "Master Mix",    { 0.0f, 1.0f, 0.001f }, 1.0f);
    addF (ParamIDs::masterOutTrim,  "Out Trim (dB)", { -24.0f, 24.0f, 0.01f }, 0.0f);
    addB (ParamIDs::masterOn,    "Master On",    true);
    addB (ParamIDs::masterSolo,  "Master Solo",  false);
    addB (ParamIDs::masterDelta, "Master Delta", false);

    // Clipper
    addF (ParamIDs::clipDrive,    "Clip Drive",      { 0.0f, 100.0f, 0.1f }, 0.0f);
    addF (ParamIDs::clipSoftness, "Clip Softness",   { 0.0f, 100.0f, 0.1f }, 100.0f);
    addF (ParamIDs::clipLink,     "Clip Link",       { 0.0f, 100.0f, 0.1f }, 100.0f);
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

    auto getF = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto getB = [&] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    const bool inMono     = getB (ParamIDs::inMono);
    const bool inPolarity = getB (ParamIDs::inPolarity);
    const bool inDelta    = getB (ParamIDs::inDelta);
    const bool compOn     = getB (ParamIDs::compOn);
    const bool compSolo   = getB (ParamIDs::compSolo);
    const bool compDelta  = getB (ParamIDs::compDelta);
    const bool satOn      = getB (ParamIDs::satOn);
    const bool satSolo    = getB (ParamIDs::satSolo);
    const bool satDelta   = getB (ParamIDs::satDelta);
    const bool limitOn    = getB (ParamIDs::limitOn);
    const bool limitSolo  = getB (ParamIDs::limitSolo);
    const bool limitDelta = getB (ParamIDs::limitDelta);
    const bool masterOn   = getB (ParamIDs::masterOn);
    const bool masterSolo = getB (ParamIDs::masterSolo);
    const bool masterDelta= getB (ParamIDs::masterDelta);
    const bool clipOn     = getB (ParamIDs::clipOn);
    const bool clipSolo   = getB (ParamIDs::clipSolo);
    const bool clipDelta  = getB (ParamIDs::clipDelta);

    hardBypass = getB (ParamIDs::bypass);

    int modes[6] = {
        inDelta ? 2 : 0,
        compDelta ? 2 : (compSolo ? 1 : 0),
        satDelta ? 2 : (satSolo ? 1 : 0),
        limitDelta ? 2 : (limitSolo ? 1 : 0),
        masterDelta ? 2 : (masterSolo ? 1 : 0),
        clipDelta ? 2 : (clipSolo ? 1 : 0)
    };
    auditionMode = 0; auditionSection = -1;
    for (int i = 0; i < 6; ++i)
        if (modes[i] > 0) { auditionMode = modes[i]; auditionSection = i; break; }

    // Working buffer chain
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    auto* inL  = buffer.getReadPointer (0);

    juce::AudioBuffer<float> dryBuf;
    dryBuf.makeCopyOf (buffer);

    // Input trim
    float inGain = flowform::dbToLin (getF (ParamIDs::inTrimDb));
    for (int i = 0; i < n; ++i)
    {
        float L = buffer.getSample (0, i) * inGain;
        float R = buffer.getSample (1, i) * inGain;
        if (inMono)      R = L;
        if (inPolarity) { L = -L; R = -R; }
        buffer.setSample (0, i, L);
        buffer.setSample (1, i, R);
    }
    for (int i = 0; i < n; ++i) inEnv.process (buffer.getSample (0,i), buffer.getSample (1,i));
    inLevelL = buffer.getSample (0, n-1);
    inLevelR = buffer.getSample (1, n-1);

    // Compressor
    if (compOn)
    {
        compressor.setThreshold  (getF (ParamIDs::compThresh));
        compressor.setRatio      (getF (ParamIDs::compRatio));
        compressor.setAttack     (getF (ParamIDs::compAttack));
        compressor.setRelease    (getF (ParamIDs::compRelease));
        compressor.setMakeup     (getF (ParamIDs::compMakeup));
        compressor.setSidechainHPF (getF (ParamIDs::compSC));
        compressor.setStereoLink (getF (ParamIDs::compStereo));
        compressor.setCompType   ((CompressorProcessor::CompType)(int)getF(ParamIDs::compType));
        compressor.setMSMode     ((CompressorProcessor::MSMode)(int)getF(ParamIDs::compMS));
        compressor.setEnabled (true);
        compressor.process (buffer);
        compGR = compressor.getGainReduction();
    }

    // Saturation (4-band via DriveEngine)
    if (satOn)
    {
        for (int b = 0; b < 4; ++b)
        {
            if (!getB (bnd (b, ParamIDs::on).toRawUTF8())) continue;
            float driveDb = getF (bnd (b, ParamIDs::driveDb).toRawUTF8()) * 0.36f;
            float mix     = getF (bnd (b, ParamIDs::mix).toRawUTF8());
            int   algo    = (int) getF (bnd (b, ParamIDs::algo).toRawUTF8());
            driveEngine.setDrive (flowform::dbToLin (driveDb));
            driveEngine.setAlgorithm (algo);
            driveEngine.process (buffer, algo, flowform::dbToLin (driveDb));
        }
    }

    // Limiter
    if (limitOn)
    {
        limiter.setThreshold (getF (ParamIDs::limitThresh));
        limiter.setGain      (getF (ParamIDs::limitGain));
        limiter.setAttack    (getF (ParamIDs::limitAttack));
        limiter.setRelease   (getF (ParamIDs::limitRelease));
        limiter.setCeiling   (getF (ParamIDs::limitCeiling));
        limiter.setEnabled (true);
        limiter.process (buffer);
    }

    // Master harmonics
    if (masterOn)
    {
        harmonics.setMTrim      (getF (ParamIDs::masterMTrim));
        harmonics.setHarmonics  (getF (ParamIDs::masterHarmonics));
        harmonics.setShape      (getF (ParamIDs::masterShape));
        harmonics.setDepth      (getF (ParamIDs::masterDepth));
        harmonics.setGlobalMix  (getF (ParamIDs::masterMix));
        harmonics.setOutputTrim (getF (ParamIDs::masterOutTrim));
        harmonics.process (buffer);
    }

    // Clipper
    if (clipOn)
    {
        clipper.setDrive    (getF (ParamIDs::clipDrive));
        clipper.setSoftness (getF (ParamIDs::clipSoftness));
        clipper.setLink     (getF (ParamIDs::clipLink));
        clipper.setEnabled (true);
        clipper.process (buffer);
    }

    // Delta/solo/hard bypass
    if (hardBypass)
    {
        for (int i = 0; i < n; ++i) { outL[i] = inL[i]; outR[i] = inL[i]; }
        return;
    }
    if (auditionMode == 2)
    {
        for (int i = 0; i < n; ++i)
        {
            outL[i] = dryBuf.getSample (0,i) - buffer.getSample (0,i);
            outR[i] = dryBuf.getSample (1,i) - buffer.getSample (1,i);
        }
    }
    // else (solo or normal): buffer already contains processed audio

    // Output metering
    for (int i = 0; i < n; ++i) outPeak.process (outL[i], outR[i]);
    outLevelL = outL[n-1]; outLevelR = outR[n-1];
    lufsMeter.process (outL, outR, n);
    lufsIntegrated = lufsMeter.getIntegrated();
    lufsShortTerm  = lufsMeter.getShortTerm();
    lufsMaxMomentary = lufsMeter.getMaxMomentary();
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
