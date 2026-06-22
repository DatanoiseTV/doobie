/*
  Doobie — analog dub delay  (Keinedelay/DFM hardware port)
  Copyright (C) 2026 DatanoiseTV

  GPL-3.0-or-later, WITHOUT ANY WARRANTY. Retain attribution to DatanoiseTV.
*/

#pragma once

#include <cmath>

namespace doobie
{
// Topology-preserving-transform state-variable filter (Zavalishin). One
// instance per channel; LP / HP / BP selected per sample from the same core,
// so sweeping the type can never blow up the state. setParams() is called per
// sample to track the smoothed cutoff (that's what keeps sweeps zipper-free);
// the stereo pair shares one tan() via copyCoefsFrom().
class Svf
{
  public:
    void prepare(double sampleRate)
    {
        sr_ = (float)sampleRate;
        reset();
    }

    void reset() { ic1_ = ic2_ = 0.0f; }

    // cutoff in Hz (clamped to a stable range), res 0..1 (soft-clamped below
    // self-oscillation so the input stage stays a mix tool, not a screamer).
    void setParams(float cutoffHz, float res) noexcept
    {
        if(cutoffHz < 20.0f)
            cutoffHz = 20.0f;
        const float maxHz = 0.45f * sr_;
        if(cutoffHz > maxHz)
            cutoffHz = maxHz;
        if(res < 0.0f)
            res = 0.0f;
        if(res > 0.95f)
            res = 0.95f;
        g_ = std::tan(3.14159265f * cutoffHz / sr_);
        k_ = 2.0f - 1.9f * res; // res 0 -> k 2 (Butterworth-ish), res 1 -> k 0.1
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    }

    // share one tan() between the stereo pair
    void copyCoefsFrom(const Svf& o) noexcept
    {
        g_  = o.g_;
        k_  = o.k_;
        a1_ = o.a1_;
    }

    // type: 0 LP, 1 HP, 2 BP
    inline float process(float in, int type) noexcept
    {
        const float v1 = a1_ * (ic1_ + g_ * (in - ic2_));
        const float v2 = ic2_ + g_ * v1;
        ic1_           = 2.0f * v1 - ic1_;
        ic2_           = 2.0f * v2 - ic2_;
        switch(type)
        {
            case 1: return in - k_ * v1 - v2; // HP
            case 2: return k_ * v1;           // BP (unity at resonance)
            default: return v2;               // LP
        }
    }

  private:
    float sr_ = 48000.0f;
    float g_ = 0.1f, k_ = 2.0f, a1_ = 1.0f;
    float ic1_ = 0.0f, ic2_ = 0.0f;
};
} // namespace doobie
