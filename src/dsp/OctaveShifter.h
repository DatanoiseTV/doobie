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

#include "DelayLine.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
// A granular octave-up pitch shifter. Two read "grains" sweep the buffer at
// twice the write rate (delay shrinking one sample per sample = +1 octave),
// offset by half a window and Hann-windowed so their crossfade hides the wrap
// discontinuity. Feeding its output back into a reverb produces the classic
// rising shimmer. Cheap and stable; a little warble is part of the character.
class OctaveShifter
{
public:
    void prepare (double sr)
    {
        buffer.prepare (sr, 0.25);
        window = (float) (sr * 0.080);  // 80 ms grain
        reset();
    }

    void reset()
    {
        buffer.reset();
        phase = 0.0f;
    }

    inline float process (float x) noexcept
    {
        constexpr float twoPi = 6.28318530718f;

        buffer.write (x);

        phase += 1.0f / window;          // ratio 2: read delay shrinks 1/sample
        if (phase >= 1.0f) phase -= 1.0f;
        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float d1 = std::max (1.0f, (1.0f - phase)  * window);
        const float d2 = std::max (1.0f, (1.0f - phase2) * window);
        const float w1 = 0.5f * (1.0f - std::cos (twoPi * phase));
        const float w2 = 0.5f * (1.0f - std::cos (twoPi * phase2));

        return w1 * buffer.read (d1) + w2 * buffer.read (d2);
    }

private:
    DelayLine buffer;
    float window = 3840.0f;
    float phase = 0.0f;
};
} // namespace doobie
