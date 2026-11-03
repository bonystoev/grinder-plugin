#include "PluginEditor.h"

namespace
{
constexpr int panelWidth = 900;
constexpr int panelHeight = 470;

void configureKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::degreesToRadians (225.0f),
                           juce::degreesToRadians (495.0f), true);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::transparentWhite);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentWhite);
}
} // namespace

GrinderAudioProcessorEditor::GrinderAudioProcessorEditor (GrinderAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      characterSwitch (p),
      vuMeter (p),
      ledChain (p)
{
    setLookAndFeel (&look);

    // ---- Big knobs -------------------------------------------------------
    configureKnob (driveSlider);
    configureKnob (mixSlider);
    configureKnob (outputSlider);

    driveSlider.setRange (0.0, 100.0, 0.1);
    driveSlider.setDoubleClickReturnValue (true, 35.0);
    mixSlider.setDoubleClickReturnValue (true, 100.0);
    outputSlider.setDoubleClickReturnValue (true, 0.0);

    // ---- Mini knobs ------------------------------------------------------
    configureKnob (tiltSlider);
    configureKnob (midCharacterSlider);
    configureKnob (lowCutSlider);
    configureKnob (presenceSlider);
    configureKnob (wowFlutterSlider);
    configureKnob (noiseSlider);

    tiltSlider.setDoubleClickReturnValue (true, 0.0);
    midCharacterSlider.setDoubleClickReturnValue (true, 0.0);
    lowCutSlider.setDoubleClickReturnValue (true, 80.0);
    presenceSlider.setDoubleClickReturnValue (true, 0.0);
    wowFlutterSlider.setDoubleClickReturnValue (true, 15.0);
    noiseSlider.setDoubleClickReturnValue (true, -60.0);

    for (auto* knob : std::initializer_list<juce::Slider*> {
             &driveSlider, &mixSlider, &outputSlider,
             &tiltSlider, &midCharacterSlider, &lowCutSlider, &presenceSlider,
             &wowFlutterSlider, &noiseSlider })
    {
        knob->setLookAndFeel (&look);
        addAndMakeVisible (*knob);
    }

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "drive", driveSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "mix", mixSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "output", outputSlider);
    tiltAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "tilt", tiltSlider);
    midCharacterAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.getApvts(), "midCharacter", midCharacterSlider);
    lowCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "lowCut", lowCutSlider);
    presenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "presence", presenceSlider);
    wowFlutterAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.getApvts(), "wowFlutter", wowFlutterSlider);
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getApvts(), "noise", noiseSlider);

    tiltWetOnlyButton.setClickingTogglesState (true);
    tiltWetOnlyButton.setLookAndFeel (&look);
    addAndMakeVisible (tiltWetOnlyButton);
    tiltWetOnlyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getApvts(), "mixSoloTilt", tiltWetOnlyButton);

    addAndMakeVisible (characterSwitch);
    addAndMakeVisible (vuMeter);
    addAndMakeVisible (ledChain);

    setSize (panelWidth, panelHeight);
    setResizable (false, false);
    startTimerHz (30);
}

GrinderAudioProcessorEditor::~GrinderAudioProcessorEditor()
{
    stopTimer();
    tiltWetOnlyButton.setLookAndFeel (nullptr);
    for (auto* knob : std::initializer_list<juce::Slider*> {
             &driveSlider, &mixSlider, &outputSlider,
             &tiltSlider, &midCharacterSlider, &lowCutSlider, &presenceSlider,
             &wowFlutterSlider, &noiseSlider })
    {
        knob->setLookAndFeel (nullptr);
    }
    setLookAndFeel (nullptr);
}

void GrinderAudioProcessorEditor::timerCallback()
{
    const auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
        processor.getApvts().getParameter ("character"));
    if (choice == nullptr)
        return;
    const auto index = choice->getIndex();
    look.accent = CharacterSwitch::accentFor (index);
    if (index != lastCharacterIndex)
    {
        lastCharacterIndex = index;
        for (auto* knob : std::initializer_list<juce::Slider*> {
                 &driveSlider, &mixSlider, &outputSlider,
                 &tiltSlider, &midCharacterSlider, &lowCutSlider, &presenceSlider,
                 &wowFlutterSlider, &noiseSlider })
            knob->repaint();
    }
}

void GrinderAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> centre,
                                             float radius) const
{
    juce::ColourGradient grad (juce::Colour (0xffb9b3a6), centre.x - radius, centre.y - radius,
                               juce::Colour (0xff6b665e), centre.x + radius, centre.y + radius,
                               false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    g.setColour (juce::Colour (0xff3c382f));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                   1.0f);
    juce::Line<float> slot (centre.x - radius * 0.7f, centre.y + radius * 0.35f,
                            centre.x + radius * 0.7f, centre.y - radius * 0.35f);
    g.drawLine (slot, radius * 0.35f);
}

void GrinderAudioProcessorEditor::drawReel (juce::Graphics& g, juce::Point<float> centre,
                                           float radius) const
{
    juce::ColourGradient grad (juce::Colour (0xff4c4c55), centre.x - radius,
                                centre.y - radius,
                                juce::Colour (0xff21212a), centre.x + radius,
                                centre.y + radius, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    g.setColour (juce::Colour (0xff15151c));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                   1.5f);

    // Spokes.
    g.setColour (juce::Colour (0xff2b2b36));
    for (int i = 0; i < 3; ++i)
    {
        const auto a = juce::degreesToRadians (30.0f + 120.0f * i);
        const auto dir = juce::Point<float> (std::cos (a), std::sin (a));
        g.drawLine (juce::Line<float> (centre - dir * radius * 0.75f,
                                       centre + dir * radius * 0.75f),
                    radius * 0.22f);
    }

    // Hub.
    g.setColour (juce::Colour (0xff8a857c));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.36f, radius * 0.36f).withCentre (centre));
}

void GrinderAudioProcessorEditor::drawKnobLabel (juce::Graphics& g, const juce::Component& knob,
                                                const juce::String& text) const
{
    auto bounds = knob.getBoundsInParent().toFloat();
    const auto labelArea = bounds.removeFromBottom (26.0f).translated (0.0f, 4.0f);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff4a3a28));
    g.drawText (text.toUpperCase(), labelArea, juce::Justification::centred);
}

void GrinderAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Warm cream faceplate with a subtle vertical sheen.
    juce::ColourGradient plate (juce::Colour (0xfff1e9d6), bounds.getCentreX(), bounds.getY(),
                               juce::Colour (0xffddd0b4), bounds.getCentreX(),
                               bounds.getBottom(), false);
    plate.addColour (0.6f, juce::Colour (0xffe7dcc4));
    g.setGradientFill (plate);
    g.fillAll();

    // Dark tape-machine head strip at the top.
    const auto headStrip = juce::Rectangle<float> (bounds.getX(), bounds.getY(),
                                                    bounds.getWidth(), 86.0f);
    juce::ColourGradient strip (juce::Colour (0xff37373f), headStrip.getTopLeft(),
                                juce::Colour (0xff1d1d24), headStrip.getBottomLeft(), false);
    g.setGradientFill (strip);
    g.fillRect (headStrip);

    // Vent slots in the head strip.
    g.setColour (juce::Colour (0xff0f0f14));
    for (int i = 0; i < 8; ++i)
    {
        const auto x = headStrip.getX() + 28.0f + i * 16.0f;
        g.fillRect (juce::Rectangle<float> (x, headStrip.getY() + 12.0f, 6.0f, 14.0f));
    }

    // Reels.
    drawReel (g, juce::Point<float> (70.0f, 52.0f), 34.0f);
    drawReel (g, juce::Point<float> (bounds.getWidth() - 70.0f, 52.0f), 34.0f);

    // Brand silkscreen.
    g.setColour (juce::Colour (0xffe8dfc9));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (34.0f).withStyle ("Bold")));
    g.drawText ("GRINDER", juce::Rectangle<float> (headStrip.getX(), 18.0f,
                                                   headStrip.getWidth(), 40.0f),
                juce::Justification::centred);
    g.setColour (juce::Colour (0xffb0a489));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    g.drawText ("SATURATION & DISTORTION", juce::Rectangle<float> (headStrip.getX(), 56.0f,
                                                                    headStrip.getWidth(), 16.0f),
                juce::Justification::centred);

    // Separation line under the head strip.
    g.setColour (juce::Colour (0xffb3a486));
    g.fillRect (juce::Rectangle<float> (bounds.getX(), 86.0f, bounds.getWidth(), 2.0f));

    // Screws.
    drawScrew (g, juce::Point<float> (18.0f, 102.0f), 6.0f);
    drawScrew (g, juce::Point<float> (bounds.getWidth() - 18.0f, 102.0f), 6.0f);
    drawScrew (g, juce::Point<float> (18.0f, bounds.getHeight() - 18.0f), 6.0f);
    drawScrew (g, juce::Point<float> (bounds.getWidth() - 18.0f, bounds.getHeight() - 18.0f), 6.0f);

    // Knob labels.
    drawKnobLabel (g, driveSlider, "DRIVE");
    drawKnobLabel (g, characterSwitch, "CHARACTER");
    drawKnobLabel (g, mixSlider, "MIX");
    drawKnobLabel (g, outputSlider, "OUTPUT");
    drawKnobLabel (g, tiltSlider, "TILT");
    drawKnobLabel (g, midCharacterSlider, "MID");
    drawKnobLabel (g, lowCutSlider, "LOW CUT");
    drawKnobLabel (g, presenceSlider, "PRESENCE");
    drawKnobLabel (g, wowFlutterSlider, "WOW/FLUTTER");
    drawKnobLabel (g, noiseSlider, "NOISE");

    // Toggle switch label.
    {
        auto tb = tiltWetOnlyButton.getBoundsInParent().toFloat();
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold")));
        g.setColour (juce::Colour (0xff4a3a28));
        g.drawText ("TILT ON WET ONLY", 
                    juce::Rectangle<float> (tb.getX() - 30.0f, tb.getBottom() + 4.0f,
                                            tb.getWidth() + 60.0f, 16.0f),
                    juce::Justification::centred);
    }
}

void GrinderAudioProcessorEditor::resized()
{
    // VU window top-centre (below head strip).
    vuMeter.setBounds (juce::Rectangle<int> (330, 104, 240, 128));

    characterSwitch.setBounds (juce::Rectangle<int> (388, 250, 124, 124));

    // Big knobs row.
    driveSlider.setBounds (juce::Rectangle<int> (56, 246, 104, 104));
    mixSlider.setBounds (juce::Rectangle<int> (212, 258, 80, 80));
    outputSlider.setBounds (juce::Rectangle<int> (560, 258, 80, 80));

    // Toggle switch.
    tiltWetOnlyButton.setBounds (juce::Rectangle<int> (760, 120, 36, 52));

    // Mini knobs row.
    const auto miniY = 384;
    const auto miniSize = 56;
    const auto miniGap = 128;
    const auto miniStartX = 60;
    const std::initializer_list<juce::Slider*> minis = {
        &tiltSlider, &midCharacterSlider, &lowCutSlider, &presenceSlider,
        &wowFlutterSlider, &noiseSlider
    };
    int i = 0;
    for (auto* knob : minis)
    {
        knob->setBounds (miniStartX + i * miniGap, miniY, miniSize, miniSize);
        ++i;
    }

    // LED chain right edge.
    ledChain.setBounds (juce::Rectangle<int> (852, 120, 30, 340));
}
