#include "Theme.h"

#include "AugurBinary.h"

namespace augur5::ui
{

namespace
{
struct Typefaces
{
    juce::Typeface::Ptr jostRegular, jostMedium, michroma, mono;

    Typefaces()
    {
        jostRegular = juce::Typeface::createSystemTypefaceFor (AugurBinary::JostRegular_ttf, AugurBinary::JostRegular_ttfSize);
        jostMedium = juce::Typeface::createSystemTypefaceFor (AugurBinary::JostMedium_ttf, AugurBinary::JostMedium_ttfSize);
        michroma = juce::Typeface::createSystemTypefaceFor (AugurBinary::MichromaRegular_ttf, AugurBinary::MichromaRegular_ttfSize);
        mono = juce::Typeface::createSystemTypefaceFor (AugurBinary::JetBrainsMonoMedium_ttf, AugurBinary::JetBrainsMonoMedium_ttfSize);
    }

    static const Typefaces& get()
    {
        static const Typefaces t;
        return t;
    }
};

// Legibility: the mockup's 9-11 px captions become ~6 px on screen at the usual 65 % window size, so small
// text is enlarged (and its wide letter spacing tightened so it still fits); large text grows a little.
float readableSize (float px) noexcept
{
    if (px <= 11.0f)
        return px * 1.32f;
    if (px <= 14.0f)
        return px * 1.18f;
    return px * 1.06f;
}

juce::Font make (const juce::Typeface::Ptr& tf, float px, float trackingEm)
{
    // CSS font-size corresponds to the point height of the font.
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

juce::Font Fonts::michroma (float px, float trackingEm) { return make (Typefaces::get().michroma, px, trackingEm); }

juce::Font Fonts::mono (float px) { return make (Typefaces::get().mono, px, 0.0f); }

void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                  juce::Colour colour, juce::Justification just)
{
    // Text never gets cut: if it is wider than its area it is scaled down to fit (to 60 % at most).
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

} // namespace augur5::ui
