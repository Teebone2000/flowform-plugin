#include "PluginEditor.h"

static juce::Colour accent() { return juce::Colour::fromRGB (0, 191, 255); }

FlowFormAudioProcessorEditor::BlueSteelLNF::BlueSteelLNF()
{
    setColour (juce::Slider::thumbColourId, accent());
    setColour (juce::Slider::rotarySliderFillColourId, accent());
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (40, 40, 40));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (18, 18, 20));
    setColour (juce::ComboBox::outlineColourId, juce::Colour::fromRGB (40, 60, 70));
    setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (42, 42, 42));
    setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
}

void FlowFormAudioProcessorEditor::BlueSteelLNF::drawRotarySlider (
    juce::Graphics& g, int x, int y, int w, int h,
    float pos, float startA, float endA, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    auto cx = r.getCentreX(), cy = r.getCentreY();
    auto radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (r.translated (0.0f, 2.0f));
    juce::Colour top (35, 40, 48), bot (10, 10, 12);
    g.setGradientFill (juce::ColourGradient (top, cx, r.getY(), bot, cx, r.getBottom(), false));
    g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (r, 1.0f);
    auto ang = startA + pos * (endA - startA);
    auto arcR = r.reduced (radius * 0.18f);
    juce::Path bg, fg;
    bg.addCentredArc (cx, cy, arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0.0f, startA, endA, true);
    fg.addCentredArc (cx, cy, arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0.0f, startA, ang, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (bg, juce::PathStrokeType (2.0f));
    g.setColour (accent().withAlpha (0.9f));
    g.strokePath (fg, juce::PathStrokeType (2.6f));
    auto p = juce::Point<float> (cx + std::cos (ang) * (radius * 0.78f), cy + std::sin (ang) * (radius * 0.78f));
    g.drawLine (cx, cy, p.x, p.y, 2.0f);
}

void FlowFormAudioProcessorEditor::BlueSteelLNF::drawLinearSlider (
    juce::Graphics& g, int x, int y, int w, int h,
    float pos, float, float, int, juce::Slider& sl)
{
    juce::ignoreUnused (sl);
    auto track = juce::Rectangle<float> ((float) x, (float) y + 8.0f, (float) w, 6.0f).reduced (4.0f, 0);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (track, 3.0f);
    float fillW = std::max (0.0f, track.getWidth() * pos);
    g.setColour (accent().withAlpha (0.5f));
    g.fillRoundedRectangle (track.withWidth (fillW), 3.0f);
    auto handle = juce::Rectangle<float> (track.getX() + fillW - 5.0f, track.getY() - 4.0f, 10.0f, 14.0f);
    g.setColour (accent());
    g.fillRoundedRectangle (handle, 3.0f);
}

void FlowFormAudioProcessorEditor::BlueSteelLNF::drawComboBox (
    juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (juce::Colour::fromRGB (18, 18, 20));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (accent().withAlpha (0.20f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawFittedText (box.getText(), r.toNearestInt().reduced (6, 0), juce::Justification::centredLeft, 1);
}

void FlowFormAudioProcessorEditor::BlueSteelLNF::drawButtonBackground (
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto on = b.getToggleState();
    juce::Colour col = on ? accent() : juce::Colour::fromRGB (24, 24, 26);
    g.setColour (col.withAlpha (on ? 0.30f : 0.9f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// Helpers
void FlowFormAudioProcessorEditor::styleKnob (juce::Slider& s, int sz)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, sz, 14);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.55f));
}

void FlowFormAudioProcessorEditor::styleFader (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 14);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.55f));
}

void FlowFormAudioProcessorEditor::styleLabel (juce::Label& l, float fs)
{
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, accent().withAlpha (0.85f));
    l.setFont (juce::FontOptions (fs).withStyle ("Bold"));
}

// SatBand
FlowFormAudioProcessorEditor::SatBand::SatBand (APVTS& s, int bIdx) : band (bIdx)
{
    static const char* names[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
    title.setText (names[bIdx], juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (title);
    addAndMakeVisible (onBtn); addAndMakeVisible (soloBtn); addAndMakeVisible (deltaBtn);
    algo.addItem ("Tube",1); algo.addItem ("Tape",2); algo.addItem ("Solid",3); algo.addItem ("Xfmr",4);
    addAndMakeVisible (algoLbl); addAndMakeVisible (algo);
    for (auto* l : { &driveLbl, &mixLbl, &msLbl, &algoLbl })
        { l->setJustificationType (juce::Justification::centred); l->setColour (juce::Label::textColourId, accent().withAlpha (0.85f)); l->setFont (juce::FontOptions (8).withStyle ("Bold")); addAndMakeVisible (*l); }
    auto sSm = [&] (juce::Slider& sl, int sx) { sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, sx, 12); sl.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f)); sl.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); sl.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.55f)); addAndMakeVisible (sl); };
    sSm (drive, 42); sSm (mix, 38);
    msFocus.setSliderStyle (juce::Slider::LinearHorizontal); msFocus.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 12);
    msFocus.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    msFocus.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    msFocus.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.55f));
    addAndMakeVisible (msFocus);
}

void FlowFormAudioProcessorEditor::SatBand::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6);
    g.setColour (juce::Colour::fromRGB (51,51,51)); g.drawRoundedRectangle (r, 6, 1);
}

void FlowFormAudioProcessorEditor::SatBand::resized()
{
    auto r = getLocalBounds().reduced (3);
    title.setBounds (r.removeFromTop (12));
    auto br = r.removeFromTop (14);
    onBtn.setBounds (br.removeFromLeft (24)); soloBtn.setBounds (br.removeFromLeft (26)); deltaBtn.setBounds (br);
    algoLbl.setBounds (r.removeFromTop (9)); algo.setBounds (r.removeFromTop (16));
    driveLbl.setBounds (r.removeFromTop (9)); drive.setBounds (r.removeFromTop (42));
    mixLbl.setBounds (r.removeFromTop (9)); mix.setBounds (r.removeFromTop (38));
    msLbl.setBounds (r.removeFromTop (9)); msFocus.setBounds (r.removeFromTop (18));
}

// ===== Constructor =====
FlowFormAudioProcessorEditor::FlowFormAudioProcessorEditor (FlowFormAudioProcessor& p)
: AudioProcessorEditor (&p), scope (p.getScopeFifo()), apvts (p.getAPVTS())
{
    setLookAndFeel (&lnf);
    logo.setFont (juce::FontOptions (20).withStyle ("Bold"));
    logo.setColour (juce::Label::textColourId, juce::Colours::black);
    logo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (logo);
    addAndMakeVisible (presetBox); addAndMakeVisible (oversampleBox);
    addAndMakeVisible (abButton); addAndMakeVisible (undoBtn); addAndMakeVisible (redoBtn);
    addAndMakeVisible (bypassBtn); addAndMakeVisible (deltaBtn); addAndMakeVisible (compBtn);
    addAndMakeVisible (scope);
    scopeToggleBtn.setButtonText ("W");
    scopeToggleBtn.setClickingTogglesState (true);
    scopeToggleBtn.onClick = [this] { scope.setShowScope (! scope.getShowScope()); };
    addAndMakeVisible (scopeToggleBtn);

    auto addKnob = [&] (juce::Slider& s, juce::Label& l, int sz) { styleKnob (s, sz); styleLabel (l, 8); addAndMakeVisible (s); addAndMakeVisible (l); };
    auto addToggle = [&] (juce::ToggleButton& b) { addAndMakeVisible (b); };
    auto addLabel = [&] (juce::Label& l) { styleLabel (l, 8); addAndMakeVisible (l); };

    // Input
    styleLabel (inTitle, 10); addAndMakeVisible (inTitle);
    addKnob (inTrim, inTrimLbl, 48); addKnob (inHPF, inHPFLbl, 40); addKnob (inLPF, inLPFLbl, 40);
    addKnob (inVoice, inVoiceLbl, 40); addKnob (inBias, inBiasLbl, 40);
    addToggle (inMonoBtn); addToggle (inPolarBtn); addToggle (inDeltaBtn); addToggle (inCompBtn);
    aInTrim = std::make_unique<SAttach> (apvts, "inTrimDb", inTrim);
    aInHPF = std::make_unique<SAttach> (apvts, "inHPFHz", inHPF);
    aInLPF = std::make_unique<SAttach> (apvts, "inLPFHz", inLPF);
    aInVoice = std::make_unique<SAttach> (apvts, "inVoice", inVoice);
    aInBias = std::make_unique<SAttach> (apvts, "inBias", inBias);
    aInMono = std::make_unique<BAttach> (apvts, "inMono", inMonoBtn);
    aInPolar = std::make_unique<BAttach> (apvts, "inPolarity", inPolarBtn);
    aInDelta = std::make_unique<BAttach> (apvts, "inDelta", inDeltaBtn);
    aInComp = std::make_unique<BAttach> (apvts, "inComp", inCompBtn);

    // Compressor
    styleLabel (compTitle, 10); addAndMakeVisible (compTitle);
    addKnob (compSC, compSCLbl, 36); addKnob (compThresh, compThreshLbl, 36);
    addKnob (compRatio, compRatioLbl, 36); addKnob (compAttack, compAttackLbl, 36);
    addKnob (compRelease, compReleaseLbl, 36); addKnob (compMakeup, compMakeupLbl, 36);
    styleFader (compStereo); addAndMakeVisible (compStereo); addLabel (compStereoLbl);
    addAndMakeVisible (compMS); addLabel (compMSLbl);
    addAndMakeVisible (compType); addLabel (compTypeLbl);
    addToggle (compOnBtn); addToggle (compSoloBtn); addToggle (compDeltaBtn);
    aCompSC = std::make_unique<SAttach> (apvts, "compSC", compSC);
    aCompThresh = std::make_unique<SAttach> (apvts, "compThresh", compThresh);
    aCompRatio = std::make_unique<SAttach> (apvts, "compRatio", compRatio);
    aCompAttack = std::make_unique<SAttach> (apvts, "compAttack", compAttack);
    aCompRelease = std::make_unique<SAttach> (apvts, "compRelease", compRelease);
    aCompMakeup = std::make_unique<SAttach> (apvts, "compMakeup", compMakeup);
    aCompStereo = std::make_unique<SAttach> (apvts, "compStereo", compStereo);
    aCompMS = std::make_unique<CAttach> (apvts, "compMS", compMS);
    aCompType = std::make_unique<CAttach> (apvts, "compType", compType);
    aCompOn = std::make_unique<BAttach> (apvts, "compOn", compOnBtn);
    aCompSolo = std::make_unique<BAttach> (apvts, "compSolo", compSoloBtn);
    aCompDelta = std::make_unique<BAttach> (apvts, "compDelta", compDeltaBtn);

    // Saturation
    styleLabel (satTitle, 10); addAndMakeVisible (satTitle);
    addKnob (x1, x1Lbl, 40); addKnob (x2, x2Lbl, 40); addKnob (x3, x3Lbl, 40);
    styleFader (satMixFader); addAndMakeVisible (satMixFader);
    addToggle (satOnBtn); addToggle (satSoloBtn); addToggle (satDeltaBtn);
    aX1 = std::make_unique<SAttach> (apvts, "x1Hz", x1);
    aX2 = std::make_unique<SAttach> (apvts, "x2Hz", x2);
    aX3 = std::make_unique<SAttach> (apvts, "x3Hz", x3);
    aSatMix = std::make_unique<SAttach> (apvts, "satMix", satMixFader);
    aSatOn = std::make_unique<BAttach> (apvts, "satOn", satOnBtn);
    aSatSolo = std::make_unique<BAttach> (apvts, "satSolo", satSoloBtn);
    aSatDelta = std::make_unique<BAttach> (apvts, "satDelta", satDeltaBtn);
    for (int i = 0; i < 4; ++i)
    {
        satBands[(size_t) i] = std::make_unique<SatBand> (apvts, i);
        addAndMakeVisible (*satBands[(size_t) i]);
    }

    // Limiter
    styleLabel (limitTitle, 10); addAndMakeVisible (limitTitle);
    addKnob (limitThresh, limitThreshLbl, 36); addKnob (limitGain, limitGainLbl, 36);
    addKnob (limitAttack, limitAttackLbl, 36); addKnob (limitCeiling, limitCeilingLbl, 36);
    addKnob (limitRelease, limitReleaseLbl, 36);
    addToggle (limitOnBtn); addToggle (limitSoloBtn); addToggle (limitDeltaBtn);
    aLimitThresh = std::make_unique<SAttach> (apvts, "limitThresh", limitThresh);
    aLimitGain = std::make_unique<SAttach> (apvts, "limitGain", limitGain);
    aLimitAttack = std::make_unique<SAttach> (apvts, "limitAttack", limitAttack);
    aLimitCeiling = std::make_unique<SAttach> (apvts, "limitCeiling", limitCeiling);
    aLimitRelease = std::make_unique<SAttach> (apvts, "limitRelease", limitRelease);
    aLimitOn = std::make_unique<BAttach> (apvts, "limitOn", limitOnBtn);
    aLimitSolo = std::make_unique<BAttach> (apvts, "limitSolo", limitSoloBtn);
    aLimitDelta = std::make_unique<BAttach> (apvts, "limitDelta", limitDeltaBtn);

    // Master
    styleLabel (masterTitle, 10); addAndMakeVisible (masterTitle);
    addKnob (masterMTrim, masterMTrimLbl, 36); addKnob (masterHarmonics, masterHarmonicsLbl, 36);
    addKnob (masterShape, masterShapeLbl, 36); addKnob (masterDepth, masterDepthLbl, 36);
    styleFader (masterMix); addAndMakeVisible (masterMix); addLabel (masterMixLbl);
    addKnob (masterOutTrim, masterOutTrimLbl, 36);
    addToggle (masterOnBtn); addToggle (masterSoloBtn); addToggle (masterDeltaBtn);
    aMasterMTrim = std::make_unique<SAttach> (apvts, "masterMTrim", masterMTrim);
    aMasterHarm = std::make_unique<SAttach> (apvts, "masterHarmonics", masterHarmonics);
    aMasterShape = std::make_unique<SAttach> (apvts, "masterShape", masterShape);
    aMasterDepth = std::make_unique<SAttach> (apvts, "masterDepth", masterDepth);
    aMasterMix = std::make_unique<SAttach> (apvts, "masterMix", masterMix);
    aMasterOutTrim = std::make_unique<SAttach> (apvts, "masterOutTrim", masterOutTrim);
    aMasterOn = std::make_unique<BAttach> (apvts, "masterOn", masterOnBtn);
    aMasterSolo = std::make_unique<BAttach> (apvts, "masterSolo", masterSoloBtn);
    aMasterDelta = std::make_unique<BAttach> (apvts, "masterDelta", masterDeltaBtn);

    // Clipper
    styleLabel (clipperTitle, 10); addAndMakeVisible (clipperTitle);
    addKnob (clipDrive, clipDriveLbl, 40); addKnob (clipSoftness, clipSoftnessLbl, 40);
    addKnob (clipLink, clipLinkLbl, 40);
    addToggle (clipOnBtn); addToggle (clipSoloBtn); addToggle (clipDeltaBtn);
    aClipDrive = std::make_unique<SAttach> (apvts, "clipDrive", clipDrive);
    aClipSoft = std::make_unique<SAttach> (apvts, "clipSoftness", clipSoftness);
    aClipLink = std::make_unique<SAttach> (apvts, "clipLink", clipLink);
    aClipOn = std::make_unique<BAttach> (apvts, "clipOn", clipOnBtn);
    aClipSolo = std::make_unique<BAttach> (apvts, "clipSolo", clipSoloBtn);
    aClipDelta = std::make_unique<BAttach> (apvts, "clipDelta", clipDeltaBtn);

    aBypass = std::make_unique<BAttach> (apvts, "bypass", bypassBtn);
    aGlobalDelta = std::make_unique<BAttach> (apvts, "deltaGlob", deltaBtn);
    aGlobalComp = std::make_unique<BAttach> (apvts, "compGlob", compBtn);

    setResizable (true, true);
    setResizeLimits (1000, 550, 2400, 1200);
    setSize (1100, 600);
    startTimerHz (30);
}

FlowFormAudioProcessorEditor::~FlowFormAudioProcessorEditor() { setLookAndFeel (nullptr); }

void FlowFormAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour::fromRGB (30,30,35), r.getCentreX(), r.getY(),
                                              juce::Colour::fromRGB (10,10,12), r.getCentreX(), r.getBottom(), false));
    g.fillAll();
    g.setColour (accent().withAlpha (0.05f));
    g.drawRect (getLocalBounds(), 1);
}

void FlowFormAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    if (r.getWidth() < 1000) return;

    // Header
    auto hdr = r.removeFromTop (30);
    logo.setBounds (hdr.removeFromLeft (140).reduced (4, 2));
    auto ctr = hdr.removeFromLeft (280);
    auto abR = ctr.removeFromTop (18).reduced (0, 1);
    undoBtn.setBounds (abR.removeFromLeft (28)); redoBtn.setBounds (abR.removeFromLeft (28));
    abButton.setBounds (abR.removeFromLeft (30)); presetBox.setBounds (abR.removeFromLeft (100));
    oversampleBox.setBounds (abR.removeFromLeft (50));
    auto rt = hdr.removeFromRight (240);
    bypassBtn.setBounds (rt.removeFromLeft (55)); deltaBtn.setBounds (rt.removeFromLeft (35));
    compBtn.setBounds (rt.removeFromLeft (55));
    r.removeFromTop (4);
    auto scopeArea = r.removeFromTop (160);
    scope.setBounds (scopeArea);
    scopeToggleBtn.setBounds (scopeArea.getX() + scopeArea.getWidth() - 48, scopeArea.getY() + 2, 46, 20);
    r.removeFromTop (4);

    // Panels
    auto p = r; const int gap = 5;
    int pw[6] = { 110, 185, 450, 130, 150, (int) p.getWidth() - 110 - 185 - 450 - 130 - 145 - 150 };
    if (pw[5] < 100) pw[5] = 100;

    // Input
    auto pr = p.removeFromLeft (pw[0]); p.removeFromLeft (gap);
    inTitle.setBounds (pr.removeFromTop (14)); pr.removeFromTop (36);
    inHPFLbl.setBounds (pr.removeFromTop (10));
    auto r1 = pr.removeFromTop (50); inHPF.setBounds (r1.removeFromLeft (r1.getWidth()/2)); inLPF.setBounds (r1);
    inTrimLbl.setBounds (pr.removeFromTop (10)); inTrim.setBounds (pr.removeFromTop (46));
    inVoiceLbl.setBounds (pr.removeFromTop (10)); inVoice.setBounds (pr.removeFromTop (42));
    inBiasLbl.setBounds (pr.removeFromTop (10)); inBias.setBounds (pr.removeFromTop (42));
    auto ib = pr.removeFromBottom (22);
    inMonoBtn.setBounds (ib.removeFromLeft (32)); inPolarBtn.setBounds (ib.removeFromLeft (22));
    inDeltaBtn.setBounds (ib.removeFromLeft (22)); inCompBtn.setBounds (ib.removeFromLeft (32));

    // Compressor
    pr = p.removeFromLeft (pw[1]); p.removeFromLeft (gap);
    compTitle.setBounds (pr.removeFromTop (14));
    auto cb = pr.removeFromTop (16);
    compOnBtn.setBounds (cb.removeFromLeft (26)); compSoloBtn.setBounds (cb.removeFromLeft (28)); compDeltaBtn.setBounds (cb.removeFromLeft (22));
    auto cg = pr.removeFromTop (170).reduced (2);
    auto pp = [&] (juce::Slider& s1, juce::Label& l1, juce::Slider& s2, juce::Label& l2, int h) {
        auto rw = cg.removeFromTop(h); auto hf = rw.getWidth()/2;
        auto a = rw.removeFromLeft(hf); l1.setBounds(a.removeFromTop(10)); s1.setBounds(a);
        l2.setBounds(rw.removeFromTop(10)); s2.setBounds(rw); };
    pp (compSC, compSCLbl, compThresh, compThreshLbl, 56);
    pp (compRatio, compRatioLbl, compAttack, compAttackLbl, 56);
    pp (compRelease, compReleaseLbl, compMakeup, compMakeupLbl, 56);
    cg = cg.removeFromBottom (40); compStereoLbl.setBounds (cg.removeFromTop (10)); compStereo.setBounds (cg);
    auto cbot = pr.removeFromBottom (30);
    compMSLbl.setBounds (cbot.removeFromLeft (cbot.getWidth()/2).removeFromTop (12)); compMS.setBounds (cbot.removeFromLeft (cbot.getWidth()-30));
    compTypeLbl.setBounds (cbot.removeFromTop (12)); compType.setBounds (cbot);

    // Saturation
    pr = p.removeFromLeft (pw[2]); p.removeFromLeft (gap);
    satTitle.setBounds (pr.removeFromTop (14));
    auto sb = pr.removeFromTop (16);
    satOnBtn.setBounds (sb.removeFromLeft (26)); satSoloBtn.setBounds (sb.removeFromLeft (28)); satDeltaBtn.setBounds (sb.removeFromLeft (22));
    auto xr = pr.removeFromTop (60);
    x1Lbl.setBounds (xr.removeFromLeft (xr.getWidth()/3).removeFromTop (12)); x1.setBounds (xr.removeFromLeft (xr.getWidth()/3));
    x2Lbl.setBounds (xr.removeFromLeft (xr.getWidth()/2).removeFromTop (12)); x2.setBounds (xr.removeFromLeft (xr.getWidth()/2));
    x3Lbl.setBounds (xr.removeFromTop (12)); x3.setBounds (xr);
    pr.removeFromTop (2);
    auto sbr = pr.removeFromTop (220); int sw = (sbr.getWidth() - 6) / 4;
    for (int i = 0; i < 4; ++i) { satBands[(size_t)i]->setBounds (sbr.removeFromLeft (sw)); sbr.removeFromLeft (2); }
    auto sbot = pr.removeFromBottom (30); satMixFader.setBounds (sbot.reduced (10, 2));

    // Limiter
    pr = p.removeFromLeft (pw[3]); p.removeFromLeft (gap);
    limitTitle.setBounds (pr.removeFromTop (14));
    auto lb = pr.removeFromTop (16);
    limitOnBtn.setBounds (lb.removeFromLeft (26)); limitSoloBtn.setBounds (lb.removeFromLeft (28)); limitDeltaBtn.setBounds (lb.removeFromLeft (22));
    auto lg = pr.removeFromTop (170);
    pp (limitThresh, limitThreshLbl, limitGain, limitGainLbl, 56);
    pp (limitAttack, limitAttackLbl, limitCeiling, limitCeilingLbl, 56);
    pp (limitRelease, limitReleaseLbl, limitRelease, limitReleaseLbl, 56);

    // Master
    pr = p.removeFromLeft (pw[4]); p.removeFromLeft (gap);
    masterTitle.setBounds (pr.removeFromTop (14));
    auto mb = pr.removeFromTop (16);
    masterOnBtn.setBounds (mb.removeFromLeft (26)); masterSoloBtn.setBounds (mb.removeFromLeft (28)); masterDeltaBtn.setBounds (mb.removeFromLeft (22));
    auto mg = pr.removeFromTop (230);
    pp (masterMTrim, masterMTrimLbl, masterHarmonics, masterHarmonicsLbl, 56);
    pp (masterShape, masterShapeLbl, masterDepth, masterDepthLbl, 56);
    mg.removeFromTop (4);
    masterMixLbl.setBounds (mg.removeFromTop (10)); masterMix.setBounds (mg.removeFromTop (26));
    auto mbot = pr.removeFromBottom (50);
    masterOutTrimLbl.setBounds (mbot.removeFromTop (10)); masterOutTrim.setBounds (mbot);

    // Clipper
    pr = p.removeFromLeft (pw[5]);
    clipperTitle.setBounds (pr.removeFromTop (14));
    auto clb = pr.removeFromTop (16);
    clipOnBtn.setBounds (clb.removeFromLeft (26)); clipSoloBtn.setBounds (clb.removeFromLeft (28)); clipDeltaBtn.setBounds (clb.removeFromLeft (22));
    clipDriveLbl.setBounds (pr.removeFromTop (10)); clipDrive.setBounds (pr.removeFromTop (54));
    clipSoftnessLbl.setBounds (pr.removeFromTop (10)); clipSoftness.setBounds (pr.removeFromTop (54));
    clipLinkLbl.setBounds (pr.removeFromTop (10)); clipLink.setBounds (pr.removeFromTop (54));
}

void FlowFormAudioProcessorEditor::timerCallback()
{
    scope.repaint();
}
