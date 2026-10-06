#pragma once

#include "../AuguryModel.h"
#include "Widgets.h"

#include <deque>

namespace augur5::ui
{

class MorphPad;

// AUGURY tab: the morph field (four captured sounds in its corners, a bird to fly between them) and OMEN, the
// reproducible variation generator. Laid out in page coordinates (1536 x 650).
class AuguryPage final : public juce::Component
{
public:
    AuguryPage (AuguryModel& model, std::function<juce::String()> currentSoundName);
    ~AuguryPage() override;
    void paint (juce::Graphics&) override;
    void poll();

private:
    void refresh();

    AuguryModel& model;
    std::function<juce::String()> soundName;
    std::unique_ptr<MorphPad> pad;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::array<ActionButton*, AuguryModel::numCorners> recallButtons {}, clearButtons {};
    std::array<LedToggle*, AuguryModel::numGroups> locks {};
    juce::Slider amount;
    juce::String shownSeed;
};

} // namespace augur5::ui
