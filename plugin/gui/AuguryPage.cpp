#include "AuguryPage.h"

namespace augur5::ui
{

namespace
{
const char* const cornerLetters[4] { "A", "B", "C", "D" };
const juce::Colour cornerColours[4] { colours::accent, colours::slate, colours::sage, colours::plum };

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::Path birdPath (float s, float beat)
{
    juce::Path bird;
    const float tip = s * 0.5f * beat;
    bird.startNewSubPath (-s, tip);
    bird.quadraticTo (-s * 0.45f, -s * 0.3f - tip * 0.3f, 0.0f, s * 0.15f);
    bird.quadraticTo (s * 0.45f, -s * 0.3f - tip * 0.3f, s, tip);
    return bird;
}
} // namespace

//==============================================================================
// The field: a templum (the augur's quartered patch of sky) with the four sounds in its corners.
class MorphPad final : public juce::Component
{
public:
    explicit MorphPad (AuguryModel& m) : model (m) {}

    void poll()
    {
        flap += 0.25f;
        const auto p = model.getPosition();
        if (trail.empty() || trail.back() != p)
        {
            trail.push_back (p);
            if (trail.size() > 48)
                trail.pop_front();
        }
        else if (trail.size() > 1)
        {
            trail.pop_front(); // the trail fades out when the bird rests
        }
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        model.beginMorph();
        moveTo (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override { moveTo (e.position); }
    void mouseUp (const juce::MouseEvent&) override { model.endMorph(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto field = fieldBounds();
        g.setColour (colours::panelBottom);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::subPanelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        // Templum: fine grid, the two axes, a circle of the sky.
        g.setColour (colours::hairline);
        for (int i = 1; i < 12; ++i)
        {
            const float fx = field.getX() + field.getWidth() * static_cast<float> (i) / 12.0f;
            const float fy = field.getY() + field.getHeight() * static_cast<float> (i) / 12.0f;
            g.fillRect (fx, field.getY(), 0.6f, field.getHeight());
            g.fillRect (field.getX(), fy, field.getWidth(), 0.6f);
        }
        g.setColour (colours::tickMinor);
        g.fillRect (field.getCentreX() - 0.5f, field.getY(), 1.0f, field.getHeight());
        g.fillRect (field.getX(), field.getCentreY() - 0.5f, field.getWidth(), 1.0f);
        for (const float k : { 0.24f, 0.46f })
        {
            const float rr = juce::jmin (field.getWidth(), field.getHeight()) * k;
            g.drawEllipse (field.getCentreX() - rr, field.getCentreY() - rr, rr * 2.0f, rr * 2.0f, 0.8f);
        }

        // Influence of each corner: a soft wash and its weight.
        const auto w = model.weights();
        const bool active = model.filledCount() >= 2;
        for (int c = 0; c < 4; ++c)
        {
            const auto corner = cornerPoint (c);
            const bool filled = model.isFilled (c);
            const float weight = active ? w[static_cast<size_t> (c)] : 0.0f;
            if (filled && weight > 0.001f)
            {
                const float rr = field.getWidth() * (0.18f + 0.5f * weight);
                juce::ColourGradient wash (cornerColours[c].withAlpha (0.16f * weight + 0.04f), corner.x, corner.y, cornerColours[c].withAlpha (0.0f),
                                           corner.x + rr, corner.y, true);
                g.setGradientFill (wash);
                g.fillEllipse (corner.x - rr, corner.y - rr, rr * 2.0f, rr * 2.0f);
            }
            // Badge.
            const juce::Rectangle<float> badge (corner.x - 15.0f, corner.y - 15.0f, 30.0f, 30.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (badge);
            g.setColour (filled ? cornerColours[c] : colours::fieldBorder);
            g.drawEllipse (badge.reduced (0.5f), filled ? 1.5f : 1.0f);
            drawTracked (g, cornerLetters[c], badge, Fonts::jost (13.0f, true), filled ? cornerColours[c].darker (0.2f) : colours::caption,
                         juce::Justification::centred);
            const bool left = c % 2 == 0, top = c < 2;
            const juce::Rectangle<float> label (left ? corner.x + 22.0f : corner.x - 22.0f - 260.0f, top ? corner.y - 15.0f : corner.y - 1.0f, 260.0f, 16.0f);
            const auto just = left ? juce::Justification::centredLeft : juce::Justification::centredRight;
            drawTracked (g, filled ? model.getName (c) : juce::String ("EMPTY"), label, Fonts::jost (11.5f, filled), filled ? colours::title : colours::caption,
                         just);
            if (filled && active)
                drawTracked (g, juce::String (juce::roundToInt (weight * 100.0f)) + " %", label.translated (0.0f, 16.0f), Fonts::mono (8.5f),
                             cornerColours[c].darker (0.15f), just);
        }

        // Flight trail and the bird.
        if (trail.size() > 1)
        {
            for (size_t i = 1; i < trail.size(); ++i)
            {
                const auto a = toField (trail[i - 1]), b = toField (trail[i]);
                g.setColour (colours::ink.withAlpha (0.35f * static_cast<float> (i) / static_cast<float> (trail.size())));
                g.drawLine ({ a, b }, 1.0f);
            }
        }
        const auto p = toField (model.getPosition());
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillEllipse (p.x - 26.0f, p.y - 26.0f, 52.0f, 52.0f);
        g.setColour (colours::accent.withAlpha (0.35f));
        g.drawEllipse (p.x - 14.0f, p.y - 14.0f, 28.0f, 28.0f, 0.8f);
        g.setColour (colours::ink);
        g.strokePath (birdPath (11.0f, std::sin (flap)), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (p.x, p.y));

        if (! active)
            drawTracked (g, "CAPTURE A SOUND INTO TWO OR MORE CORNERS, THEN FLY BETWEEN THEM",
                         juce::Rectangle<float> (field.getX(), field.getCentreY() + 64.0f, field.getWidth(), 16.0f), Fonts::jost (9.5f, true, 0.2f),
                         colours::caption, juce::Justification::centred);
    }

private:
    juce::Rectangle<float> fieldBounds() const { return getLocalBounds().toFloat().reduced (44.0f, 44.0f); }
    juce::Point<float> cornerPoint (int c) const
    {
        const auto f = fieldBounds();
        return { c % 2 == 0 ? f.getX() : f.getRight(), c < 2 ? f.getY() : f.getBottom() };
    }
    juce::Point<float> toField (juce::Point<float> n) const
    {
        const auto f = fieldBounds();
        return { f.getX() + n.x * f.getWidth(), f.getY() + n.y * f.getHeight() };
    }
    void moveTo (juce::Point<float> pos)
    {
        const auto f = fieldBounds();
        model.setPosition ({ (pos.x - f.getX()) / f.getWidth(), (pos.y - f.getY()) / f.getHeight() });
    }

    AuguryModel& model;
    std::deque<juce::Point<float>> trail;
    float flap = 0.0f;
};

//==============================================================================
AuguryPage::AuguryPage (AuguryModel& m, std::function<juce::String()> name) : model (m), soundName (std::move (name))
{
    pad = std::make_unique<MorphPad> (model);
    addAndMakeVisible (*pad);
    pad->setBounds (68, 56, 728, 570);

    const auto add = [this] (auto c, juce::Rectangle<int> r) {
        auto* raw = c.get();
        raw->setBounds (r);
        addAndMakeVisible (*raw);
        owned.push_back (std::move (c));
        return raw;
    };

    // Corners: capture / recall / clear.
    for (int c = 0; c < AuguryModel::numCorners; ++c)
    {
        const int y = 60 + c * 58;
        add (std::make_unique<ActionButton> ("CAPTURE", cornerColours[c], [this, c] {
                 model.capture (c, soundName ? soundName() : juce::String ("Sound"));
                 refresh();
             }),
             { 1180, y + 6, 100, 30 })
            ->setTooltip ("Store the current sound in this corner");
        recallButtons[static_cast<size_t> (c)] = static_cast<ActionButton*> (
            add (std::make_unique<ActionButton> ("RECALL", cornerColours[c], [this, c] { model.recall (c); }), { 1288, y + 6, 92, 30 }));
        recallButtons[static_cast<size_t> (c)]->setTooltip ("Play this corner's sound exactly");
        clearButtons[static_cast<size_t> (c)] = static_cast<ActionButton*> (add (std::make_unique<ActionButton> ("CLEAR", colours::caption, [this, c] {
                                                                                     model.clear (c);
                                                                                     refresh();
                                                                                 }),
                                                                                 { 1388, y + 6, 76, 30 }));
    }

    // OMEN.
    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setRange (0.0, 1.0);
    amount.setValue (0.35, juce::dontSendNotification);
    amount.setDoubleClickReturnValue (true, 0.35);
    amount.getProperties().set ("bipolar", false);
    amount.onValueChange = [this] { repaint (840, 430, 640, 40); };
    addAndMakeVisible (amount);
    amount.setBounds (944, 438, 400, 24);

    for (int gIndex = 0; gIndex < AuguryModel::numGroups; ++gIndex)
    {
        auto* t = static_cast<LedToggle*> (add (std::make_unique<LedToggle> (AuguryModel::groupName (gIndex), colours::slate),
                                                { 840 + (gIndex % 3) * 212, 508 + (gIndex / 3) * 38, 200, 30 }));
        t->setClickingTogglesState (true);
        t->setTooltip ("Locked groups keep their settings");
        locks[static_cast<size_t> (gIndex)] = t;
    }
    add (std::make_unique<ActionButton> (u8 ("CAST THE OMEN  \xe2\x80\xba"), colours::accent, [this] {
             juce::uint32 locked = 0;
             for (int gIndex = 0; gIndex < AuguryModel::numGroups; ++gIndex)
                 if (locks[static_cast<size_t> (gIndex)]->getToggleState())
                     locked |= 1u << gIndex;
             model.castOmen (static_cast<float> (amount.getValue()), locked);
             refresh();
         }),
         { 840, 594, 300, 34 })
        ->setTooltip ("A new variation of the current sound (UNDO in the header goes back)");
    refresh();
}

AuguryPage::~AuguryPage() = default;

void AuguryPage::refresh()
{
    for (int c = 0; c < AuguryModel::numCorners; ++c)
    {
        recallButtons[static_cast<size_t> (c)]->setEnabled (model.isFilled (c));
        clearButtons[static_cast<size_t> (c)]->setEnabled (model.isFilled (c));
    }
    shownSeed = model.getLastSeed() > 0 ? juce::String (model.getLastSeed()) : juce::String();
    repaint();
}

void AuguryPage::poll()
{
    pad->poll();
}

void AuguryPage::paint (juce::Graphics& g)
{
    drawSection (g, { 52.0f, 10.0f, 760.0f, 632.0f }, "MORPH", colours::accent);
    drawTracked (g, u8 ("drag the bird: the four corner sounds blend by their distance  \xc2\xb7  switches follow the nearest corner"),
                 { 200.0f, 24.0f, 598.0f, 16.0f }, Fonts::jost (9.5f, false, 0.05f), colours::caption, juce::Justification::centredRight);

    // Corners
    drawSection (g, { 824.0f, 10.0f, 660.0f, 296.0f }, "CORNERS", colours::slate);
    for (int c = 0; c < AuguryModel::numCorners; ++c)
    {
        const float y = 60.0f + 58.0f * static_cast<float> (c);
        const bool filled = model.isFilled (c);
        g.setColour (colours::subPanel);
        g.fillRoundedRectangle (840.0f, y, 628.0f, 42.0f, 6.0f);
        const juce::Rectangle<float> badge (850.0f, y + 7.0f, 28.0f, 28.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (badge);
        g.setColour (filled ? cornerColours[c] : colours::fieldBorder);
        g.drawEllipse (badge.reduced (0.5f), filled ? 1.5f : 1.0f);
        drawTracked (g, cornerLetters[c], badge, Fonts::jost (12.0f, true), filled ? cornerColours[c].darker (0.2f) : colours::caption,
                     juce::Justification::centred);
        drawTracked (g, filled ? model.getName (c) : juce::String ("EMPTY"), { 890.0f, y + 6.0f, 280.0f, 18.0f }, Fonts::jost (12.5f, filled),
                     filled ? colours::title : colours::caption, juce::Justification::centredLeft);
        drawTracked (g, c < 2 ? (c == 0 ? "TOP LEFT" : "TOP RIGHT") : (c == 2 ? "BOTTOM LEFT" : "BOTTOM RIGHT"), { 890.0f, y + 23.0f, 280.0f, 12.0f },
                     Fonts::jost (8.0f, true, 0.2f), colours::caption, juce::Justification::centredLeft);
    }

    // Omen
    drawSection (g, { 824.0f, 318.0f, 660.0f, 324.0f }, "OMEN", colours::accent);
    drawNote (g, "A reproducible variation of the sound on the panel: every unlocked group drifts by a seeded random walk "
                 "whose size is AMOUNT. Locked groups stay exactly as they are. Each omen is numbered; UNDO returns to the sound before it.",
              { 840.0f, 368.0f, 628.0f, 48.0f });
    drawCaption (g, "AMOUNT", 840.0f, 444.0f, 90.0f, colours::captionLight);
    drawTracked (g, juce::String (juce::roundToInt (amount.getValue() * 100.0)) + " %", { 1352.0f, 440.0f, 116.0f, 20.0f }, Fonts::mono (10.0f),
                 colours::accent.darker (0.2f), juce::Justification::centredLeft);
    drawCaption (g, "LOCKED GROUPS", 840.0f, 488.0f, 200.0f, colours::captionLight);
    if (shownSeed.isNotEmpty())
        drawTracked (g, u8 ("OMEN  \xe2\x84\x96 ") + shownSeed, { 1160.0f, 594.0f, 308.0f, 34.0f }, Fonts::light (17.0f, 0.2f), colours::title,
                     juce::Justification::centredRight);
}

} // namespace augur5::ui
