#include "VesperaLookAndFeel.h"

namespace vespera
{

//==============================================================================
VesperaLookAndFeel::VesperaLookAndFeel()
{
    applyPalette();
}

void VesperaLookAndFeel::setDark (bool shouldBeDark)
{
    if (dark == shouldBeDark)
        return;
    dark = shouldBeDark;
    applyPalette();
}

void VesperaLookAndFeel::applyPalette()
{
    juce::Colour bg, panel, enclosure, accent, accentDim, cool, hot, text, textDim;

    if (dark)
    {
        bg        = juce::Colour (0xff08090e);
        panel     = juce::Colour (0xff161a26);
        enclosure = juce::Colour (0xff1c2130);
        accent    = juce::Colour (0xffa9c2ff);
        accentDim = juce::Colour (0xff4b5a7d);
        cool      = juce::Colour (0xffe0b27a);
        hot       = juce::Colour (0xffe08a5a);
        text      = juce::Colour (0xffe8eaf2);
        textDim   = juce::Colour (0xff878ea6);
    }
    else
    {
        bg        = juce::Colour (0xffd4d8e2);
        panel     = juce::Colour (0xffe6e9f1);
        enclosure = juce::Colour (0xffdde1ea);
        accent    = juce::Colour (0xff3f5fa8);
        accentDim = juce::Colour (0xff8b96ae);
        cool      = juce::Colour (0xffa26b2c);
        hot       = juce::Colour (0xffb04a1e);
        text      = juce::Colour (0xff202431);
        textDim   = juce::Colour (0xff626a7d);
    }

    setColour (bgColourId,        bg);
    setColour (panelColourId,     panel);
    setColour (enclosureColourId, enclosure);
    setColour (accentColourId,    accent);
    setColour (accentDimColourId, accentDim);
    setColour (coolColourId,      cool);
    setColour (hotColourId,       hot);
    setColour (textColourId,      text);
    setColour (textDimColourId,   textDim);

    setColour (juce::ResizableWindow::backgroundColourId, enclosure);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, textDim);
    setColour (juce::Slider::textBoxOutlineColourId, textDim.withAlpha (0.30f));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, accentDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId, dark ? text : juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, accentDim);
    setColour (juce::TextButton::textColourOffId, dark ? textDim : text);
    setColour (juce::TextButton::textColourOnId, dark ? bg : juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, textDim);
    setColour (juce::ToggleButton::tickColourId, accent);
}

//==============================================================================
void VesperaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                           int width, int height,
                                           float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    const auto area   = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto panel = findColour (panelColourId);
    const auto accent = findColour (accentColourId);
    const auto textDim = findColour (textDimColourId);

    const int numTicks = 11;
    const float tickOuter = radius * 0.98f;
    const float tickInner = radius * 0.80f;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = (float) i / (float) (numTicks - 1);
        const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float sx = centre.x + std::sin (a) * tickInner;
        const float sy = centre.y - std::cos (a) * tickInner;
        const float ex = centre.x + std::sin (a) * tickOuter;
        const float ey = centre.y - std::cos (a) * tickOuter;
        const bool active = a <= angle + 1.0e-4f;
        g.setColour (active ? accent : textDim.withAlpha (0.30f));
        g.drawLine (sx, sy, ex, ey, active ? 2.0f : 1.2f);
    }

    const auto knobR = radius * 0.68f;
    const auto knob  = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (dark ? 0.55f : 0.22f));
    g.fillEllipse (knob.translated (0.0f, 2.5f).expanded (1.0f));

    {
        juce::ColourGradient body (panel.brighter (0.24f), knob.getX(), knob.getY(),
                                   panel.darker (0.40f), knob.getRight(), knob.getBottom(), false);
        g.setGradientFill (body);
        g.fillEllipse (knob);
    }

    const auto face = knob.reduced (knobR * 0.20f);
    {
        juce::ColourGradient faceGrad (panel.brighter (0.30f), face.getX(), face.getY(),
                                       panel.darker (0.18f), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (faceGrad);
        g.fillEllipse (face);
    }

    g.setColour (juce::Colours::black.withAlpha (dark ? 0.65f : 0.35f));
    g.drawEllipse (knob, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (dark ? 0.06f : 0.25f));
    g.drawEllipse (knob.reduced (1.5f), 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (centre.x + std::sin (angle) * (knobR * 0.18f),
                             centre.y - std::cos (angle) * (knobR * 0.18f));
    pointer.lineTo (centre.x + std::sin (angle) * (knobR * 0.92f),
                    centre.y - std::cos (angle) * (knobR * 0.92f));
    g.setColour (accent);
    g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

//==============================================================================
void VesperaLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                       bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
    const float corner = 5.0f;

    const auto panel = findColour (panelColourId);
    const auto accent = findColour (accentColourId);
    const auto accentDim = findColour (accentDimColourId);

    g.setColour (juce::Colours::black.withAlpha (dark ? 0.35f : 0.12f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (panel);
    g.fillRoundedRectangle (r.reduced (1.0f), corner);
    g.setColour (accentDim.withAlpha (0.7f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    juce::Path arrow;
    const float cx = (float) width - 14.0f;
    const float cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.isEnabled() ? accent : accentDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

//==============================================================================
void VesperaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&,
                                               bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 5.0f;
    const bool on = button.getToggleState();

    const auto base = button.findColour (juce::TextButton::buttonColourId);
    const auto accent = findColour (accentColourId);
    const auto accentDim = findColour (accentDimColourId);

    auto fill = on ? accent : base;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down)        fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (accentDim.withAlpha (on ? 0.0f : 0.7f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

//==============================================================================
void VesperaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool highlighted, bool)
{
    const auto r = button.getLocalBounds().toFloat();
    const float boxSize = juce::jmin (r.getHeight() - 4.0f, 15.0f);
    juce::Rectangle<float> box (r.getX() + 2.0f, r.getCentreY() - boxSize * 0.5f,
                                boxSize, boxSize);

    const auto panel = findColour (panelColourId);
    const auto accent = findColour (accentColourId);
    const auto accentDim = findColour (accentDimColourId);
    const auto bg = findColour (bgColourId);
    const auto text = findColour (textColourId);
    const auto textDim = findColour (textDimColourId);

    g.setColour (button.getToggleState() ? accent : panel);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (accentDim);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);

    if (button.getToggleState())
    {
        g.setColour (dark ? bg : juce::Colours::white);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + boxSize * 0.25f, box.getCentreY());
        tick.lineTo (box.getCentreX() - 1.0f, box.getBottom() - boxSize * 0.30f);
        tick.lineTo (box.getRight() - boxSize * 0.22f, box.getY() + boxSize * 0.28f);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }

    g.setColour (highlighted ? text : (dark ? textDim : text));
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText (button.getButtonText(),
                r.withTrimmedLeft (boxSize + 8.0f),
                juce::Justification::centredLeft, false);
}

//==============================================================================
juce::Font VesperaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

juce::Font VesperaLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

//==============================================================================
IconButton::IconButton (Icon iconToDraw, const juce::String& name)
    : juce::Button (name), icon (iconToDraw)
{
    setButtonText (name);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto panel   = findColour (VesperaLookAndFeel::panelColourId);
    const auto accent  = findColour (VesperaLookAndFeel::accentColourId);
    const auto accentDim = findColour (VesperaLookAndFeel::accentDimColourId);

    auto fill = panel;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down)        fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (accentDim.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    auto c = bounds.getCentre();
    const float s = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.30f;
    g.setColour (accent);

    if (icon == Icon::Save)
    {
        auto body = juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c);
        g.drawRoundedRectangle (body, 1.5f, 1.4f);
        g.fillRect (juce::Rectangle<float> (c.x - s * 0.6f, body.getY() + s * 0.25f,
                                            s * 1.2f, s * 0.55f));
        g.drawRect (juce::Rectangle<float> (c.x - s * 0.7f, c.y + s * 0.15f,
                                            s * 1.4f, s * 0.7f), 1.2f);
    }
    else if (icon == Icon::Sun)
    {
        g.drawEllipse (juce::Rectangle<float> (s, s).withCentre (c), 1.4f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
            g.drawLine (c.x + std::cos (a) * s * 0.75f, c.y + std::sin (a) * s * 0.75f,
                        c.x + std::cos (a) * s * 1.35f, c.y + std::sin (a) * s * 1.35f, 1.3f);
        }
    }
    else if (icon == Icon::Log)
    {
        auto body = juce::Rectangle<float> (s * 1.9f, s * 2.1f).withCentre (c);
        g.drawRoundedRectangle (body, 1.5f, 1.3f);
        for (int i = 0; i < 3; ++i)
        {
            const float y = body.getY() + body.getHeight() * (0.28f + 0.22f * (float) i);
            g.drawLine (body.getX() + s * 0.35f, y, body.getRight() - s * 0.35f, y, 1.1f);
        }
    }
    else // Moon
    {
        auto area = bounds.reduced (2.5f);
        const float halfW = area.getWidth() * 0.5f;
        const float r = juce::jmin (s * 0.95f, halfW / 1.8f);

        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (bounds.toNearestInt());

        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

        g.setColour (fill);
        g.fillEllipse (juce::Rectangle<float> (r * 1.9f, r * 1.9f)
                           .withCentre ({ c.x + r * 0.85f, c.y - r * 0.25f }));
    }

    if (warning)
    {
        const float rr = juce::jmax (3.0f, s * 0.55f);
        g.setColour (juce::Colour (0xffe2402c));
        g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f)
                           .withCentre ({ bounds.getRight() - rr, bounds.getY() + rr }));
        g.setColour (juce::Colours::white);
        g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f)
                           .withCentre ({ bounds.getRight() - rr, bounds.getY() + rr }), 1.0f);
    }
}

} // namespace vespera
