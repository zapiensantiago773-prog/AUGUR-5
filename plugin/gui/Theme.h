#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Values taken 1:1 from the "AUGUR-5 — Dark (principal)" design mockup (1536 x 1024 artboard).
namespace augur5::ui
{

namespace colours
{
    inline const juce::Colour background { 0xff0b0b0c };
    inline const juce::Colour panelTop { 0xff1b1b1e };
    inline const juce::Colour panelBottom { 0xff161618 };
    inline const juce::Colour panelBorder { 0xff2b2b30 };
    inline const juce::Colour subPanel { 0xff1d1d21 };
    inline const juce::Colour subPanelBorder { 0xff2c2c31 };
    inline const juce::Colour field { 0xff111113 };
    inline const juce::Colour fieldBorder { 0xff2e2e33 };
    inline const juce::Colour title { 0xffdcd6cc };
    inline const juce::Colour subTitle { 0xffcfc8bd };
    inline const juce::Colour label { 0xff9a948b };
    inline const juce::Colour caption { 0xff6e6961 };
    inline const juce::Colour captionLight { 0xff7e786f };
    inline const juce::Colour headerButton { 0xff8c867d };
    inline const juce::Colour text { 0xffe4ddd2 };
    inline const juce::Colour icon { 0xffcfc6b8 };
    inline const juce::Colour accent { 0xffe8833a };
    inline const juce::Colour led { 0xfff08a3c };
    inline const juce::Colour ledText { 0xfff0b27a };
    inline const juce::Colour ledOff { 0xff3a3a3f };
    inline const juce::Colour pointer { 0xfff1ddbe };
    inline const juce::Colour tickMajor { 0xff8a837a };
    inline const juce::Colour tickMinor { 0xff4e4a45 };
} // namespace colours

// Embedded OFL fonts: Jost (UI), Michroma (logo), JetBrains Mono (ids / values).
struct Fonts
{
    static juce::Font jost (float px, bool medium = false, float trackingEm = 0.0f);
    static juce::Font michroma (float px, float trackingEm = 0.0f);
    static juce::Font mono (float px);
};

// Draws text with CSS-like letter spacing (JUCE kerning factor = tracking in em).
void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                  juce::Colour colour, juce::Justification just);

// SVG path data from the mockup, parsed once.
juce::Path svgPath (const char* data);

} // namespace augur5::ui
