#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class FlowFormAudioProcessor;

struct ScopeFifo
{
    static constexpr int cap = 1024;
    void reset() { w = 0; }
    void push (const float* inL, const float* inR, const float* dL, const float* dR, int n);
    int pull (float* outL, float* outR, float* outDL, float* outDR, int max) const;
    int peek (float* outL, float* outR, float* outDL, float* outDR, int max) const;
private:
    std::array<float, cap> bufferL {}, bufferR {}, deltaL {}, deltaR {};
    std::atomic<int> w { 0 };
};

class ScopeComponent : public juce::Component
{
public:
    ScopeComponent (ScopeFifo& f) : fifo (f) {}
    void paint (juce::Graphics&) override;
    void setShowScope (bool show) { showScope = show; repaint(); }
    bool getShowScope() const noexcept { return showScope; }
private:
    ScopeFifo& fifo;
    bool showScope = true;
    std::array<float, ScopeFifo::cap> inL {}, inR {}, dL {}, dR {};
};
