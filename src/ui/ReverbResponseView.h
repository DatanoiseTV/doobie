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
