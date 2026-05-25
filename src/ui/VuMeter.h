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
