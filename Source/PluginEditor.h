#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CamelotWheel.h"

class KeyFinderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit KeyFinderAudioProcessorEditor(KeyFinderAudioProcessor&);
    ~KeyFinderAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void drawCamelotWheel(juce::Graphics& g, juce::Rectangle<float> area);
    void drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area);
    void drawKeyLabel(juce::Graphics& g, juce::Rectangle<float> area);

    KeyFinderAudioProcessor& audioProcessor;

    KeyDetector::Result lastResult;
    float displayedLevelNorm = 0.0f; // per smorzare il VU meter graficamente

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyFinderAudioProcessorEditor)
};
