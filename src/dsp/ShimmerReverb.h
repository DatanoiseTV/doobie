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

#include "FdnReverb.h"
#include "FftPitchShifter.h"

namespace doobie
{
// A shimmer reverb: a dense FDN tail whose output is pitched up one octave and
// fed back into its own input. Each pass climbs another octave, so the tail
// blooms into a soft, rising, choir-like pad over the dry signal. The "shimmer"
// amount controls how much of the octave is regenerated; a low-pass on the
// feedback keeps it smooth rather than glassy.
class ShimmerReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    // decay/size/damp in 0..1, predelay in ms, shimmer 0..1 (regeneration).
    void setParams (float decay, float size, float damp, float predelayMs, float shimmer) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    FdnReverb core;
    FftPitchShifter shiftL, shiftR;

    float shimmerAmt = 0.5f;
    float fbL = 0.0f, fbR = 0.0f;
    float lpfL = 0.0f, lpfR = 0.0f;
};
} // namespace doobie
