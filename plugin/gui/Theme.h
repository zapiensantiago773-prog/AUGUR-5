#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// AUGUR-5's panel in three skins (the TONAL LAB series layout of MANTIS-37 and PYTHIA 32: header with the preset bar
// and tabs, the tab pages, and the performance strip with the horizon at the bottom):
//   LIGHT     - the instrument's own look: paper-white sections on a warm grey desk, ink type, terracotta and slate;
//   NEON      - the series' night skin: blue-black glass under sodium lamps; every section's edge is a lit tube in
//               its colour (sodium amber, turquoise, electric red), and arcs, LEDs and the horizon glow;
//   IMPERIAL  - the augur's purple: near-black wine lacquer with a gloss, double gold rules with Art Deco cut corners,
//               an engine-turned sunburst rising from the horizon's sun, gold-rimmed knobs, ivory and ebony keys.
// Every colour the GUI paints comes from here. A skin is a user preference shared by every AUGUR-5 window (kept in
// <app data>/TONAL LAB/AUGUR-5/settings.xml): applying one rewrites these values (message thread) and the editors
// rebuild themselves, so controls that keep a colour pick up the new one.
namespace augur5::ui
{

enum class Skin
{
    Light,
    Neon,
    Imperial
};
inline constexpr int numSkins = 3;

// The values below are LIGHT's; applySkin() rewrites them.
namespace colours
{
    // Surfaces
    inline juce::Colour background { 0xffe9e6e0 };     // the desk around the sections
    inline juce::Colour deskTop { 0xffefece7 };        // the desk's gradient, top
    inline juce::Colour panelTop { 0xffffffff };
    inline juce::Colour panelBottom { 0xfffaf8f4 };
    inline juce::Colour panelBorder { 0xffd9d4cc };
    inline juce::Colour subPanel { 0xfff7f5f0 };
    inline juce::Colour subPanelBorder { 0xffe4dfd6 };
    inline juce::Colour field { 0xffffffff };
    inline juce::Colour fieldBorder { 0xffd3cec5 };
    inline juce::Colour hairline { 0xffe6e2da };
    inline juce::Colour surface { 0xffffffff };        // button faces, badges, the preset box, overlay cards
    inline juce::Colour surfaceShade { 0xfff3f0ea };   // plain text buttons
    inline juce::Colour popup { 0xfffdfcfa };          // menus and alert windows
    inline juce::Colour tooltip { 0xff26272b };        // tooltips and value bubbles
    inline juce::Colour tooltipText { 0xfff4f1ec };
    inline juce::Colour shadow { 0xff000000 };         // drop shadows (their strength: style::shadow)

    // Type
    inline juce::Colour title { 0xff1b1b1d };
    inline juce::Colour subTitle { 0xff2a2a2d };
    inline juce::Colour text { 0xff19191b };
    inline juce::Colour label { 0xff3b3936 };
    inline juce::Colour captionLight { 0xff6a665f };
    inline juce::Colour caption { 0xff8b867d };
    inline juce::Colour headerButton { 0xff55524c };
    inline juce::Colour icon { 0xff46443f };

    // Accents
    inline juce::Colour accent { 0xffc8552f };   // terracotta
    inline juce::Colour slate { 0xff3c6684 };    // slate blue
    inline juce::Colour sage { 0xff6b8661 };     // muted green
    inline juce::Colour plum { 0xff7d6596 };     // muted violet
    inline juce::Colour amber { 0xffb7822b };    // tape / amber
    inline juce::Colour ink { 0xff26272b };

    // Controls
    inline juce::Colour led { 0xffd2582f };
    inline juce::Colour ledOff { 0xffd6d1c8 };
    inline juce::Colour ledText { 0xffb24a26 };
    inline juce::Colour pointer { 0xff1d1d20 };
    inline juce::Colour tickMajor { 0xffaaa49a };
    inline juce::Colour tickMinor { 0xffd2ccc3 };
    inline juce::Colour knobBody { 0xfffdfcfa };
    inline juce::Colour knobEdge { 0xffcfc9bf };
    inline juce::Colour knobFace { 0xffffffff };       // the turned face's gradient, lit side
    inline juce::Colour knobFaceShade { 0xffe9e5de };  // ... and far side
    inline juce::Colour knobRing { 0xffeeebe5 };       // the cap's inner ring
    inline juce::Colour thumb { 0xffffffff };          // fader and slider thumbs
    inline juce::Colour wheelEdge { 0xffe6e2db };      // the wheels' rolled edges (centre: surface)
    inline juce::Colour keyWhite { 0xffffffff };
    inline juce::Colour keyWhiteShade { 0xfff3f0ea };
    inline juce::Colour keyWhiteDown { 0xfff6e3d8 };
    inline juce::Colour keyWhiteDownShade { 0xffefcdbb };
    inline juce::Colour keyBlack { 0xff3a3a3e };
    inline juce::Colour keyBlackShade { 0xff1e1e21 };
    inline juce::Colour idText { 0xff3c6684 };

    // The horizon
    inline juce::Colour skyTop { 0xfffefdfb };
    inline juce::Colour skyLow { 0xfff3eee6 };
    inline juce::Colour groundTop { 0xfffbf9f5 };
    inline juce::Colour groundLow { 0xfff2eee7 };
    inline juce::Colour sunHigh { 0xfffbeee4 };
    inline juce::Colour sunLow { 0xfff0c4aa };
    inline juce::Colour ridge { 0xf7ffffff };          // the ridges' fill, top (towards groundLow)
} // namespace colours

// What a skin changes besides colours.
namespace style
{
    inline bool dark = false;       // a dark skin: the TONAL LAB logos for a dark background, light ribs on the wheels
    inline bool glow = false;       // NEON: section edges are lit tubes; value arcs, LEDs and marks give light
    inline bool gilded = false;     // IMPERIAL: lacquer gloss, double gold rules, the sunburst on the desk
    inline float shadow = 1.0f;     // drop-shadow strength (deeper on the dark skins)
} // namespace style

Skin currentSkin() noexcept;
const char* skinName (Skin) noexcept; // "LIGHT", "NEON", "IMPERIAL"
void applySkin (Skin);                // message thread; open editors must rebuild afterwards
Skin loadSavedSkin();                 // the user's choice (LIGHT when none was saved)
void saveSkin (Skin);

// A drop shadow of the given strength in the current skin.
inline juce::Colour shade (float alpha) { return colours::shadow.withAlpha (alpha * style::shadow); }
// Text in a section colour, set off from the surface it sits on: darker on LIGHT, brighter on the dark skins.
juce::Colour lit (juce::Colour c, float amount);

// Embedded OFL fonts: Jost (UI; Light for large titles), Michroma (logo), JetBrains Mono (values / ids).
struct Fonts
{
    static juce::Font jost (float px, bool medium = false, float trackingEm = 0.0f);
    static juce::Font light (float px, float trackingEm = 0.0f);
    static juce::Font michroma (float px, float trackingEm = 0.0f);
    static juce::Font mono (float px);
};

// Draws text with CSS-like letter spacing; text wider than its area is scaled down to fit (never cut).
void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                  juce::Colour colour, juce::Justification just);

// SVG path data, parsed once per call site.
juce::Path svgPath (const char* data);

// Shared panel painting.
void drawSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent);
void drawSubSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent);
void drawCaption (juce::Graphics& g, const juce::String& text, float x, float y, float w = 220.0f, juce::Colour colour = colours::caption);
void drawNote (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r);
void drawLedDot (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float intensity);
// IMPERIAL's sunburst on the desk, rising from the horizon's sun (nothing in the other skins).
void drawDeskPattern (juce::Graphics& g, juce::Rectangle<float> r, juce::Point<float> sun);
// The skin's frame over a card's edge: NEON's lit tube, IMPERIAL's inner gold rule (nothing on LIGHT).
void drawCardFrame (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour accent);

// The AUGUR-5 name, type only (as MANTIS-37's): "AUGUR-5" + "3340" + subtitle; (x, y) = top left; height ~ 70 * scale.
// Returns its width.
float drawAugurLogo (juce::Graphics& g, float x, float y, float scale = 1.0f, bool withSubtitle = true);

// The maker's brand (plugin/resources/brand/tonal_lab, the TONAL LAB logo kit), as in MANTIS-37 and PYTHIA 32: the
// horizontal lockup in the header's corner, the stacked one as the maker's plate of the performance strip. LIGHT uses the
// light-background "brand" colouring (its sun runs from dusk orange to night blue, like AUGUR's terracotta and slate);
// NEON and IMPERIAL the dark-background one.
enum class TonalLabLogo
{
    horizontal, // sun + TONAL LAB / INSTRUMENTS, side by side
    stacked,    // sun above TONAL LAB / INSTRUMENTS
    mark        // the sun on the horizon alone
};
void drawTonalLabLogo (juce::Graphics& g, juce::Rectangle<float> area, TonalLabLogo which,
                       juce::RectanglePlacement placement = juce::RectanglePlacement::centred, float opacity = 1.0f);

// "IDs" mode: labels show parameter IDs instead of names (automation lookup).
inline bool showIds = false;

} // namespace augur5::ui
