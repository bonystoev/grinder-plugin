#include "PluginEditor.h"

GrinderAudioProcessorEditor::GrinderAudioProcessorEditor (GrinderAudioProcessor& p)
    : AudioProcessorEditor (p), processor (p), generic (p)
{
    addAndMakeVisible (generic);
    setSize (420, 560);
    startTimerHz (30);
}

GrinderAudioProcessorEditor::~GrinderAudioProcessorEditor() { stopTimer(); }

void GrinderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c22));
}

void GrinderAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}

void GrinderAudioProcessorEditor::timerCallback() {}
