#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GrinderAudioProcessor::GrinderAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMS", createParameterLayout())
{
    driveParam          = parameters.getRawParameterValue ("drive");
    characterParam      = parameters.getRawParameterValue ("character");
    tiltParam           = parameters.getRawParameterValue ("tilt");
    midCharacterParam   = parameters.getRawParameterValue ("midCharacter");
    lowCutParam         = parameters.getRawParameterValue ("lowCut");
    presenceParam       = parameters.getRawParameterValue ("presence");
    wowFlutterParam     = parameters.getRawParameterValue ("wowFlutter");
    noiseParam          = parameters.getRawParameterValue ("noise");
    mixParam            = parameters.getRawParameterValue ("mix");
    outputParam         = parameters.getRawParameterValue ("output");
    mixSoloTiltParam    = parameters.getRawParameterValue ("mixSoloTilt");
}

juce::AudioProcessorValueTreeState::ParameterLayout
GrinderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive", 0.0f, 100.0f, 35.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character",
        juce::StringArray { "Tube", "Diode", "Fuzz", "Tape", "Cassette", "Dictaphone" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tilt", 1 }, "Tilt",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "midCharacter", 1 }, "Mid Character", 0.0f, 100.0f, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lowCut", 1 }, "Low Cut",
        juce::NormalisableRange<float> (20.0f, 800.0f, 0.0f, 0.3f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "presence", 1 }, "Presence",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "wowFlutter", 1 }, "Wow & Flutter", 0.0f, 100.0f, 15.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "noise", 1 }, "Noise",
        juce::NormalisableRange<float> (-80.0f, -40.0f, 0.5f), -60.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix", 0.0f, 100.0f, 100.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "mixSoloTilt", 1 }, "Tilt on Wet Only", false));

    return layout;
}

//==============================================================================
void GrinderAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    syncParams();
    for (auto& ch : channels)
        ch.prepare (sampleRate, dspParams);
}

void GrinderAudioProcessor::releaseResources() {}

bool GrinderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut) return false;
    return mainIn == juce::AudioChannelSet::stereo()
           || mainIn == juce::AudioChannelSet::mono();
}

void GrinderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    syncParams();

    const int numChannels = std::min (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int c = 0; c < numChannels; ++c)
    {
        auto* data = buffer.getWritePointer (c);
        channels[static_cast<size_t> (c)].processBlock (data, data, numSamples);
    }

    double levelSum = 0.0;
    int levelCount = 0;
    for (int c = 0; c < numChannels; ++c)
    {
        auto* data = buffer.getReadPointer (c);
        for (int n = 0; n < numSamples; ++n)
        {
            levelSum += static_cast<double> (data[n]) * static_cast<double> (data[n]);
            ++levelCount;
        }
    }
    const double rms = levelCount > 0 ? std::sqrt (levelSum / levelCount) : 0.0;
    outputLevelDb.store (static_cast<float> (juce::Decibels::gainToDecibels (rms, -60.0)),
                         std::memory_order_relaxed);
}

void GrinderAudioProcessor::syncParams()
{
    dspParams.drive          = driveParam->load() / 100.0f;
    const int charIndex      = static_cast<int> (characterParam->load());
    dspParams.character      = static_cast<grinder::Params::Character> (charIndex);
    dspParams.tiltDb         = tiltParam->load();
    dspParams.midCharacter   = midCharacterParam->load() / 100.0f;
    dspParams.lowCutHz       = lowCutParam->load();
    dspParams.presenceDb     = presenceParam->load();
    dspParams.wowFlutter     = wowFlutterParam->load() / 100.0f;
    const float noiseDb      = noiseParam->load();
    dspParams.noiseEnabled   = noiseDb > -79.5f;
    dspParams.noiseDb        = noiseDb;
    dspParams.mix           = mixParam->load() / 100.0f;
    dspParams.outputDb      = outputParam->load();
    dspParams.mixSoloTilt   = mixSoloTiltParam->load() != 0.0f;

    for (auto& ch : channels)
        ch.updateParams (dspParams);
}

//==============================================================================
juce::AudioProcessorEditor* GrinderAudioProcessor::createEditor() { return new GrinderAudioProcessorEditor (*this); }

void GrinderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GrinderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrinderAudioProcessor();
}
