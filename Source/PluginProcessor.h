#pragma once
#include <JuceHeader.h>
#include "ScopeComponent.h"
#include "DSP/CompressorProcessor.h"
#include "DSP/DriveEngine.h"
#include "DSP/HarmonicProcessor.h"
#include "DSP/LimiterProcessor.h"
#include "DSP/ClipperProcessor.h"
#include "DSP/MeterProcessor.h"

//==============================================================================
struct ParamIDs
{
    static constexpr auto inTrimDb  = "inTrimDb";
    static constexpr auto inHPFHz   = "inHPFHz";
    static constexpr auto inLPFHz   = "inLPFHz";
    static constexpr auto inVoice   = "inVoice";
    static constexpr auto inBias    = "inBias";
    static constexpr auto inMono    = "inMono";
    static constexpr auto inPolarity = "inPolarity";
    static constexpr auto inDelta   = "inDelta";
    static constexpr auto inComp    = "inComp";

    static constexpr auto compSC    = "compSC";
    static constexpr auto compThresh= "compThresh";
    static constexpr auto compRatio = "compRatio";
    static constexpr auto compAttack= "compAttack";
    static constexpr auto compRelease="compRelease";
    static constexpr auto compMakeup= "compMakeup";
    static constexpr auto compStereo= "compStereo";
    static constexpr auto compMS    = "compMS";
    static constexpr auto compType  = "compType";
    static constexpr auto compOn    = "compOn";
    static constexpr auto compSolo  = "compSolo";
    static constexpr auto compDelta = "compDelta";

    static constexpr auto x1Hz      = "x1Hz";
    static constexpr auto x2Hz      = "x2Hz";
    static constexpr auto x3Hz      = "x3Hz";
    static constexpr auto satMix    = "satMix";
    static constexpr auto satOn     = "satOn";
    static constexpr auto satSolo   = "satSolo";
    static constexpr auto satDelta  = "satDelta";

    static juce::String band (int b, const juce::String& p) { return "b" + juce::String(b) + "_" + p; }
    static constexpr auto driveDb   = "driveDb";
    static constexpr auto mix       = "mix";
    static constexpr auto msFocus   = "msFocus";
    static constexpr auto on        = "on";
    static constexpr auto solo      = "solo";
    static constexpr auto delta     = "delta";
    static constexpr auto algo      = "algo";

    static constexpr auto limitThresh  = "limitThresh";
    static constexpr auto limitGain    = "limitGain";
    static constexpr auto limitAttack  = "limitAttack";
    static constexpr auto limitCeiling = "limitCeiling";
    static constexpr auto limitRelease = "limitRelease";
    static constexpr auto limitOn      = "limitOn";
    static constexpr auto limitSolo    = "limitSolo";
    static constexpr auto limitDelta   = "limitDelta";

    static constexpr auto masterMTrim   = "masterMTrim";
    static constexpr auto masterHarmonics="masterHarmonics";
    static constexpr auto masterShape   = "masterShape";
    static constexpr auto masterDepth   = "masterDepth";
    static constexpr auto masterMix     = "masterMix";
    static constexpr auto masterOutTrim = "masterOutTrim";
    static constexpr auto masterOn      = "masterOn";
    static constexpr auto masterSolo    = "masterSolo";
    static constexpr auto masterDelta   = "masterDelta";

    static constexpr auto clipDrive   = "clipDrive";
    static constexpr auto clipSoftness= "clipSoftness";
    static constexpr auto clipLink    = "clipLink";
    static constexpr auto clipOn      = "clipOn";
    static constexpr auto clipSolo    = "clipSolo";
    static constexpr auto clipDelta   = "clipDelta";

    static constexpr auto bypass      = "bypass";
    static constexpr auto deltaGlob   = "deltaGlob";
    static constexpr auto compGlob    = "compGlob";
    static constexpr auto oversample  = "oversample";
};

//==============================================================================
class FlowFormAudioProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    FlowFormAudioProcessor();
    ~FlowFormAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    APVTS& getAPVTS() { return apvts; }
    ScopeFifo& getScopeFifo() { return scopeFifo; }

    // Meter data for UI
    float getInLevelL()  const noexcept { return inLevelL; }
    float getInLevelR()  const noexcept { return inLevelR; }
    float getCompGR()    const noexcept { return compGR; }
    float getOutLevelL() const noexcept { return outLevelL; }
    float getOutLevelR() const noexcept { return outLevelR; }
    float getLufsIntegrated()   const noexcept { return lufsIntegrated; }
    float getLufsShortTerm()    const noexcept { return lufsShortTerm; }
    float getLufsMaxMomentary() const noexcept { return lufsMaxMomentary; }
    float getLimiterGR()        const noexcept { return limiter.getGainReduction(); }
    float getLimiterInput()     const noexcept { return limiter.getInputLevel(); }

    // Audition state
    int  getAuditionSection() const noexcept { return auditionSection; }
    int  getAuditionMode()    const noexcept { return auditionMode; }
    bool isBypassed()         const noexcept { return hardBypass; }

    static APVTS::ParameterLayout createParameterLayout();

private:
    APVTS apvts;

    // ====== DSP Modules ======
    // Input
    flowform::EnvelopeFollower inEnv;
    flowform::PeakMeter inPeak;

    // Tone shaping (Voice/Bias from DriveEngine)
    DriveEngine driveEngine;

    // Compressor
    CompressorProcessor compressor;
    flowform::EnvelopeFollower compEnv;
    float compGR = 0.0f;

    // Saturation
    static constexpr int numSatBands = 4;
    struct SatBandDSP
    {
        flowform::EnvelopeFollower env;
        float driveLin = 1.0f;
        float mixVal = 1.0f;
        int algo = 0;
    };
    SatBandDSP satBands[numSatBands];

    // Limiter
    LimiterProcessor limiter;

    // Master harmonics
    HarmonicProcessor harmonics;

    // Clipper
    ClipperProcessor clipper;

    // Oversampling
    flowform::Oversampler oversampler;
    int oversampleFactor = 1;

    // Metering (post-processing)
    flowform::PeakMeter outPeak;
    flowform::LufsMeter lufsMeter;
    float inLevelL = 0.0f, inLevelR = 0.0f;
    float outLevelL = 0.0f, outLevelR = 0.0f;
    float lufsIntegrated = -70.0f;
    float lufsShortTerm  = -70.0f;
    float lufsMaxMomentary = -70.0f;

    // Audition
    enum Section { SEC_INPUT = 0, SEC_COMP, SEC_SAT, SEC_LIMIT, SEC_MASTER, SEC_CLIP, NUM_SECTIONS };
    int auditionSection = -1;
    int auditionMode = 0; // 0=none, 1=solo, 2=delta
    bool hardBypass = false;

    // Scope
    // Level compensation
    bool  compActive = false;
    float compGain = 1.0f;
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;
    float rmsCoeff = 0.0f;  // 300ms window
    
    // Scope
    ScopeFifo scopeFifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowFormAudioProcessor)
};
