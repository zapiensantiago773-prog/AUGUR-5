#include "Widgets.h"

#include "../ParameterLayout.h"
#include "../Parameters.h"

namespace augur5::ui
{

namespace
{
constexpr float stroke = 1.4f;

void setupDefaultOnDoubleClick (juce::Slider& s, APVTS& state, const juce::String& id)
{
    if (auto* p = state.getParameter (id))
    {
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s.setTooltip (p->getName (64));
    }
}

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, float intensity, bool square = false)
{
    const float corner = square ? 2.0f : r.getWidth() * 0.5f;
    if (intensity > 0.02f)
    {
        g.setColour (colours::led.withAlpha (0.35f * intensity));
        g.fillRoundedRectangle (r.expanded (3.0f), corner + 3.0f);
        g.setColour (colours::led.withAlpha (0.55f * intensity));
        g.fillRoundedRectangle (r.expanded (1.2f), corner + 1.2f);
    }
    g.setColour (colours::ledOff.interpolatedWith (colours::led, intensity));
    g.fillRoundedRectangle (r, corner);
    if (intensity <= 0.02f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
}
} // namespace

//==============================================================================
// Knob

void Knob::FineSlider::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1800 : 240);
    juce::Slider::mouseDown (e);
}

void Knob::FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    juce::Slider::mouseDrag (e);
}

Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& text, int knobSize)
    : label (text), size (knobSize)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (240);
    slider.setPopupDisplayEnabled (true, false, nullptr, 1200);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    setupDefaultOnDoubleClick (slider, state, paramId);
}

void Knob::resized()
{
    slider.setBounds (4 + labelMargin, 0, size + 16, size + 16);
}

void Knob::paint (juce::Graphics& g)
{
    const auto area = juce::Rectangle<float> (0.0f, static_cast<float> (size + 16 + 5), static_cast<float> (getWidth()), 13.0f);
    drawTracked (g, label, area, Fonts::jost (10.0f, true, 0.12f), colours::label, juce::Justification::centredTop);
}

//==============================================================================
// LED toggle

LedToggle::LedToggle (const juce::String& text) : juce::Button (text) {}

void LedToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const auto r = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 4.0f);
    g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xff2a221b), 0.0f, r.getY(), juce::Colour (0xff1d1814), 0.0f, r.getBottom(), false)
                          : juce::ColourGradient (juce::Colour (0xff202024), 0.0f, r.getY(), juce::Colour (0xff17171a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
    g.setColour (on ? colours::accent.withAlpha (over ? 0.75f : 0.55f) : juce::Colour (over ? 0xff3c3c43 : 0xff303036));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const auto font = Fonts::jost (10.0f, true, 0.12f);
    const float textW = juce::GlyphArrangement::getStringWidth (font, getButtonText());
    const float total = 6.0f + 7.0f + textW;
    const float x0 = r.getCentreX() - total * 0.5f;
    drawLed (g, { x0, r.getCentreY() - 3.0f, 6.0f, 6.0f }, on ? 1.0f : 0.0f);
    drawTracked (g, getButtonText(), { x0 + 13.0f, r.getY(), textW + 8.0f, r.getHeight() }, font,
                 on ? colours::ledText : juce::Colour (0xffa39d94), juce::Justification::centredLeft);
}

ParamToggle::ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& text) : LedToggle (text)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    if (auto* p = state.getParameter (paramId))
        setTooltip (p->getName (64));
}

//==============================================================================
// Wave buttons

WaveButton::WaveButton (WaveIcon icon) : juce::Button ("wave")
{
    switch (icon)
    {
        case WaveIcon::Saw:        path = svgPath ("M2 12 L12 2 L12 12 L22 2"); break;
        case WaveIcon::Pulse:      path = svgPath ("M2 12 L2 2 L10 2 L10 12 L16 12 L16 2 L22 2"); break;
        case WaveIcon::Triangle:   path = svgPath ("M2 12 L7 2 L12 12 L17 2 L22 12"); break;
        case WaveIcon::Square:     path = svgPath ("M2 12 L2 2 L12 2 L12 12 L22 12 L22 2"); break;
        case WaveIcon::SampleHold: path = svgPath ("M2 10 L6 10 L6 4 L11 4 L11 12 L16 12 L16 6 L22 6"); break;
    }
}

void WaveButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (on ? juce::Colour (0xff24201c) : juce::Colour (0xff141417));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::accent.withAlpha (0.55f) : juce::Colour (over ? 0xff3c3c43 : 0xff2e2e33));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? colours::accent : colours::icon);
    g.strokePath (path, juce::PathStrokeType (stroke, juce::PathStrokeType::mitered, juce::PathStrokeType::butt),
                  juce::AffineTransform::translation (r.getCentreX() - 12.0f, r.getCentreY() - 7.0f));
}

ParamWaveButton::ParamWaveButton (APVTS& state, const juce::String& paramId, WaveIcon icon) : button (icon)
{
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
    if (auto* p = state.getParameter (paramId))
        button.setTooltip (p->getName (64));
}

//==============================================================================
// Choice group

ChoiceGroup::ChoiceGroup (APVTS& state, const juce::String& paramId, std::vector<std::unique_ptr<juce::Button>> b)
    : buttons (std::move (b))
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        addAndMakeVisible (*buttons[i]);
        buttons[i]->onClick = [this, i] {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture (static_cast<float> (i));
        };
    }
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { update (v); }, state.undoManager);
        attachment->sendInitialUpdate();
        for (auto& btn : buttons)
            btn->setTooltip (p->getName (64));
    }
}

void ChoiceGroup::update (float value)
{
    const int index = juce::roundToInt (value);
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (static_cast<int> (i) == index, juce::dontSendNotification);
}

//==============================================================================
// Choice drop-down

ParamChoiceBox::ParamChoiceBox (APVTS& state, const juce::String& paramId)
{
    auto* p = state.getParameter (paramId);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
        box.addItemList (choice->choices, 1);
    else if (auto* integer = dynamic_cast<juce::AudioParameterInt*> (p))
        for (int v = integer->getRange().getStart(); v <= integer->getRange().getEnd(); ++v)
            box.addItem (juce::String (v), v - integer->getRange().getStart() + 1);
    addAndMakeVisible (box);
    if (p != nullptr)
    {
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
        box.setTooltip (p->getName (64));
    }
}

//==============================================================================
// FX LED

FxLed::FxLed (APVTS& state, const juce::String& paramId) : juce::Button ("fx")
{
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    if (auto* p = state.getParameter (paramId))
        setTooltip (p->getName (64));
}

void FxLed::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    drawLed (g, { c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f }, getToggleState() ? 1.0f : (over ? 0.15f : 0.0f), true);
}

//==============================================================================
// Envelope display

EnvelopeDisplay::EnvelopeDisplay (APVTS& state, const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r)
{
    params = { state.getParameter (a), state.getParameter (d), state.getParameter (s), state.getParameter (r) };
    setInterceptsMouseClicks (false, false);
}

void EnvelopeDisplay::poll()
{
    bool changed = false;
    for (size_t i = 0; i < 4; ++i)
    {
        const float v = params[i] != nullptr ? params[i]->getValue() : 0.0f;
        if (v != last[i])
        {
            last[i] = v;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    const float na = last[0], nd = last[1], ns = juce::jlimit (0.0f, 1.0f, last[2]), nr = last[3];
    const float left = 4.0f, right = 238.0f, top = 6.0f, bottom = 76.0f;
    float aw = 6.0f + 46.0f * na, dw = 8.0f + 56.0f * nd, rw = 8.0f + 60.0f * nr;
    const float sw = juce::jmax (16.0f, (right - left) - aw - dw - rw);
    const float scale = (right - left) / (aw + dw + sw + rw);
    aw *= scale;
    dw *= scale;
    rw *= scale;
    const float swS = sw * scale;
    const auto yOf = [&] (float level) { return bottom - (bottom - top) * level; };
    constexpr float k = 4.6f;
    const float ek = std::exp (-k);

    juce::Path curve;
    curve.startNewSubPath (left, bottom);
    constexpr int steps = 24;
    for (int i = 1; i <= steps; ++i) // attack: RC aimed at 1.5, cut at 1
    {
        const float t = static_cast<float> (i) / steps;
        curve.lineTo (left + aw * t, yOf (1.5f * (1.0f - std::pow (3.0f, -t))));
    }
    const float xA = left + aw;
    for (int i = 1; i <= steps; ++i) // decay towards sustain
    {
        const float t = static_cast<float> (i) / steps;
        curve.lineTo (xA + dw * t, yOf (ns + (1.0f - ns) * (std::exp (-k * t) - ek) / (1.0f - ek)));
    }
    const float xD = xA + dw;
    const float xS = xD + swS;
    curve.lineTo (xS, yOf (ns));
    for (int i = 1; i <= steps; ++i) // release
    {
        const float t = static_cast<float> (i) / steps;
        curve.lineTo (xS + rw * t, yOf (ns * (std::exp (-k * t) - ek) / (1.0f - ek)));
    }

    g.setColour (juce::Colour (0xff2a2a2f));
    g.drawLine (0.0f, 79.5f, 242.0f, 79.5f, 1.0f);

    juce::Path fill (curve);
    fill.lineTo (right, bottom);
    fill.closeSubPath();
    g.setColour (colours::accent.withAlpha (0.07f));
    g.fillPath (fill);
    g.setColour (colours::icon);
    g.strokePath (curve, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::accent);
    for (const auto& p : { juce::Point<float> (xA, top), juce::Point<float> (xD, yOf (ns)), juce::Point<float> (xS, yOf (ns)) })
        g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
}

//==============================================================================
// Voice activity

VoiceActivity::VoiceActivity (APVTS& state, std::function<float (int)> fn)
    : voiceCount (state.getRawParameterValue (params::voice_count)), levelOf (std::move (fn))
{
    setInterceptsMouseClicks (false, false);
}

void VoiceActivity::poll()
{
    const int count = juce::jlimit (1, 16, juce::roundToInt (voiceCount->load()));
    bool changed = count != shownCount;
    for (int i = 0; i < 16; ++i)
    {
        const float target = i < count ? juce::jlimit (0.0f, 1.0f, std::sqrt (levelOf (i))) : 0.0f;
        const float v = shown[static_cast<size_t> (i)] + (target - shown[static_cast<size_t> (i)]) * 0.5f;
        if (std::abs (v - shown[static_cast<size_t> (i)]) > 0.01f)
            changed = true;
        shown[static_cast<size_t> (i)] = v;
    }
    shownCount = count;
    if (changed)
        repaint();
}

void VoiceActivity::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::subPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::subPanelBorder);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    drawTracked (g, "VOICE ACTIVITY", { 12.0f, 12.0f, 200.0f, 11.0f }, Fonts::jost (9.0f, false, 0.2f), colours::caption,
                 juce::Justification::centredLeft);

    const float pitch = juce::jmin (18.0f, (r.getWidth() - 24.0f) / static_cast<float> (juce::jmax (1, shownCount)));
    for (int i = 0; i < shownCount; ++i)
    {
        const float cx = 12.0f + 4.0f + pitch * static_cast<float> (i);
        drawLed (g, { cx - 4.0f, 31.0f, 8.0f, 8.0f }, shown[static_cast<size_t> (i)]);
        drawTracked (g, juce::String (i + 1), { cx - 8.0f, 43.0f, 16.0f, 11.0f }, Fonts::jost (9.0f), colours::caption,
                     juce::Justification::centredTop);
    }
}

//==============================================================================
// Level fader

LevelFader::LevelFader (APVTS& state)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, false, nullptr, 1200);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, params::amp_level, slider);
    setupDefaultOnDoubleClick (slider, state, params::amp_level);
}

void LevelFader::resized()
{
    slider.setBounds (0, 0, getWidth(), getHeight() - 21);
}

void LevelFader::paint (juce::Graphics& g)
{
    drawTracked (g, "LEVEL", { 0.0f, static_cast<float> (getHeight() - 13), static_cast<float> (getWidth()), 13.0f },
                 Fonts::jost (10.0f, true, 0.12f), colours::label, juce::Justification::centred);
}

//==============================================================================
// Voice counter

void VoiceCounter::StepButton::paintButton (juce::Graphics& g, bool over, bool)
{
    g.setColour (over ? colours::subTitle : colours::captionLight);
    g.setFont (Fonts::jost (14.0f));
    g.drawText (getButtonText() == "-" ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : getButtonText(),
                getLocalBounds(), juce::Justification::centred);
}

VoiceCounter::VoiceCounter (APVTS& state)
{
    addAndMakeVisible (minus);
    addAndMakeVisible (plus);
    if (auto* p = state.getParameter (params::voice_count))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) {
            value = juce::roundToInt (v);
            repaint();
        }, state.undoManager);
        attachment->sendInitialUpdate();
        setTooltip (p->getName (64));
    }
    minus.onClick = [this] { if (attachment) attachment->setValueAsCompleteGesture (static_cast<float> (juce::jmax (1, value - 1))); };
    plus.onClick = [this] { if (attachment) attachment->setValueAsCompleteGesture (static_cast<float> (juce::jmin (16, value + 1))); };
}

void VoiceCounter::resized()
{
    minus.setBounds (0, 21, 22, 34);
    plus.setBounds (getWidth() - 22, 21, 22, 34);
}

void VoiceCounter::paint (juce::Graphics& g)
{
    drawTracked (g, "VOICES", { 0.0f, 0.0f, static_cast<float> (getWidth()), 13.0f }, Fonts::jost (10.0f, true, 0.12f),
                 colours::label, juce::Justification::centred);
    const auto box = juce::Rectangle<float> (0.0f, 21.0f, static_cast<float> (getWidth()), 34.0f).reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (colours::fieldBorder);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (Fonts::jost (17.0f));
    g.drawText (juce::String (value), box, juce::Justification::centred);
}

//==============================================================================
// Matrix row

MatrixRow::MatrixRow (APVTS& state, int s) : slot (s)
{
    source.addItemList (matrixSourceNames(), 1);
    dest.addItemList (matrixDestNames(), 1);
    for (auto* c : { &source, &dest })
        addAndMakeVisible (*c);

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setPopupDisplayEnabled (true, false, nullptr, 1200);
    addAndMakeVisible (amount);

    srcAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, params::mmSrc (slot), source);
    dstAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, params::mmDst (slot), dest);
    amtAttachment = std::make_unique<APVTS::SliderAttachment> (state, params::mmAmt (slot), amount);
    setupDefaultOnDoubleClick (amount, state, params::mmAmt (slot));
}

void MatrixRow::resized()
{
    source.setBounds (24, 0, 130, 30);
    dest.setBounds (190, 0, 150, 30);
    amount.setBounds (352, 5, getWidth() - 352, 20);
}

void MatrixRow::paint (juce::Graphics& g)
{
    drawTracked (g, juce::String (slot), { 0.0f, 0.0f, 12.0f, 30.0f }, Fonts::jost (11.0f), colours::captionLight,
                 juce::Justification::centredLeft);
    static const juce::Path arrow = svgPath ("M1 4 H10 M7 1 L10 4 L7 7");
    g.setColour (juce::Colour (0xff5a554f));
    g.strokePath (arrow, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (166.0f, 11.0f));
}

//==============================================================================
// Header buttons

HeaderButton::HeaderButton (const juce::String& label, std::vector<juce::Path> paths, bool)
    : juce::Button (label), icon (std::move (paths))
{
}

void HeaderButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto col = down ? colours::accent : (over ? colours::subTitle : colours::label);
    const float x = static_cast<float> (getWidth()) * 0.5f - 9.0f;
    g.setColour (col);
    for (const auto& p : icon)
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (x, 4.0f));
    drawTracked (g, getButtonText(), { 0.0f, 28.0f, static_cast<float> (getWidth()), 12.0f }, Fonts::jost (9.0f, false, 0.16f),
                 over ? colours::subTitle : colours::headerButton, juce::Justification::centredTop);
}

ArrowButton::ArrowButton (bool right) : juce::Button (right ? "next" : "previous")
{
    path = svgPath (right ? "M3 2 L8 7 L3 12" : "M7 2 L2 7 L7 12");
}

void ArrowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    g.setColour (down ? colours::accent : (over ? colours::subTitle : colours::label));
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (static_cast<float> (getWidth()) * 0.5f - 5.0f, static_cast<float> (getHeight()) * 0.5f - 7.0f));
}

} // namespace augur5::ui
