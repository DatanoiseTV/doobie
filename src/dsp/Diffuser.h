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
    void prepare (double sr)
    {
        constexpr std::array<float, 4> ms { 13.7f, 22.9f, 31.1f, 41.3f };
        for (int i = 0; i < 4; ++i)
        {
            ap[(size_t) i].prepare (sr, 0.06);
            len[(size_t) i] = ms[(size_t) i] * 0.001f * (float) sr;
        }
        reset();
    }

    void reset() { for (auto& a : ap) a.reset(); }

    inline float process (float x) noexcept
    {
        for (int i = 0; i < 4; ++i)
        {
            const float z = ap[(size_t) i].read (len[(size_t) i]);
            const float y = -g * x + z;
            ap[(size_t) i].write (x + g * y);
            x = y;
        }
        return x;
    }

private:
    std::array<DelayLine, 4> ap;
    std::array<float, 4> len {};
    float g = 0.7f;
};
} // namespace doobie
