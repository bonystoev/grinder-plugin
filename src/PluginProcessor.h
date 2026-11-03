#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/grinder_dsp.h"

class GrinderAudioProcessor : public juce::AudioProcessor
{
public:
    GrinderAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() { return parameters; }

    // Post-processing RMS level in dBFS (for the VU meter / LED chain).
    float getOutputLevelDb() const noexcept { return outputLevelDb.load (std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void syncParams();

    juce::AudioProcessorValueTreeState parameters;
    grinder::Params dspParams;
    std::array<grinder::GrinderChannel, 2> channels;
    std::atomic<float> outputLevelDb { -60.0f };
    double currentSampleRate = 48000.0;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* tiltParam = nullptr;
    std::atomic<float>* midCharacterParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* presenceParam = nullptr;
    std::atomic<float>* wowFlutterParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixSoloTiltParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrinderAudioProcessor)
};
