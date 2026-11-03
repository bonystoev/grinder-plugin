#pragma once

#include "PluginProcessor.h"
#include "GrinderLookAndFeel.h"

// Animated VU meter with real ballistics (300 ms rise / 1.5 s fall).
class VUMeter : public juce::Component, private juce::Timer
{
public:
    explicit VUMeter (GrinderAudioProcessor&);
    ~VUMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float vuAngleFor (float db);

    GrinderAudioProcessor& processor;
    float needlePos = 0.0f;   // 0..1 across the VU arc
    float smoothedDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeter)
};

// Vertical peak LED chain (-40 dB ... +6 dB).
class LedChain : public juce::Component, private juce::Timer
{
public:
    explicit LedChain (GrinderAudioProcessor&);
    ~LedChain() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    GrinderAudioProcessor& processor;
    float smoothedDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedChain)
};

// 6-position rotary character selector switch (skew work: click/wheel).
class CharacterSwitch : public juce::Component, private juce::Timer
{
public:
    explicit CharacterSwitch (GrinderAudioProcessor&);
    ~CharacterSwitch() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::Colour accentFor (int characterIndex);

private:
    void timerCallback() override;
    void setCharacterIndex (int index);
    int positionFromPoint (juce::Point<float> p) const;

    GrinderAudioProcessor& processor;
    juce::AudioParameterChoice* characterParam = nullptr;

    static constexpr int numPositions = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CharacterSwitch)
};
