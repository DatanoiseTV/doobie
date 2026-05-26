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

#include "FftPitchShifter.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
namespace
{
    constexpr float kPi    = 3.14159265358979324f;
    constexpr float kTwoPi = 6.28318530717958648f;

    // Wrap a phase difference into -pi..pi.
    inline float wrapPhase (float p) noexcept
    {
        int qpd = (int) (p / kPi);
        if (qpd >= 0) qpd += qpd & 1;
        else          qpd -= qpd & 1;
        return p - kPi * (float) qpd;
    }
}

void FftPitchShifter::prepare (double sr)
{
    sampleRate = sr;
    freqPerBin = (float) sr / (float) N;
    expct = kTwoPi * (float) hop / (float) N;

    for (int i = 0; i < N; ++i)
        window[(size_t) i] = 0.5f * (1.0f - std::cos (kTwoPi * (float) i / (float) N));

    spec.assign (N, {});
    time.assign (N, {});

    normGain = 1.0f;
    reset();

    // Auto-calibrate output level: at ratio 1 the vocoder reconstructs the
    // input, so measure the raw gain and invert it. This removes any dependence
    // on the FFT's internal normalisation.
    const float savedRatio = ratio;
    ratio = 1.0f;
    reset();
    double inSum = 0.0, outSum = 0.0;
    const int warmup = N * 2;
    const int total  = N * 8;
    for (int i = 0; i < total; ++i)
    {
        const float s = std::sin (kTwoPi * 1000.0f * (float) i / (float) sr);
        const float o = process (s);
        if (i > warmup)
        {
            inSum  += (double) s * s;
            outSum += (double) o * o;
        }
    }
    normGain = (outSum > 1.0e-9) ? (float) std::sqrt (inSum / outSum) : 1.0f;

    ratio = savedRatio;
    reset();
}

void FftPitchShifter::reset()
{
    inFIFO.fill (0.0f);
    outFIFO.fill (0.0f);
    outAccum.fill (0.0f);
    lastPhase.fill (0.0f);
    sumPhase.fill (0.0f);
    rover = N - hop;
}

float FftPitchShifter::process (float x) noexcept
{
    inFIFO[(size_t) rover] = x;
    const float out = outFIFO[(size_t) (rover - (N - hop))];
    ++rover;
    if (rover >= N)
    {
        rover = N - hop;
        processFrame();
    }
    return out * normGain;
}

void FftPitchShifter::processFrame() noexcept
{
    // ---- Analysis ----------------------------------------------------------
    for (int i = 0; i < N; ++i)
        spec[(size_t) i] = { inFIFO[(size_t) i] * window[(size_t) i], 0.0f };

    fft.perform (spec.data(), time.data(), false); // time[] now holds the spectrum

    for (int k = 0; k < bins; ++k)
    {
        const float re = time[(size_t) k].real();
        const float im = time[(size_t) k].imag();
        const float mag = 2.0f * std::sqrt (re * re + im * im);
        const float phase = std::atan2 (im, re);

        float delta = phase - lastPhase[(size_t) k];
        lastPhase[(size_t) k] = phase;
        delta -= (float) k * expct;
        delta = wrapPhase (delta);
        delta = (float) (N / hop) * delta / kTwoPi;

        anaMag[(size_t) k]  = mag;
        anaFreq[(size_t) k] = ((float) k + delta) * freqPerBin; // true frequency (Hz)
    }

    // ---- Shift partials to ratio * frequency -------------------------------
    synMag.fill (0.0f);
    synFreq.fill (0.0f);
    for (int k = 0; k < bins; ++k)
    {
        const int j = (int) ((float) k * ratio);
        if (j >= 0 && j < bins)
        {
            synMag[(size_t) j]  += anaMag[(size_t) k];
            synFreq[(size_t) j]  = anaFreq[(size_t) k] * ratio;
        }
    }

    // ---- Synthesis ---------------------------------------------------------
    for (int k = 0; k < bins; ++k)
    {
        const float mag = synMag[(size_t) k];
        float tmp = synFreq[(size_t) k];
        tmp -= (float) k * freqPerBin;
        tmp /= freqPerBin;
        tmp = kTwoPi * tmp / (float) (N / hop);
        tmp += (float) k * expct;
        sumPhase[(size_t) k] += tmp;
        const float phase = sumPhase[(size_t) k];
        spec[(size_t) k] = { mag * std::cos (phase), mag * std::sin (phase) };
    }
    // Hermitian symmetry for a real inverse transform.
    for (int k = 1; k < N / 2; ++k)
        spec[(size_t) (N - k)] = std::conj (spec[(size_t) k]);

    fft.perform (spec.data(), time.data(), true); // time[] now holds the frame

    // ---- Windowed overlap-add ---------------------------------------------
    constexpr float acc = 2.0f / ((float) (N / 2) * (float) (N / hop));
    for (int i = 0; i < N; ++i)
        outAccum[(size_t) i] += acc * window[(size_t) i] * time[(size_t) i].real();

    for (int i = 0; i < hop; ++i)
        outFIFO[(size_t) i] = outAccum[(size_t) i];

    std::move (outAccum.begin() + hop, outAccum.end(), outAccum.begin());
    std::fill (outAccum.end() - hop, outAccum.end(), 0.0f);

    for (int i = 0; i < N - hop; ++i)
        inFIFO[(size_t) i] = inFIFO[(size_t) (i + hop)];
}
} // namespace doobie
