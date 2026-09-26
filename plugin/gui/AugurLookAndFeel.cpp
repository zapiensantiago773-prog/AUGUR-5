#include "AugurLookAndFeel.h"

#include "Theme.h"

namespace augur5::ui
{

AugurLookAndFeel::AugurLookAndFeel()
{
    using namespace colours;
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff151517));
    setColour (juce::PopupMenu::textColourId, subTitle);
    setColour (juce::PopupMenu::headerTextColourId, caption);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a221b));
    setColour (juce::PopupMenu::highlightedTextColourId, ledText);
    setColour (juce::ComboBox::textColourId, subTitle);
    setColour (juce::ComboBox::backgroundColourId, field);
    setColour (juce::ComboBox::outlineColourId, fieldBorder);
    setColour (juce::ComboBox::arrowColourId, captionLight);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff151517));
    setColour (juce::TooltipWindow::textColourId, subTitle);
    setColour (juce::TooltipWindow::outlineColourId, fieldBorder);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff151517));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0x8ce8833a));
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff18181b));
    setColour (juce::AlertWindow::textColourId, subTitle);
    setColour (juce::AlertWindow::outlineColourId, panelBorder);
    setColour (juce::TextEditor::backgroundColourId, field);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, fieldBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202024));
    setColour (juce::TextButton::textColourOffId, subTitle);
    setColour (juce::Slider::textBoxTextColourId, text);
}

//==============================================================================
// Knob: ticks, knurled body, domed cap, cream pointer with an orange glow.

void AugurLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                         float endAngle, juce::Slider&)
{
    const float ring = static_cast<float> (juce::jmin (w, h));
    const float half = ring * 0.5f;
    const juce::Point<float> c (static_cast<float> (x) + static_cast<float> (w) * 0.5f, static_cast<float> (y) + static_cast<float> (h) * 0.5f);
    const float size = ring - 16.0f;
    const float r = size * 0.5f;

    // 11 ticks, every 27 degrees; ends and centre brighter.
    for (int i = 0; i < 11; ++i)
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * static_cast<float> (i));
        juce::Path tick;
        tick.addRoundedRectangle (c.x - 0.75f, c.y - half, 1.5f, 4.0f, 1.0f);
        g.setColour (i == 0 || i == 5 || i == 10 ? colours::tickMajor : colours::tickMinor);
        g.fillPath (tick, juce::AffineTransform::rotation (a, c.x, c.y));
    }

    const juce::Rectangle<float> body (c.x - r, c.y - r, size, size);

    // Soft drop shadow (0 7px 12px rgba(0,0,0,.75) + 0 2px 3px rgba(0,0,0,.85)), approximated with layers.
    for (int i = 4; i >= 1; --i)
    {
        const float spread = static_cast<float> (i) * 2.2f;
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (body.expanded (spread * 0.6f).translated (0.0f, 3.0f + static_cast<float> (i)));
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (body.translated (0.0f, 2.0f));

    // Knurled skirt: repeating-conic-gradient(#161618 0 4deg, #2f2f33 4deg 8deg).
    g.setColour (juce::Colour (0xff161618));
    g.fillEllipse (body);
    juce::Path knurl;
    for (int i = 0; i < 45; ++i)
    {
        const float a0 = juce::degreesToRadians (4.0f + 8.0f * static_cast<float> (i));
        knurl.addPieSegment (body, a0, a0 + juce::degreesToRadians (4.0f), 0.0f);
    }
    g.setColour (juce::Colour (0xff2f2f33));
    g.fillPath (knurl);

    // linear-gradient(180deg, rgba(255,255,255,.10), rgba(0,0,0,.35)) over the skirt.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), c.x, body.getY(),
                                             juce::Colours::black.withAlpha (0.35f), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff070708));
    g.drawEllipse (body.expanded (0.5f), 1.0f);

    // Cap
    const float capOff = std::round (size * 0.1f);
    const float capSize = size - capOff * 2.0f;
    const juce::Rectangle<float> cap (body.getX() + capOff, body.getY() + capOff, capSize, capSize);
    juce::ColourGradient capGrad (juce::Colour (0xff414146), c.x, cap.getY() + capSize * 0.26f,
                                  juce::Colour (0xff0c0c0e), c.x, cap.getY() + capSize * 1.15f, true);
    capGrad.addColour (0.52, juce::Colour (0xff202023));
    g.setGradientFill (capGrad);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (cap.expanded (0.5f), 1.0f);

    // inset 0 1px 0 rgba(255,255,255,.16) and inset 0 -3px 5px rgba(0,0,0,.65)
    {
        juce::Path top;
        top.addCentredArc (c.x, c.y, capSize * 0.5f - 0.6f, capSize * 0.5f - 0.6f, 0.0f, -1.1f, 1.1f, true);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (top, juce::PathStrokeType (1.0f));
        juce::Path bottom;
        bottom.addCentredArc (c.x, c.y, capSize * 0.5f - 2.0f, capSize * 0.5f - 2.0f, 0.0f,
                              juce::MathConstants<float>::pi - 1.2f, juce::MathConstants<float>::pi + 1.2f, true);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.strokePath (bottom, juce::PathStrokeType (3.0f));
    }

    // Pointer: 2px wide, 32% of the cap, 3px from its edge, rotated with the value.
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float pointerH = std::round (capSize * 0.32f);
    const auto rot = juce::AffineTransform::rotation (angle, c.x, c.y);
    juce::Path glow, pointer;
    glow.addRoundedRectangle (c.x - 2.5f, cap.getY() + 1.5f, 5.0f, pointerH + 3.0f, 2.5f);
    pointer.addRoundedRectangle (c.x - 1.0f, cap.getY() + 3.0f, 2.0f, pointerH, 1.0f);
    g.setColour (colours::accent.withAlpha (0.28f));
    g.fillPath (glow, rot);
    g.setColour (colours::pointer);
    g.fillPath (pointer, rot);
}

//==============================================================================
// Linear sliders: vertical = LEVEL fader, horizontal = matrix amount (bipolar, filled from centre).

void AugurLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = bounds.getCentreX();
        const auto track = juce::Rectangle<float> (cx - 2.0f, bounds.getY(), 4.0f, bounds.getHeight());
        g.setColour (juce::Colour (0xff0a0a0b));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (track.getX(), track.getBottom() + 0.5f, track.getRight(), track.getBottom() + 0.5f, 1.0f);

        const auto thumb = juce::Rectangle<float> (cx - 12.0f, pos - 7.0f, 24.0f, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 3.0f).expanded (1.0f), 3.0f);
        juce::ColourGradient grad (juce::Colour (0xff5a5a60), 0.0f, thumb.getY(), juce::Colour (0xff3a3a3f), 0.0f, thumb.getBottom(), false);
        grad.addColour (0.48, juce::Colour (0xff2a2a2e));
        grad.addColour (0.52, juce::Colour (0xff1a1a1d));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (thumb, 2.0f);
        g.setColour (juce::Colour (0xff070708));
        g.drawRoundedRectangle (thumb, 2.0f, 1.0f);
        g.setColour (colours::accent);
        g.fillRect (thumb.getX() + 4.0f, thumb.getCentreY() - 0.75f, 16.0f, 1.5f);
        return;
    }

    // Horizontal (matrix amount): 3px track, orange from centre to the thumb.
    const float cy = bounds.getCentreY();
    const auto track = juce::Rectangle<float> (bounds.getX(), cy - 1.5f, bounds.getWidth(), 3.0f);
    g.setColour (juce::Colour (0xff34343a));
    g.fillRoundedRectangle (track, 1.5f);

    const double mid = (s.getMinimum() + s.getMaximum()) * 0.5;
    const float centreX = static_cast<float> (s.getPositionOfValue (mid));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centreX, pos), cy - 1.5f, juce::jmax (centreX, pos), cy + 1.5f));
    g.setColour (juce::Colour (0xff5a554f));
    g.fillRect (centreX - 0.5f, cy - 4.0f, 1.0f, 8.0f);

    const auto thumb = juce::Rectangle<float> (pos - 5.0f, cy - 9.0f, 10.0f, 18.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f), 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5a60), 0.0f, thumb.getY(), juce::Colour (0xff26262a), 0.0f, thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 2.0f);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawRoundedRectangle (thumb, 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawLine (thumb.getX() + 1.5f, thumb.getY() + 1.0f, thumb.getRight() - 1.5f, thumb.getY() + 1.0f, 1.0f);
}

//==============================================================================

void AugurLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h)).reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::fieldBorder);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    static const juce::Path chevron = svgPath ("M1 1 L4.5 5 L8 1");
    g.setColour (colours::captionLight);
    g.strokePath (chevron, juce::PathStrokeType (1.3f), juce::AffineTransform::translation (static_cast<float> (w) - 18.0f, static_cast<float> (h) * 0.5f - 3.0f));
}

juce::Font AugurLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::jost (10.0f, false, 0.04f); }

void AugurLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 0, box.getWidth() - 36, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize ({});
}

juce::Font AugurLookAndFeel::getPopupMenuFont() { return Fonts::jost (13.0f, false, 0.04f); }

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
    g.setColour (juce::Colour (0xff151517));
    g.fillRoundedRectangle (body, 3.0f);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.drawRoundedRectangle (body.reduced (0.5f), 3.0f, 1.0f);
}

juce::Rectangle<int> AugurLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto font = Fonts::jost (12.0f);
    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, tip)) + 18;
    const int h = 22;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

void AugurLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h));
    g.setColour (juce::Colour (0xff151517));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::fieldBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (colours::subTitle);
    g.setFont (Fonts::jost (12.0f));
    g.drawText (text, r, juce::Justification::centred);
}

juce::Font AugurLookAndFeel::getAlertWindowTitleFont() { return Fonts::jost (15.0f, true, 0.1f); }
juce::Font AugurLookAndFeel::getAlertWindowMessageFont() { return Fonts::jost (13.0f); }
juce::Font AugurLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::jost (12.0f, true, 0.1f); }

} // namespace augur5::ui
