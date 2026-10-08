#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// AUGUR-5 "Light": a white, minimal instrument panel. Paper-white sections on a warm grey desk, ink type,
// one warm accent (terracotta) and one cool accent (slate blue). The TONAL LAB series layout (MANTIS-37,
// PYTHIA 32): header with the preset bar and tabs, the tab pages, and the performance strip with the
// horizon at the bottom.
namespace augur5::ui
{

namespace colours
{
    // Surfaces
    inline const juce::Colour background { 0xffe9e6e0 };     // the desk around the sections
    inline const juce::Colour panelTop { 0xffffffff };
    inline const juce::Colour panelBottom { 0xfffaf8f4 };
    inline const juce::Colour panelBorder { 0xffd9d4cc };
    inline const juce::Colour subPanel { 0xfff7f5f0 };
    inline const juce::Colour subPanelBorder { 0xffe4dfd6 };
    inline const juce::Colour field { 0xffffffff };
    inline const juce::Colour fieldBorder { 0xffd3cec5 };
    inline const juce::Colour hairline { 0xffe6e2da };

    // Type
    inline const juce::Colour title { 0xff1b1b1d };
    inline const juce::Colour subTitle { 0xff2a2a2d };
    inline const juce::Colour text { 0xff19191b };
    inline const juce::Colour label { 0xff3b3936 };
    inline const juce::Colour captionLight { 0xff6a665f };
    inline const juce::Colour caption { 0xff8b867d };
    inline const juce::Colour headerButton { 0xff55524c };
    inline const juce::Colour icon { 0xff46443f };

    // Accents
    inline const juce::Colour accent { 0xffc8552f };   // terracotta
    inline const juce::Colour slate { 0xff3c6684 };    // slate blue
    inline const juce::Colour sage { 0xff6b8661 };     // muted green
    inline const juce::Colour plum { 0xff7d6596 };     // muted violet
    inline const juce::Colour amber { 0xffb7822b };    // tape / amber
    inline const juce::Colour ink { 0xff26272b };

    // Controls
    inline const juce::Colour led { 0xffd2582f };
    inline const juce::Colour ledOff { 0xffd6d1c8 };
    inline const juce::Colour ledText { 0xffb24a26 };
    inline const juce::Colour pointer { 0xff1d1d20 };
    inline const juce::Colour tickMajor { 0xffaaa49a };
    inline const juce::Colour tickMinor { 0xffd2ccc3 };
    inline const juce::Colour knobBody { 0xfffdfcfa };
    inline const juce::Colour knobEdge { 0xffcfc9bf };
    inline const juce::Colour idText { 0xff3c6684 };
} // namespace colours

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

// The AUGUR-5 logo, in the series' composition (PYTHIA: mark in a circle + light wordmark + number in colour;
// MANTIS: wordmark + number + model). The mark: the augur's lituus (the curled staff that marked out the sky)
// inside the templum (the quartered circle of sky that was read), with a bird crossing it.
void drawAugurMark (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour ink = colours::ink,
                    juce::Colour accent = colours::accent);
// Wordmark "AUGUR-5" + "3340" + subtitle, mark on the left; (x, y) = top left; height ~ 70 * scale. Returns its width.
float drawAugurLogo (juce::Graphics& g, float x, float y, float scale = 1.0f, bool withSubtitle = true);

// The maker's brand (plugin/resources/brand/tonal_lab, from the TONAL LAB logo kit): the light-background "brand"
// colouring, whose sun runs from dusk orange to night blue like AUGUR's two accents. Drawn from the SVG, so it stays sharp
// at any window size.
enum class TonalLabLogo
{
    horizontal, // sun + TONAL LAB / INSTRUMENTS
    mark        // the sun on the horizon alone
};
void drawTonalLabLogo (juce::Graphics& g, juce::Rectangle<float> area, TonalLabLogo which,
                       juce::RectanglePlacement placement = juce::RectanglePlacement::centred, float opacity = 1.0f);

// "IDs" mode: labels show parameter IDs instead of names (automation lookup).
inline bool showIds = false;

} // namespace augur5::ui
