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

#include "SpringReverb.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
void SpringReverb::Biquad::set (double sr, float fc, float q) noexcept
{
    const float w0    = 6.28318530718f * fc / (float) sr;
    const float cosw0 = std::cos (w0);
    const float alpha = std::sin (w0) / (2.0f * q);

    // RBJ band-pass (constant skirt gain, peak gain = Q).
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cosw0) / a0;
    a2 = (1.0f - alpha) / a0;
}

void SpringReverb::Line::prepare (double sr, float delaySeconds, float modHz)
{
    delay.prepare (sr, 0.2);
    baseDelay = (float) (sr * delaySeconds);
    modRate   = modHz;
    ting.set (sr, 2800.0f, 1.6f);
    reset();
}

void SpringReverb::Line::reset()
{
    delay.reset();
    apX.fill (0.0f);
    apY.fill (0.0f);
    dampZ = dcX = dcY = 0.0f;
    modPhase = 0.0f;
    ting.reset();
}

float SpringReverb::Line::process (float x, double sr) noexcept
{
    constexpr float twoPi = 6.28318530718f;

    // Read the loop with a sinusoidal delay modulation for movement.
    modPhase += modRate / (float) sr;
    if (modPhase >= 1.0f) modPhase -= 1.0f;
    const float mod = 1.0f + (0.0015f + 0.008f * modDepth) * std::sin (twoPi * modPhase);
    float d = delay.read (baseDelay * mod);

    // Dispersion: cascade of first-order all-pass filters. Stacked, they smear
    // an impulse into the characteristic rising spring chirp.
    for (int i = 0; i < kStages; ++i)
    {
        const float in = d;
        const float y  = apCoef * (in - apY[(size_t) i]) + apX[(size_t) i];
        apX[(size_t) i] = in;
        apY[(size_t) i] = y;
        d = y;
    }

    // Damping low-pass inside the loop (energy loss along the spring).
    dampZ += dampCoef * (d - dampZ);
    d = dampZ;

    // DC blocker keeps the recirculating loop from drifting.
    const float blocked = d - dcX + 0.999f * dcY;
    dcX = d;
    dcY = blocked;
    d = blocked;

    // Inject input plus feedback, write back to the tank.
    delay.write (x + fb * d);
    return d;
}

void SpringReverb::prepare (double sr)
{
    sampleRate = sr;
    lineL.prepare (sr, 0.0471f, 0.83f);
    lineR.prepare (sr, 0.0529f, 0.91f);
    reset();
}

void SpringReverb::reset()
{
    lineL.reset();
    lineR.reset();
    hpL = hpR = lpL = lpR = 0.0f;
}

void SpringReverb::setParams (float decay, float tone, float mod) noexcept
{
    decay = std::clamp (decay, 0.0f, 1.0f);
    tone  = std::clamp (tone,  0.0f, 1.0f);
    mod   = std::clamp (mod,   0.0f, 1.0f);

    const float fb = 0.55f + 0.43f * decay;
    lineL.fb = fb;
    lineR.fb = fb;

    // Tone opens the in-loop damping low-pass and the input low-pass together.
    const float dampCutoff = 0.18f + 0.55f * tone;
    lineL.dampCoef = dampCutoff;
    lineR.dampCoef = dampCutoff;
    lpCoef = 0.20f + 0.45f * tone;

    lineL.modDepth = mod;
    lineR.modDepth = mod;

    // Brighter tone pushes the resonant "ting" higher and lets a touch more
    // of it through.
    lineL.ting.set (sampleRate, 2200.0f + 1800.0f * tone, 1.6f);
    lineR.ting.set (sampleRate, 2350.0f + 1800.0f * tone, 1.6f);
    tingMix = 0.25f + 0.25f * tone;

    lineL.apCoef = 0.62f;
    lineR.apCoef = 0.65f;
}

void SpringReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    // Pre band-pass: one-pole high-pass (~120 Hz) then a tone-tracking low-pass.
    constexpr float hpCoef = 0.985f;

    hpL = hpCoef * (hpL + inL);  const float bpL = inL - hpL * (1.0f - hpCoef);
    hpR = hpCoef * (hpR + inR);  const float bpR = inR - hpR * (1.0f - hpCoef);

    lpL += lpCoef * (bpL - lpL);
    lpR += lpCoef * (bpR - lpR);

    float sL = lineL.process (lpL, sampleRate);
    float sR = lineR.process (lpR, sampleRate);

    // Resonant emphasis on the wet output adds the metallic high chirp. Applied
    // outside the feedback loop so it can never destabilise the tank.
    sL += tingMix * lineL.ting.process (sL);
    sR += tingMix * lineR.ting.process (sR);

    sL *= outGain;
    sR *= outGain;

    // A little cross-bleed widens the tank without collapsing the centre.
    outL = sL + 0.15f * sR;
    outR = sR + 0.15f * sL;
}
} // namespace doobie
