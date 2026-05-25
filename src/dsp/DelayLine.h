#pragma once

#include <vector>
#include <cmath>

namespace doobie
{
// A single-channel circular delay buffer with fractional read positions.
//
// The buffer length is rounded up to a power of two so wrapping is a cheap
// bitmask. Reads use 4-point Catmull-Rom (cubic Hermite) interpolation, which
// keeps the modulated tape pitch-shifts smooth without the dull top end you
// get from linear interpolation.
class DelayLine
{
public:
    void prepare (double sampleRate, double maxDelaySeconds)
    {
        const auto maxSamples = (int) std::ceil (sampleRate * maxDelaySeconds) + 4;

        int pow2 = 1;
        while (pow2 < maxSamples)
            pow2 <<= 1;

        size = pow2;
        mask = pow2 - 1;
        buffer.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    // Store one sample and advance the write head.
    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Read `delaySamples` (fractional, >= 1) behind the newest sample.
    inline float read (float delaySamples) const noexcept
    {
        const float readPos = (float) writePos - 1.0f - delaySamples;
        const int   i      = (int) std::floor (readPos);
        const float frac   = readPos - (float) i;

        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) ( i      & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];

        // Catmull-Rom spline coefficients.
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + x0;
    }

    int maxSamples() const noexcept { return size; }

private:
    std::vector<float> buffer;
    int size = 0;
    int mask = 0;
    int writePos = 0;
};
} // namespace doobie
