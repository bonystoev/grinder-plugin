#include "GrinderComponents.h"

namespace
{
const char* characterNames[6] = { "TUBE", "DIODE", "FUZZ", "TAPE", "CASSETTE", "DICTAPHONE" };

constexpr float vuMinDb = -30.0f;   // dBFS scale
constexpr float vuMaxDb = 3.0f;
} // namespace

//==============================================================================
VUMeter::VUMeter (GrinderAudioProcessor& p, const juce::ToggleButton& source)
    : processor (p), sourceSwitch (source)
{
    startTimerHz (30);
}

VUMeter::~VUMeter() { stopTimer(); }

float VUMeter::vuAngleFor (float db)
{
    const auto clamped = juce::jlimit (vuMinDb, vuMaxDb, db);
    return juce::jmap (clamped, vuMinDb, vuMaxDb, 0.0f, 1.0f);
}

void VUMeter::timerCallback()
{
    const auto target = sourceSwitch.getToggleState() ? processor.getInputLevelDb()
                                                      : processor.getOutputLevelDb();
    const auto dt = 1.0f / 30.0f;

    // VU ballistics: fast-ish rise, slow fall (approximated one-pole).
    const auto tau = target > smoothedDb ? 0.10f : 0.7f;
    smoothedDb += (target - smoothedDb) * (dt / (tau + dt));

    const auto targetPos = vuAngleFor (smoothedDb);
    const auto posTau = targetPos > needlePos ? 0.045f : 0.16f;
    needlePos += (targetPos - needlePos) * (dt / (posTau + dt));

    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Backlit cream window.
    juce::ColourGradient back (juce::Colour (0xfff6ecd4), bounds.getCentreX(),
                               bounds.getY(),
                               juce::Colour (0xffdcc99c), bounds.getCentreX(),
                               bounds.getBottom(), false);
    g.setGradientFill (back);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (juce::Colour (0xff2b2622));
    g.drawRoundedRectangle (bounds, 6.0f, 2.5f);

    const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getBottom() - 14.0f);
    const auto needleLen = bounds.getHeight() * 0.58f;
    const auto arcRadius = needleLen * 0.94f;

    // Direction helper: t in 0..1 sweeps the needle from -45° (left) to
    // +45° (right) around the upward axis.
    auto dirFor = [] (float t)
    {
        const auto degrees = juce::jmap (t, -45.0f, 45.0f);
        return juce::Point<float> (std::sin (juce::degreesToRadians (degrees)),
                                   -std::cos (juce::degreesToRadians (degrees)));
    };

    // Scale ticks with dB labels.
    g.setColour (juce::Colour (0xff4a4238));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
    const float marks[] = { -30.0f, -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 3.0f };
    for (auto db : marks)
    {
        const auto t = juce::jmap (db, vuMinDb, vuMaxDb, 0.0f, 1.0f);
        const auto dir = dirFor (t);
        const auto outer = centre + dir * arcRadius;
        const auto inner = centre + dir * (arcRadius - (db >= 0.0f ? 9.0f : 6.0f));
        g.setColour (db >= 0.0f ? juce::Colour (0xffa83226) : juce::Colour (0xff4a4238));
        g.drawLine (juce::Line<float> (inner, outer), db >= 0.0f ? 2.2f : 1.4f);

        const auto textPos = centre + dir * (arcRadius - 18.0f);
        g.setColour (db >= 0.0f ? juce::Colour (0xffa83226) : juce::Colour (0xff4a4238));
        g.drawText (juce::String (static_cast<int> (db)),
                    juce::Rectangle<float> (30.0f, 12.0f).withCentre (textPos),
                    juce::Justification::centred);
    }

    // Red zone arc above 0 dBFS (segmented to avoid arc-direction pitfalls).
    const auto redStartT = juce::jmap (0.0f, vuMinDb, vuMaxDb, 0.0f, 1.0f);
    g.setColour (juce::Colour (0xffa83226).withAlpha (0.28f));
    juce::Point<float> prev = centre + dirFor (redStartT) * arcRadius * 1.02f;
    for (int i = 1; i <= 8; ++i)
    {
        const auto t = redStartT + (1.0f - redStartT) * static_cast<float> (i) / 8.0f;
        const auto cur = centre + dirFor (t) * arcRadius * 1.02f;
        g.drawLine (juce::Line<float> (prev, cur), 3.0f);
        prev = cur;
    }

    // Needle.
    {
        const auto dir = dirFor (needlePos);
        g.setColour (juce::Colour (0xff2b2622));
        juce::Line<float> needle (centre, centre + dir * needleLen);
        g.drawLine (needle, 2.2f);
    }

    // Pivot screw.
    g.setColour (juce::Colour (0xff8f887c));
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (centre));

    g.setColour (juce::Colour (0xff4a4238));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold")));
    g.drawText ("dBFS", bounds.withHeight (18.0f), juce::Justification::centred);
}

//==============================================================================
LedChain::LedChain (GrinderAudioProcessor& p) : processor (p)
{
    startTimerHz (30);
}

LedChain::~LedChain() { stopTimer(); }

void LedChain::timerCallback()
{
    const auto target = processor.getOutputLevelDb();
    const auto dt = 1.0f / 30.0f;
    // Peak-ish: fast rise, medium fall.
    const auto tau = target > smoothedDb ? 0.02f : 0.25f;
    smoothedDb += (target - smoothedDb) * (dt / (tau + dt));
    repaint();
}

void LedChain::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff232327));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour (0xff101014));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    constexpr int numLeds = 14;
    const auto track = bounds.reduced (6.0f, 8.0f);
    const auto ledH = track.getHeight() / static_cast<float> (numLeds) - 2.0f;
    const auto ledW = track.getWidth() * 0.75f;
    const auto ledX = track.getCentreX() - ledW * 0.5f;

    // Bottom LED: -40 dB, top LED: +6 dB.
    for (int i = 0; i < numLeds; ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (numLeds - 1);
        const auto thresholdDb = juce::jmap (t, -40.0f, 6.0f);
        const auto lit = smoothedDb >= thresholdDb;

        juce::Colour baseColour = t > 0.82f ? juce::Colour (0xffc53226)
                                  : t > 0.55f ? juce::Colour (0xffe0a232)
                                              : juce::Colour (0xff4fae54);
        const auto colour = lit ? baseColour
                                : baseColour.withAlpha (0.14f).withBrightness (0.12f);
        const auto ledY = track.getBottom() - static_cast<float> (i + 1) * (ledH + 2.0f) - 2.0f;
        const auto ledRect = juce::Rectangle<float> (ledX, ledY, ledW, ledH);
        g.setColour (colour);
        g.fillRoundedRectangle (ledRect, ledH * 0.45f);
        if (lit)
        {
            g.setColour (colour.withAlpha (0.35f));
            g.fillRoundedRectangle (ledRect.expanded (1.5f), ledH * 0.6f);
        }
    }
}

//==============================================================================
CharacterSwitch::CharacterSwitch (GrinderAudioProcessor& p) : processor (p)
{
    characterParam = dynamic_cast<juce::AudioParameterChoice*> (
        processor.getApvts().getParameter ("character"));
    jassert (characterParam != nullptr);
    startTimerHz (30);
}

CharacterSwitch::~CharacterSwitch() { stopTimer(); }

juce::Colour CharacterSwitch::accentFor (int characterIndex)
{
    switch (characterIndex)
    {
        case 0: return juce::Colour (0xffe07b39); // Tube: orange
        case 1: return juce::Colour (0xffd9a521); // Diode: amber
        case 2: return juce::Colour (0xffc53226); // Fuzz: red
        case 3: return juce::Colour (0xff4fae54); // Tape: green
        case 4: return juce::Colour (0xff3d7ec9); // Cassette: blue
        case 5: return juce::Colour (0xff8f4fc9); // Dictaphone: purple
        default: return juce::Colour (0xffe07b39);
    }
}

void CharacterSwitch::timerCallback() { repaint(); }

int CharacterSwitch::positionFromPoint (juce::Point<float> p) const
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto v = p - centre;
    auto angle = std::atan2 (v.x, -v.y); // 0 = up, clockwise positive
    if (angle < 0.0f) angle += juce::MathConstants<float>::twoPi;

    const auto sector = juce::MathConstants<float>::twoPi / static_cast<float> (numPositions);
    const auto index = juce::roundToInt (angle / sector) % numPositions;
    return index;
}

void CharacterSwitch::setCharacterIndex (int index)
{
    const auto norm = static_cast<float> (index) / static_cast<float> (numPositions - 1);
    auto* param = processor.getApvts().getParameter ("character");
    param->beginChangeGesture();
    param->setValueNotifyingHost (norm);
    param->endChangeGesture();
}

void CharacterSwitch::mouseDown (const juce::MouseEvent& e)
{
    setCharacterIndex (positionFromPoint (e.position));
}

void CharacterSwitch::mouseDrag (const juce::MouseEvent& e)
{
    setCharacterIndex (positionFromPoint (e.position));
}

void CharacterSwitch::mouseWheelMove (const juce::MouseEvent&,
                                     const juce::MouseWheelDetails& wheel)
{
    const auto current = characterParam != nullptr ? characterParam->getIndex() : 0;
    auto next = current + (wheel.deltaY > 0.0f ? 1 : -1);
    next = juce::jlimit (0, numPositions - 1, next);
    setCharacterIndex (next);
}

void CharacterSwitch::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f * 0.60f;
    const auto index = characterParam != nullptr ? characterParam->getIndex() : 0;
    const auto accent = accentFor (index);

    // Position labels around the switch.
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold")));
    for (int i = 0; i < numPositions; ++i)
    {
        const auto a = static_cast<float> (i) / static_cast<float> (numPositions)
                       * juce::MathConstants<float>::twoPi;
        const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
        const auto labelPos = centre + dir * (radius + 20.0f);
        const auto selected = (i == index);
        g.setColour (selected ? accent : juce::Colour (0xff5a4a38));
        juce::String label = characterNames[i];
        if (i == 4) label = "CASS";       // keep labels compact
        if (i == 5) label = "DICTA";
        g.drawText (label, juce::Rectangle<float> (56.0f, 14.0f).withCentre (labelPos),
                    juce::Justification::centred);
    }

    // Knurled metal switch body.
    {
        juce::ColourGradient body (juce::Colour (0xffd9d3c6), centre.x - radius,
                                    centre.y - radius,
                                    juce::Colour (0xff77726a), centre.x + radius * 0.5f,
                                    centre.y + radius, false);
        body.addColour (0.5f, juce::Colour (0xffaca69a));
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        g.setColour (juce::Colour (0xff3c382f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                       1.5f);

        // Knurling ring.
        const auto knurlRadius = radius * 0.88f;
        g.setColour (juce::Colour (0xff55504a));
        for (int i = 0; i < 24; ++i)
        {
            const auto a = static_cast<float> (i) / 24.0f
                           * juce::MathConstants<float>::twoPi;
            const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
            g.drawLine (juce::Line<float> (centre + dir * (knurlRadius * 0.8f),
                                           centre + dir * knurlRadius),
                        1.4f);
        }
    }

    // Pointer.
    {
        const auto a = static_cast<float> (index) / static_cast<float> (numPositions)
                       * juce::MathConstants<float>::twoPi;
        const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
        const auto pointerLen = radius * 0.92f;
        juce::Path pointer;
        pointer.addTriangle (centre, centre + dir.rotatedAboutOrigin (juce::degreesToRadians (6.0f)) * pointerLen,
                             centre + dir.rotatedAboutOrigin (juce::degreesToRadians (-6.0f)) * pointerLen);
        g.setColour (accent);
        g.fillPath (pointer);
    }

    // Centre cap.
    g.setColour (juce::Colour (0xff2b2622));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.30f, radius * 0.30f).withCentre (centre));
}
