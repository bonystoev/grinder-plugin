#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Vintage analog-hardware LookAndFeel: chunky potentiometer knobs with
// metal bezel, warm silkscreen ticks, character accent pointer.
class GrinderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrinderLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                           bool isButtonDown, bool isOver,
                           juce::ToggleButton&) override;

    // Current character accent (set by the editor).
    juce::Colour accent { 0xffe07b39 };

    static juce::Colour getAccentFromSlider (const juce::Slider& slider,
                                             juce::Colour fallback);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrinderLookAndFeel)
};
