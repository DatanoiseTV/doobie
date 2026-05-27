// Minimal assertion-based tests for the DSP cores. No framework: each check
// prints on failure and the process returns non-zero so CTest flags it.
#include "dsp/DelayLine.h"
#include "dsp/DcBlocker.h"
#include "dsp/Saturation.h"
#include "dsp/WowFlutter.h"
#include "dsp/PlateReverb.h"
#include "dsp/SpringReverb.h"
#include "dsp/FdnReverb.h"

#include <cmath>
#include <cstdio>
#include <random>

namespace
{
    int failures = 0;

    void check (bool cond, const char* what)
    {
        if (! cond)
        {
            std::printf ("FAIL: %s\n", what);
            ++failures;
        }
    }

    bool finiteAndBounded (float x, float bound) { return std::isfinite (x) && std::fabs (x) < bound; }
}

static void testDelayLine()
{
    doobie::DelayLine d;
    d.prepare (48000.0, 1.0);
    d.reset();

    d.write (1.0f);
    for (int i = 0; i < 100; ++i)
        d.write (0.0f);

    // The impulse is now 100 samples behind the newest sample.
    check (std::fabs (d.read (100.0f) - 1.0f) < 1.0e-3f, "DelayLine returns impulse at exact delay");
    check (std::fabs (d.read (50.0f)) < 1.0e-3f, "DelayLine reads silence between impulses");
}

static void testSaturation()
{
    doobie::Saturation s;
    s.reset();
    s.setDrive (1.0f);
    float maxOut = 0.0f;
    for (int i = 0; i < 1000; ++i)
        maxOut = std::fmax (maxOut, std::fabs (s.process (i % 2 == 0 ? 50.0f : -50.0f)));
    check (maxOut < 1.5f, "Saturation bounds a hot input");
}

static void testDcBlocker()
{
    constexpr double sr = 48000.0;

    // A constant DC input must be driven toward zero.
    doobie::DcBlocker dc;
    dc.prepare (sr);
    float out = 0.0f;
    for (int i = 0; i < (int) sr; ++i)   // 1 second to settle
        out = dc.process (1.0f);
    check (std::fabs (out) < 0.01f, "DcBlocker removes a constant DC offset");

    // A 1 kHz tone (well above the few-Hz cutoff) must pass at near unity gain.
    doobie::DcBlocker dc2;
    dc2.prepare (sr);
    float peak = 0.0f;
    for (int i = 0; i < (int) sr; ++i)
    {
        const float in = std::sin (6.2831853f * 1000.0f * (float) i / (float) sr);
        const float y  = dc2.process (in);
        if (i > (int) sr / 2)            // after the transient settles
            peak = std::fmax (peak, std::fabs (y));
    }
    check (peak > 0.97f && peak < 1.03f, "DcBlocker preserves audio-band signal");

    // A DC-biased tone keeps its swing but loses the offset (mean ~ 0).
    doobie::DcBlocker dc3;
    dc3.prepare (sr);
    double mean = 0.0;
    int counted = 0;
    for (int i = 0; i < (int) sr; ++i)
    {
        const float in = 0.5f + 0.3f * std::sin (6.2831853f * 200.0f * (float) i / (float) sr);
        const float y  = dc3.process (in);
        if (i > (int) sr / 2) { mean += y; ++counted; }
    }
    check (std::fabs (mean / counted) < 0.01f, "DcBlocker centres a DC-biased tone");
}

// Feeds a short noise burst then silence and confirms the reverb (a) never
// blows up and (b) has clearly decayed by the end. Comparing late-tail energy
// to early-tail energy keeps the test valid whatever the absolute decay time.
template <typename Reverb>
static void testReverbDecay (Reverb& rev)
{
    constexpr int sr = 48000;
    constexpr int total = sr * 3;        // 3 seconds
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    double early = 0.0, late = 0.0;
    for (int i = 0; i < total; ++i)
    {
        const float in = i < sr / 10 ? dist (rng) : 0.0f; // 0.1 s burst
        float l, r;
        rev.process (in, in, l, r);

        if (! (finiteAndBounded (l, 50.0f) && finiteAndBounded (r, 50.0f)))
        {
            check (false, "reverb stays finite/bounded");
            return;
        }

        const double e = (double) std::fabs (l) + (double) std::fabs (r);
        if (i >= sr / 10 && i < sr / 3)        // [0.1 s, 0.33 s] early tail
            early += e;
        else if (i >= sr * 5 / 2)              // [2.5 s, 3.0 s] late tail
            late += e;
    }
    check (early > 0.0 && late < 0.5 * early, "reverb tail decays after input stops");
}

static void testPlateStability()
{
    doobie::PlateReverb plate;
    plate.prepare (48000.0);
    plate.setParams (0.7f, 0.6f, 0.3f, 20.0f, 0.5f);
    testReverbDecay (plate);
}

static void testSpringStability()
{
    doobie::SpringReverb spring;
    spring.prepare (48000.0);
    spring.setParams (0.7f, 0.6f, 0.4f);
    testReverbDecay (spring);
}

static void testHallStability()
{
    doobie::FdnReverb hall;
    hall.prepare (48000.0, 40.0f, 115.0f);
    hall.setParams (0.7f, 0.7f, 0.4f, 20.0f, 0.4f);
    testReverbDecay (hall);
}


static void testWowFlutter()
{
    doobie::WowFlutter wf;
    wf.prepare (48000.0);
    wf.setAmounts (1.0f, 1.0f);
    float minF = 2.0f, maxF = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        const float f = wf.next();
        minF = std::fmin (minF, f);
        maxF = std::fmax (maxF, f);
    }
    // Modulation should stay a small deviation around 1.0.
    check (minF > 0.95f && maxF < 1.05f && maxF > 1.0f, "WowFlutter stays a small factor around unity");
}

int main()
{
    testDelayLine();
    testDcBlocker();
    testSaturation();
    testPlateStability();
    testSpringStability();
    testHallStability();
    testWowFlutter();

    if (failures == 0)
        std::printf ("All DSP tests passed.\n");
    return failures == 0 ? 0 : 1;
}
