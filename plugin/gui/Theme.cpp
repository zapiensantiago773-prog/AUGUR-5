#include "Theme.h"

#include "AugurBinary.h"

namespace augur5::ui
{

namespace
{
struct Typefaces
{
    juce::Typeface::Ptr jostRegular, jostMedium, jostLight, michroma, mono;

    Typefaces()
    {
        jostRegular = juce::Typeface::createSystemTypefaceFor (AugurBinary::JostRegular_ttf, AugurBinary::JostRegular_ttfSize);
        jostMedium = juce::Typeface::createSystemTypefaceFor (AugurBinary::JostMedium_ttf, AugurBinary::JostMedium_ttfSize);
        jostLight = juce::Typeface::createSystemTypefaceFor (AugurBinary::JostLight_ttf, AugurBinary::JostLight_ttfSize);
        michroma = juce::Typeface::createSystemTypefaceFor (AugurBinary::MichromaRegular_ttf, AugurBinary::MichromaRegular_ttfSize);
        mono = juce::Typeface::createSystemTypefaceFor (AugurBinary::JetBrainsMonoMedium_ttf, AugurBinary::JetBrainsMonoMedium_ttfSize);
    }

    static const Typefaces& get()
    {
        static const Typefaces t;
        return t;
    }
};

// Legibility: small captions are drawn larger (and their wide letter spacing tightened so they still fit),
// so they stay readable at the usual window sizes; large type grows a little.
float readableSize (float px) noexcept
{
    if (px <= 11.0f)
        return px * 1.3f;
    if (px <= 14.0f)
        return px * 1.16f;
    return px * 1.05f;
}

juce::Font make (const juce::Typeface::Ptr& tf, float px, float trackingEm)
{
    const float size = readableSize (px);
    const float tracking = px <= 11.0f ? trackingEm * 0.55f : trackingEm * 0.8f;
    return juce::Font (juce::FontOptions (tf).withPointHeight (size).withKerningFactor (tracking));
}
} // namespace

juce::Font Fonts::jost (float px, bool medium, float trackingEm)
{
    const auto& t = Typefaces::get();
    return make (medium ? t.jostMedium : t.jostRegular, px, trackingEm);
}

juce::Font Fonts::light (float px, float trackingEm) { return make (Typefaces::get().jostLight, px, trackingEm); }

juce::Font Fonts::michroma (float px, float trackingEm) { return make (Typefaces::get().michroma, px, trackingEm); }

juce::Font Fonts::mono (float px) { return make (Typefaces::get().mono, px, 0.0f); }

void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                  juce::Colour colour, juce::Justification just)
{
    auto f = font;
    const float width = juce::GlyphArrangement::getStringWidth (f, text);
    if (width > area.getWidth() && width > 0.0f)
        f = f.withHeight (f.getHeight() * juce::jmax (0.6f, area.getWidth() / width));
    g.setFont (f);
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

juce::Path svgPath (const char* data)
{
    return juce::Drawable::parseSVGPath (data);
}

void drawSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    // Paper card: a soft two-step shadow, white face, hairline border.
    g.setColour (juce::Colours::black.withAlpha (0.035f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.5f), 11.0f);
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (colours::panelTop, 0.0f, r.getY(), colours::panelBottom, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    if (title.isEmpty())
        return;
    drawTracked (g, title, { r.getX() + 18.0f, r.getY() + 14.0f, r.getWidth() - 36.0f, 16.0f }, Fonts::jost (12.0f, true, 0.3f),
                 colours::title, juce::Justification::centredLeft);
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 18.0f, r.getY() + 35.0f, 18.0f, 2.0f, 1.0f);
}

void drawSubSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    g.setColour (colours::subPanel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (colours::subPanelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    if (title.isEmpty())
        return;
    g.setColour (accent);
    g.fillEllipse (r.getX() + 12.0f, r.getY() + 16.0f, 5.0f, 5.0f);
    drawTracked (g, title, { r.getX() + 23.0f, r.getY() + 11.0f, r.getWidth() - 34.0f, 15.0f }, Fonts::jost (10.5f, true, 0.24f),
                 colours::subTitle, juce::Justification::centredLeft);
}

void drawCaption (juce::Graphics& g, const juce::String& text, float x, float y, float w, juce::Colour colour)
{
    drawTracked (g, text, { x, y, w, 12.0f }, Fonts::jost (8.5f, true, 0.24f), colour, juce::Justification::centredLeft);
}

void drawNote (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r)
{
    g.setFont (Fonts::jost (10.0f, false, 0.02f));
    g.setColour (colours::captionLight);
    g.drawFittedText (text, r.toNearestInt(), juce::Justification::topLeft, 8, 1.0f);
}

void drawLedDot (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float intensity)
{
    if (intensity > 0.02f)
    {
        g.setColour (colour.withAlpha (0.22f * intensity));
        g.fillEllipse (r.expanded (2.5f));
    }
    g.setColour (colours::ledOff.interpolatedWith (colour, intensity));
    g.fillEllipse (r);
    if (intensity <= 0.02f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.drawEllipse (r.reduced (0.4f), 0.8f);
    }
}

void drawTonalLabLogo (juce::Graphics& g, juce::Rectangle<float> area, TonalLabLogo which, juce::RectanglePlacement placement, float opacity)
{
    // Parsed once (message thread), on first use.
    static const std::unique_ptr<juce::Drawable> horizontal =
        juce::Drawable::createFromImageData (AugurBinary::tonallablogohorizontallightbg_svg, AugurBinary::tonallablogohorizontallightbg_svgSize);
    static const std::unique_ptr<juce::Drawable> mark =
        juce::Drawable::createFromImageData (AugurBinary::tonallabmarkbrandlightbg_svg, AugurBinary::tonallabmarkbrandlightbg_svgSize);
    if (const auto& d = which == TonalLabLogo::horizontal ? horizontal : mark)
        d->drawWithin (g, area, placement, opacity);
}

void drawAugurMark (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour ink, juce::Colour accent)
{
    const float stroke = juce::jmax (1.0f, r * 0.06f);

    // The circle of sky the augur read (the templum).
    g.setColour (ink);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, stroke);

    // The horizon, a chord across the lower half.
    const float hy = c.y + r * 0.2f;
    const float half = std::sqrt (r * r - (hy - c.y) * (hy - c.y)) * 0.84f;
    g.fillRoundedRectangle (c.x - half, hy - stroke * 0.5f, half * 2.0f, stroke, stroke * 0.5f);

    // The sun setting on it, in the accent, cut by thin bands like the sunset of the horizon panel.
    const float sr = r * 0.3f;
    juce::Path sun;
    sun.addPieSegment (c.x - sr, hy - sr, sr * 2.0f, sr * 2.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, 0.0f);
    {
        // Three bands of the half disc; the two cuts between them stay transparent (any background shows through).
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (sun);
        g.setColour (accent);
        const float cut1 = hy - sr * 0.48f, cut2 = hy - sr * 0.22f;
        const float w1 = stroke * 0.5f, w2 = stroke * 0.7f;
        g.fillRect (c.x - sr, hy - sr, sr * 2.0f, (cut1 - w1 * 0.5f) - (hy - sr));
        g.fillRect (c.x - sr, cut1 + w1 * 0.5f, sr * 2.0f, (cut2 - w2 * 0.5f) - (cut1 + w1 * 0.5f));
        g.fillRect (c.x - sr, cut2 + w2 * 0.5f, sr * 2.0f, hy - (cut2 + w2 * 0.5f));
    }

    // Its reflection under the horizon: two short strokes.
    g.setColour (accent.withAlpha (0.55f));
    g.fillRoundedRectangle (c.x - r * 0.2f, hy + r * 0.16f, r * 0.4f, stroke * 0.8f, stroke * 0.4f);
    g.setColour (accent.withAlpha (0.35f));
    g.fillRoundedRectangle (c.x - r * 0.11f, hy + r * 0.3f, r * 0.22f, stroke * 0.8f, stroke * 0.4f);

    // Two birds crossing the sky: the sign the augur waited for.
    const auto bird = [&] (juce::Point<float> b, float s) {
        juce::Path p;
        p.startNewSubPath (b.x - s, b.y + s * 0.2f);
        p.quadraticTo (b.x - s * 0.45f, b.y - s * 0.5f, b.x, b.y + s * 0.18f);
        p.quadraticTo (b.x + s * 0.45f, b.y - s * 0.5f, b.x + s, b.y + s * 0.2f);
        g.strokePath (p, juce::PathStrokeType (stroke * 1.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    g.setColour (ink);
    bird ({ c.x + r * 0.3f, c.y - r * 0.42f }, r * 0.22f);
    bird ({ c.x - r * 0.12f, c.y - r * 0.58f }, r * 0.14f);
}

float drawAugurLogo (juce::Graphics& g, float x, float y, float scale, bool withSubtitle)
{
    const float r = 24.0f * scale;
    drawAugurMark (g, { x + r, y + 28.0f * scale }, r);

    // AUGUR in light type, the hyphen as a short accent bar, 5 in the accent; the model number in slate (as MANTIS'
    // "904" sits after its name).
    const float tx = x + 2.0f * r + 18.0f * scale;
    const float baseline = y + 42.0f * scale;
    const auto word = Fonts::light (38.0f * scale, 0.34f);
    g.setFont (word);
    g.setColour (colours::title);
    g.drawSingleLineText ("AUGUR", juce::roundToInt (tx), juce::roundToInt (baseline));
    // Measured widths include the tracking after the last letter: take it off so the gaps are optical.
    const float trailing = word.getExtraKerningFactor() * word.getHeight();
    const float wWord = juce::GlyphArrangement::getStringWidth (word, "AUGUR") - trailing;
    const float dashX = tx + wWord + 9.0f * scale;
    g.setColour (colours::accent);
    g.fillRoundedRectangle (dashX, baseline - 13.0f * scale, 13.0f * scale, 2.0f * scale, 1.0f * scale);
    const float fiveX = dashX + 13.0f * scale + 8.0f * scale;
    g.setFont (word);
    g.drawSingleLineText ("5", juce::roundToInt (fiveX), juce::roundToInt (baseline));
    const float wFive = juce::GlyphArrangement::getStringWidth (word, "5") - trailing;
    const auto model = Fonts::michroma (10.0f * scale, 0.3f);
    g.setColour (colours::slate);
    g.setFont (model);
    const float modelX = fiveX + wFive + 14.0f * scale;
    g.drawSingleLineText ("3340", juce::roundToInt (modelX), juce::roundToInt (baseline));
    const float right = modelX + juce::GlyphArrangement::getStringWidth (model, "3340");

    if (withSubtitle)
        drawTracked (g, juce::String::fromUTF8 ("ANALOG MODELING  \xc2\xb7  5-VOICE POLYSYNTH"),
                     { tx + 2.0f * scale, baseline + 10.0f * scale, right - tx, 12.0f * scale }, Fonts::jost (8.5f * scale, false, 0.32f),
                     colours::captionLight, juce::Justification::centredLeft);
    return right - x;
}

} // namespace augur5::ui
