#include "GrinderLookAndFeel.h"

GrinderLookAndFeel::GrinderLookAndFeel() = default;

juce::Colour GrinderLookAndFeel::getAccentFromSlider (const juce::Slider& slider,
                                                      juce::Colour fallback)
{
    const auto* var = slider.getProperties().getVarPointer ("accent");
    if (var != nullptr && var->isString())
        return juce::Colour::fromString (var->toString());
    return fallback;
}

void GrinderLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = getAccentFromSlider (slider, accentColour);

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (w),
                                                static_cast<float> (h));
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f * 0.82f;
    const auto angle = rotaryStartAngle
                       + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Silkscreen tick marks around the knob.
    const auto tickRadius = radius + 5.0f;
    const auto tickCount = 11;
    for (int i = 0; i < tickCount; ++i)
    {
        const auto a = rotaryStartAngle
                       + static_cast<float> (i) / static_cast<float> (tickCount - 1)
                         * (rotaryEndAngle - rotaryStartAngle);
        const auto tickOuter = juce::Point<float> (std::sin (a), -std::cos (a));
        const auto inner = centre + tickOuter * (tickRadius - 3.5f);
        const auto outer = centre + tickOuter * tickRadius;
        const auto active = static_cast<float> (i) / static_cast<float> (tickCount - 1)
                            <= sliderPos + 0.001f;
        g.setColour (active ? accent.darker (0.1f)
                            : juce::Colour (0xff5a4a38).withAlpha (0.85f));
        juce::Line<float> tick (inner, outer);
        g.drawLine (tick, active ? 2.4f : 1.6f);
    }

    // Metal bezel.
    {
        juce::ColourGradient bezel (juce::Colour (0xffcfc9bd), centre.x - radius,
                                    centre.y - radius,
                                    juce::Colour (0xff6e6a62), centre.x + radius * 0.4f,
                                    centre.y + radius);
        bezel.addColour (0.55f, juce::Colour (0xffa9a49a));
        g.setGradientFill (bezel);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.2f, radius * 2.2f)
                            .withCentre (centre));
    }

    // Knob body: dark chocolate with a subtle top highlight.
    {
        const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                              .withCentre (centre);
        juce::ColourGradient grad (juce::Colour (0xff5c4232).brighter (0.25f),
                                   centre.x, centre.y - radius,
                                   juce::Colour (0xff2e211a), centre.x,
                                   centre.y + radius);
        grad.addColour (0.5f, juce::Colour (0xff463227));
        g.setGradientFill (grad);
        g.fillEllipse (body);

        g.setColour (juce::Colour (0xff171310).withAlpha (0.7f));
        g.drawEllipse (body, 1.0f);

        // Grooved skirt ring.
        auto skirt = body.reduced (radius * 0.16f);
        g.setColour (juce::Colour (0xff241a14));
        g.drawEllipse (skirt, 1.2f);
    }

    // Pointer line in accent colour.
    {
        const auto pointerLength = radius * 0.78f;
        const auto direction = juce::Point<float> (std::sin (angle), -std::cos (angle));
        const auto pointerLine = juce::Line<float> (
            centre + direction * (radius * 0.24f), centre + direction * pointerLength);
        g.setColour (accent);
        g.drawLine (pointerLine, radius * 0.10f);
    }

    // Centre pivot dot.
    g.setColour (juce::Colour (0xffc9bfae).withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.16f, radius * 0.16f).withCentre (centre));
}

void GrinderLookAndFeel::drawToggleButton (juce::Graphics& g,
                                           juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    const auto isOn = button.getToggleState();

    // Toggle plate: dark metal with two positions.
    juce::ColourGradient plate (juce::Colour (0xff3a3a40), bounds.getTopLeft(),
                                 juce::Colour (0xff232327), bounds.getBottomRight());
    g.setGradientFill (plate);
    g.fillRoundedRectangle (bounds, bounds.getWidth() * 0.25f);
    g.setColour (juce::Colour (0xff15151a));
    g.drawRoundedRectangle (bounds, bounds.getWidth() * 0.25f, 1.0f);

    // Labels ON / OFF.
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (bounds.getHeight() * 0.22f)
                                                 .withStyle ("Bold")));
    const auto onColour = isOn ? juce::Colour (0xffd8d2c2)
                              : juce::Colour (0xff6a6a70);
    const auto offColour = isOn ? juce::Colour (0xff6a6a70)
                                : juce::Colour (0xffd8d2c2);
    g.setColour (onColour);
    g.drawText ("ON", bounds.removeFromTop (bounds.getHeight() * 0.35f),
                juce::Justification::centred);
    g.setColour (offColour);
    g.drawText ("OFF", bounds, juce::Justification::centred);

    // Lever: knob that slides between the two labels.
    const auto leverTrack = button.getLocalBounds().toFloat().reduced (
        button.getLocalBounds().getWidth() * 0.30f, 2.0f);
    const auto leverH = leverTrack.getHeight() * 0.5f;
    const auto leverY = isOn ? leverTrack.getY()
                             : leverTrack.getBottom() - leverH;
    const auto leverRect = juce::Rectangle<float> (leverTrack.getX(), leverY,
                                                   leverTrack.getWidth(), leverH)
                               .reduced (1.5f);
    juce::ColourGradient lever (juce::Colour (0xffe6e0d2), leverRect.getTopLeft(),
                                juce::Colour (0xff8f887c), leverRect.getBottomRight());
    g.setGradientFill (lever);
    g.fillRoundedRectangle (leverRect, 3.0f);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bounds.expanded (1.0f), 4.0f);
    }
}
