#pragma once

#include <cmath>

namespace doobie
{
// Tape-style soft saturation sitting in the feedback path. Each repeat is
// driven a little harder, so heavy feedback grows warm and compressed instead
// of harsh. A small asymmetry adds even harmonics; a one-pole DC blocker
// removes the offset that asymmetry would otherwise pump into the loop.
class Saturation
{
public:
    void reset() noexcept { dcX = dcY = 0.0f; }

    // drive in 0..1.
    void setDrive (float d) noexcept
    {
        drive    = 1.0f + d * 7.0f;        // 1..8
        normGain = 1.0f / std::tanh (drive); // keep full-scale roughly unity
    }

    inline float process (float x) noexcept
    {
        const float pre = drive * x + 0.05f * drive * x * x; // gentle asymmetry
        float y = std::tanh (pre) * normGain;

        // DC blocker (one-pole high-pass at a few Hz).
        const float out = y - dcX + 0.9995f * dcY;
        dcX = y;
        dcY = out;
        return out;
    }

private:
    float drive = 1.0f;
    float normGain = 1.0f / std::tanh (1.0f);
    float dcX = 0.0f, dcY = 0.0f;
};
} // namespace doobie
