#pragma once

#include "PluginProcessor.h"
#include "GrinderLookAndFeel.h"
#include "GrinderComponents.h"

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
    int lastCharacterIndex = -1;
    void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius) const;
    void drawReel (juce::Graphics&, juce::Point<float> centre, float radius) const;
    void drawKnobLabel (juce::Graphics&, const juce::Component& knob,
                        const juce::String& text) const;

    GrinderAudioProcessor& processor;

    GrinderLookAndFeel look;

    // Big row.
    juce::Slider driveSlider, mixSlider, outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment,
        mixAttachment, outputAttachment;

    // Mini row.
    juce::Slider tiltSlider, midCharacterSlider, lowCutSlider, presenceSlider,
        wowFlutterSlider, noiseSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tiltAttachment,
        midCharacterAttachment, lowCutAttachment, presenceAttachment, wowFlutterAttachment,
        noiseAttachment;

    juce::ToggleButton tiltWetOnlyButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tiltWetOnlyAttachment;

    CharacterSwitch characterSwitch;
    VUMeter vuMeter;
    LedChain ledChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrinderAudioProcessorEditor)
};
