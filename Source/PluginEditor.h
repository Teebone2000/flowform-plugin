#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ScopeComponent.h"

//==============================================================================
class FlowFormAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    FlowFormAudioProcessorEditor (FlowFormAudioProcessor&);
    ~FlowFormAudioProcessorEditor() ;

    void paint (juce::Graphics&) ;
    void resized() ;

private:
    using APVTS = FlowFormAudioProcessor::APVTS;
    APVTS& apvts;
    using SAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // ===== Look and Feel =====
    struct BlueSteelLNF : public juce::LookAndFeel_V4
    {
        BlueSteelLNF();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float pos, float startA, float endA, juce::Slider&) ;
        void drawComboBox (juce::Graphics&, int w, int h, bool,
                           int, int, int, int, juce::ComboBox&) ;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                   bool, bool) ;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                               float pos, float min, float max,
                               int, juce::Slider&) ;
    } lnf;

    // ===== Helpers =====
    static juce::Colour accent() { return juce::Colour::fromRGB (0, 191, 255); }
    void addScrews (juce::Graphics&, juce::Rectangle<float>);
    void styleKnob (juce::Slider&, int sizePx = 56);
    void styleFader (juce::Slider&);
    void styleToggle (juce::ToggleButton&, bool isDelta = false);
    void styleLabel (juce::Label&, float fontSize = 9.0f);

    // ===== Header =====
    juce::Label logo { {}, "DIGITAL CAPERS" };
    juce::ComboBox presetBox, oversampleBox;
    juce::TextButton abButton { "A" }, undoBtn { "←" }, redoBtn { "→" };
    juce::ToggleButton bypassBtn { "BYPASS" }, deltaBtn { "Δ" }, compBtn { "COMP" };

    // ===== Scope =====
    ScopeComponent scope;

    // ===== Input Panel =====
    juce::Label inTitle { {}, "INPUT" };
    juce::Slider inTrim, inHPF, inLPF, inVoice, inBias;
    juce::Label inTrimLbl { {}, "TRIM" }, inHPFLbl { {}, "LO-PASS" },
                inLPFLbl { {}, "HI-PASS" }, inVoiceLbl { {}, "VOICE" }, inBiasLbl { {}, "BIAS" };
    juce::ToggleButton inMonoBtn { "MONO" }, inPolarBtn { "Ø" },
                       inDeltaBtn { "Δ" }, inCompBtn { "COMP" };
    std::unique_ptr<SAttach> aInTrim, aInHPF, aInLPF, aInVoice, aInBias;
    std::unique_ptr<BAttach> aInMono, aInPolar, aInDelta, aInComp;

    // ===== Compressor Panel =====
    juce::Label compTitle { {}, "COMPRESSOR" };
    juce::Slider compSC, compThresh, compRatio, compAttack, compRelease, compMakeup, compStereo;
    juce::Label compSCLbl { {}, "S/C HPF" }, compThreshLbl { {}, "THRESH" },
                compRatioLbl { {}, "RATIO" }, compAttackLbl { {}, "ATTACK" },
                compReleaseLbl { {}, "RELEASE" }, compMakeupLbl { {}, "MAKEUP" },
                compStereoLbl { {}, "STEREO\nLINK" };
    juce::ComboBox compMS, compType;
    juce::Label compMSLbl { {}, "M/S" }, compTypeLbl { {}, "TYPE" };
    juce::ToggleButton compOnBtn { "ON" }, compSoloBtn { "SOLO" }, compDeltaBtn { "Δ" };
    std::unique_ptr<SAttach> aCompSC, aCompThresh, aCompRatio, aCompAttack, aCompRelease, aCompMakeup, aCompStereo;
    std::unique_ptr<CAttach> aCompMS, aCompType;
    std::unique_ptr<BAttach> aCompOn, aCompSolo, aCompDelta;

    // ===== Saturation Panel =====
    juce::Label satTitle { {}, "SATURATION" };
    juce::Slider x1, x2, x3;
    juce::Label x1Lbl { {}, "LOW\nSPLIT" }, x2Lbl { {}, "MID\nSPLIT" }, x3Lbl { {}, "HIGH\nSPLIT" };
    juce::Slider satMixFader;
    juce::ToggleButton satOnBtn { "ON" }, satSoloBtn { "SOLO" }, satDeltaBtn { "Δ" };
    std::unique_ptr<SAttach> aX1, aX2, aX3, aSatMix;
    std::unique_ptr<BAttach> aSatOn, aSatSolo, aSatDelta;

    struct SatBand : public juce::Component
    {
        SatBand (APVTS&, int bandIndex);
        void paint (juce::Graphics&) ;
        void resized() ;

        int band;
        juce::Label title;

        juce::ToggleButton onBtn { "ON" }, soloBtn { "SOLO" }, deltaBtn { "Δ" };
        juce::Slider drive, mix, msFocus;
        juce::ComboBox algo;
        juce::Label driveLbl { {}, "DRIVE" }, mixLbl { {}, "MIX" },
                    msLbl { {}, "M/S" }, algoLbl { {}, "ALGO" };

        std::unique_ptr<BAttach> aOn, aSolo, aDelta;
        std::unique_ptr<SAttach> aDrive, aMix, aMS;
        std::unique_ptr<CAttach> aAlgo;
    };
    std::array<std::unique_ptr<SatBand>, 4> satBands;

    // ===== Limiter Panel =====
    juce::Label limitTitle { {}, "LIMITER" };
    juce::Slider limitThresh, limitGain, limitAttack, limitCeiling, limitRelease;
    juce::Label limitThreshLbl { {}, "THRESH" }, limitGainLbl { {}, "GAIN" },
                limitAttackLbl { {}, "ATTACK" }, limitCeilingLbl { {}, "CEIL" },
                limitReleaseLbl { {}, "RELEASE" };
    juce::ToggleButton limitOnBtn { "ON" }, limitSoloBtn { "SOLO" }, limitDeltaBtn { "Δ" };
    std::unique_ptr<SAttach> aLimitThresh, aLimitGain, aLimitAttack, aLimitCeiling, aLimitRelease;
    std::unique_ptr<BAttach> aLimitOn, aLimitSolo, aLimitDelta;

    // ===== Master Panel =====
    juce::Label masterTitle { {}, "MASTER" };
    juce::Slider masterMTrim, masterHarmonics, masterShape, masterDepth, masterMix, masterOutTrim;
    juce::Label masterMTrimLbl { {}, "M TRIM" }, masterHarmonicsLbl { {}, "HARM" },
                masterShapeLbl { {}, "SHAPE" }, masterDepthLbl { {}, "DEPTH" },
                masterMixLbl { {}, "MIX" }, masterOutTrimLbl { {}, "OUT" };
    juce::ToggleButton masterOnBtn { "ON" }, masterSoloBtn { "SOLO" }, masterDeltaBtn { "Δ" };
    std::unique_ptr<SAttach> aMasterMTrim, aMasterHarm, aMasterShape, aMasterDepth, aMasterMix, aMasterOutTrim;
    std::unique_ptr<BAttach> aMasterOn, aMasterSolo, aMasterDelta;

    // ===== Clipper Panel =====
    juce::Label clipperTitle { {}, "CLIPPER" };
    juce::Slider clipDrive, clipSoftness, clipLink;
    juce::Label clipDriveLbl { {}, "DRIVE" }, clipSoftnessLbl { {}, "SOFT" },
                clipLinkLbl { {}, "LINK" };
    juce::ToggleButton clipOnBtn { "ON" }, clipSoloBtn { "SOLO" }, clipDeltaBtn { "Δ" };
    std::unique_ptr<SAttach> aClipDrive, aClipSoft, aClipLink;
    std::unique_ptr<BAttach> aClipOn, aClipSolo, aClipDelta;
    std::unique_ptr<BAttach> aBypass, aGlobalDelta, aGlobalComp;

    // A/B state
    int currentAB = 0; // 0 = A, 1 = B

    void timerCallback() ;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowFormAudioProcessorEditor)
};
