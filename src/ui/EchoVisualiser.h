#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DoobieAudioProcessor;

namespace doobie
{
// A live map of the echo pattern. Each of the four heads gets a lane; the tap
// times (head ratio x delay time) and the decaying repeats (scaled by feedback)
// are drawn as fading amber dots, so you can see the rhythm the current settings
// produce. Active heads glow according to their live output level.
class EchoVisualiser : public juce::Component,
                       private juce::Timer
{
public:
    explicit EchoVisualiser (DoobieAudioProcessor& p);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DoobieAudioProcessor& processor;
};
} // namespace doobie
