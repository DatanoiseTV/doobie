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

#include "ShimmerReverb.h"
#include <algorithm>

namespace doobie
{
void ShimmerReverb::prepare (double sr)
{
    core.prepare (sr, 30.0f, 90.0f);
    shiftL.prepare (sr);
    shiftR.prepare (sr);
    reset();
}

void ShimmerReverb::reset()
{
    core.reset();
    shiftL.reset();
    shiftR.reset();
    fbL = fbR = lpfL = lpfR = 0.0f;
}

void ShimmerReverb::setParams (float decay, float size, float damp, float predelayMs, float shimmer) noexcept
{
    // The core runs fairly lush; modulation is fixed-moderate for movement.
    core.setParams (decay, size, damp, predelayMs, 0.4f);
    shimmerAmt = 0.2f + 0.6f * std::clamp (shimmer, 0.0f, 1.0f); // 0.2..0.8, always < 1
}

void ShimmerReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    float wL = 0.0f, wR = 0.0f;
    core.process (inL + fbL, inR + fbR, wL, wR);

    // Octave-up the wet tail, soften it, and feed it back to climb each pass.
    const float sL = shiftL.process (wL);
    const float sR = shiftR.process (wR);
    lpfL += 0.30f * (sL - lpfL);
    lpfR += 0.30f * (sR - lpfR);
    fbL = shimmerAmt * lpfL;
    fbR = shimmerAmt * lpfR;

    outL = wL;
    outR = wR;
}
} // namespace doobie
