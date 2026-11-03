#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class GrinderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit GrinderAudioProcessorEditor (GrinderAudioProcessor&);
    ~GrinderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    GrinderAudioProcessor& processor;

    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrinderAudioProcessorEditor)
};
