#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class FlowFormAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::AudioProcessorValueTreeState::Listener,
                                      private juce::Timer
{
public:
    using APVTS = FlowFormAudioProcessor::APVTS;

    FlowFormAudioProcessorEditor (FlowFormAudioProcessor&);
    ~FlowFormAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    void parameterChanged (const juce::String& paramID, float newValue) override;

private:
    void timerCallback() override;
    void loadUI();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void sendToUI (const juce::String& json);

    FlowFormAudioProcessor& audioProcessor;
    juce::WebBrowserComponent webView;

    bool uiLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowFormAudioProcessorEditor)
};
