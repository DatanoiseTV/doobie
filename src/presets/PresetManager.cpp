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

#include "PresetManager.h"
#include "../ParameterIDs.h"

namespace
{
    using PV = std::pair<juce::String, float>;

    // ---- Head helpers ------------------------------------------------------
    PV ho (int i, bool on) { return { dID::headOn[(size_t) i], on ? 1.0f : 0.0f }; }
    PV hl (int i, float v) { return { dID::headLevel[(size_t) i], v }; }
    PV hp (int i, float v) { return { dID::headPan[(size_t) i],   v }; }
    PV hr (int i, float v) { return { dID::headRatio[(size_t) i], v }; }

    // ---- Reverb-mode mapping (HW -> Plugin) --------------------------------
    // HW index space (used by the hardware Keinedelay/DFM build):
    //   0=off, 1=spring, 2=plate, 3=spr>plt, 4=spr+plt, 5=hall, 6=gated, 7=shimmer
    // Plugin index space (this codebase, see ParameterIDs.h reverbModeChoices):
    //   0=off, 1=spring, 2=plate, 3=series, 4=parallel, 5=hall, 6=shimmer,
    //   7=convolution, 8=gated
    // 0..5 match; gated and shimmer swap slot.
    int hwRev (int hw) { return hw == 6 ? 8 : hw == 7 ? 6 : hw; }

    // ---- Tape-character helpers ------------------------------------------------
    // The hardware engine consumes its TONE knob as 0..1 (turned into a feedback
    // low-pass via `lpFreq = 400 * 40^tone` and a treble tilt of `(tone - 0.5)`).
    // Presets there set TONE in Hz via `tone(hz)`. Here we convert that Hz value
    // into the plugin's two underlying APVTS params (lpFreq + treble) so the
    // sonic intent ports 1:1.
    std::vector<PV> tone (float hz)
    {
        const float t = std::log (hz / 400.0f) / std::log (40.0f);
        const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return {
            { dID::lpFreq, hz },
            { dID::treble, clamped - 0.5f },
        };
    }

    // singleHead matches the HW helper: head 0 only, full ratio, centred. HW
    // default DubParams has head 3 on too -- this matters here because the
    // plugin's default state has just head 0 on, so we must explicitly
    // disable the others in every preset that uses singleHead to keep the
    // sonic intent identical to the hardware build.
    std::vector<PV> singleHead()
    {
        return { ho (0, true),  hl (0, 0.9f), hr (0, 1.0f), hp (0, 0.0f),
                 ho (1, false), ho (2, false), ho (3, false) };
    }

    // ---- Mod-matrix helpers -----------------------------------------------
    // Source / dest indices follow doobie::ModSource / doobie::ModDest in
    // src/dsp/ModMatrix.h. We use ints here because PresetManager values are
    // floats stored as ints in the APVTS choice params.
    enum { SrcOff = 0, SrcLfo1 = 1, SrcLfo2 = 2, SrcEnv = 3 };
    // Plugin ModDest indices (must mirror src/dsp/ModMatrix.h's enum order).
    // The hardware appends 7 destinations the plugin didn't have; we keep the
    // same names + add them in the same order at the end of the enum.
    enum {
        DstOff = 0, DstDelayTime, DstFeedback, DstMix, DstWidth, DstDuck,
        DstDrive, DstWow, DstFlutter, DstAge,
        DstPreHpFreq, DstPreLpFreq, DstHpFreq, DstLpFreq,
        DstBass, DstTreble,
        DstHead1Level, DstHead2Level, DstHead3Level, DstHead4Level,
        DstReverbMix, DstReverbMod,
        DstPlateDecay, DstPlateSize, DstPlateDamp, DstPlatePredelay,
        DstSpringDecay, DstSpringTone, DstIRGain,
        DstHead1Pan, DstHead2Pan, DstHead3Pan, DstHead4Pan,
        DstHead1Ratio, DstHead2Ratio, DstHead3Ratio, DstHead4Ratio,
        // Appended for the hardware port:
        DstInFilterCutoff, DstInFilterRes,
        DstPan, DstOutLevel,
        DstPhaserRate, DstPhaserDepth, DstPhaserMix,
    };

    // Per-slot helpers. slot1 is the first user slot, slot2 the second, etc.
    // Plugin amount is stored 0..1 with 0.5 = bipolar centre, so we map
    // -1..+1 -> 0..1 here.
    PV slotSrc (int slot, int src) { return { dID::modSlotSrc[(size_t) slot], (float) src }; }
    PV slotDst (int slot, int dst) { return { dID::modSlotDst[(size_t) slot], (float) dst }; }
    PV slotAmt (int slot, float a) { return { dID::modSlotAmt[(size_t) slot], a }; }

    std::vector<PV> mod (int slot, int src, int dst, float amt)
    {
        return { slotSrc (slot, src), slotDst (slot, dst), slotAmt (slot, amt) };
    }
    PV lfoRate  (int i, float hz)  { return { i == 0 ? dID::lfo1Rate  : dID::lfo2Rate,  hz }; }
    PV lfoDepth (int i, float d)   { return { i == 0 ? dID::lfo1Depth : dID::lfo2Depth, d }; }
    PV lfoWave  (int i, int wave)  { return { i == 0 ? dID::lfo1Wave  : dID::lfo2Wave,  (float) wave }; }
    PV envAtk   (float ms)         { return { dID::envAttack,  ms }; }
    PV envRel   (float ms)         { return { dID::envRelease, ms }; }
    PV envSens  (float v)          { return { dID::envSens,    v }; }

    // Concatenate helper.
    std::vector<PV> operator+ (std::vector<PV> a, const std::vector<PV>& b)
    {
        a.insert (a.end(), b.begin(), b.end());
        return a;
    }
    std::vector<PV>& operator+= (std::vector<PV>& a, const std::vector<PV>& b)
    {
        a.insert (a.end(), b.begin(), b.end());
        return a;
    }

    // ---- The factory bank ---------------------------------------------------
    // Ported from /Users/syso/dev/Keinedelay2.1_DFM/src/dub/Presets.cpp (the
    // hardware Keinedelay/DFM build). 64 curated presets exercising every
    // corner of the engine: dub voices, ambient washes, modulated filters,
    // phaser routings, frozen pads, gated 80s, octave shimmer.
    //
    // Each entry only lists overrides; PresetManager::applyPreset resets
    // every APVTS parameter to its default first. Helpers above map the
    // hardware preset vocabulary (singleHead, tone(Hz), mod()) onto the
    // plugin's APVTS IDs.
    std::vector<PresetManager::Preset> buildFactory()
    {
        std::vector<PresetManager::Preset> out;
        const auto P = [&] (juce::String name, std::vector<PV> v)
        { out.push_back ({ std::move (name), std::move (v) }); };

        // 0: Classic Dub
        P ("Classic Dub", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.68f },
            { dID::drive, 0.45f }, { dID::hiss, 0.22f }, { dID::wow, 0.2f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.40f }, { dID::springDecay, 0.6f },
            { dID::mix, 0.42f },
        } + singleHead() + tone (2800.0f));

        // 1: King Tubby
        P ("King Tubby", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.78f },
            { dID::drive, 0.55f }, { dID::hiss, 0.35f }, { dID::wow, 0.3f },
            { dID::hpFreq, 180.0f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.45f }, { dID::springDecay, 0.65f },
            { dID::mix, 0.42f },
        } + singleHead() + tone (2300.0f));

        // 2: Steppers
        P ("Steppers", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.6f }, { dID::pingPong, 1 },
            ho (0, true),  hl (0, 0.9f), hr (0, 1.0f),  hp (0, -0.4f),
            ho (1, false),
            ho (2, true),  hl (2, 0.7f), hr (2, 0.5f),  hp (2,  0.4f),
            ho (3, false),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.4f },
        } + tone (3400.0f));

        // 3: Sub Echo
        P ("Sub Echo", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.6f },
            { dID::hpFreq, 60.0f }, { dID::drive, 0.4f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.32f }, { dID::plateDecay, 0.5f },
            { dID::mix, 0.4f },
        } + singleHead() + tone (2600.0f));

        // 4: Ambient Wash
        P ("Ambient Wash", std::vector<PV>{
            { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::feedback, 0.55f },
            { dID::reverbMode, (float) hwRev (4) }, { dID::reverbRoute, 1 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.85f },
            { dID::plateSize, 0.8f }, { dID::reverbMod, 0.4f },
            { dID::width, 1.3f },
            ho (0, true), ho (1, false), ho (2, false), ho (3, true),
            { dID::mix, 0.6f },
        } + tone (8000.0f));

        // 5: Frozen
        P ("Frozen", std::vector<PV>{
            { dID::freeze, 1 },
            { dID::reverbMode, (float) hwRev (4) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.9f },
            { dID::plateSize, 0.92f }, { dID::reverbMod, 0.6f },
            { dID::width, 1.4f }, { dID::mix, 0.7f },
        });

        // 6: Cathedral
        P ("Cathedral", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 16 }, { dID::timeMs, 2000.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.4f },
            { dID::reverbMode, (float) hwRev (3) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::springDecay, 0.92f }, { dID::plateDecay, 0.92f },
            { dID::plateSize, 0.9f },
            { dID::mix, 0.55f },
        } + singleHead() + tone (6000.0f));

        // 7: Glacier Hall
        P ("Glacier Hall", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.4f },
            { dID::reverbMode, (float) hwRev (5) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.9f },
            { dID::plateDamp, 0.2f }, { dID::reverbMod, 0.35f },
            { dID::width, 1.4f }, { dID::mix, 0.5f },
        } + singleHead() + tone (9000.0f));

        // 8: Pristine
        P ("Pristine", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::feedback, 0.5f }, { dID::pingPong, 1 },
            { dID::wow, 0.0f }, { dID::flutter, 0.0f },
            { dID::reverbMode, 0 }, { dID::mix, 0.35f },
        } + singleHead() + tone (12000.0f));

        // 9: Quarter Note
        P ("Quarter Note", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.5f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.22f }, { dID::width, 1.3f }, { dID::mix, 0.4f },
        } + singleHead() + tone (10000.0f));

        // 10: Triplet Fan
        P ("Triplet Fan", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 8 }, { dID::timeMs, 333.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.5f },
            ho (0, true), hl (0, 0.85f), hr (0, 1.0f),  hp (0, -0.5f),
            ho (1, true), hl (1, 0.7f),  hr (1, 0.66f), hp (1,  0.0f),
            ho (2, true), hl (2, 0.6f),  hr (2, 0.33f), hp (2,  0.5f),
            ho (3, false),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.28f }, { dID::width, 1.2f }, { dID::mix, 0.42f },
        });

        // 11: Vintage Tape
        P ("Vintage Tape", std::vector<PV>{
            { dID::timeMs, 320.0f }, { dID::syncMode, 1 }, { dID::syncDiv, 8 },
            { dID::delayMode, 1 }, { dID::feedback, 0.5f },
            { dID::wow, 0.55f }, { dID::flutter, 0.5f }, { dID::drive, 0.6f },
            { dID::hiss, 0.5f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbMix, 0.3f },
            { dID::mix, 0.4f },
        } + singleHead() + tone (2600.0f));

        // 12: Broken Tape
        P ("Broken Tape", std::vector<PV>{
            { dID::timeMs, 380.0f }, { dID::syncMode, 1 }, { dID::syncDiv, 9 },
            { dID::delayMode, 1 }, { dID::feedback, 0.5f },
            { dID::wow, 0.9f }, { dID::flutter, 0.7f }, { dID::drive, 0.6f }, { dID::hiss, 0.9f },
            { dID::preLpFreq, 4000.0f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbMix, 0.3f },
            { dID::mix, 0.42f },
        } + singleHead() + tone (2400.0f));

        // 13: BBD Brigade
        P ("BBD Brigade", std::vector<PV>{
            { dID::delayMode, 2 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::feedback, 0.62f }, { dID::wow, 0.3f }, { dID::flutter, 0.25f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbMix, 0.3f },
            { dID::mix, 0.4f },
        } + singleHead() + tone (3200.0f));

        // 14: Gated 80s
        P ("Gated 80s", std::vector<PV>{
            { dID::timeMs, 120.0f }, { dID::syncMode, 1 }, { dID::syncDiv, 4 },
            { dID::delayMode, 0 }, { dID::feedback, 0.18f },
            { dID::reverbMode, (float) hwRev (6) /* gated */ }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.7f },
            { dID::plateDamp, 0.2f },
            { dID::gateThreshold, -26.0f }, { dID::gateHold, 160.0f }, { dID::gateRelease, 8.0f },
            { dID::mix, 0.5f },
        } + singleHead() + tone (9000.0f));

        // 15: Wide Bouncer
        P ("Wide Bouncer", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.55f }, { dID::pingPong, 1 },
            { dID::width, 1.6f },
            ho (0, true), hl (0, 0.9f), hr (0, 1.0f), hp (0, -0.9f),
            ho (1, false), ho (2, false),
            ho (3, true), hl (3, 0.8f), hr (3, 0.5f), hp (3,  0.9f),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.42f },
        } + tone (5000.0f));

        // 16: Shimmer
        P ("Shimmer", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.45f },
            { dID::reverbMode, (float) hwRev (7) /* shimmer */ }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.8f },
            { dID::plateDamp, 0.4f }, { dID::reverbMod, 0.5f },
            { dID::width, 1.3f }, { dID::mix, 0.5f },
        } + singleHead() + tone (6000.0f));

        // 17: Octave Up
        P ("Octave Up", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 4 }, { dID::feedback, 0.55f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.42f },
        } + singleHead() + tone (5000.0f));

        // 18: Auto Wah
        P ("Auto Wah", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.45f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 2 },
            { dID::inFilterCutoff, 500.0f }, { dID::inFilterRes, 0.6f },
            envAtk (4.0f), envRel (160.0f), envSens (18.0f),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcEnv, DstInFilterCutoff, 0.7f));

        // 19: Filter Sweep
        P ("Filter Sweep", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.66f }, { dID::drive, 0.4f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 1600.0f }, { dID::inFilterRes, 0.45f },
            lfoRate (0, 0.07f), lfoDepth (0, 1.0f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.35f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3000.0f) + mod (0, SrcLfo1, DstInFilterCutoff, 0.8f));

        // 20: Phase Echo
        P ("Phase Echo", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.55f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 0 }, { dID::phaserRate, 0.3f },
            { dID::phaserDepth, 0.8f }, { dID::phaserFb, 0.5f }, { dID::phaserMix, 0.6f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f },
            { dID::width, 1.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (5000.0f));

        // 21: Phaser Bloom
        P ("Phaser Bloom", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.7f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 2 }, { dID::phaserRate, 0.15f },
            { dID::phaserDepth, 0.9f }, { dID::phaserFb, 0.35f }, { dID::phaserMix, 0.5f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f },
            { dID::width, 1.2f }, { dID::mix, 0.5f },
        } + singleHead() + tone (3600.0f));

        // 22: Auto Pan
        P ("Auto Pan", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.5f },
            lfoRate (0, 0.4f), lfoWave (0, 1),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f },
            { dID::width, 1.2f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcLfo1, DstPan, 0.9f));

        // 23: Tremolo
        P ("Tremolo", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.45f },
            lfoRate (0, 5.0f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.5f },
        } + singleHead() + tone (4500.0f) + mod (0, SrcLfo1, DstOutLevel, 0.5f));

        // 24: Pulse Chop
        P ("Pulse Chop", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::feedback, 0.45f }, { dID::pingPong, 1 },
            { dID::wow, 0.0f }, { dID::flutter, 0.0f },
            lfoRate (0, 6.0f), lfoWave (0, 4),
            { dID::reverbMode, 0 }, { dID::width, 1.2f }, { dID::mix, 0.5f },
        } + singleHead() + tone (9000.0f) + mod (0, SrcLfo1, DstOutLevel, 0.9f));

        // 25: Wobbler
        P ("Wobbler", std::vector<PV>{
            { dID::timeMs, 300.0f }, { dID::syncMode, 1 }, { dID::syncDiv, 8 },
            { dID::delayMode, 1 }, { dID::feedback, 0.55f },
            { dID::wow, 0.4f }, { dID::flutter, 0.3f }, { dID::drive, 0.5f },
            lfoRate (0, 0.6f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3000.0f) + mod (0, SrcLfo1, DstDelayTime, 0.35f));

        // 26: Liquid Filter
        P ("Liquid Filter", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.66f }, { dID::drive, 0.4f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 800.0f }, { dID::inFilterRes, 0.5f },
            lfoRate (0, 0.10f), lfoWave (0, 0),
            lfoRate (1, 0.23f), lfoWave (1, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3500.0f)
          + mod (0, SrcLfo1, DstInFilterCutoff, 0.85f)
          + mod (1, SrcLfo2, DstInFilterRes,    0.4f));

        // 27: Dyna Pan
        P ("Dyna Pan", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.5f },
            envAtk (6.0f), envRel (220.0f), envSens (14.0f),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f },
            { dID::width, 1.2f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4500.0f) + mod (0, SrcEnv, DstPan, 0.8f));

        // 28: Phaser Drift
        P ("Phaser Drift", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.55f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 0 }, { dID::phaserRate, 0.3f },
            { dID::phaserDepth, 0.5f }, { dID::phaserFb, 0.45f }, { dID::phaserMix, 0.6f },
            lfoRate (0, 0.10f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f },
            { dID::width, 1.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (5000.0f) + mod (0, SrcLfo1, DstPhaserDepth, 0.5f));

        // 29: Stereo Bloom
        P ("Stereo Bloom", std::vector<PV>{
            { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::feedback, 0.55f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 1 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.8f }, { dID::plateSize, 0.85f },
            { dID::reverbMod, 0.3f },
            ho (0, true), ho (1, false), ho (2, false), ho (3, true),
            { dID::width, 1.4f },
            lfoRate (0, 0.08f), lfoWave (0, 1),
            lfoRate (1, 0.15f), lfoWave (1, 0),
            { dID::mix, 0.6f },
        } + tone (8000.0f)
          + mod (0, SrcLfo1, DstPan,       0.7f)
          + mod (1, SrcLfo2, DstReverbMod, 0.5f));

        // 30: Shimmer Drift
        P ("Shimmer Drift", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.45f },
            { dID::reverbMode, (float) hwRev (7) /* shimmer */ }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.8f },
            { dID::plateDamp, 0.4f }, { dID::reverbMod, 0.5f },
            { dID::width, 1.3f },
            lfoRate (0, 0.06f), lfoWave (0, 1),
            { dID::mix, 0.5f },
        } + singleHead() + tone (6000.0f) + mod (0, SrcLfo1, DstPan, 0.6f));

        // 31: Nebula
        P ("Nebula", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 4 }, { dID::feedback, 0.5f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 4000.0f }, { dID::inFilterRes, 0.3f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.75f }, { dID::plateSize, 0.85f },
            { dID::plateDamp, 0.25f }, { dID::reverbMod, 0.35f },
            { dID::width, 1.4f },
            lfoRate (0, 0.05f), lfoWave (0, 1),
            lfoRate (1, 0.13f), lfoWave (1, 0),
            { dID::mix, 0.55f },
        } + singleHead() + tone (6000.0f)
          + mod (0, SrcLfo1, DstPan,             0.7f)
          + mod (1, SrcLfo2, DstInFilterCutoff,  0.6f));

        // 32: Dub Siren
        P ("Dub Siren", std::vector<PV>{
            { dID::delayMode, 2 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::feedback, 0.72f }, { dID::wow, 0.2f },
            lfoRate (0, 1.2f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.35f }, { dID::mix, 0.45f },
        } + singleHead() + tone (2600.0f) + mod (0, SrcLfo1, DstDelayTime, 0.12f));

        // 33: Space Echo
        P ("Space Echo", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 11 }, { dID::timeMs, 600.0f },
            { dID::feedback, 0.6f }, { dID::wow, 0.3f }, { dID::flutter, 0.25f }, { dID::drive, 0.5f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.35f }, { dID::plateDecay, 0.55f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3000.0f));

        // 34: Echo Chamber
        P ("Echo Chamber", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 750.0f },
            { dID::feedback, 0.55f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 1 },
            { dID::reverbMix, 0.45f }, { dID::plateDecay, 0.7f }, { dID::plateSize, 0.8f },
            { dID::width, 1.4f }, { dID::mix, 0.5f },
        } + singleHead() + tone (4000.0f));

        // 35: Self Osc
        P ("Self Osc", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.95f }, { dID::drive, 0.4f },
            lfoRate (0, 0.2f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.4f },
        } + singleHead() + tone (3000.0f) + mod (0, SrcLfo1, DstFeedback, 0.2f));

        // 36: Endless Pad
        P ("Endless Pad", std::vector<PV>{
            { dID::freeze, 1 },
            { dID::reverbMode, (float) hwRev (7) /* shimmer */ }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.9f }, { dID::plateSize, 0.9f },
            { dID::plateDamp, 0.4f }, { dID::reverbMod, 0.5f },
            { dID::width, 1.4f },
            lfoRate (0, 0.05f), lfoWave (0, 1),
            { dID::mix, 0.7f },
        } + mod (0, SrcLfo1, DstPan, 0.6f));

        // 37: Aurora
        P ("Aurora", std::vector<PV>{
            { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 16 }, { dID::timeMs, 2000.0f },
            { dID::feedback, 0.5f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 1 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.85f },
            { dID::reverbMod, 0.4f },
            { dID::width, 1.4f },
            lfoRate (0, 0.04f), lfoWave (0, 1),
            lfoRate (1, 0.07f), lfoWave (1, 0),
            ho (0, true), ho (1, false), ho (2, false), ho (3, true),
            { dID::mix, 0.62f },
        } + tone (9000.0f)
          + mod (0, SrcLfo1, DstPan,       0.7f)
          + mod (1, SrcLfo2, DstPlateSize, 0.4f));

        // 38: Drone
        P ("Drone", std::vector<PV>{
            { dID::freeze, 1 },
            { dID::reverbMode, (float) hwRev (5) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.9f }, { dID::plateSize, 0.95f },
            { dID::plateDamp, 0.2f }, { dID::reverbMod, 0.5f },
            { dID::width, 1.4f },
            lfoRate (0, 0.08f), lfoWave (0, 0),
            { dID::mix, 0.7f },
        } + mod (0, SrcLfo1, DstReverbMod, 0.5f));

        // 39: Whale Song
        P ("Whale Song", std::vector<PV>{
            { dID::delayMode, 4 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::feedback, 0.5f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.8f }, { dID::plateSize, 0.9f },
            { dID::width, 1.3f },
            lfoRate (0, 0.15f), lfoWave (0, 0),
            { dID::mix, 0.55f },
        } + singleHead() + tone (5000.0f) + mod (0, SrcLfo1, DstDelayTime, 0.4f));

        // 40: Trance Gate
        P ("Trance Gate", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 6 }, { dID::timeMs, 180.0f },
            { dID::feedback, 0.4f }, { dID::pingPong, 1 },
            { dID::wow, 0.0f }, { dID::flutter, 0.0f },
            lfoRate (0, 8.0f), lfoWave (0, 4),
            { dID::reverbMode, 0 }, { dID::width, 1.2f }, { dID::mix, 0.5f },
        } + singleHead() + tone (11000.0f) + mod (0, SrcLfo1, DstOutLevel, 1.0f));

        // 41: Helicopter
        P ("Helicopter", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::feedback, 0.45f },
            lfoRate (0, 4.0f), lfoWave (0, 1),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.2f }, { dID::width, 1.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (8000.0f) + mod (0, SrcLfo1, DstPan, 1.0f));

        // 42: Random Filter
        P ("Random Filter", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 8 }, { dID::timeMs, 333.0f },
            { dID::feedback, 0.55f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 1500.0f }, { dID::inFilterRes, 0.6f },
            lfoRate (0, 4.0f), lfoWave (0, 5),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcLfo1, DstInFilterCutoff, 0.8f));

        // 43: Sputter
        P ("Sputter", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 200.0f },
            { dID::feedback, 0.4f },
            envAtk (4.0f), envRel (180.0f), envSens (16.0f),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcEnv, DstFeedback, 0.4f));

        // 44: Acid
        P ("Acid", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 8 }, { dID::timeMs, 300.0f },
            { dID::feedback, 0.6f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 2 },
            { dID::inFilterCutoff, 700.0f }, { dID::inFilterRes, 0.75f },
            lfoRate (0, 0.5f), lfoWave (0, 3),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3500.0f) + mod (0, SrcLfo1, DstInFilterCutoff, 0.9f));

        // 45: Vox
        P ("Vox", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::timeMs, 250.0f },
            { dID::feedback, 0.45f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 2 },
            { dID::inFilterCutoff, 600.0f }, { dID::inFilterRes, 0.7f },
            envAtk (5.0f), envRel (150.0f), envSens (18.0f),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4500.0f) + mod (0, SrcEnv, DstInFilterCutoff, 0.75f));

        // 46: Downsweep
        P ("Downsweep", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.66f }, { dID::drive, 0.4f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 3000.0f }, { dID::inFilterRes, 0.45f },
            lfoRate (0, 0.3f), lfoWave (0, 3),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (3000.0f) + mod (0, SrcLfo1, DstInFilterCutoff, 0.85f));

        // 47: Twin Sweep
        P ("Twin Sweep", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.6f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 1800.0f }, { dID::inFilterRes, 0.5f },
            lfoRate (0, 0.11f), lfoWave (0, 0),
            lfoRate (1, 0.17f), lfoWave (1, 0),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f)
          + mod (0, SrcLfo1, DstInFilterCutoff, 0.7f)
          + mod (1, SrcLfo2, DstInFilterCutoff, 0.5f));

        // 48: Jet Phaser (character 5 = NONE -> delayBypass in plugin)
        P ("Jet Phaser", std::vector<PV>{
            { dID::delayBypass, 1 }, { dID::feedback, 0.0f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 0 }, { dID::phaserRate, 0.8f },
            { dID::phaserDepth, 0.9f }, { dID::phaserFb, 0.6f }, { dID::phaserMix, 0.7f },
            { dID::reverbMode, 0 }, { dID::mix, 0.6f },
        } + singleHead());

        // 49: Phaser Wash
        P ("Phaser Wash", std::vector<PV>{
            { dID::delayBypass, 1 }, { dID::feedback, 0.0f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 0 }, { dID::phaserRate, 0.2f },
            { dID::phaserDepth, 0.8f }, { dID::phaserFb, 0.4f }, { dID::phaserMix, 0.6f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.8f }, { dID::plateSize, 0.85f },
            { dID::width, 1.3f }, { dID::mix, 0.6f },
        } + singleHead());

        // 50: Univibe
        P ("Univibe", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::feedback, 0.5f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 0 }, { dID::phaserRate, 0.5f },
            { dID::phaserDepth, 0.7f }, { dID::phaserFb, 0.3f }, { dID::phaserMix, 0.6f },
            lfoRate (0, 0.1f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.2f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcLfo1, DstPhaserRate, 0.4f));

        // 51: Flange Echo
        P ("Flange Echo", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::feedback, 0.55f },
            { dID::phaserOn, 1 }, { dID::phaserRoute, 2 }, { dID::phaserRate, 0.25f },
            { dID::phaserDepth, 0.9f }, { dID::phaserFb, 0.7f }, { dID::phaserMix, 0.7f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::width, 1.2f }, { dID::mix, 0.45f },
        } + singleHead() + tone (5000.0f));

        // 52: Warped
        P ("Warped", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 8 }, { dID::timeMs, 350.0f },
            { dID::feedback, 0.55f },
            { dID::wow, 0.6f }, { dID::flutter, 0.5f }, { dID::drive, 0.5f }, { dID::hiss, 0.4f },
            lfoRate (0, 0.3f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (2600.0f) + mod (0, SrcLfo1, DstWow, 0.5f));

        // 53: Cassette
        P ("Cassette", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 375.0f },
            { dID::feedback, 0.5f },
            { dID::wow, 0.35f }, { dID::flutter, 0.3f }, { dID::hiss, 0.55f }, { dID::drive, 0.45f },
            { dID::preLpFreq, 5000.0f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.42f },
        } + singleHead() + tone (2400.0f));

        // 54: Melted
        P ("Melted", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::timeMs, 400.0f },
            { dID::feedback, 0.5f },
            { dID::wow, 0.7f }, { dID::flutter, 0.6f }, { dID::hiss, 0.7f }, { dID::drive, 0.6f },
            { dID::preLpFreq, 3500.0f },
            lfoRate (0, 0.4f), lfoWave (0, 0),
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::mix, 0.45f },
        } + singleHead() + tone (2200.0f) + mod (0, SrcLfo1, DstAge, 0.4f));

        // 55: Sun Ra
        P ("Sun Ra", std::vector<PV>{
            { dID::delayMode, 4 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.6f },
            lfoRate (0, 0.5f), lfoWave (0, 1),
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.35f }, { dID::width, 1.2f }, { dID::mix, 0.45f },
        } + singleHead() + tone (4000.0f) + mod (0, SrcLfo1, DstDelayTime, 0.2f));

        // 56: Octaver
        P ("Octaver", std::vector<PV>{
            { dID::delayMode, 4 }, { dID::syncMode, 1 }, { dID::syncDiv, 8 }, { dID::timeMs, 300.0f },
            { dID::feedback, 0.4f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::mix, 0.45f },
        } + singleHead() + tone (6000.0f));

        // 57: Risset
        P ("Risset", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 900.0f },
            { dID::feedback, 0.42f },
            { dID::reverbMode, (float) hwRev (7) /* shimmer */ }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.72f }, { dID::plateSize, 0.8f },
            { dID::plateDamp, 0.4f }, { dID::reverbMod, 0.45f },
            { dID::width, 1.3f }, { dID::mix, 0.5f },
        } + singleHead() + tone (6000.0f));

        // 58: Glass
        P ("Glass", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::feedback, 0.5f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 1 },
            { dID::inFilterCutoff, 200.0f }, { dID::inFilterRes, 0.2f },
            { dID::reverbMode, (float) hwRev (4) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.4f }, { dID::plateDecay, 0.7f }, { dID::plateSize, 0.7f },
            { dID::width, 1.3f },
            lfoRate (0, 0.2f), lfoWave (0, 0),
            { dID::mix, 0.45f },
        } + singleHead() + tone (12000.0f) + mod (0, SrcLfo1, DstTreble, 0.4f));

        // 59: Slapback
        P ("Slapback", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 3 }, { dID::timeMs, 110.0f },
            { dID::feedback, 0.15f }, { dID::wow, 0.12f }, { dID::drive, 0.4f },
            { dID::reverbMode, 0 }, { dID::mix, 0.4f },
        } + singleHead() + tone (5000.0f));

        // 60: Doubler
        P ("Doubler", std::vector<PV>{
            { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 2 }, { dID::timeMs, 45.0f },
            { dID::feedback, 0.1f },
            { dID::reverbMode, 0 }, { dID::width, 1.5f }, { dID::mix, 0.4f },
        } + singleHead() + tone (11000.0f));

        // 61: Nashville
        P ("Nashville", std::vector<PV>{
            { dID::delayMode, 1 }, { dID::syncMode, 1 }, { dID::syncDiv, 4 }, { dID::timeMs, 130.0f },
            { dID::feedback, 0.2f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.3f }, { dID::springDecay, 0.4f }, { dID::mix, 0.4f },
        } + singleHead() + tone (6000.0f));

        // 62: Swell
        P ("Swell", std::vector<PV>{
            { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::feedback, 0.55f }, { dID::duck, 0.6f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 1 },
            { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.8f }, { dID::plateSize, 0.85f },
            { dID::width, 1.3f },
            ho (0, true), ho (1, false), ho (2, false), ho (3, true),
            { dID::mix, 0.55f },
        } + tone (8000.0f));

        // 63: Infinity
        P ("Infinity", std::vector<PV>{
            { dID::freeze, 1 }, { dID::delayMode, 1 },
            { dID::reverbMode, (float) hwRev (5) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.9f }, { dID::plateSize, 0.95f },
            { dID::plateDamp, 0.25f },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 0 },
            { dID::inFilterCutoff, 4000.0f }, { dID::inFilterRes, 0.3f },
            { dID::width, 1.4f },
            lfoRate (0, 0.06f), lfoWave (0, 0),
            { dID::mix, 0.7f },
        } + mod (0, SrcLfo1, DstInFilterCutoff, 0.5f));

        // ====================================================================
        // 64..79: Live Vocal FX bank. Designed for solo vocal — short slaps for
        // body, plate / hall / shimmer for ambience, pitch tricks for harmony,
        // a MIDI-playable harmoniser, and movement patches that the mod matrix
        // animates. Defaults assume the dry vocal lands at -12 dBFS-ish; most
        // patches sit at mix 0.30..0.45 so they sweeten without burying the
        // source. Cleaned input via a gentle high-pass at 80–120 Hz to keep
        // breath / plosives out of the wet.
        // ====================================================================

        // 64: Vox Slap — classic ADT-style slapback for body and width
        P ("Vox Slap", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 110.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.18f },
            { dID::preHpFreq, 90.0f }, { dID::lpFreq, 7000.0f },
            { dID::drive, 0.18f }, { dID::wow, 0.10f }, { dID::flutter, 0.05f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.10f }, { dID::plateDecay, 0.30f }, { dID::plateSize, 0.35f },
            { dID::width, 1.3f }, { dID::mix, 0.30f },
        } + singleHead() + tone (5500.0f));

        // 65: Vox Doubler — slapback at unison with the two playback heads
        // panned hard for a wider, double-tracked feel. Digital character
        // (no pitching) — for a detune-thicken version use "Stereo Doubler"
        // below (preset 76).
        P ("Vox Doubler", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 42.0f },
            { dID::delayMode, 0 },                               // Digital
            { dID::feedback, 0.10f },
            { dID::preHpFreq, 100.0f }, { dID::lpFreq, 9000.0f },
            // Two heads, slightly different times + opposite pans = haas.
            ho (0, true),  hl (0, 0.85f), hr (0, 1.00f), hp (0, -0.4f),
            ho (1, true),  hl (1, 0.80f), hr (1, 0.92f), hp (1,  0.4f),
            ho (2, false), ho (3, false),
            // 6 ms offset between heads pulls them off the unison grid for
            // micro-doubling stereo width without combing.
            { dID::headOffset[0], -3.0f }, { dID::headOffset[1], 3.0f },
            { dID::reverbMode, 0 },
            { dID::width, 1.5f }, { dID::mix, 0.42f },
        });

        // 66: Vox Hall — lush long hall behind the vocal. No delay tail,
        // pure ambience; the reverb size + decay carry it.
        P ("Vox Hall", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 80.0f },
            { dID::delayMode, 0 }, { dID::feedback, 0.0f }, { dID::delayBypass, 1 },
            { dID::preHpFreq, 90.0f },
            { dID::reverbMode, (float) hwRev (5) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.45f }, { dID::plateDecay, 0.82f }, { dID::plateSize, 0.88f },
            { dID::plateDamp, 0.35f }, { dID::platePredelay, 30.0f },
            { dID::reverbMod, 0.12f },
            { dID::width, 1.5f }, { dID::mix, 0.40f },
        } + singleHead());

        // 67: Vox Plate Vintage — bright plate, classic 80s vocal sound
        // (Padgham / Lillywhite era). Bigger predelay so the dry stays
        // forward.
        P ("Vox Plate", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::preHpFreq, 110.0f }, { dID::treble, 0.15f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.42f }, { dID::plateDecay, 0.65f }, { dID::plateSize, 0.55f },
            { dID::plateDamp, 0.20f }, { dID::platePredelay, 45.0f },
            { dID::reverbMod, 0.05f },
            { dID::width, 1.2f }, { dID::mix, 0.38f },
        } + singleHead());

        // 68: Vox Shimmer Soar — octave-up shimmer for ethereal lifts on
        // long held notes. Moderate predelay so the dry attack lands clean.
        P ("Vox Shimmer", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::preHpFreq, 100.0f },
            { dID::reverbMode, (float) hwRev (7) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.40f }, { dID::plateDecay, 0.78f }, { dID::plateSize, 0.85f },
            { dID::plateDamp, 0.35f }, { dID::platePredelay, 40.0f },
            { dID::reverbMod, 0.20f }, { dID::shimmerSemis, 12.0f },
            { dID::width, 1.5f }, { dID::mix, 0.32f },
        } + singleHead());

        // 69: Dub Vox Echo — tempo-synced 1/4 dub delay for reggae /
        // electronic vocal chops. In-feedback spring for the wash.
        P ("Dub Vox Echo", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::timeMs, 500.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.62f },
            { dID::preHpFreq, 120.0f }, { dID::drive, 0.32f },
            { dID::wow, 0.18f }, { dID::flutter, 0.08f }, { dID::hiss, 0.15f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.38f }, { dID::springDecay, 0.55f },
            { dID::mix, 0.36f },
        } + singleHead() + tone (2400.0f));

        // 70: Whisper Wash — soft, breathy pad behind whispered vocals.
        // Diffuse character smears the repeats so individual hits are
        // indistinguishable.
        P ("Whisper Wash", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 380.0f },
            { dID::delayMode, 3 }, { dID::feedback, 0.55f },
            { dID::preHpFreq, 200.0f }, { dID::lpFreq, 4500.0f },
            { dID::reverbMode, (float) hwRev (4) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.50f }, { dID::plateDecay, 0.75f }, { dID::plateSize, 0.80f },
            { dID::springDecay, 0.40f }, { dID::reverbMod, 0.15f },
            { dID::width, 1.4f }, { dID::mix, 0.55f },
        } + singleHead());

        // 71: Tape Vox — warm tape echo behind the vocal, the Sun-Studio /
        // Elvis sound. Single repeat with tape character bringing the colour.
        P ("Tape Vox", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 140.0f },
            { dID::delayMode, 1 }, { dID::feedback, 0.32f },
            { dID::preHpFreq, 110.0f },
            { dID::drive, 0.40f }, { dID::wow, 0.28f }, { dID::flutter, 0.18f },
            { dID::hiss, 0.32f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.18f }, { dID::springDecay, 0.40f },
            { dID::mix, 0.38f },
        } + singleHead() + tone (3800.0f));

        // 72: Telephone Vox — narrow band-pass + tape grit for old-radio /
        // dispatch effects. Use as a lyrical contrast or for spoken-word
        // sections.
        P ("Telephone Vox", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 2 },     // BP
            { dID::inFilterCutoff, 1400.0f }, { dID::inFilterRes, 0.55f },
            { dID::drive, 0.55f }, { dID::hiss, 0.20f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.12f }, { dID::springDecay, 0.35f },
            { dID::mix, 0.0f },                                   // 100% wet for the FX channel
        } + singleHead());

        // 73: Ghost Octave — pitch character at -12 layered under the dry
        // for a haunted underbelly. Low feedback so it's a single ghost
        // echo, not a tower.
        P ("Ghost Octave", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 220.0f },
            { dID::delayMode, 4 }, { dID::pitchOn, 1 }, { dID::pitchSemis, -12.0f },
            { dID::feedback, 0.32f },
            { dID::preHpFreq, 80.0f }, { dID::lpFreq, 3500.0f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 2 },
            { dID::reverbMix, 0.28f }, { dID::plateDecay, 0.60f },
            { dID::width, 1.3f }, { dID::mix, 0.32f },
        } + singleHead());

        // 74: Cathedral Vox — long, dense gothic-cathedral reverb for solo
        // sustained vocals. No delay; the predelay + tail do the work.
        P ("Cathedral", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::preHpFreq, 100.0f },
            { dID::reverbMode, (float) hwRev (5) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.95f }, { dID::plateSize, 0.95f },
            { dID::plateDamp, 0.45f }, { dID::platePredelay, 80.0f },
            { dID::reverbMod, 0.08f },
            { dID::width, 1.7f }, { dID::mix, 0.45f },
        } + singleHead());

        // 75: Fifth Stack — shimmer set to +7 st (a perfect fifth) for a
        // Gregorian / power-chord vocal stack. Sounds great on long held
        // notes.
        P ("Fifth Stack", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::preHpFreq, 100.0f },
            { dID::reverbMode, (float) hwRev (7) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.42f }, { dID::plateDecay, 0.78f }, { dID::plateSize, 0.85f },
            { dID::plateDamp, 0.30f }, { dID::platePredelay, 50.0f },
            { dID::reverbMod, 0.15f }, { dID::shimmerSemis, 7.0f },  // perfect fifth up
            { dID::width, 1.5f }, { dID::mix, 0.38f },
        } + singleHead());

        // 76: Stereo Doubler — Pitch char at unison with maximum spread
        // and short delay = the classic "two-shifter chorus" thickener.
        // Pitch ON at semis=0 + spread does it; mix high since this IS the
        // effect.
        P ("Stereo Doubler", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 25.0f },
            { dID::delayMode, 4 }, { dID::pitchOn, 1 }, { dID::pitchSemis, 0.0f },
            { dID::pitchSpread, 30.0f },  // ±15 c
            { dID::feedback, 0.08f },
            { dID::preHpFreq, 110.0f },
            { dID::reverbMode, 0 },
            { dID::width, 1.6f }, { dID::mix, 0.50f },
        } + singleHead());

        // 77: Megaphone — heavy band-pass + drive + small spring, for
        // theatrical effect calls. Mid-honk-y on purpose; use as 100% wet
        // FX bus.
        P ("Megaphone", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::delayBypass, 1 },
            { dID::inFilterOn, 1 }, { dID::inFilterType, 2 },
            { dID::inFilterCutoff, 1800.0f }, { dID::inFilterRes, 0.70f },
            { dID::drive, 0.80f }, { dID::hiss, 0.28f },
            { dID::reverbMode, (float) hwRev (1) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.20f }, { dID::springDecay, 0.30f }, { dID::springTone, 0.65f },
            { dID::mix, 0.0f },
        } + singleHead());

        // 78: Frozen Pad — freeze the buffer behind the vocal as a sustained
        // bed. Hit Freeze, sing over it, let the bed evolve through the
        // shimmer.
        P ("Frozen Pad", std::vector<PV>{
            { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::timeMs, 1000.0f },
            { dID::delayMode, 3 }, { dID::freeze, 1 }, { dID::feedback, 0.85f },
            { dID::preHpFreq, 150.0f }, { dID::lpFreq, 5500.0f },
            { dID::reverbMode, (float) hwRev (7) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.90f },
            { dID::reverbMod, 0.25f }, { dID::shimmerSemis, 12.0f },
            { dID::width, 1.6f }, { dID::mix, 0.45f },
        } + singleHead());

        // 79: MIDI Harmonizer — keyboard-driven pitch on the delay tail.
        // Pitch char + MIDI Pitch Mode + portamento so smoothly-played
        // melody lines glide through the harmony. C3 (MIDI 60) = unison.
        P ("MIDI Harmonizer", std::vector<PV>{
            { dID::syncMode, 0 }, { dID::timeMs, 180.0f },
            { dID::delayMode, 4 }, { dID::pitchOn, 1 }, { dID::pitchSemis, 0.0f },
            { dID::midiPitchMode, 1 }, { dID::midiPortaOn, 1 }, { dID::midiPortaMs, 60.0f },
            { dID::pitchSpread, 0.0f },
            { dID::feedback, 0.30f },
            { dID::preHpFreq, 100.0f }, { dID::lpFreq, 8000.0f },
            { dID::reverbMode, (float) hwRev (2) }, { dID::reverbRoute, 0 },
            { dID::reverbMix, 0.25f }, { dID::plateDecay, 0.55f }, { dID::platePredelay, 30.0f },
            { dID::width, 1.4f }, { dID::mix, 0.42f },
        } + singleHead());

        return out;
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state), factory (buildFactory())
{
    currentName = factory.empty() ? juce::String() : factory.front().name;
}

juce::File PresetManager::userPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Doobie")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::migrateLegacyState (juce::AudioProcessorValueTreeState& state,
                                        const juce::ValueTree& loaded)
{
    int  legacyMode = -1;
    bool hasHeadOn  = false;

    for (const auto& child : loaded)
    {
        if (! child.hasType ("PARAM"))
            continue;
        const auto id = child.getProperty ("id").toString();
        if (id == dID::legacyModeSel)
            legacyMode = (int) (float) child.getProperty ("value");
        for (auto* h : dID::headOn)
            if (id == h)
                hasHeadOn = true;
    }

    // A current state already carries the head switches; only convert when an
    // old "modeSel" is present and no switches are.
    if (legacyMode < 0 || hasHeadOn)
        return;

    const int mask = dID::legacyModeMask[(size_t) juce::jlimit (0, 11, legacyMode)];
    for (int i = 0; i < 4; ++i)
        if (auto* p = state.getParameter (dID::headOn[(size_t) i]))
            p->setValueNotifyingHost ((mask & (1 << i)) != 0 ? 1.0f : 0.0f);
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (const auto& p : factory)
        names.add (p.name);
    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (const auto& f : userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

juce::StringArray PresetManager::getAllNames() const
{
    auto names = getFactoryNames();
    names.addArray (getUserNames());
    return names;
}

int PresetManager::getCurrentIndex() const
{
    return getAllNames().indexOf (currentName);
}

void PresetManager::resetToDefaults()
{
    for (const auto& child : apvts.state)
    {
        const auto id = child.getProperty ("id").toString();
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    }
}

void PresetManager::applyPreset (const Preset& preset)
{
    // Click-free preset swap (ported from hardware): the engine dips its
    // output to silence and ramps back over ~12 ms while the new parameter
    // values flow in. Without this, every reload produces an audible
    // "DC pulse" pop as the smoothers chase the new targets.
    if (preLoadHook) preLoadHook();

    resetToDefaults();
    for (const auto& [id, value] : preset.values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));

    currentName = preset.name;

    // Notify the processor that any "dirty-since-last-load" flag it
    // tracks can now be cleared — we just finished applying.
    if (postLoadHook) postLoadHook();
}

void PresetManager::loadFactory (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factory.size()))
        applyPreset (factory[(size_t) index]);
}

void PresetManager::loadByName (const juce::String& name)
{
    for (int i = 0; i < (int) factory.size(); ++i)
        if (factory[(size_t) i].name == name)
        {
            applyPreset (factory[(size_t) i]);
            return;
        }

    auto file = userPresetDirectory().getChildFile (name + ".xml");
    if (file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            migrateLegacyState (apvts, tree); // convert a pre-matrix user preset
            currentName = name;
        }
    }
}

void PresetManager::saveUser (const juce::String& name)
{
    if (name.isEmpty())
        return;

    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->writeTo (userPresetDirectory().getChildFile (name + ".xml"));
        currentName = name;
    }
}

bool PresetManager::deleteUser (const juce::String& name)
{
    auto file = userPresetDirectory().getChildFile (name + ".xml");
    return file.existsAsFile() && file.deleteFile();
}

void PresetManager::next()
{
    auto names = getAllNames();
    if (names.isEmpty())
        return;
    int idx = (names.indexOf (currentName) + 1) % names.size();
    loadByName (names[idx]);
}

void PresetManager::previous()
{
    auto names = getAllNames();
    if (names.isEmpty())
        return;
    int idx = (names.indexOf (currentName) - 1 + names.size()) % names.size();
    loadByName (names[idx]);
}
