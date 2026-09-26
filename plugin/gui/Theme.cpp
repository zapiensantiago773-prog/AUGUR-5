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

juce::Font make (const juce::Typeface::Ptr& tf, float px, float trackingEm)
{
    // CSS font-size corresponds to the point height of the font.
    return juce::Font (juce::FontOptions (tf).withPointHeight (px).withKerningFactor (trackingEm));
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
    g.setFont (font);
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

juce::Path svgPath (const char* data)
{
    return juce::Drawable::parseSVGPath (data);
}

} // namespace augur5::ui
