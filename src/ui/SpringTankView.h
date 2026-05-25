#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace doobie
{
// Decorative spring tank: three suspended coils that shimmer with the signal
// level, echoing the physical reverb tank the plugin emulates. Purely visual,
// but it ties the reverb section to the unit's identity.
class SpringTankView : public juce::Component,
                       private juce::Timer
{
public:
    SpringTankView();

    void setLevelSource (std::function<float()> fn) { source = std::move (fn); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> source;
    float phase = 0.0f;
    float excite = 0.0f;
};
} // namespace doobie
