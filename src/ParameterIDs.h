#pragma once

// Central registry of every automatable parameter ID and the fixed option
// lists that the editor and the DSP must agree on. Keeping the IDs in one
// header avoids the classic "front-end and back-end drifted apart" bug.

#include <array>
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
    inline constexpr auto syncMode    = "syncMode";    // Free / Sync
    inline constexpr auto timeMs      = "timeMs";      // free-running time
    inline constexpr auto syncDiv     = "syncDiv";     // tempo division
    inline constexpr auto feedback    = "feedback";    // "intensity"
    inline constexpr auto modeSel     = "modeSel";     // Space-Echo mode dial (12)
    inline constexpr auto pingPong    = "pingPong";
    inline constexpr auto freeze      = "freeze";
    inline constexpr auto duck        = "duck";

    // ---- Multi-head ---------------------------------------------------------
    // Four read heads tapping one tape. ratio scales the base delay time.
    inline constexpr std::array<const char*, 4> headLevel { "head1Level", "head2Level", "head3Level", "head4Level" };
    inline constexpr std::array<const char*, 4> headPan   { "head1Pan",   "head2Pan",   "head3Pan",   "head4Pan"   };
    inline constexpr std::array<const char*, 4> headRatio { "head1Ratio", "head2Ratio", "head3Ratio", "head4Ratio" };

    // ---- Tape character -----------------------------------------------------
    inline constexpr auto wow         = "wow";
    inline constexpr auto flutter     = "flutter";
    inline constexpr auto drive       = "drive";       // tape saturation
    inline constexpr auto hiss        = "hiss";        // tape age / noise floor

    // ---- Feedback tone (the dub tone knobs) --------------------------------
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

    // ---- Fixed option lists (UI and DSP read the same arrays) --------------
    inline const juce::StringArray syncModeChoices { "Free", "Sync" };

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

    inline const juce::StringArray reverbModeChoices {
        "Off", "Spring", "Plate", "Spring > Plate", "Spring + Plate"
    };

    inline const juce::StringArray reverbRouteChoices {
        "Post", "Pre", "In Feedback"
    };

    // 12-position Space-Echo style mode dial. Each position activates a
    // combination of the four playback heads (A,B,C,D). The head-on masks are
    // defined in DubDelayEngine::kModeMask and must stay index-aligned here.
    inline const juce::StringArray modeChoices {
        "1: A",
        "2: B",
        "3: C",
        "4: D",
        "5: A+B",
        "6: C+D",
        "7: A+C",
        "8: B+D",
        "9: A+B+C",
        "10: B+C+D",
        "11: A+D",
        "12: A+B+C+D"
    };
}
