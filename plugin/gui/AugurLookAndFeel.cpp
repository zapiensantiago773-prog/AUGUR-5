#include "AugurLookAndFeel.h"

#include "Theme.h"

namespace augur5::ui
{

AugurLookAndFeel::AugurLookAndFeel()
{
    using namespace colours;
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfffdfcfa));
    setColour (juce::PopupMenu::textColourId, subTitle);
    setColour (juce::PopupMenu::headerTextColourId, caption);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.10f));
    setColour (juce::PopupMenu::highlightedTextColourId, ledText);
    setColour (juce::ComboBox::textColourId, subTitle);
    setColour (juce::ComboBox::backgroundColourId, field);
    setColour (juce::ComboBox::outlineColourId, fieldBorder);
    setColour (juce::ComboBox::arrowColourId, captionLight);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff26272b));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xfff4f1ec));
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff26272b));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff26272b));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xff26272b));
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xfffdfcfa));
    setColour (juce::AlertWindow::textColourId, subTitle);
    setColour (juce::AlertWindow::outlineColourId, panelBorder);
    setColour (juce::TextEditor::backgroundColourId, field);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, fieldBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.7f));
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.2f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff3f0ea));
    setColour (juce::TextButton::textColourOffId, subTitle);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::ListBox::backgroundColourId, field);
    setColour (juce::ScrollBar::thumbColourId, accent.withAlpha (0.5f));
}

//==============================================================================
// Knob: hairline ticks, a white turned body with a soft shadow, the value arc in the section colour (from the
// centre for bipolar controls) and a fine ink pointer. Colour / bipolar come from the slider's properties.

void AugurLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                         float endAngle, juce::Slider& slider)
{
    const float ring = static_cast<float> (juce::jmin (w, h));
    const float half = ring * 0.5f;
    const juce::Point<float> c (static_cast<float> (x) + static_cast<float> (w) * 0.5f, static_cast<float> (y) + static_cast<float> (h) * 0.5f);
    const float size = ring - 16.0f;
    const float r = size * 0.5f;
    const auto& props = slider.getProperties();
    const auto arcColour = props.contains ("arc") ? juce::Colour (static_cast<juce::uint32> (static_cast<juce::int64> (props["arc"])))
                                                  : colours::accent;
    const bool bipolar = static_cast<bool> (props.getWithDefault ("bipolar", false));
    const bool hot = slider.isMouseOverOrDragging();

    // 11 hairline ticks; ends and centre a little darker.
    for (int i = 0; i < 11; ++i)
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * static_cast<float> (i));
        juce::Path tick;
        tick.addRectangle (c.x - 0.5f, c.y - half + 0.5f, 1.0f, 3.5f);
        g.setColour (i == 0 || i == 5 || i == 10 ? colours::tickMajor : colours::tickMinor);
        g.fillPath (tick, juce::AffineTransform::rotation (a, c.x, c.y));
    }

    // Value arc on a faint track, just outside the body.
    const float arcR = r + 3.0f;
    {
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::hairline);
        g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (arcColour.withAlpha (hot ? 1.0f : 0.85f));
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Body: soft shadow, white turned face (light from the top left), hairline edge.
    const juce::Rectangle<float> body (c.x - r, c.y - r, size, size);
    g.setColour (juce::Colours::black.withAlpha (0.07f));
    g.fillEllipse (body.translated (0.0f, 2.5f).expanded (1.0f));
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillEllipse (body.translated (0.0f, 1.0f));
    juce::ColourGradient face (juce::Colours::white, c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xffe9e5de), c.x + r * 0.6f, c.y + r, true);
    g.setGradientFill (face);
    g.fillEllipse (body);
    g.setColour (colours::knobEdge);
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // A faint inner ring: the turned cap.
    g.setColour (juce::Colour (0xffeeebe5));
    g.drawEllipse (body.reduced (size * 0.16f), 1.0f);

    // Pointer: a fine ink line from near the centre to the edge.
    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addRoundedRectangle (c.x - 1.0f, body.getY() + size * 0.08f, 2.0f, size * 0.34f, 1.0f);
    g.setColour (colours::pointer);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle, c.x, c.y));
}

//==============================================================================
// Linear sliders: vertical = LEVEL fader, horizontal = matrix amount (bipolar, filled from the centre).

void AugurLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = bounds.getCentreX();
        const auto track = juce::Rectangle<float> (cx - 1.5f, bounds.getY(), 3.0f, bounds.getHeight());
        g.setColour (colours::hairline);
        g.fillRoundedRectangle (track, 1.5f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (track.getX(), pos, track.getRight(), track.getBottom()), 1.5f);

        const auto thumb = juce::Rectangle<float> (cx - 12.0f, pos - 6.0f, 24.0f, 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 1.5f), 3.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (colours::knobEdge);
        g.drawRoundedRectangle (thumb.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (colours::ink);
        g.fillRect (thumb.getX() + 6.0f, thumb.getCentreY() - 0.5f, 12.0f, 1.0f);
        return;
    }

    const float cy = bounds.getCentreY();
    const auto track = juce::Rectangle<float> (bounds.getX(), cy - 1.0f, bounds.getWidth(), 2.0f);
    g.setColour (colours::hairline);
    g.fillRoundedRectangle (track, 1.0f);
    const double mid = (s.getMinimum() + s.getMaximum()) * 0.5;
    const float centreX = static_cast<float> (s.getPositionOfValue (mid));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centreX, pos), cy - 1.0f, juce::jmax (centreX, pos), cy + 1.0f));
    g.setColour (colours::tickMajor);
    g.fillRect (centreX - 0.5f, cy - 4.0f, 1.0f, 8.0f);

    const auto thumb = juce::Rectangle<float> (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillEllipse (thumb.translated (0.0f, 1.0f));
    g.setColour (juce::Colours::white);
    g.fillEllipse (thumb);
    g.setColour (colours::accent);
    g.drawEllipse (thumb.reduced (0.75f), 1.5f);
}

//==============================================================================

void AugurLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h)).reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver (true) ? colours::captionLight.withAlpha (0.6f) : colours::fieldBorder);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    static const juce::Path chevron = svgPath ("M1 1 L4.5 5 L8 1");
    g.setColour (colours::captionLight);
    g.strokePath (chevron, juce::PathStrokeType (1.3f), juce::AffineTransform::translation (static_cast<float> (w) - 17.0f, static_cast<float> (h) * 0.5f - 3.0f));
}

juce::Font AugurLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::jost (10.0f, false, 0.04f); }

void AugurLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 0, box.getWidth() - 30, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize ({});
}

juce::Font AugurLookAndFeel::getPopupMenuFont() { return Fonts::jost (13.0f, false, 0.03f); }

void AugurLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::fieldBorder);
    g.drawRect (0, 0, w, h);
}

juce::Font AugurLookAndFeel::getSliderPopupFont (juce::Slider&) { return Fonts::mono (11.0f); }

int AugurLookAndFeel::getSliderPopupPlacement (juce::Slider&) { return juce::BubbleComponent::above; }

void AugurLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xff26272b));
    g.fillRoundedRectangle (body, 4.0f);
}

juce::Rectangle<int> AugurLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto font = Fonts::jost (12.0f);
    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, tip)) + 20;
    const int h = 24;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

void AugurLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h));
    g.setColour (juce::Colour (0xff26272b));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xfff4f1ec));
    g.setFont (Fonts::jost (12.0f));
    g.drawText (text, r, juce::Justification::centred);
}

juce::Font AugurLookAndFeel::getAlertWindowTitleFont() { return Fonts::jost (15.0f, true, 0.1f); }
juce::Font AugurLookAndFeel::getAlertWindowMessageFont() { return Fonts::jost (13.0f); }
juce::Font AugurLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::jost (12.0f, true, 0.1f); }

} // namespace augur5::ui
