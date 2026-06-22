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

#include <vector>
#include <cmath>
#include <algorithm>

namespace doobie
{
// Dual-head crossfaded granular pitch shifter (the classic Eventide H910 /
// Lexicon technique). Operates entirely in the time domain on a circular
// buffer; no FFT analysis frame, so it has no inherent buffer-length
// latency — only the half-window grain length, ~25 ms here.
//
// Two read heads scan the same write-buffer at the pitch ratio, 180°
// apart in window phase. A Hann window crossfades them so the wrap-around
// (when each head catches the writer or hits the window edge) is
// inaudible — at any moment one head is in the middle of its window
// (full amplitude) while the other is fading in/out at the edges.
//
// Trade-offs vs. the FFT phase vocoder (FftPitchShifter):
//   + Lower latency (~25 ms half-window vs. ~17 ms N-hop)
//   + Smoother on transients and percussive sources (no spectral smear)
//   + Cleaner at extreme intervals (±24 st) — no bin-shift aliasing
//   - More phasey on sustained pitched material (the crossfade is audible
//     as a slow flutter); the phase vocoder is better there
//
// Both algorithms ship; user picks per-instance via a UI switch.
class GranularPitchShifter
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        // 200 ms buffer (next power of two). Window is half the buffer
        // so each head has a ~100 ms scan range — long enough that even
        // -24 st (read at half speed) doesn't run out of samples before
        // the next wrap.
        int n = 1;
        while (n < (int) (0.2 * sr)) n <<= 1;
        bufSize = n;
        bufMask = n - 1;
        buffer.assign ((size_t) bufSize, 0.0f);
        windowSize = 0.5f * (float) bufSize;
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        // Start the two heads 180° apart in window phase. The static
        // offset means head 0 is at the window centre (full amplitude)
        // while head 1 is at the window edge (fading), and vice-versa
        // half a window later — so one is always carrying the signal.
        readDelta[0] = 0.0f;
        readDelta[1] = windowSize * 0.5f;
    }

    void setIntervalSemitones (float semitones) noexcept
    {
        const float clamped = std::max (-24.0f, std::min (24.0f, semitones));
        ratio = std::pow (2.0f, clamped / 12.0f);
    }

    float process (float in) noexcept
    {
        // Write into the circular buffer first so a unity-ratio readback
        // (ratio == 1, delta == 0) reads the just-written sample exactly.
        buffer[(size_t) (writePos & bufMask)] = in;

        float out = 0.0f;
        for (int h = 0; h < 2; ++h)
        {
            // Read position = writePos - readDelta (fractional, into the
            // past). 4-point linear interp on the wrapped index — fine
            // here because the window envelope masks the small audible
            // artefacts of linear interpolation.
            const float rp = (float) writePos - readDelta[h];
            const int   i0 = (int) std::floor (rp);
            const float f  = rp - (float) i0;
            const float s0 = buffer[(size_t) (i0       & bufMask)];
            const float s1 = buffer[(size_t) ((i0 + 1) & bufMask)];
            const float sample = s0 + f * (s1 - s0);

            // Hann window over the 0..windowSize span. At delta == 0
            // (head exactly at writer) the window is 0 — no signal,
            // which avoids the audible click of reading the
            // discontinuity at the wrap point.
            const float winX = readDelta[h] / windowSize;
            const float w = 0.5f - 0.5f * std::cos (6.2831853f * winX);

            out += w * sample;

            // Advance the read head's distance from the writer. If
            // ratio > 1 we read faster than we write, so the head
            // approaches the writer (delta shrinks). At ratio < 1 the
            // head falls behind (delta grows). When delta exits the
            // [0, windowSize] window, wrap by one window — the Hann
            // envelope is already at zero there, so the wrap is
            // inaudible.
            readDelta[h] += (1.0f - ratio);
            if (readDelta[h] >= windowSize) readDelta[h] -= windowSize;
            else if (readDelta[h] < 0.0f)   readDelta[h] += windowSize;
        }

        ++writePos;
        return out;
    }

private:
    std::vector<float> buffer;
    int   bufSize = 0;
    int   bufMask = 0;
    float windowSize = 0.0f;
    int   writePos = 0;
    float readDelta[2] { 0.0f, 0.0f };
    float ratio = 1.0f;
    double sampleRate = 44100.0;
};
} // namespace doobie
