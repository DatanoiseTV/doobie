// Minimal assertion-based tests for the DSP cores. No framework: each check
// prints on failure and the process returns non-zero so CTest flags it.
#include "dsp/DelayLine.h"
#include "dsp/Saturation.h"
#include "dsp/WowFlutter.h"
#include "dsp/PlateReverb.h"
#include "dsp/SpringReverb.h"

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

static void testPlateStability()
{
    doobie::PlateReverb plate;
    plate.prepare (48000.0);
    plate.setParams (0.95f, 0.8f, 0.3f, 20.0f, 0.5f);

    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    float maxOut = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float in = i < 4800 ? dist (rng) : 0.0f; // 0.1 s of noise, then silence
        float l, r;
        plate.process (in, in, l, r);
        check (finiteAndBounded (l, 50.0f) && finiteAndBounded (r, 50.0f), "Plate output stays finite/bounded");
        if (i > 24000)
            maxOut = std::fmax (maxOut, std::fmax (std::fabs (l), std::fabs (r)));
    }
    check (maxOut < 0.5f, "Plate tail decays after input stops");
}

static void testSpringStability()
{
    doobie::SpringReverb spring;
    spring.prepare (48000.0);
    spring.setParams (0.9f, 0.6f);

    std::mt19937 rng (2);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    float maxTail = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float in = i < 4800 ? dist (rng) : 0.0f;
        float l, r;
        spring.process (in, in, l, r);
        check (finiteAndBounded (l, 50.0f) && finiteAndBounded (r, 50.0f), "Spring output stays finite/bounded");
        if (i > 24000)
            maxTail = std::fmax (maxTail, std::fmax (std::fabs (l), std::fabs (r)));
    }
    check (maxTail < 0.5f, "Spring tail decays after input stops");
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
    testSaturation();
    testPlateStability();
    testSpringStability();
    testWowFlutter();

    if (failures == 0)
        std::printf ("All DSP tests passed.\n");
    return failures == 0 ? 0 : 1;
}
