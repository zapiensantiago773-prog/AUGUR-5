#include "Widgets.h"

#include "../ParameterLayout.h"
#include "../Parameters.h"

namespace augur5::ui
{

namespace
{
void setupDefaultOnDoubleClick (juce::Slider& s, APVTS& state, const juce::String& id)
{
    if (auto* p = state.getParameter (id))
    {
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s.setTooltip (p->getName (64));
    }
}

juce::Font labelFont() { return Fonts::jost (9.5f, true, 0.12f); }
} // namespace

//==============================================================================
// Knob

void Knob::FineSlider::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1800 : 240);
    juce::Slider::mouseDown (e);
}

Knob::Knob (APVTS& state, const juce::String& id, const juce::String& text, int knobSize, juce::Colour c)
    : label (text), paramId (id), param (state.getParameter (id)), size (knobSize), colour (c)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (240);
    slider.getProperties().set ("arc", static_cast<juce::int64> (c.getARGB()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    setupDefaultOnDoubleClick (slider, state, id);
    if (param != nullptr)
    {
        const auto range = param->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
    }
    slider.addListener (this);
    slider.addMouseListener (this, false);
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::resized()
{
    slider.setBounds (4 + labelMargin, 0, size + 16, size + 16);
}

void Knob::paint (juce::Graphics& g)
{
    // The label may use a little more than the knob's own width, never its neighbour's (it shrinks to fit).
    const float labelWidth = static_cast<float> (size + 40);
    const auto area = juce::Rectangle<float> ((static_cast<float> (getWidth()) - labelWidth) * 0.5f, static_cast<float> (size + 16 + 5),
                                              labelWidth + 0.0f, 18.0f);
    if (showIds)
    {
        drawTracked (g, paramId, area.expanded (14.0f, 0.0f), Fonts::mono (8.0f), colours::idText, juce::Justification::centredTop);
        return;
    }
    if ((slider.isMouseOverOrDragging() || slider.isMouseButtonDown()) && param != nullptr)
    {
        // The exact value, in the parameter's own units, while the knob is under the hand.
        drawTracked (g, param->getCurrentValueAsText(), area.expanded (14.0f, 0.0f), Fonts::mono (9.5f), colour.darker (0.25f),
                     juce::Justification::centredTop);
        return;
    }
    // Small knobs sit closer together: their names are set a size smaller.
    drawTracked (g, label, area, size <= 34 ? Fonts::jost (8.5f, true, 0.1f) : labelFont(), colours::label, juce::Justification::centredTop);
}

//==============================================================================
// LED toggle

LedToggle::LedToggle (const juce::String& text, juce::Colour c) : juce::Button (text), colour (c) {}

void LedToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState() || down;
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (! isEnabled())
        g.beginTransparencyLayer (0.4f); // nothing to act on (an empty AUGURY corner)
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (r, 5.0f);
    if (on)
    {
        g.setColour (colour.withAlpha (0.09f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setColour (on ? colour.withAlpha (over ? 0.85f : 0.65f) : (over ? colours::captionLight.withAlpha (0.55f) : colours::fieldBorder));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const bool ids = showIds && idText.isNotEmpty();
    const auto font = ids ? Fonts::mono (8.0f) : labelFont();
    const auto shownText = ids ? idText : getButtonText();
    const float textW = juce::jmin (juce::GlyphArrangement::getStringWidth (font, shownText), r.getWidth() - 22.0f);
    const float total = 6.0f + 7.0f + textW;
    const float x0 = r.getCentreX() - total * 0.5f;
    drawLedDot (g, { x0, r.getCentreY() - 3.0f, 6.0f, 6.0f }, colour, on ? 1.0f : 0.0f);
    drawTracked (g, shownText, { x0 + 13.0f, r.getY(), textW + 4.0f, r.getHeight() }, font,
                 on ? colour.darker (0.2f) : colours::label, juce::Justification::centredLeft);
    if (! isEnabled())
        g.endTransparencyLayer();
}

ParamToggle::ParamToggle (APVTS& state, const juce::String& id, const juce::String& text, juce::Colour c) : LedToggle (text, c)
{
    idText = id;
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, *this);
    if (auto* p = state.getParameter (id))
        setTooltip (p->getName (64));
}

ActionButton::ActionButton (const juce::String& text, juce::Colour c, std::function<void()> action) : LedToggle (text, c)
{
    onClick = std::move (action);
}

void TabButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (on)
    {
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r.reduced (0.5f), 5.0f);
        g.setColour (colours::panelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (r.getX() + 12.0f, r.getBottom() - 4.0f, r.getWidth() - 24.0f, 2.0f, 1.0f);
    }
    drawTracked (g, getButtonText(), r.withTrimmedBottom (2.0f), Fonts::jost (9.5f, true, 0.24f),
                 on ? colours::title : (over ? colours::subTitle : colours::captionLight), juce::Justification::centred);
}

void SegmentButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState() || down;
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (r, 4.0f);
    if (on)
    {
        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    g.setColour (on ? colour.withAlpha (0.75f) : (over ? colours::captionLight.withAlpha (0.55f) : colours::fieldBorder));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const bool ids = showIds && idText.isNotEmpty();
    drawTracked (g, ids ? idText : getButtonText(), r.reduced (3.0f, 0.0f), ids ? Fonts::mono (7.0f) : labelFont(),
                 on ? colour.darker (0.25f) : colours::label, juce::Justification::centred);
}

//==============================================================================
// Wave buttons

WaveButton::WaveButton (WaveIcon icon, juce::Colour c) : juce::Button ("wave"), colour (c)
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
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (r, 5.0f);
    if (on)
    {
        g.setColour (colour.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setColour (on ? colour.withAlpha (0.7f) : (over ? colours::captionLight.withAlpha (0.55f) : colours::fieldBorder));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (on ? colour.darker (0.1f) : colours::icon);
    g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt),
                  juce::AffineTransform::translation (r.getCentreX() - 12.0f, r.getCentreY() - 7.0f));
}

ParamWaveButton::ParamWaveButton (APVTS& state, const juce::String& paramId, WaveIcon icon, juce::Colour c) : button (icon, c)
{
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
    if (auto* p = state.getParameter (paramId))
        button.setTooltip (p->getName (64));
}

//==============================================================================
// Choice group / drop-down

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
// Envelope display

EnvelopeDisplay::EnvelopeDisplay (APVTS& state, const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r,
                                  juce::Colour c)
    : colour (c)
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
    const float W = static_cast<float> (getWidth()), H = static_cast<float> (getHeight());
    const auto norm = [this] (size_t i) { return params[i] != nullptr ? params[i]->getValue() : 0.0f; };
    const float na = norm (0), nd = norm (1), ns = juce::jlimit (0.0f, 1.0f, norm (2)), nr = norm (3);
    const float left = 4.0f, right = W - 4.0f, top = 6.0f, bottom = H - 6.0f;
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
    for (int i = 1; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / steps;
        curve.lineTo (xA + dw * t, yOf (ns + (1.0f - ns) * (std::exp (-k * t) - ek) / (1.0f - ek)));
    }
    const float xD = xA + dw;
    const float xS = xD + swS;
    curve.lineTo (xS, yOf (ns));
    for (int i = 1; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / steps;
        curve.lineTo (xS + rw * t, yOf (ns * (std::exp (-k * t) - ek) / (1.0f - ek)));
    }

    g.setColour (colours::hairline);
    g.drawLine (0.0f, bottom + 0.5f, W, bottom + 0.5f, 1.0f);
    juce::Path fill (curve);
    fill.lineTo (right, bottom);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.14f), 0.0f, top, colour.withAlpha (0.02f), 0.0f, bottom, false));
    g.fillPath (fill);
    g.setColour (colours::ink);
    g.strokePath (curve, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour);
    for (const auto& p : { juce::Point<float> (xA, top), juce::Point<float> (xD, yOf (ns)), juce::Point<float> (xS, yOf (ns)) })
        g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
}

//==============================================================================
// Voice activity

VoiceActivity::VoiceActivity (APVTS& state, std::function<float (int)> fn)
    : voiceCount (state.getRawParameterValue (params::voice_count)), levelOf (std::move (fn))
{
    setInterceptsMouseClicks (false, false);
    poll();
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
    drawCaption (g, "VOICE ACTIVITY", 12.0f, 10.0f, 200.0f);
    const float pitch = juce::jmin (20.0f, (r.getWidth() - 24.0f) / static_cast<float> (juce::jmax (1, shownCount)));
    for (int i = 0; i < shownCount; ++i)
    {
        const float cx = 12.0f + 5.0f + pitch * static_cast<float> (i);
        drawLedDot (g, { cx - 4.0f, 30.0f, 8.0f, 8.0f }, colours::accent, shown[static_cast<size_t> (i)]);
        drawTracked (g, juce::String (i + 1), { cx - 9.0f, 42.0f, 18.0f, 12.0f }, Fonts::jost (8.5f), colours::caption, juce::Justification::centredTop);
    }
}

//==============================================================================
// Level fader

LevelFader::LevelFader (APVTS& state, const juce::String& id, const juce::String& text) : label (text)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, false, nullptr, 1200);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    setupDefaultOnDoubleClick (slider, state, id);
}

void LevelFader::resized()
{
    slider.setBounds (0, 0, getWidth(), getHeight() - 22);
}

void LevelFader::paint (juce::Graphics& g)
{
    drawTracked (g, label, { 0.0f, static_cast<float> (getHeight() - 14), static_cast<float> (getWidth()), 14.0f }, labelFont(), colours::label,
                 juce::Justification::centred);
}

//==============================================================================
// Voice counter

void VoiceCounter::StepButton::paintButton (juce::Graphics& g, bool over, bool)
{
    g.setColour (over ? colours::accent : colours::captionLight);
    g.setFont (Fonts::jost (14.0f));
    g.drawText (getButtonText() == "-" ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : getButtonText(), getLocalBounds(),
                juce::Justification::centred);
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
    minus.setBounds (0, 21, 24, 34);
    plus.setBounds (getWidth() - 24, 21, 24, 34);
}

void VoiceCounter::paint (juce::Graphics& g)
{
    drawTracked (g, "VOICES", { 0.0f, 0.0f, static_cast<float> (getWidth()), 14.0f }, labelFont(), colours::label, juce::Justification::centred);
    const auto box = juce::Rectangle<float> (0.0f, 21.0f, static_cast<float> (getWidth()), 34.0f).reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (colours::fieldBorder);
    g.drawRoundedRectangle (box, 5.0f, 1.0f);
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
    amount.onValueChange = [this] { repaint(); };
    addAndMakeVisible (amount);
    srcAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, params::mmSrc (slot), source);
    dstAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, params::mmDst (slot), dest);
    amtAttachment = std::make_unique<APVTS::SliderAttachment> (state, params::mmAmt (slot), amount);
    setupDefaultOnDoubleClick (amount, state, params::mmAmt (slot));
}

void MatrixRow::resized()
{
    const int w = getWidth();
    const int combo = (w - 24 - 26 - 170) / 2;
    source.setBounds (24, 0, combo, 30);
    dest.setBounds (24 + combo + 26, 0, combo, 30);
    amount.setBounds (24 + 2 * combo + 38, 5, w - (24 + 2 * combo + 38) - 52, 20);
}

void MatrixRow::paint (juce::Graphics& g)
{
    drawTracked (g, juce::String (slot), { 0.0f, 0.0f, 16.0f, 30.0f }, Fonts::jost (11.0f, true), colours::captionLight, juce::Justification::centredLeft);
    static const juce::Path arrow = svgPath ("M1 4 H10 M7 1 L10 4 L7 7");
    g.setColour (colours::caption);
    g.strokePath (arrow, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (static_cast<float> (source.getRight() + 8), 11.0f));
    const int v = juce::roundToInt (amount.getValue() * 100.0);
    drawTracked (g, (v > 0 ? "+" : "") + juce::String (v) + " %", { static_cast<float> (getWidth() - 50), 0.0f, 50.0f, 30.0f }, Fonts::mono (9.5f),
                 v == 0 ? colours::caption : colours::accent.darker (0.2f), juce::Justification::centredRight);
}

//==============================================================================
// Wheels and indicators

Wheel::Wheel (bool isPitch, std::function<float()> g, std::function<void (float)> s) : pitch (isPitch), get (std::move (g)), set (std::move (s)) {}

void Wheel::poll()
{
    const float v = get ? get() : 0.0f;
    if (std::abs (v - shown) > 0.002f)
    {
        shown = v;
        repaint();
    }
}

void Wheel::mouseDown (const juce::MouseEvent&) { dragStart = shown; }

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    const float range = pitch ? 2.0f : 1.0f;
    const float v = juce::jlimit (pitch ? -1.0f : 0.0f, 1.0f, dragStart - static_cast<float> (e.getDistanceFromDragStartY()) / (static_cast<float> (getHeight()) * 0.9f) * range);
    if (set)
        set (v);
}

void Wheel::mouseUp (const juce::MouseEvent&)
{
    if (pitch && set)
        set (0.0f);
}

void Wheel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 7.0f);
    juce::ColourGradient grad (juce::Colour (0xffe6e2db), r.getX(), 0.0f, juce::Colour (0xffe6e2db), r.getRight(), 0.0f, false);
    grad.addColour (0.5, juce::Colours::white);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);
    const float norm = pitch ? 0.5f * (shown + 1.0f) : shown;
    const float offset = (1.0f - norm) * 12.0f;
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    for (float y = r.getY() + 4.0f + std::fmod (offset, 6.0f); y < r.getBottom() - 3.0f; y += 6.0f)
        g.fillRect (r.getX() + 3.0f, y, r.getWidth() - 6.0f, 1.0f);
    const float markY = r.getBottom() - 8.0f - norm * (r.getHeight() - 16.0f);
    const auto colour = pitch ? colours::accent : colours::slate;
    g.setColour (colour);
    g.fillRoundedRectangle (r.getX() + 3.0f, markY - 1.5f, r.getWidth() - 6.0f, 3.0f, 1.5f);
    g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

Indicator::Indicator (const juce::String& text, juce::Colour c, std::function<float()> fn) : label (text), colour (c), level (std::move (fn))
{
    setInterceptsMouseClicks (false, false);
}

void Indicator::poll()
{
    const float v = juce::jlimit (0.0f, 1.0f, level ? level() : 0.0f);
    if (std::abs (v - shown) > 0.02f)
    {
        shown = v;
        repaint();
    }
}

void Indicator::paint (juce::Graphics& g)
{
    const float w = static_cast<float> (getWidth());
    drawTracked (g, label, { 0.0f, 0.0f, w, 12.0f }, Fonts::jost (8.5f, true, 0.18f), shown > 0.05f ? colour : colours::caption, juce::Justification::centred);
    drawLedDot (g, { w * 0.5f - 3.5f, 17.0f, 7.0f, 7.0f }, colour, shown > 0.02f ? 0.35f + 0.65f * shown : 0.0f);
}

//==============================================================================
// Header buttons

HeaderButton::HeaderButton (const juce::String& label, std::vector<juce::Path> paths) : juce::Button (label), icon (std::move (paths)) {}

void HeaderButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto col = down ? colours::accent : (over ? colours::title : colours::headerButton);
    const float x = static_cast<float> (getWidth()) * 0.5f - 9.0f;
    const float y = getButtonText().isEmpty() ? static_cast<float> (getHeight()) * 0.5f - 9.0f : 4.0f;
    g.setColour (col);
    for (const auto& p : icon)
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (x, y));
    if (getButtonText().isNotEmpty())
        drawTracked (g, getButtonText(), { 0.0f, 28.0f, static_cast<float> (getWidth()), 12.0f }, Fonts::jost (8.5f, false, 0.16f), col,
                     juce::Justification::centredTop);
}

ArrowButton::ArrowButton (bool right) : juce::Button (right ? "next" : "previous")
{
    path = svgPath (right ? "M3 2 L8 7 L3 12" : "M7 2 L2 7 L7 12");
}

void ArrowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    g.setColour (down ? colours::accent : (over ? colours::title : colours::captionLight));
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (static_cast<float> (getWidth()) * 0.5f - 5.0f, static_cast<float> (getHeight()) * 0.5f - 7.0f));
}

} // namespace augur5::ui
