#pragma once

#include "Widgets.h"

#include <array>

namespace augur5::ui
{

// What the FX page needs from the processor (live read-outs and the rack order).
struct FxModel
{
    virtual ~FxModel() = default;
    virtual std::array<int, 8> getOrder() const = 0;
    virtual void setOrder (const std::array<int, 8>&) = 0;
    virtual float compGainReduction() const = 0; // dB
    virtual float phaserHz() const = 0;          // current sweep centre
    virtual float flangerMs() const = 0;         // current delay
    virtual float echoHeadMs() const = 0;        // playback head 1, with the motor's glide
    virtual double tempo() const = 0;            // BPM in use
};

class FxEditor;
class FxSlot;

// FX tab: the effects chain and a large editor for the selected effect, with a live view of what it does.
// FUZZ sits on the voice bus (before the effects rack, fixed); the eight rack units can be dragged into any order.
class FxPage final : public juce::Component
{
public:
    static constexpr int fuzzId = 8, numEffects = 9;

    FxPage (APVTS&, FxModel&);
    ~FxPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void poll(); // GUI timer: live views, order changes from presets

    void select (int effectId);
    void dragSlot (int effectId, int x); // live reordering while dragging a slot
    void dropSlot();

private:
    void layoutSlots();
    juce::Rectangle<int> slotBounds (int position) const;

    APVTS& state;
    FxModel& model;
    juce::OwnedArray<FxSlot> slots;     // by effect id (0..7 rack, 8 fuzz)
    juce::OwnedArray<FxEditor> editors; // by effect id
    std::array<int, 8> order { 0, 1, 2, 3, 4, 5, 6, 7 };
    int selected = 0, dragging = -1;
};

} // namespace augur5::ui
