#include "PluginEditor.h"

static juce::Colour accent() { return juce::Colour::fromRGB (0, 191, 255); }



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
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    logo.setFont (juce::FontOptions (20).withStyle ("Bold"));
    logo.setColour (juce::Label::textColourId, juce::Colour (DigitalCapersLNF::COL_ACCENT));
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

    // Zoom selector
    zoomBox.addItem ("50%",  1); zoomBox.addItem ("75%",  2);
    zoomBox.addItem ("100%", 3); zoomBox.addItem ("125%", 4);
    zoomBox.addItem ("150%", 5); zoomBox.addItem ("175%", 6);
    zoomBox.addItem ("200%", 7);
    zoomBox.setSelectedId (3, juce::dontSendNotification);
    zoomBox.onChange = [this] {
        static const float kFactors[] = { 0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f };
        applyZoom (kFactors[zoomBox.getSelectedId() - 1]);
    };
    addAndMakeVisible (zoomBox);

    setResizable (false, false);
    setSize (1400, 600);
    startTimerHz (30);
}

FlowFormAudioProcessorEditor::~FlowFormAudioProcessorEditor() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

void FlowFormAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int topH = juce::roundToInt (48 * zoomFactor);

    g.fillAll (juce::Colour (DigitalCapersLNF::COL_BG));

    // Panel backgrounds
    for (auto& pb : panelBounds)
        if (! pb.isEmpty())
            DigitalCapersLNF::drawPanel (g, pb);

    // Top bar (drawn after panels so it always overlaps the top edge)
    g.setColour (juce::Colour (DigitalCapersLNF::COL_TOPBAR_BG));
    g.fillRect (0, 0, getWidth(), topH);
    g.setColour (juce::Colour (DigitalCapersLNF::COL_PANEL_BORDER));
    g.drawLine (0.0f, (float) topH, (float) getWidth(), (float) topH, 1.0f);

    // Plugin name in top bar
    g.setColour (juce::Colour (DigitalCapersLNF::COL_TEXT));
    g.setFont (juce::FontOptions (14.0f * zoomFactor).withStyle ("Bold"));
    g.drawText ("FlowForm",
                juce::roundToInt (16 * zoomFactor), 0,
                juce::roundToInt (120 * zoomFactor), topH,
                juce::Justification::centredLeft);
}

void FlowFormAudioProcessorEditor::resized()
{
    const float f = zoomFactor;
    auto z = [f] (int px) { return juce::roundToInt (px * f); };

    auto r = getLocalBounds().withTrimmedTop (z (48)).reduced (z (12));
    if (r.getWidth() < z (800)) return;

    // Header
    auto hdr = r.removeFromTop (z (30));
    logo.setBounds (hdr.removeFromLeft (z (140)).reduced (z (4), z (2)));
    auto ctr = hdr.removeFromLeft (z (280));
    auto abR = ctr.removeFromTop (z (18)).reduced (0, z (1));
    undoBtn.setBounds (abR.removeFromLeft (z (28))); redoBtn.setBounds (abR.removeFromLeft (z (28)));
    abButton.setBounds (abR.removeFromLeft (z (30))); presetBox.setBounds (abR.removeFromLeft (z (100)));
    oversampleBox.setBounds (abR.removeFromLeft (z (50)));
    auto rt = hdr.removeFromRight (z (240));
    bypassBtn.setBounds (rt.removeFromLeft (z (55))); deltaBtn.setBounds (rt.removeFromLeft (z (35)));
    compBtn.setBounds (rt.removeFromLeft (z (55)));
    rt.removeFromLeft (z (8));
    zoomBox.setBounds (rt.removeFromLeft (z (70)));
    r.removeFromTop (z (4));
    auto scopeArea = r.removeFromTop (z (160));
    scope.setBounds (scopeArea);
    scopeToggleBtn.setBounds (scopeArea.getX() + scopeArea.getWidth() - z (48),
                              scopeArea.getY() + z (2), z (46), z (20));
    r.removeFromTop (z (4));

    // Layout helper: two label+knob pairs side by side in 'grid'
    auto pp = [f] (juce::Rectangle<int>& grid,
                   juce::Slider& s1, juce::Label& l1,
                   juce::Slider& s2, juce::Label& l2, int h)
    {
        auto rw = grid.removeFromTop (h);
        auto hf = rw.getWidth() / 2;
        auto a  = rw.removeFromLeft (hf);
        const int lh = juce::roundToInt (10 * f);
        l1.setBounds (a.removeFromTop (lh)); s1.setBounds (a);
        l2.setBounds (rw.removeFromTop (lh)); s2.setBounds (rw);
    };

    // Panels
    auto p = r;
    const int gap = z (6);
    int pw[6] = { z (168), z (260), z (470), z (184), z (112), 0 };
    pw[5] = p.getWidth() - pw[0] - pw[1] - pw[2] - pw[3] - pw[4] - 5 * gap;
    if (pw[5] < z (80)) pw[5] = z (80);

    // ── Input ────────────────────────────────────────────────────────────────
    auto pr = p.removeFromLeft (pw[0]); p.removeFromLeft (gap);
    panelBounds[0] = pr;
    inTitle.setBounds (pr.removeFromTop (z (14))); pr.removeFromTop (z (4));
    inHPFLbl.setBounds (pr.removeFromTop (z (10)));
    auto r1 = pr.removeFromTop (z (50));
    inHPF.setBounds (r1.removeFromLeft (r1.getWidth() / 2)); inLPF.setBounds (r1);
    inTrimLbl.setBounds (pr.removeFromTop (z (10))); inTrim.setBounds (pr.removeFromTop (z (46)));
    inVoiceLbl.setBounds (pr.removeFromTop (z (10))); inVoice.setBounds (pr.removeFromTop (z (42)));
    inBiasLbl.setBounds (pr.removeFromTop (z (10))); inBias.setBounds (pr.removeFromTop (z (42)));
    auto ib = pr.removeFromBottom (z (22));
    inMonoBtn.setBounds (ib.removeFromLeft (z (32))); inPolarBtn.setBounds (ib.removeFromLeft (z (22)));
    inDeltaBtn.setBounds (ib.removeFromLeft (z (22))); inCompBtn.setBounds (ib.removeFromLeft (z (32)));

    // ── Compressor ───────────────────────────────────────────────────────────
    pr = p.removeFromLeft (pw[1]); p.removeFromLeft (gap);
    panelBounds[1] = pr;
    compTitle.setBounds (pr.removeFromTop (z (14)));
    { auto cb = pr.removeFromTop (z (16));
      compOnBtn.setBounds (cb.removeFromLeft (z (26))); compSoloBtn.setBounds (cb.removeFromLeft (z (28)));
      compDeltaBtn.setBounds (cb.removeFromLeft (z (22))); }
    auto cbot = pr.removeFromBottom (z (28));
    { auto cl = cbot.removeFromLeft (cbot.getWidth() / 2);
      compMSLbl.setBounds (cl.removeFromTop (z (10))); compMS.setBounds (cl);
      compTypeLbl.setBounds (cbot.removeFromTop (z (10))); compType.setBounds (cbot); }
    auto cslArea = pr.removeFromBottom (z (38));
    compStereoLbl.setBounds (cslArea.removeFromTop (z (10))); compStereo.setBounds (cslArea);
    auto cg = pr.reduced (z (1));
    pp (cg, compSC,      compSCLbl,      compThresh,  compThreshLbl,  cg.getHeight() / 3);
    pp (cg, compRatio,   compRatioLbl,   compAttack,  compAttackLbl,  cg.getHeight() / 2);
    pp (cg, compRelease, compReleaseLbl, compMakeup,  compMakeupLbl,  cg.getHeight());

    // ── Saturation ───────────────────────────────────────────────────────────
    pr = p.removeFromLeft (pw[2]); p.removeFromLeft (gap);
    panelBounds[2] = pr;
    satTitle.setBounds (pr.removeFromTop (z (14)));
    { auto sb = pr.removeFromTop (z (16));
      satOnBtn.setBounds (sb.removeFromLeft (z (26))); satSoloBtn.setBounds (sb.removeFromLeft (z (28)));
      satDeltaBtn.setBounds (sb.removeFromLeft (z (22))); }
    { auto xr = pr.removeFromTop (z (62));
      int xw = xr.getWidth() / 3;
      auto x1r = xr.removeFromLeft (xw);
      auto x2r = xr.removeFromLeft (xw);
      auto x3r = xr;
      x1Lbl.setBounds (x1r.removeFromTop (z (18))); x1.setBounds (x1r);
      x2Lbl.setBounds (x2r.removeFromTop (z (18))); x2.setBounds (x2r);
      x3Lbl.setBounds (x3r.removeFromTop (z (18))); x3.setBounds (x3r); }
    pr.removeFromTop (z (2));
    auto sbr = pr.removeFromTop (juce::jmin (z (220), pr.getHeight() - z (32)));
    int sw = (sbr.getWidth() - z (6)) / 4;
    for (int i = 0; i < 4; ++i)
        { satBands[(size_t)i]->setBounds (sbr.removeFromLeft (sw)); sbr.removeFromLeft (z (2)); }
    auto sbot = pr.removeFromBottom (z (30));
    satMixFader.setBounds (sbot.reduced (z (10), z (2)));

    // ── Limiter ──────────────────────────────────────────────────────────────
    pr = p.removeFromLeft (pw[3]); p.removeFromLeft (gap);
    panelBounds[3] = pr;
    limitTitle.setBounds (pr.removeFromTop (z (14)));
    { auto btnCol = pr.removeFromRight (z (26));
      limitOnBtn.setBounds    (btnCol.removeFromTop (z (22))); btnCol.removeFromTop (z (3));
      limitSoloBtn.setBounds  (btnCol.removeFromTop (z (22))); btnCol.removeFromTop (z (3));
      limitDeltaBtn.setBounds (btnCol.removeFromTop (z (22))); }
    pr.removeFromTop (z (12));
    { auto kw = pr.removeFromTop (z (86));
      int  ks = juce::jmin (kw.getHeight(), z (80));
      limitThreshLbl.setBounds (kw.removeFromTop (z (11)).withSizeKeepingCentre (kw.getWidth(), z (11)));
      limitThresh.setBounds    (kw.withSizeKeepingCentre (ks, ks - z (4))); }
    pr.removeFromTop (z (16));
    { auto kw = pr.removeFromTop (z (74));
      int  ks = juce::jmin (kw.getHeight(), z (62));
      limitReleaseLbl.setBounds (kw.removeFromTop (z (11)).withSizeKeepingCentre (kw.getWidth(), z (11)));
      limitRelease.setBounds    (kw.withSizeKeepingCentre (ks, ks - z (4))); }

    // ── Master ───────────────────────────────────────────────────────────────
    pr = p.removeFromLeft (pw[4]); p.removeFromLeft (gap);
    panelBounds[4] = pr;
    masterTitle.setBounds (pr.removeFromTop (z (14)));
    { auto btnCol = pr.removeFromRight (z (26));
      masterOnBtn.setBounds    (btnCol.removeFromTop (z (22))); btnCol.removeFromTop (z (3));
      masterSoloBtn.setBounds  (btnCol.removeFromTop (z (22))); btnCol.removeFromTop (z (3));
      masterDeltaBtn.setBounds (btnCol.removeFromTop (z (22))); }
    pr.removeFromTop (z (16));
    { auto kw = pr.removeFromTop (z (90));
      int  ks = juce::jmin (kw.getHeight(), z (80));
      masterOutTrimLbl.setBounds (kw.removeFromTop (z (11)).withSizeKeepingCentre (kw.getWidth(), z (11)));
      masterOutTrim.setBounds    (kw.withSizeKeepingCentre (ks, ks - z (4))); }

    // ── Clipper ──────────────────────────────────────────────────────────────
    pr = p.removeFromLeft (pw[5]);
    panelBounds[5] = pr;
    clipperTitle.setBounds (pr.removeFromTop (z (14)));
    { auto clb = pr.removeFromTop (z (16));
      clipOnBtn.setBounds   (clb.removeFromLeft (z (26))); clipSoloBtn.setBounds  (clb.removeFromLeft (z (28)));
      clipDeltaBtn.setBounds (clb.removeFromLeft (z (22))); }
    clipDriveLbl.setBounds    (pr.removeFromTop (z (10))); clipDrive.setBounds    (pr.removeFromTop (z (54)));
    clipSoftnessLbl.setBounds (pr.removeFromTop (z (10))); clipSoftness.setBounds (pr.removeFromTop (z (54)));
    clipLinkLbl.setBounds     (pr.removeFromTop (z (10))); clipLink.setBounds     (pr.removeFromTop (z (54)));
}

void FlowFormAudioProcessorEditor::applyZoom (float z)
{
    zoomFactor = z;
    setSize (juce::roundToInt (1400 * z), juce::roundToInt (600 * z));
}

void FlowFormAudioProcessorEditor::timerCallback()
{
    scope.repaint();
}
