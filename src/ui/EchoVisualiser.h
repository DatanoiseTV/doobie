/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.
*/

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
