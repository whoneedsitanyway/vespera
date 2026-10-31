#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vespera
{

//==============================================================================
//  Tactile look-and-feel with a switchable dark / soft-light theme.
//
//  Knobs are drawn like chunky hardware controls: a raised cylinder with a
//  light pointer and a ring of tick marks (no value arc).  The palette is a
//  deep midnight blue with moonlight, amber and warm orange accents for the
//  notes, pattern, voice, filter and character sections.
//==============================================================================
class VesperaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum ColourIds
    {
        bgColourId        = 0x1a2b0001,
        panelColourId     = 0x1a2b0002,
        enclosureColourId = 0x1a2b0003,
        accentColourId    = 0x1a2b0004,
        accentDimColourId = 0x1a2b0005,
        coolColourId      = 0x1a2b0006,
        textColourId      = 0x1a2b0007,
        textDimColourId   = 0x1a2b0008,
        hotColourId       = 0x1a2b0009
    };

    VesperaLookAndFeel();

    void setDark (bool shouldBeDark);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    void applyPalette();
    bool dark = true;
};

//==============================================================================
//  A small square button that draws a vector icon (save / sun / moon / log).
//==============================================================================
class IconButton : public juce::Button
{
public:
    enum class Icon { Save, Sun, Moon, Log };

    IconButton (Icon iconToDraw, const juce::String& name);

    void setIcon (Icon newIcon) { icon = newIcon; repaint(); }

    void setWarning (bool shouldWarn)
    {
        if (warning != shouldWarn) { warning = shouldWarn; repaint(); }
    }

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    Icon icon;
    bool warning = false;
};

} // namespace vespera
