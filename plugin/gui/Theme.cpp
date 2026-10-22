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

//==============================================================================
// Skins

namespace
{
// Every colour of the GUI in the three skins, side by side: LIGHT (the values Theme.h starts with), NEON, IMPERIAL.
struct Token
{
    juce::Colour& colour;
    juce::uint32 light, neon, imperial;
};

const Token palette[] {
    // Surfaces
    { colours::background, 0xffe9e6e0, 0xff080b12, 0xff10060b },
    { colours::deskTop, 0xffefece7, 0xff0d121c, 0xff1b0a13 },
    { colours::panelTop, 0xffffffff, 0xff131a27, 0xff28111b },
    { colours::panelBottom, 0xfffaf8f4, 0xff0f141f, 0xff1c0b13 },
    { colours::panelBorder, 0xffd9d4cc, 0xff253049, 0xff8f7244 },
    { colours::subPanel, 0xfff7f5f0, 0xff0c111a, 0xff200d16 },
    { colours::subPanelBorder, 0xffe4dfd6, 0xff1d2639, 0xff4d3726 },
    { colours::field, 0xffffffff, 0xff090d15, 0xff15070e },
    { colours::fieldBorder, 0xffd3cec5, 0xff2b3652, 0xff624a31 },
    { colours::hairline, 0xffe6e2da, 0xff1c2434, 0xff3a2420 },
    { colours::surface, 0xffffffff, 0xff151c2a, 0xff23101a },
    { colours::surfaceShade, 0xfff3f0ea, 0xff111723, 0xff1d0d15 },
    { colours::popup, 0xfffdfcfa, 0xff111824, 0xff1f0c16 },
    { colours::tooltip, 0xff26272b, 0xff1d2638, 0xff2f1521 },
    { colours::tooltipText, 0xfff4f1ec, 0xffeef3ff, 0xfff4ead8 },
    { colours::shadow, 0xff000000, 0xff000000, 0xff000000 },
    // Type
    { colours::title, 0xff1b1b1d, 0xfff2f5ff, 0xfff6ecd9 },
    { colours::subTitle, 0xff2a2a2d, 0xffdde4f2, 0xffeadfc8 },
    { colours::text, 0xff19191b, 0xffeaf0fb, 0xfff4ead8 },
    { colours::label, 0xff3b3936, 0xffb8c2d8, 0xffcfc1a8 },
    { colours::captionLight, 0xff6a665f, 0xff8e99b3, 0xffa99883 },
    { colours::caption, 0xff8b867d, 0xff6c7791, 0xff88786b },
    { colours::headerButton, 0xff55524c, 0xffaeb8cf, 0xffcfc1a8 },
    { colours::icon, 0xff46443f, 0xffc4cde2, 0xffddd0b7 },
    // Accents: terracotta / slate / sage / plum / amber on LIGHT; sodium amber / turquoise / lime / electric red /
    // sodium orange on NEON; gold / jade / platinum / rose quartz / copper on IMPERIAL.
    { colours::accent, 0xffc8552f, 0xffffb000, 0xffdcb46a },
    { colours::slate, 0xff3c6684, 0xff00e5c7, 0xff86c5a8 },
    { colours::sage, 0xff6b8661, 0xffb6f23a, 0xffd9d4cc },
    { colours::plum, 0xff7d6596, 0xffff3b5c, 0xffe8a0b4 },
    { colours::amber, 0xffb7822b, 0xffff8a3d, 0xffde915e },
    { colours::ink, 0xff26272b, 0xffeef3ff, 0xfff4ead8 },
    // Controls
    { colours::led, 0xffd2582f, 0xffffb000, 0xffe9c477 },
    { colours::ledOff, 0xffd6d1c8, 0xff1b2333, 0xff3a2229 },
    { colours::ledText, 0xffb24a26, 0xffffc23d, 0xffe9c477 },
    { colours::pointer, 0xff1d1d20, 0xfff4f7ff, 0xfff6ecd9 },
    { colours::tickMajor, 0xffaaa49a, 0xff58647f, 0xffa88a52 },
    { colours::tickMinor, 0xffd2ccc3, 0xff2a3450, 0xff4c3428 },
    { colours::knobBody, 0xfffdfcfa, 0xff151c2a, 0xff231019 },
    { colours::knobEdge, 0xffcfc9bf, 0xff313c58, 0xffbb985c },
    { colours::knobFace, 0xffffffff, 0xff222b3d, 0xff3d1d2b },
    { colours::knobFaceShade, 0xffe9e5de, 0xff0c111a, 0xff12060c },
    { colours::knobRing, 0xffeeebe5, 0xff2a3448, 0xff5f4529 },
    { colours::thumb, 0xffffffff, 0xff222b3d, 0xff2b131e },
    { colours::wheelEdge, 0xffe6e2db, 0xff0b0f18, 0xff0e050a },
    { colours::keyWhite, 0xffffffff, 0xff2b3549, 0xfff3ead6 },
    { colours::keyWhiteShade, 0xfff3f0ea, 0xff1d2536, 0xffdccfb3 },
    { colours::keyWhiteDown, 0xfff6e3d8, 0xff3a2f12, 0xfff0d796 },
    { colours::keyWhiteDownShade, 0xffefcdbb, 0xff54400c, 0xffd3ad5d },
    { colours::keyBlack, 0xff3a3a3e, 0xff070a10, 0xff1c1316 },
    { colours::keyBlackShade, 0xff1e1e21, 0xff020306, 0xff050304 },
    { colours::idText, 0xff3c6684, 0xff00e5c7, 0xffdcb46a },
    // The horizon: LIGHT's paper landscape; NEON's night sky with a sodium-red sun; IMPERIAL's wine dusk and a gold sun.
    { colours::skyTop, 0xfffefdfb, 0xff04060b, 0xff0c0408 },
    { colours::skyLow, 0xfff3eee6, 0xff111a2c, 0xff2c1321 },
    { colours::groundTop, 0xfffbf9f5, 0xff0b101a, 0xff1c0b13 },
    { colours::groundLow, 0xfff2eee7, 0xff06090f, 0xff0b0408 },
    { colours::sunHigh, 0xfffbeee4, 0xffffd27a, 0xfffbe8b4 },
    { colours::sunLow, 0xfff0c4aa, 0xffff3b5c, 0xffc48a3a },
    { colours::ridge, 0xf7ffffff, 0xf7141b29, 0xf71e0c15 },
};

Skin current = Skin::Light;

juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("TONAL LAB")
        .getChildFile ("AUGUR-5")
        .getChildFile ("settings.xml");
}

// IMPERIAL's inner frame: a rectangle with cut corners, the Art Deco rule inside the gold edge.
juce::Path cutCornerFrame (juce::Rectangle<float> r, float cut)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + cut, r.getY());
    p.lineTo (r.getRight() - cut, r.getY());
    p.lineTo (r.getRight(), r.getY() + cut);
    p.lineTo (r.getRight(), r.getBottom() - cut);
    p.lineTo (r.getRight() - cut, r.getBottom());
    p.lineTo (r.getX() + cut, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - cut);
    p.lineTo (r.getX(), r.getY() + cut);
    p.closeSubPath();
    return p;
}

// IMPERIAL's title mark: a small gold lozenge.
void drawLozenge (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour)
{
    juce::Path p;
    p.addQuadrilateral (c.x, c.y - r, c.x + r, c.y, c.x, c.y + r, c.x - r, c.y);
    g.setColour (colour);
    g.fillPath (p);
}
} // namespace

Skin currentSkin() noexcept { return current; }

const char* skinName (Skin s) noexcept
{
    switch (s)
    {
        case Skin::Neon: return "NEON";
        case Skin::Imperial: return "IMPERIAL";
        default: return "LIGHT";
    }
}

void applySkin (Skin s)
{
    current = s;
    for (const auto& t : palette)
        t.colour = juce::Colour (s == Skin::Neon ? t.neon : (s == Skin::Imperial ? t.imperial : t.light));
    style::dark = s != Skin::Light;
    style::glow = s == Skin::Neon;
    style::gilded = s == Skin::Imperial;
    style::shadow = s == Skin::Light ? 1.0f : 2.2f;
}

Skin loadSavedSkin()
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
    {
        const auto name = xml->getStringAttribute ("skin");
        for (int i = 0; i < numSkins; ++i)
            if (name == skinName (static_cast<Skin> (i)))
                return static_cast<Skin> (i);
    }
    return Skin::Light;
}

void saveSkin (Skin s)
{
    const auto file = settingsFile();
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AUGUR5_SETTINGS"))
        xml = std::make_unique<juce::XmlElement> ("AUGUR5_SETTINGS");
    xml->setAttribute ("skin", skinName (s));
    file.getParentDirectory().createDirectory();
    xml->writeTo (file);
}

juce::Colour lit (juce::Colour c, float amount)
{
    return current == Skin::Light ? c.darker (amount) : c.brighter (amount * 0.6f);
}

void drawDeskPattern (juce::Graphics& g, juce::Rectangle<float> r, juce::Point<float> sun)
{
    if (! style::gilded)
        return;
    // IMPERIAL: an engine-turned sunburst, as on a fine watch dial, rising from the sun of the horizon; every third
    // ray a little brighter, and faint rings around the sun.
    const float reach = r.getWidth() + r.getHeight();
    for (int i = 0; i < 240; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 240.0f;
        g.setColour (colours::accent.withAlpha (i % 3 == 0 ? 0.075f : 0.035f));
        g.drawLine (sun.x, sun.y, sun.x + std::cos (a) * reach, sun.y + std::sin (a) * reach, 0.7f);
    }
    g.setColour (colours::accent.withAlpha (0.03f));
    for (float rr = 120.0f; rr < reach; rr += 36.0f)
        g.drawEllipse (sun.x - rr, sun.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
}

void drawCardFrame (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour accent)
{
    if (style::glow)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.drawRoundedRectangle (r.expanded (2.0f), corner + 2.0f, 4.0f);
        g.setColour (accent.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
    else if (style::gilded)
    {
        g.setColour (colours::panelBorder.withAlpha (0.5f));
        g.strokePath (cutCornerFrame (r.reduced (6.0f), corner - 3.0f), juce::PathStrokeType (0.8f));
    }
}

//==============================================================================

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
    // The card: a soft two-step shadow, the face (white paper on LIGHT), the edge.
    g.setColour (shade (0.035f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.5f), 11.0f);
    g.setColour (shade (0.05f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (colours::panelTop, 0.0f, r.getY(), colours::panelBottom, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    if (style::gilded)
    {
        // IMPERIAL: the lacquer's gloss along the top edge, and the inner rule of a double gold frame with cut corners.
        const float gloss = juce::jmin (64.0f, r.getHeight() * 0.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f,
                                                 r.getY() + gloss, false));
        g.fillRoundedRectangle (r.withHeight (gloss), 10.0f);
        g.setColour (colours::panelBorder.withAlpha (0.5f));
        g.strokePath (cutCornerFrame (r.reduced (5.0f), 7.0f), juce::PathStrokeType (0.8f));
    }
    if (style::glow)
    {
        // NEON, as on every TONAL LAB instrument: the edge is a lit tube in the section's colour.
        g.setColour (accent.withAlpha (0.12f));
        g.drawRoundedRectangle (r.expanded (2.0f), 12.0f, 4.0f);
        g.setColour (accent.withAlpha (0.16f));
        g.drawRoundedRectangle (r.reduced (1.5f), 8.5f, 2.0f);
        g.setColour (accent.withAlpha (0.8f));
    }
    else
        g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    if (title.isEmpty())
        return;
    drawTracked (g, title, { r.getX() + 18.0f, r.getY() + 14.0f, r.getWidth() - 36.0f, 16.0f }, Fonts::jost (12.0f, true, 0.3f),
                 style::glow ? accent : colours::title, juce::Justification::centredLeft);
    if (style::gilded)
    {
        // A small lozenge and a fine rule under the title.
        drawLozenge (g, { r.getX() + 21.0f, r.getY() + 36.0f }, 3.2f, accent);
        g.setColour (accent.withAlpha (0.55f));
        g.fillRect (r.getX() + 28.0f, r.getY() + 35.6f, 26.0f, 0.8f);
        return;
    }
    if (style::glow)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r.getX() + 15.0f, r.getY() + 32.5f, 24.0f, 7.0f, 3.5f);
    }
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 18.0f, r.getY() + 35.0f, 18.0f, 2.0f, 1.0f);
}

void drawSubSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    g.setColour (colours::subPanel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (style::glow ? accent.withAlpha (0.45f) : colours::subPanelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    if (title.isEmpty())
        return;
    if (style::gilded)
        drawLozenge (g, { r.getX() + 14.5f, r.getY() + 18.5f }, 3.0f, accent);
    else
    {
        g.setColour (accent);
        g.fillEllipse (r.getX() + 12.0f, r.getY() + 16.0f, 5.0f, 5.0f);
    }
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
        if (style::glow)
        {
            g.setColour (colour.withAlpha (0.16f * intensity));
            g.fillEllipse (r.expanded (5.5f));
        }
        g.setColour (colour.withAlpha (0.22f * intensity));
        g.fillEllipse (r.expanded (2.5f));
    }
    g.setColour (colours::ledOff.interpolatedWith (colour, intensity));
    g.fillEllipse (r);
    if (intensity <= 0.02f)
    {
        g.setColour (shade (0.10f));
        g.drawEllipse (r.reduced (0.4f), 0.8f);
    }
}

void drawTonalLabLogo (juce::Graphics& g, juce::Rectangle<float> area, TonalLabLogo which, juce::RectanglePlacement placement, float opacity)
{
    // Parsed once (message thread), on first use; vector, so sharp at every window size.
    static const auto load = [] (const char* data, int size) { return juce::Drawable::createFromImageData (data, static_cast<size_t> (size)); };
    static const std::unique_ptr<juce::Drawable> logos[] = {
        load (AugurBinary::tonallablogohorizontallightbg_svg, AugurBinary::tonallablogohorizontallightbg_svgSize),
        load (AugurBinary::tonallablogostackedlightbg_svg, AugurBinary::tonallablogostackedlightbg_svgSize),
        load (AugurBinary::tonallabmarkbrandlightbg_svg, AugurBinary::tonallabmarkbrandlightbg_svgSize),
        load (AugurBinary::tonallablogohorizontaldarkbg_svg, AugurBinary::tonallablogohorizontaldarkbg_svgSize),
        load (AugurBinary::tonallablogostackeddarkbg_svg, AugurBinary::tonallablogostackeddarkbg_svgSize),
        load (AugurBinary::tonallabmarkbrand_svg, AugurBinary::tonallabmarkbrand_svgSize),
    };
    if (const auto& d = logos[static_cast<int> (which) + (style::dark ? 3 : 0)])
        d->drawWithin (g, area, placement, opacity);
}

float drawAugurLogo (juce::Graphics& g, float x, float y, float scale, bool withSubtitle)
{
    // Type only (as MANTIS' name): AUGUR in light type, the hyphen as a short accent bar, 5 in the accent, the model
    // number in slate.
    const float tx = x;
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
