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

// Central registry of every automatable parameter ID and the fixed option
// lists that the editor and the DSP must agree on. Keeping the IDs in one
// header avoids the classic "front-end and back-end drifted apart" bug.

#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

namespace dID
{
    // Version hint for APVTS parameter IDs. Bump only on breaking changes.
    inline constexpr int kVersion = 1;

    // ---- I/O / global -------------------------------------------------------
    inline constexpr auto inputDrive  = "inputDrive";
    inline constexpr auto mix         = "mix";
    inline constexpr auto outputGain  = "outputGain";
    inline constexpr auto width       = "width";

    // ---- Delay core ---------------------------------------------------------
    inline constexpr auto delayMode   = "delayMode";   // character / type
    inline constexpr auto syncMode    = "syncMode";    // Free / Sync
    inline constexpr auto timeMs      = "timeMs";      // free-running time
    inline constexpr auto syncDiv     = "syncDiv";     // tempo division
    inline constexpr auto feedback    = "feedback";    // "intensity"
    inline constexpr auto pingPong    = "pingPong";
    inline constexpr auto freeze      = "freeze";
    inline constexpr auto delayBypass = "delayBypass";   // skip the tape; let the user run the rest of the chain alone
    inline constexpr auto duck        = "duck";

    // ---- Multi-head ---------------------------------------------------------
    // Four read heads tapping one tape. Each head has an on/off switch (the head
    // matrix), a level, a pan and a time ratio that scales the base delay.
    inline constexpr std::array<const char*, 4> headOn    { "head1On",    "head2On",    "head3On",    "head4On"    };
    inline constexpr std::array<const char*, 4> headLevel { "head1Level", "head2Level", "head3Level", "head4Level" };
    inline constexpr std::array<const char*, 4> headPan   { "head1Pan",   "head2Pan",   "head3Pan",   "head4Pan"   };
    inline constexpr std::array<const char*, 4> headRatio { "head1Ratio", "head2Ratio", "head3Ratio", "head4Ratio" };

    // ---- Tape character -----------------------------------------------------
    inline constexpr auto wow         = "wow";
    inline constexpr auto flutter     = "flutter";
    inline constexpr auto drive       = "drive";       // tape saturation
    inline constexpr auto hiss        = "hiss";        // AGE macro: hiss + dropouts + HF loss + instability (id kept for compatibility)

    // ---- Input filter (shapes the signal entering the delay) ---------------
    inline constexpr auto preHpFreq   = "preHpFreq";   // input high-pass
    inline constexpr auto preLpFreq   = "preLpFreq";   // input low-pass
    inline constexpr auto preBass     = "preBass";     // input low shelf
    inline constexpr auto preTreble   = "preTreble";   // input high shelf

    // ---- Feedback tone (the dub tone knobs, applied on every repeat) -------
    inline constexpr auto bass        = "bass";        // low shelf in feedback
    inline constexpr auto treble      = "treble";      // high shelf in feedback
    inline constexpr auto hpFreq      = "hpFreq";      // feedback high-pass
    inline constexpr auto lpFreq      = "lpFreq";      // feedback low-pass

    // ---- Reverb -------------------------------------------------------------
    inline constexpr auto reverbMode    = "reverbMode";    // off/spring/plate/series/parallel
    inline constexpr auto reverbRoute   = "reverbRoute";   // post / pre / feedback
    inline constexpr auto reverbMix     = "reverbMix";
    inline constexpr auto springDecay   = "springDecay";
    inline constexpr auto springTone    = "springTone";
    inline constexpr auto plateDecay    = "plateDecay";
    inline constexpr auto plateSize     = "plateSize";
    inline constexpr auto plateDamp     = "plateDamp";
    inline constexpr auto platePredelay = "platePredelay";
    inline constexpr auto reverbMod     = "reverbMod";
    inline constexpr auto irGain        = "irGain";    // dB, makeup gain on the convolution wet
    inline constexpr auto irSpeed       = "irSpeed";   // playback speed multiplier (0.25..4.0)

    // ---- Modulation matrix --------------------------------------------------
    // Two LFOs + one envelope follower feed a 4-slot matrix; each slot picks a
    // source, a destination (curated subset of engine params, see ModMatrix.h)
    // and a bipolar amount. Mod is applied per block to the base params; the
    // engine's own smoothers ramp toward the modulated targets.
    inline constexpr auto lfo1Rate   = "lfo1Rate";    // Hz
    inline constexpr auto lfo1Depth  = "lfo1Depth";   // 0..1
    inline constexpr auto lfo1Wave   = "lfo1Wave";    // choice index
    inline constexpr auto lfo2Rate   = "lfo2Rate";
    inline constexpr auto lfo2Depth  = "lfo2Depth";
    inline constexpr auto lfo2Wave   = "lfo2Wave";
    inline constexpr auto envAttack  = "envAttack";   // ms
    inline constexpr auto envRelease = "envRelease";  // ms
    inline constexpr auto envSens    = "envSens";     // dB

    // Per-slot triples (source, destination, amount). kNumModSlots in
    // ModMatrix.h must match the array size here.
    inline constexpr std::array<const char*, 4> modSlotSrc {
        "mod1Src", "mod2Src", "mod3Src", "mod4Src" };
    inline constexpr std::array<const char*, 4> modSlotDst {
        "mod1Dst", "mod2Dst", "mod3Dst", "mod4Dst" };
    inline constexpr std::array<const char*, 4> modSlotAmt {
        "mod1Amt", "mod2Amt", "mod3Amt", "mod4Amt" };

    inline const juce::StringArray lfoWaveChoices {
        "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H"
    };

    // ---- Fixed option lists (UI and DSP read the same arrays) --------------
    inline const juce::StringArray syncModeChoices { "Free", "Sync" };

    // Delay character / type. Tape is the default so existing presets are
    // unchanged. Digital = clean, BBD = dark analog bucket-brigade, Diffuse =
    // smeared/ambient, Pitch = each repeat climbs an octave.
    inline const juce::StringArray delayModeChoices {
        "Digital", "Tape", "BBD", "Diffuse", "Pitch"
    };

    // Tempo divisions, fastest to slowest. Index order is the contract.
    inline const juce::StringArray syncDivChoices {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/16.",
        "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2",
        "1/1T", "1/2.", "1/1", "2 bars", "4 bars"
    };

    // Multiplier (in quarter notes) for each sync division above.
    inline constexpr std::array<double, 19> syncDivQuarters {
        0.0625, 0.0833333, 0.125, 0.1666667, 0.25, 0.3333333, 0.375,
        0.5, 0.6666667, 0.75, 1.0, 1.3333333, 1.5, 2.0,
        2.6666667, 3.0, 4.0, 8.0, 16.0
    };

    // Musical note values (in quarter notes) a playback head can tap in sync
    // mode. The head TIME control snaps to one of these so taps land on the
    // grid instead of a continuous fraction of the repeat.
    inline constexpr std::array<double, 14> headDivQuarters {
        0.125, 0.1666667, 0.25, 0.3333333, 0.375, 0.5, 0.6666667,
        0.75, 1.0, 1.3333333, 1.5, 2.0, 3.0, 4.0
    };

    // Display labels for each head division above (index-aligned).
    inline const juce::StringArray headDivNames {
        "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T",
        "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/2.", "1/1"
    };

    // Snap a target time (in quarter notes) to the nearest head division that
    // does not exceed maxQuarters (so a head never taps beyond the repeat).
    inline double snapHeadQuarters (double targetQuarters, double maxQuarters)
    {
        double best = -1.0, bestErr = 1.0e9;
        for (double d : headDivQuarters)
        {
            if (d > maxQuarters + 1.0e-6) continue;
            const double e = std::abs (d - targetQuarters);
            if (e < bestErr) { bestErr = e; best = d; }
        }
        return best > 0.0 ? best : maxQuarters;
    }

    // New algorithms are appended so existing presets keep their stored index
    // (Off=0, Spring=1, Plate=2, Spring>Plate=3, Spring+Plate=4, Hall=5, Shimmer=6,
    // Convolution=7 — uses the user-loaded impulse response).
    inline const juce::StringArray reverbModeChoices {
        "Off", "Spring", "Plate", "Spring > Plate", "Spring + Plate", "Hall", "Shimmer", "Convolution"
    };

    // Properties on the APVTS state tree carrying the loaded IR. Convolution
    // mode reads them back on session restore. Not automatable parameters —
    // file paths and indices don't belong in a knob.
    //   irPath          -> custom file path (string), empty if not custom.
    //   factoryIrIndex  -> built-in IR index (int >= 0), -1 if not factory.
    // The two are mutually exclusive; whichever is set wins on restore.
    inline constexpr auto irPathProperty        = "irPath";
    inline constexpr auto factoryIrIndexProperty = "factoryIrIndex";

    inline const juce::StringArray reverbRouteChoices {
        "Post", "Pre", "In Feedback"
    };

    // ---- Legacy migration ---------------------------------------------------
    // Versions up to 0.1.0 used a single 12-position "modeSel" choice (a
    // Space-Echo style dial) instead of four independent head on/off switches.
    // When an old state or preset is loaded we map that index to the head
    // matrix via this table (bit i = head i on). Kept only for backwards
    // compatibility; new states store the four headOn switches directly.
    inline constexpr auto legacyModeSel = "modeSel";
    inline constexpr std::array<int, 12> legacyModeMask {
        0b0001, 0b0010, 0b0100, 0b1000,   // single heads A,B,C,D
        0b0011, 0b1100, 0b0101, 0b1010,   // pairs
        0b0111, 0b1110, 0b1001, 0b1111    // triples / all
    };
}
