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
#include <array>

namespace doobie
{
// A short cascade of Schroeder all-pass filters. Placed in the delay feedback
// it smears each repeat into a soft, reverb-like wash (the "Diffuse" delay
// character) without changing the repeat timing.
class Diffuser
{
public:
    static constexpr int kStages = 7;

    void prepare (double sr)
    {
        constexpr std::array<float, kStages> ms { 13.7f, 21.3f, 29.9f, 41.1f, 53.7f, 67.3f, 79.1f };
        for (int i = 0; i < kStages; ++i)
        {
            ap[(size_t) i].prepare (sr, 0.1);
            len[(size_t) i] = ms[(size_t) i] * 0.001f * (float) sr;
        }
        reset();
    }

    void reset() { for (auto& a : ap) a.reset(); }

    inline float process (float x) noexcept
    {
        // A long all-pass cascade turns each repeat into a dense reverberant
        // smear while preserving energy (so the feedback decay is unchanged).
        for (int i = 0; i < kStages; ++i)
        {
            const float z = ap[(size_t) i].read (len[(size_t) i]);
            const float y = -g * x + z;
            ap[(size_t) i].write (x + g * y);
            x = y;
        }
        return x;
    }

private:
    std::array<DelayLine, kStages> ap;
    std::array<float, kStages> len {};
    float g = 0.72f;
};
} // namespace doobie
