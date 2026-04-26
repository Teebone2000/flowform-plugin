#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

//==============================================================================
// Small LED dot — OVR / DYN indicators in the Input panel
//==============================================================================
class LedDot : public juce::Component
{
public:
    juce::Colour onColour  { 0xffef4444 };
    juce::Colour offColour { 0xff1e1e1e };

    void setLit (bool b) noexcept { if (b != lit) { lit = b; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        if (lit)
        {
            g.setColour (onColour.withAlpha (0.35f));
            g.fillEllipse (b.expanded (2.5f));
            g.setColour (onColour);
            g.fillEllipse (b);
            g.setColour (onColour.brighter (0.5f));
            g.fillEllipse (b.reduced (b.getWidth() * 0.3f));
        }
        else
        {
            g.setColour (offColour);
            g.fillEllipse (b);
            g.setColour (juce::Colour (0xff404040));
            g.drawEllipse (b, 0.5f);
        }
    }

private:
    bool lit = false;
};

//==============================================================================
// Vertical LED meter  –  fills bottom-to-top
//   • Bottom 85 % = electric blue   • Top 15 % = red
//   • Segment count adapts to component height (min 1 px / segment)
//==============================================================================
class VerticalMeterBar : public juce::Component
{
public:
    // Feed linear amplitude  (0 → 1+),  maps to -60 → 0 dB range
    void setLevel (float lin) noexcept
    {
        float db   = 20.0f * std::log10 (std::max (std::abs (lin), 1e-7f));
        float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (std::abs (norm - level) > 0.003f) { level = norm; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const float W  = (float) getWidth();
        const float H  = (float) getHeight();
        const int   N  = juce::jmax (10, (int)(H / 1.5f));   // never sub-pixel
        const float sh = H / N;

        for (int i = 0; i < N; ++i)
        {
            // i = 0 → bottom of bar,  i = N-1 → top
            float y        = H - (i + 1) * sh;
            float normPos  = (float) i / (N - 1);
            bool  lit      = normPos <= level;

            juce::Colour c;
            if (!lit)                c = juce::Colour (0xff1a1a1a);
            else if (normPos > 0.85f) c = juce::Colour (0xffef4444).withAlpha (0.95f);
            else                     c = juce::Colour (0xff00bfff).withAlpha (0.90f);

            g.setColour (c);
            g.fillRect (1.0f, y + 0.5f, W - 2.0f, sh - 1.0f);
        }

        // 1-px border
        g.setColour (juce::Colour (0xff2a2a2a));
        g.drawRect (getLocalBounds());
    }

private:
    float level = 0.0f;
};

//==============================================================================
// Horizontal LED meter  –  30 segments
//   isGainReduction = true  → fills right-to-left in red (GR meter)
//   otherwise               → fills left-to-right, blue → orange → red
//==============================================================================
class HorizontalMeterBar : public juce::Component
{
public:
    bool isGainReduction = false;

    // Linear amplitude → 0 dB reference = 1.0
    void setLevel (float lin) noexcept
    {
        float db   = 20.0f * std::log10 (std::max (std::abs (lin), 1e-7f));
        float norm = juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 40.0f);
        if (std::abs (norm - level) > 0.003f) { level = norm; repaint(); }
    }

    // For GR meters: pass negative dB (e.g. -6.0 for 6 dB reduction)
    void setGainReductionDb (float grDb) noexcept
    {
        float norm = juce::jlimit (0.0f, 1.0f, (-grDb) / 20.0f);
        if (std::abs (norm - level) > 0.003f) { level = norm; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const int   N   = 30;
        const float H   = (float) getHeight();
        const float W   = (float) getWidth();
        const float sw  = W / N;

        g.setColour (juce::Colour (0xff0d0d0d));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 2.0f);

        for (int i = 0; i < N; ++i)
        {
            float x, normPos;
            bool  lit;

            if (isGainReduction)
            {
                int j    = N - 1 - i;
                x        = (float) j * sw;
                normPos  = (float)(N - 1 - j) / (N - 1);
                lit      = normPos <= level;
            }
            else
            {
                x       = (float) i * sw;
                normPos = (float) i / (N - 1);
                lit     = normPos <= level;
            }

            juce::Colour c;
            if (!lit)                           c = juce::Colour (0xff181818);
            else if (isGainReduction)           c = juce::Colour (0xffef4444).withAlpha (0.9f);
            else if (normPos > 0.85f)           c = juce::Colour (0xffef4444).withAlpha (0.9f);
            else if (normPos > 0.70f)           c = juce::Colour (0xffF97316).withAlpha (0.9f);
            else                                c = juce::Colour (0xff00bfff).withAlpha (0.9f);

            g.setColour (c);
            g.fillRect (x + 1.0f, 1.0f, sw - 1.5f, H - 2.0f);
        }
    }

private:
    float level = 0.0f;
};

//==============================================================================
// Compression waveform display
//   • Blue waveform (bottom)  = input signal shape
//   • Red waveform (top)      = gain-reduction delta
//   • Dashed white line       = threshold
//   • tick() advances animation – call from timer
//==============================================================================
class CompCurveComponent : public juce::Component
{
public:
    void setParams (float threshDb, float ratio, float grDb) noexcept
    {
        if (threshDb != thresh || ratio != compRatio || grDb != compGR)
        { thresh = threshDb; compRatio = ratio; compGR = grDb; repaint(); }
    }

    void tick() noexcept { offset += 2; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        // Background + border
        g.setColour (juce::Colour (0xff141414));
        g.fillRoundedRectangle (0, 0, W, H, 3.0f);
        g.setColour (juce::Colour (0xff00bfff).withAlpha (0.25f));
        g.drawRoundedRectangle (0.5f, 0.5f, W - 1.0f, H - 1.0f, 3.0f, 1.0f);

        // Grid
        g.setColour (juce::Colour (0xff242424));
        for (int i = 1; i < 4; ++i)
            g.drawLine (0, H * i / 4.0f, W, H * i / 4.0f, 1.0f);

        // Threshold line
        float threshNorm = juce::jlimit (0.0f, 1.0f, (0.0f - thresh) / 40.0f);
        float threshY    = threshNorm * H;
        {
            juce::Path d;
            float x = 0;
            bool  on = true;
            while (x < W)
            { if (on) { d.startNewSubPath (x, threshY); d.lineTo (juce::jmin (x + 4.0f, W), threshY); }
              x += 4.0f; on = !on; }
            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.strokePath (d, juce::PathStrokeType (1.5f));
        }

        // Waveform generation (same pseudo-random shape as React reference)
        const int pts = juce::jmax (2, (int) W);
        std::vector<float> inY (pts), cY (pts);
        const float ctr = H * 0.5f;

        for (int x = 0; x < pts; ++x)
        {
            float amp = (std::sin ((x + offset) * 0.05f) * 0.30f
                       + std::sin ((x + offset) * 0.15f) * 0.15f
                       + std::sin ((x + offset) * 0.08f) * 0.20f
                       + std::sin ((x + offset) * 0.30f) * std::sin ((x + offset) * 0.17f) * 0.15f) * H * 0.40f;
            inY[x] = ctr + amp;

            float dist = std::abs (inY[x] - ctr);
            float tDist = std::abs (threshY - ctr);
            cY[x] = inY[x];
            if (dist > tDist)
            {
                float excess = dist - tDist;
                float comp   = excess / std::max (compRatio, 1.0f);
                float dir    = inY[x] > ctr ? 1.0f : -1.0f;
                cY[x] = ctr + dir * (tDist + comp);
            }
        }

        // Input waveform — blue, at bottom half
        {
            juce::Path p;
            for (int x = 0; x < pts; ++x)
            {
                float y = H - (H / 2.0f - inY[x]) * 0.50f;
                if (x == 0) p.startNewSubPath (0, y); else p.lineTo ((float)x, y);
            }
            g.setColour (juce::Colour (0xff00bfff).withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (5.0f));
            g.setColour (juce::Colour (0xff00bfff));
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }

        // Delta (GR) — red, at top
        {
            juce::Path p;
            for (int x = 0; x < pts; ++x)
            {
                float delta = std::abs (inY[x] - cY[x]) * 0.8f;
                if (x == 0) p.startNewSubPath (0, delta); else p.lineTo ((float)x, delta);
            }
            g.setColour (juce::Colour (0xffef4444).withAlpha (0.85f));
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }

        // Labels
        g.setFont (juce::FontOptions (8.0f));
        g.setColour (juce::Colour (0xff00bfff).withAlpha (0.8f));
        g.drawText ("INPUT", 3, (int)H - 13, 40, 11, juce::Justification::left);
        g.setColour (juce::Colour (0xffef4444).withAlpha (0.8f));
        g.drawText ("DELTA", 3, 2, 40, 11, juce::Justification::left);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::FontOptions (7.5f));
        g.drawText (juce::String (thresh, 1) + "dB", (int)W - 42, (int)threshY - 12, 40, 11,
                    juce::Justification::right);
    }

private:
    float thresh    = -18.0f;
    float compRatio =   4.0f;
    float compGR    =   0.0f;
    int   offset    =   0;
};

//==============================================================================
// Saturation waveform display
//   • Animated blue fundamental waveform across 4 frequency bands
//   • Red harmonic overtones (2nd–4th) scaled by drive × mix
//   • Vertical divider lines at x1 / x2 / x3 split frequencies
//   • tick() advances animation – call from timer
//==============================================================================
class SatWaveformComponent : public juce::Component
{
public:
    struct BandParams { float drive = 0.0f; float mix = 1.0f; bool on = true; };

    void setSplits (float x1, float x2, float x3) noexcept
    {
        split[0] = x1; split[1] = x2; split[2] = x3;
    }

    void setBand (int b, float driveNorm, float mixNorm, bool on) noexcept
    {
        if (b >= 0 && b < 4) bands[b] = { driveNorm, mixNorm, on };
    }

    void setAudioInput (const float* inData, const float* deltaData, int len) noexcept
    {
        // Pull latest audio data into circular buffer for rendering
        if (inData && len > 0)
        {
            for (int i = 0; i < juce::jmin (len, displayLen); ++i)
            {
                displayBuf[(readIdx + i) % displayLen] = inData[i];
                deltaBuf[(readIdx + i) % displayLen]  = deltaData ? deltaData[i] : 0.0f;
            }
            readIdx = (readIdx + len) % displayLen;
            hasData = true;
        }
    }

    void tick() noexcept
    {
        // Only advance offset if we have no real audio data (fallback animation)
        if (!hasData)
            offset += 2;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        // Background + border
        g.setColour (juce::Colour (0xff1a1f2b));
        g.fillRoundedRectangle (0, 0, W, H, 3.0f);
        g.setColour (juce::Colour (0xff00bfff).withAlpha (0.25f));
        g.drawRoundedRectangle (0.5f, 0.5f, W - 1.0f, H - 1.0f, 3.0f, 1.0f);

        // Frequency → X (log-biased, identical to React WaveformDisplay)
        auto freqToX = [W](float freq) -> float
        {
            const float logMin = std::log10 (20.0f), logMax = std::log10 (20000.0f);
            float logF = std::log10 (std::max (freq, 20.0f));
            float norm = (logF - logMin) / (logMax - logMin);
            const float lbLog = (std::log10 (500.0f)  - logMin) / (logMax - logMin);
            const float hbLog = (std::log10 (5000.0f) - logMin) / (logMax - logMin);
            float pos;
            if      (norm <= lbLog) pos = (norm / lbLog) * 0.30f;
            else if (norm <= hbLog) pos = 0.30f + ((norm - lbLog) / (hbLog - lbLog)) * 0.45f;
            else                    pos = 0.75f + ((norm - hbLog) / (1.0f - hbLog)) * 0.25f;
            return pos * W;
        };

        float sx[3] = { freqToX (split[0]), freqToX (split[1]), freqToX (split[2]) };
        float bx[5] = { 0, sx[0], sx[1], sx[2], W };

        // Band background tints
        const juce::Colour tint[4] = {
            juce::Colour (0xff191919), juce::Colour (0xff222222),
            juce::Colour (0xff2b2b2b), juce::Colour (0xff323232) };
        for (int b = 0; b < 4; ++b)
        {
            g.setColour (tint[b].withAlpha (bands[b].on ? 1.0f : 0.3f));
            g.fillRect (bx[b], 0.0f, bx[b+1] - bx[b], H);
        }

        // Divider lines
        g.setColour (juce::Colour (0xff00bfff).withAlpha (0.4f));
        for (int d = 0; d < 3; ++d)
            g.drawLine (sx[d], 0, sx[d], H, 1.5f);

        const float ctr = H * 0.5f;

        // Draw real audio from displayBuf when available
        {
            juce::Path inPath;
            bool inFirst = true;
            for (int xi = 0; xi < (int)W && xi < displayLen; ++xi)
            {
                int bufIdx = (readIdx + xi) % displayLen;
                float val = displayBuf[bufIdx];
                float amp = val * H * 0.42f;
                float y = ctr - juce::jlimit (-ctr, ctr, amp);
                if (inFirst) { inPath.startNewSubPath ((float)xi, y); inFirst = false; }
                else          inPath.lineTo ((float)xi, y);
            }
            g.setColour (juce::Colour (0xff00bfff).withAlpha (hasData ? 0.85f : 0.12f));
            g.strokePath (inPath, juce::PathStrokeType (1.4f));

            // Delta (change from processing) in red
            if (hasData)
            {
                juce::Path dPath;
                bool dFirst = true;
                for (int xi = 0; xi < (int)W && xi < displayLen; ++xi)
                {
                    int bufIdx = (readIdx + xi) % displayLen;
                    float val = deltaBuf[bufIdx] * 2.0f; // amplify delta for visibility
                    float amp = val * H * 0.20f;
                    float y = ctr - juce::jlimit (-ctr, ctr, amp);
                    if (dFirst) { dPath.startNewSubPath ((float)xi, y); dFirst = false; }
                    else         dPath.lineTo ((float)xi, y);
                }
                g.setColour (juce::Colour (0xffff4444).withAlpha (0.60f));
                g.strokePath (dPath, juce::PathStrokeType (0.8f));
            }

            // Fallback: synthetic animation when no real audio
            if (!hasData)
            {
                for (int b = 0; b < 4; ++b)
                {
                    float xS = bx[b], xE = bx[b+1];
                    juce::Path fp;
                    bool fFirst = true;
                    float amp = H * 0.25f;
                    for (int xi = (int)xS; xi < (int)xE; ++xi)
                    {
                        float y = ctr + std::sin ((xi + offset) * 0.020f) * amp * 0.5f;
                        if (fFirst) { fp.startNewSubPath ((float)xi, y); fFirst = false; }
                        else         fp.lineTo ((float)xi, y);
                    }
                    g.setColour (juce::Colour (0xff00bfff).withAlpha (0.20f));
                    g.strokePath (fp, juce::PathStrokeType (1.0f));
                }
            }
        }

        // Split freq labels
        g.setFont (juce::FontOptions (8.0f));
        g.setColour (juce::Colour (0xff00bfff).withAlpha (0.65f));
        for (int d = 0; d < 3; ++d)
            g.drawText (formatFreq (split[d]), (int)sx[d] - 18, 2, 36, 10,
                        juce::Justification::centred);
    }

private:
    float      split[3] = { 300.0f, 2000.0f, 8000.0f };
    BandParams bands[4];
    int        offset   = 0;

    // Real audio display buffer
    static constexpr int displayLen = 1024;
    float      displayBuf[displayLen] = {0};
    float      deltaBuf[displayLen]   = {0};
    int        readIdx = 0;
    bool       hasData = false;

    static juce::String formatFreq (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k"
                             : juce::String ((int)hz) + "Hz";
    }

    float splineInterp (const float* buf, float idx, int len) const
    {
        int i0 = (int) idx % len;
        int i1 = (i0 + 1) % len;
        float t = idx - (float)(int)idx;
        return buf[i0] * (1.0f - t) + buf[i1] * t;
    }
};
