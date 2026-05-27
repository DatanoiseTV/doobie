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

#include <cmath>

namespace doobie
{
// One-pole DC blocker (high-pass at a few Hz): y[n] = x[n] - x[n-1] + R*y[n-1].
//
// In a recirculating delay any small DC offset — from asymmetric saturation,
// the BBD/diffuse/pitch stages, or filter transients — is fed back and grows
// each pass, eventually pushing the loop into a steady rail and squashing the
// audio. A DC blocker in the feedback path stops that build-up; one on the wet
// output keeps the mix centred. The cutoff is sub-audible so the low end is
// untouched.
class DcBlocker
{
public:
    void prepare (double sampleRate, float cutoffHz = 8.0f) noexcept
    {
        // R = e^(-2*pi*fc/fs); closer to 1 as fc drops, giving a gentler high-pass.
        R = std::exp (-6.2831853f * cutoffHz / (float) sampleRate);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float R  = 0.999f;
    float x1 = 0.0f, y1 = 0.0f;
};
} // namespace doobie
