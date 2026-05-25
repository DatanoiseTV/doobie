#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace doobie
{
// A small analog-style VU meter: a swinging needle over a marked arc with a red
// zone near the top. The needle uses classic VU ballistics (slow attack and
// release) so it reads musically rather than twitching on every transient.
class VuMeter : public juce::Component,
                private juce::Timer
{
public:
    explicit VuMeter (juce::String caption);

    // Callback returning the current linear peak level (0..1).
    void setLevelSource (std::function<float()> fn) { source = std::move (fn); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> source;
    juce::String caption;
    float level = 0.0f;     // smoothed, 0..1 across the scale
    float peak  = 0.0f;     // peak-hold marker position
    int   peakHold = 0;
};
} // namespace doobie
