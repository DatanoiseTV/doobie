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

    // Shorthand for head parameter overrides.
    PV ho (int i, bool on) { return { dID::headOn[(size_t) i], on ? 1.0f : 0.0f }; }
    PV hl (int i, float v) { return { dID::headLevel[(size_t) i], v }; }
    PV hp (int i, float v) { return { dID::headPan[(size_t) i],   v }; }
    PV hr (int i, float v) { return { dID::headRatio[(size_t) i], v }; }

    // The factory bank, grouped by use. Each preset lists only the values that
    // differ from the parameter defaults; everything else is reset before a
    // preset is applied, so loads are deterministic. Values are in real units
    // (Hz, ms, dB) and, for choice parameters, the option index:
    //   syncDiv:  4=1/16 5=1/8T 6=1/16. 7=1/8 8=1/4T 9=1/8. 10=1/4 11=1/2T
    //             12=1/4. 13=1/2 14=1/1T 15=1/2. 16=1/1 17=2 bars 18=4 bars
    //   heads:    the head matrix. ho(i,true/false) switches head i (0=A 1=B
    //             2=C 3=D); head A is on by default. hl/hp/hr set a head's
    //             level (0..1), pan (-1..1) and time ratio (fraction of repeat).
    //   delayMode:   0 digital, 1 tape, 2 BBD, 3 diffuse, 4 pitch
    //   reverbMode:  0 off, 1 spring, 2 plate, 3 spring>plate, 4 spring+plate,
    //                5 hall, 6 shimmer
    //   reverbRoute: 0 post, 1 pre, 2 in-feedback
    //   hiss is the AGE macro (0..1): hiss + dropouts + HF loss + instability.
    std::vector<PresetManager::Preset> buildFactory()
    {
        return {
            // ---- Dub / reggae -----------------------------------------------
            { "Classic Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.68f },
                { dID::drive, 0.45f }, { dID::lpFreq, 3000.0f }, { dID::bass, 0.20f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.40f },
                { dID::springDecay, 0.6f }, { dID::wow, 0.25f }, { dID::hiss, 0.20f },
                { dID::mix, 0.40f } } },

            { "King Tubby", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.75f },
                { dID::drive, 0.5f }, { dID::lpFreq, 2800.0f }, { dID::bass, 0.3f },
                { dID::preHpFreq, 180.0f }, { dID::reverbMode, 1 }, { dID::reverbRoute, 2 },
                { dID::reverbMix, 0.45f }, { dID::wow, 0.3f }, { dID::hiss, 0.3f },
                { dID::mix, 0.42f } } },

            { "Dub Siren", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 1.0f },
                { dID::lpFreq, 2400.0f }, { dID::hpFreq, 240.0f }, { dID::drive, 0.5f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.45f },
                { dID::mix, 0.45f } } },

            { "Stepper's Skank", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, ho (0, true), ho (2, true),
                { dID::feedback, 0.6f }, { dID::pingPong, 1 }, { dID::lpFreq, 3500.0f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.3f },
                { dID::mix, 0.4f } } },

            { "Two-Head Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, ho (0, true), ho (3, true),
                hl (0, 0.9f), hl (3, 0.7f), hp (0, -0.3f), hp (3, 0.3f), hr (0, 1.0f), hr (3, 0.5f),
                { dID::feedback, 0.6f }, { dID::lpFreq, 3200.0f }, { dID::bass, 0.2f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.4f },
                { dID::hiss, 0.25f }, { dID::mix, 0.42f } } },

            { "Sufferah Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::feedback, 0.62f },
                { dID::drive, 0.4f }, { dID::bass, 0.2f }, { dID::treble, -0.2f },
                { dID::lpFreq, 3000.0f }, { dID::reverbMode, 1 }, { dID::reverbRoute, 2 },
                { dID::reverbMix, 0.4f }, { dID::hiss, 0.3f }, { dID::mix, 0.4f } } },

            { "Sub Bass Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.65f },
                { dID::preLpFreq, 2000.0f }, { dID::hpFreq, 60.0f }, { dID::bass, 0.4f },
                { dID::drive, 0.45f }, { dID::reverbMode, 2 }, { dID::reverbRoute, 2 },
                { dID::reverbMix, 0.35f }, { dID::mix, 0.4f } } },

            { "Melodica Space", {
                { dID::syncMode, 1 }, { dID::syncDiv, 12 }, { dID::feedback, 0.7f },
                { dID::reverbMode, 2 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.35f },
                { dID::plateDecay, 0.7f }, { dID::lpFreq, 4000.0f }, { dID::mix, 0.45f } } },

            // ---- Multi-head (head-matrix showcase) --------------------------
            { "Quad Cascade", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, ho (0, true), ho (1, true), ho (2, true), ho (3, true),
                hl (0, 0.9f), hl (1, 0.7f), hl (2, 0.6f), hl (3, 0.55f),
                hp (0, 0.0f), hp (1, -0.5f), hp (2, 0.5f), hp (3, 0.0f),
                hr (0, 1.0f), hr (1, 0.75f), hr (2, 0.5f), hr (3, 0.25f),
                { dID::feedback, 0.45f }, { dID::reverbMode, 2 }, { dID::reverbMix, 0.3f },
                { dID::mix, 0.45f } } },

            { "Triplet Fan", {
                { dID::syncMode, 1 }, { dID::syncDiv, 8 }, ho (0, true), ho (1, true), ho (2, true),
                hl (0, 0.85f), hl (1, 0.7f), hl (2, 0.6f), hp (0, -0.4f), hp (1, 0.0f), hp (2, 0.4f),
                hr (0, 1.0f), hr (1, 0.66f), hr (2, 0.33f),
                { dID::pingPong, 1 }, { dID::feedback, 0.5f }, { dID::reverbMode, 2 },
                { dID::reverbMix, 0.28f }, { dID::mix, 0.42f } } },

            { "Wide Bounce", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, ho (0, true), ho (3, true),
                hp (0, -0.9f), hp (3, 0.9f), hr (0, 1.0f), hr (3, 0.5f),
                { dID::pingPong, 1 }, { dID::width, 1.6f }, { dID::feedback, 0.55f },
                { dID::reverbMode, 2 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.4f } } },

            // Heads B+C+D with A switched off — a combination the old mode dial
            // could not make.
            { "Off-Grid Trio", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, ho (0, false), ho (1, true), ho (2, true), ho (3, true),
                hl (1, 0.8f), hl (2, 0.7f), hl (3, 0.6f), hp (1, -0.5f), hp (2, 0.2f), hp (3, 0.5f),
                hr (1, 0.75f), hr (2, 0.5f), hr (3, 0.25f),
                { dID::feedback, 0.5f }, { dID::reverbMode, 1 }, { dID::reverbMix, 0.3f },
                { dID::mix, 0.42f } } },

            { "Polyrhythm 3:4", {
                { dID::syncMode, 1 }, { dID::syncDiv, 8 }, ho (0, true), ho (1, true), ho (2, true),
                hr (0, 1.0f), hr (1, 0.66f), hr (2, 0.33f),
                { dID::feedback, 0.45f }, { dID::pingPong, 1 }, { dID::reverbMode, 1 },
                { dID::reverbMix, 0.2f }, { dID::mix, 0.42f } } },

            { "Galloping Eighths", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, ho (0, true), ho (3, true),
                hr (0, 1.0f), hr (3, 0.66f), { dID::feedback, 0.6f }, { dID::pingPong, 1 },
                { dID::reverbMode, 1 }, { dID::reverbMix, 0.2f }, { dID::mix, 0.42f } } },

            { "Dub Quartet", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, ho (0, true), ho (1, true), ho (2, true), ho (3, true),
                hl (0, 0.9f), hl (1, 0.55f), hl (2, 0.55f), hl (3, 0.7f),
                hp (0, 0.0f), hp (1, -0.6f), hp (2, 0.6f), hp (3, 0.0f),
                { dID::feedback, 0.55f }, { dID::lpFreq, 3000.0f }, { dID::bass, 0.2f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.35f },
                { dID::hiss, 0.2f }, { dID::mix, 0.45f } } },

            // ---- Ambient / cinematic ----------------------------------------
            { "Ambient Wash", {
                { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 },
                { dID::reverbMode, 4 }, { dID::reverbRoute, 1 }, { dID::reverbMix, 0.60f },
                { dID::plateDecay, 0.85f }, { dID::plateSize, 0.80f }, { dID::reverbMod, 0.4f },
                { dID::feedback, 0.5f }, { dID::lpFreq, 8000.0f }, { dID::width, 1.3f },
                { dID::mix, 0.6f } } },

            { "Frozen Pad", {
                { dID::freeze, 1 }, { dID::reverbMode, 4 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.9f }, { dID::plateSize, 0.9f },
                { dID::reverbMod, 0.6f }, { dID::width, 1.4f }, { dID::mix, 0.7f } } },

            { "Cathedral", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::reverbMode, 3 },
                { dID::reverbRoute, 0 }, { dID::reverbMix, 0.55f }, { dID::plateDecay, 0.92f },
                { dID::plateSize, 0.9f }, { dID::platePredelay, 60.0f }, { dID::springDecay, 0.6f },
                { dID::feedback, 0.4f }, { dID::mix, 0.55f } } },

            { "Cosmic Drift", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.82f },
                { dID::reverbMode, 4 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.5f },
                { dID::plateDecay, 0.88f }, { dID::reverbMod, 0.7f }, { dID::springDecay, 0.7f },
                { dID::wow, 0.4f }, { dID::hiss, 0.3f }, { dID::lpFreq, 4200.0f }, { dID::mix, 0.55f } } },

            { "Glacier", {
                { dID::freeze, 1 }, { dID::reverbMode, 4 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.6f }, { dID::plateDecay, 0.95f }, { dID::plateSize, 0.95f },
                { dID::plateDamp, 0.2f }, { dID::reverbMod, 0.5f }, { dID::preHpFreq, 200.0f },
                { dID::width, 1.5f }, { dID::mix, 0.7f } } },

            { "Slow Tide", {
                { dID::syncMode, 1 }, { dID::syncDiv, 16 }, { dID::feedback, 0.6f },
                { dID::reverbMode, 4 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.5f },
                { dID::plateDecay, 0.85f }, { dID::plateSize, 0.8f }, { dID::reverbMod, 0.6f },
                { dID::mix, 0.6f } } },

            { "Underwater", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.6f },
                { dID::preLpFreq, 1500.0f }, { dID::lpFreq, 1800.0f }, { dID::reverbMode, 2 },
                { dID::reverbRoute, 0 }, { dID::reverbMix, 0.4f }, { dID::reverbMod, 0.7f },
                { dID::wow, 0.5f }, { dID::hiss, 0.3f }, { dID::mix, 0.55f } } },

            { "Pitch Cathedral", {
                { dID::delayMode, 4 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 },
                { dID::feedback, 0.5f }, { dID::reverbMode, 5 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.5f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.9f },
                { dID::mix, 0.5f } } },

            // ---- Hall & Shimmer ---------------------------------------------
            { "Grand Hall", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.4f },
                { dID::reverbMode, 5 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.5f },
                { dID::plateDecay, 0.82f }, { dID::plateSize, 0.9f }, { dID::platePredelay, 45.0f },
                { dID::reverbMod, 0.35f }, { dID::lpFreq, 9000.0f }, { dID::mix, 0.5f } } },

            { "Big Room", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.35f },
                { dID::reverbMode, 5 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.42f },
                { dID::plateDecay, 0.7f }, { dID::plateSize, 0.75f }, { dID::platePredelay, 25.0f },
                { dID::mix, 0.42f } } },

            { "Hall of Mirrors", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.7f },
                { dID::reverbMode, 5 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.45f },
                { dID::plateDecay, 0.85f }, { dID::plateSize, 0.9f }, { dID::reverbMod, 0.5f },
                { dID::lpFreq, 5000.0f }, { dID::mix, 0.55f } } },

            { "Angel Choir", {
                { dID::syncMode, 1 }, { dID::syncDiv, 16 }, { dID::feedback, 0.4f },
                { dID::reverbMode, 6 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.6f },
                { dID::plateDecay, 0.8f }, { dID::plateSize, 0.85f }, { dID::reverbMod, 0.75f },
                { dID::preHpFreq, 200.0f }, { dID::treble, 0.3f }, { dID::width, 1.4f }, { dID::mix, 0.6f } } },

            { "Shimmer Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.6f },
                { dID::reverbMode, 6 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.4f },
                { dID::plateDecay, 0.7f }, { dID::reverbMod, 0.6f }, { dID::lpFreq, 4500.0f },
                { dID::hiss, 0.2f }, { dID::mix, 0.45f } } },

            { "Frozen Shimmer", {
                { dID::freeze, 1 }, { dID::reverbMode, 6 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.65f }, { dID::plateDecay, 0.85f }, { dID::plateSize, 0.9f },
                { dID::reverbMod, 0.8f }, { dID::width, 1.5f }, { dID::mix, 0.7f } } },

            // ---- Rhythmic / electronic --------------------------------------
            { "Pristine Digital", {
                { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 },
                { dID::feedback, 0.5f }, { dID::pingPong, 1 }, { dID::reverbMode, 0 },
                { dID::wow, 0.0f }, { dID::flutter, 0.0f }, { dID::mix, 0.35f } } },

            { "Dotted Pop", {
                { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 },
                { dID::feedback, 0.45f }, { dID::reverbMode, 2 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.22f }, { dID::lpFreq, 9000.0f }, { dID::mix, 0.35f } } },

            { "Ping Triplets", {
                { dID::syncMode, 1 }, { dID::syncDiv, 5 }, ho (0, true), ho (2, true),
                { dID::feedback, 0.5f }, { dID::pingPong, 1 }, { dID::reverbMode, 1 },
                { dID::reverbMix, 0.2f }, { dID::mix, 0.38f } } },

            { "Techno Quarter", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.55f },
                { dID::pingPong, 1 }, { dID::drive, 0.2f }, { dID::reverbMode, 0 },
                { dID::lpFreq, 6500.0f }, { dID::mix, 0.35f } } },

            { "Stutter 16ths", {
                { dID::syncMode, 1 }, { dID::syncDiv, 4 }, ho (0, true), ho (1, true),
                hr (0, 1.0f), hr (1, 0.5f), { dID::feedback, 0.6f }, { dID::pingPong, 1 },
                { dID::reverbMode, 1 }, { dID::reverbMix, 0.18f }, { dID::mix, 0.4f } } },

            { "Dub Techno Chord", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.7f },
                { dID::drive, 0.3f }, { dID::lpFreq, 3000.0f }, { dID::reverbMode, 2 },
                { dID::reverbRoute, 2 }, { dID::reverbMix, 0.4f }, { dID::width, 1.3f },
                { dID::mix, 0.4f } } },

            { "Trance Gate", {
                { dID::delayMode, 0 }, { dID::syncMode, 1 }, { dID::syncDiv, 4 },
                ho (0, true), ho (1, true), { dID::feedback, 0.5f }, { dID::pingPong, 1 },
                { dID::lpFreq, 9000.0f }, { dID::reverbMode, 2 }, { dID::reverbMix, 0.2f },
                { dID::mix, 0.4f } } },

            { "Wide Quarter", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.5f },
                { dID::width, 1.6f }, { dID::pingPong, 1 }, { dID::reverbMode, 0 },
                { dID::mix, 0.35f } } },

            // ---- Lo-fi / vintage (the AGE macro on show) --------------------
            { "Vintage Echo", {
                { dID::syncMode, 0 }, { dID::timeMs, 320.0f }, { dID::wow, 0.55f },
                { dID::flutter, 0.5f }, { dID::drive, 0.6f }, { dID::lpFreq, 2600.0f },
                { dID::hiss, 0.5f }, { dID::feedback, 0.5f }, { dID::reverbMode, 1 },
                { dID::reverbMix, 0.3f }, { dID::mix, 0.4f } } },

            { "Warped Tape", {
                { dID::syncMode, 0 }, { dID::timeMs, 480.0f }, { dID::wow, 0.8f },
                { dID::flutter, 0.6f }, { dID::drive, 0.5f }, { dID::lpFreq, 3200.0f },
                { dID::hiss, 0.6f }, { dID::feedback, 0.55f }, { dID::reverbMode, 1 },
                { dID::reverbRoute, 2 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.42f } } },

            { "Dusty Slap", {
                { dID::syncMode, 0 }, { dID::timeMs, 140.0f }, { dID::feedback, 0.2f },
                { dID::drive, 0.5f }, { dID::wow, 0.4f }, { dID::hiss, 0.6f },
                { dID::lpFreq, 3000.0f }, { dID::reverbMode, 0 }, { dID::mix, 0.3f } } },

            // Heavy AGE: the dropouts and HF loss really wobble and crumble here.
            { "Broken Cassette", {
                { dID::syncMode, 0 }, { dID::timeMs, 380.0f }, { dID::wow, 0.9f },
                { dID::flutter, 0.7f }, { dID::drive, 0.6f }, { dID::hiss, 0.85f },
                { dID::lpFreq, 2600.0f }, { dID::preLpFreq, 4000.0f }, { dID::feedback, 0.5f },
                { dID::reverbMode, 1 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.42f } } },

            { "AM Radio", {
                { dID::syncMode, 0 }, { dID::timeMs, 240.0f }, { dID::preHpFreq, 400.0f },
                { dID::preLpFreq, 3500.0f }, { dID::hpFreq, 300.0f }, { dID::lpFreq, 3000.0f },
                { dID::drive, 0.5f }, { dID::hiss, 0.5f }, { dID::feedback, 0.45f },
                { dID::reverbMode, 0 }, { dID::mix, 0.4f } } },

            { "Old Film Reel", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::wow, 0.6f },
                { dID::flutter, 0.5f }, { dID::hiss, 0.7f }, { dID::drive, 0.4f },
                { dID::lpFreq, 3000.0f }, { dID::feedback, 0.5f }, { dID::reverbMode, 1 },
                { dID::reverbMix, 0.35f }, { dID::mix, 0.4f } } },

            { "Worn Reel", {
                { dID::delayMode, 2 }, { dID::syncMode, 0 }, { dID::timeMs, 600.0f },
                { dID::wow, 0.7f }, { dID::drive, 0.55f }, { dID::feedback, 0.6f },
                { dID::bass, 0.2f }, { dID::treble, -0.3f }, { dID::hiss, 0.6f },
                { dID::reverbMode, 1 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.45f } } },

            // ---- Delay characters (BBD / Diffuse / Pitch) -------------------
            { "BBD Analog", {
                { dID::delayMode, 2 }, { dID::syncMode, 1 }, { dID::syncDiv, 9 },
                { dID::feedback, 0.6f }, { dID::wow, 0.3f }, { dID::flutter, 0.25f },
                { dID::reverbMode, 1 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.4f } } },

            { "Diffuse Wash", {
                { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 },
                { dID::feedback, 0.62f }, { dID::lpFreq, 5000.0f }, { dID::reverbMode, 2 },
                { dID::reverbRoute, 0 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.45f } } },

            { "Octave Climber", {
                { dID::delayMode, 4 }, { dID::syncMode, 1 }, { dID::syncDiv, 10 },
                { dID::feedback, 0.55f }, { dID::reverbMode, 2 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.3f }, { dID::lpFreq, 7000.0f }, { dID::mix, 0.45f } } },

            { "Ambient Diffusion", {
                { dID::delayMode, 3 }, { dID::syncMode, 1 }, { dID::syncDiv, 13 },
                { dID::feedback, 0.7f }, { dID::reverbMode, 6 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.5f }, { dID::reverbMod, 0.6f }, { dID::lpFreq, 6000.0f },
                { dID::width, 1.4f }, { dID::mix, 0.55f } } },

            // ---- Special FX / sound design ----------------------------------
            { "Infinity Hold", {
                { dID::freeze, 1 }, { dID::reverbMode, 0 }, { dID::mix, 0.5f } } },

            { "Runaway Oscillator", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::feedback, 1.1f },
                { dID::lpFreq, 3000.0f }, { dID::drive, 0.6f }, { dID::reverbMode, 1 },
                { dID::reverbRoute, 2 }, { dID::reverbMix, 0.3f }, { dID::mix, 0.4f } } },

            { "Ducked Throw", {
                { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::feedback, 0.6f },
                { dID::duck, 0.8f }, { dID::reverbMode, 2 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.35f }, { dID::mix, 0.45f } } },

            { "Swell Verb", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.5f },
                { dID::reverbMode, 2 }, { dID::reverbRoute, 1 }, { dID::reverbMix, 0.55f },
                { dID::platePredelay, 150.0f }, { dID::reverbMod, 0.6f }, { dID::duck, 0.6f },
                { dID::mix, 0.5f } } },

            { "Detuned Doubler", {
                { dID::syncMode, 0 }, { dID::timeMs, 25.0f }, { dID::feedback, 0.0f },
                { dID::wow, 0.4f }, { dID::flutter, 0.4f }, { dID::width, 1.5f },
                { dID::reverbMode, 0 }, { dID::mix, 0.5f } } },

            { "Black Hole", {
                { dID::freeze, 1 }, { dID::reverbMode, 4 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.7f }, { dID::plateDecay, 0.97f }, { dID::plateSize, 0.97f },
                { dID::plateDamp, 0.1f }, { dID::reverbMod, 0.6f }, { dID::width, 1.6f },
                { dID::mix, 0.75f } } },

            // ---- Instruments ------------------------------------------------
            { "Vocal Throw", {
                { dID::syncMode, 1 }, { dID::syncDiv, 12 }, { dID::feedback, 0.45f },
                { dID::duck, 0.7f }, { dID::reverbMode, 2 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.35f }, { dID::platePredelay, 40.0f }, { dID::lpFreq, 7000.0f },
                { dID::preHpFreq, 150.0f }, { dID::mix, 0.4f } } },

            { "Guitar Slap", {
                { dID::syncMode, 0 }, { dID::timeMs, 130.0f }, { dID::feedback, 0.2f },
                { dID::drive, 0.35f }, { dID::lpFreq, 6000.0f }, { dID::reverbMode, 1 },
                { dID::reverbRoute, 0 }, { dID::reverbMix, 0.25f }, { dID::mix, 0.32f } } },

            { "Synth Width", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::feedback, 0.5f },
                { dID::pingPong, 1 }, { dID::width, 1.5f }, { dID::reverbMode, 2 },
                { dID::reverbMix, 0.3f }, { dID::mix, 0.4f } } },

            { "Piano Ambience", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.35f },
                { dID::reverbMode, 2 }, { dID::reverbRoute, 0 }, { dID::reverbMix, 0.4f },
                { dID::platePredelay, 30.0f }, { dID::lpFreq, 9000.0f }, { dID::mix, 0.35f } } },

            { "Drum Room Echo", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::feedback, 0.3f },
                { dID::duck, 0.5f }, { dID::reverbMode, 1 }, { dID::reverbRoute, 0 },
                { dID::reverbMix, 0.3f }, { dID::mix, 0.35f } } },
        };
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
    resetToDefaults();
    for (const auto& [id, value] : preset.values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));

    currentName = preset.name;
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
