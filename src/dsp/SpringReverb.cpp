#include "SpringReverb.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
void SpringReverb::Line::prepare (double sr, float delaySeconds, float modHz)
{
    delay.prepare (sr, 0.2);
    baseDelay = (float) (sr * delaySeconds);
    modRate   = modHz;
    reset();
}

void SpringReverb::Line::reset()
{
    delay.reset();
    apX.fill (0.0f);
    apY.fill (0.0f);
    dampZ = dcX = dcY = 0.0f;
    modPhase = 0.0f;
}

float SpringReverb::Line::process (float x, double sr) noexcept
{
    constexpr float twoPi = 6.28318530718f;

    // Read the loop with a small sinusoidal delay modulation for movement.
    modPhase += modRate / (float) sr;
    if (modPhase >= 1.0f) modPhase -= 1.0f;
    const float mod = 1.0f + 0.003f * std::sin (twoPi * modPhase);
    float d = delay.read (baseDelay * mod);

    // Dispersion: cascade of first-order all-pass filters. Each stage adds a
    // touch of frequency-dependent delay; stacked, they smear an impulse into
    // the characteristic spring chirp.
    for (int i = 0; i < kStages; ++i)
    {
        const float in = d;
        const float y  = apCoef * (in - apY[(size_t) i]) + apX[(size_t) i];
        apX[(size_t) i] = in;
        apY[(size_t) i] = y;
        d = y;
    }

    // Damping low-pass inside the loop (energy loss over the spring length).
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
    // Two detuned spring lengths and modulation rates for a wider tank.
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

void SpringReverb::setParams (float decay, float tone) noexcept
{
    const float fb = 0.55f + 0.43f * std::clamp (decay, 0.0f, 1.0f);
    lineL.fb = fb;
    lineR.fb = fb;

    // Tone opens the in-loop damping low-pass and the input low-pass together.
    const float t = std::clamp (tone, 0.0f, 1.0f);
    const float dampCutoff = 0.18f + 0.55f * t;   // normalised one-pole coeff
    lineL.dampCoef = dampCutoff;
    lineR.dampCoef = dampCutoff;

    lpCoef = 0.20f + 0.45f * t;

    // Slightly different dispersion coefficient per line avoids a static comb.
    lineL.apCoef = 0.64f;
    lineR.apCoef = 0.68f;
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

    // A little cross-bleed widens the tank without collapsing the centre.
    outL = sL + 0.15f * sR;
    outR = sR + 0.15f * sL;
}
} // namespace doobie
