#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DoobieAudioProcessor;

namespace doobie
{
// A reverb decay display. It draws the estimated impulse-decay envelope for the
// currently active reverb (spring, plate, their series chain or parallel), so
// you can see roughly how long and how dense the tail is, with the estimated
// decay time printed. It is the reverb-section counterpart to the delay's echo
// visualiser, replacing the earlier purely decorative spring coils.
class ReverbResponseView : public juce::Component,
                           private juce::Timer
{
public:
    explicit ReverbResponseView (DoobieAudioProcessor& p);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    // Rough RT60-style decay time (seconds) for the active reverb settings.
    float estimateDecaySeconds() const;
    float predelaySeconds() const;

    DoobieAudioProcessor& processor;
};
} // namespace doobie
