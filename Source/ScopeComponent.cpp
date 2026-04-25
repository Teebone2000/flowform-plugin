#include "ScopeComponent.h"

void ScopeFifo::push (const float* inL, const float* inR, const float* dL, const float* dR, int n)
{
    const auto cap = (int) bufferL.size();
    for (int i = 0; i < n; ++i)
    {
        int idx = (w + i) % cap;
        bufferL[(size_t) idx] = inL ? inL[i] : 0.0f;
        bufferR[(size_t) idx] = inR ? inR[i] : bufferL[(size_t) idx];
        deltaL[(size_t) idx]  = dL  ? dL[i]  : 0.0f;
        deltaR[(size_t) idx]  = dR  ? dR[i]  : deltaL[(size_t) idx];
    }
    w = (w + n) % cap;
}

int ScopeFifo::pull (float* outL, float* outR, float* outDL, float* outDR, int max) const
{
    const auto cap = (int) bufferL.size();
    int rp = w.load();
    int count = std::min (max, cap);
    for (int i = 0; i < count; ++i)
    {
        int idx = (rp + i) % cap;
        outL[i]  = bufferL[(size_t) idx];
        outR[i]  = bufferR[(size_t) idx];
        outDL[i] = deltaL[(size_t) idx];
        outDR[i] = deltaR[(size_t) idx];
    }
    return count;
}

static juce::Colour accent() { return juce::Colour::fromRGB (0, 191, 255); }

void ScopeComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (r, 8.0f);

    if (! showScope)
    {
        g.setColour (accent().withAlpha (0.3f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText ("Scope Off", r.toNearestInt(), juce::Justification::centred, 1);
        return;
    }

    const int n = fifo.pull (inL.data(), inR.data(), dL.data(), dR.data(), (int) inL.size());
    if (n <= 4) return;

    auto plot = r.reduced (8.0f);
    auto midY = plot.getCentreY();

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawLine (plot.getX(), midY, plot.getRight(), midY, 1.0f);

    // Dry (input) — neutral white/grey
    juce::Path dryPath;
    for (int i = 0; i < n; ++i)
    {
        float t = (float) i / (float) (n - 1);
        float x = plot.getX() + t * plot.getWidth();
        float y = midY - juce::jlimit (-1.0f, 1.0f, inL[i]) * (plot.getHeight() * 0.42f);
        if (i == 0) dryPath.startNewSubPath (x, y); else dryPath.lineTo (x, y);
    }
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.strokePath (dryPath, juce::PathStrokeType (1.0f));

    // Wet (processed = input + delta) — accent electric blue
    juce::Path wetPath;
    for (int i = 0; i < n; ++i)
    {
        float t = (float) i / (float) (n - 1);
        float x = plot.getX() + t * plot.getWidth();
        float wet = inL[i] + dL[i];
        float y = midY - juce::jlimit (-1.0f, 1.0f, wet) * (plot.getHeight() * 0.42f);
        if (i == 0) wetPath.startNewSubPath (x, y); else wetPath.lineTo (x, y);
    }
    g.setColour (accent().withAlpha (0.65f));
    g.strokePath (wetPath, juce::PathStrokeType (1.6f));

    // Delta envelope (thin, shows what changed)
    juce::Path dPath;
    for (int i = 0; i < n; ++i)
    {
        float t = (float) i / (float) (n - 1);
        float x = plot.getX() + t * plot.getWidth();
        float y = midY - juce::jlimit (-1.0f, 1.0f, dL[i] * 2.0f) * (plot.getHeight() * 0.35f);
        if (i == 0) dPath.startNewSubPath (x, y); else dPath.lineTo (x, y);
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.strokePath (dPath, juce::PathStrokeType (0.8f));
}
