/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  Vector-cassette visualiser. Ported (1:1 visual port, re-implemented in
  JUCE Graphics) from the same author's Recordy project — see
  https://github.com/DatanoiseTV/Recordy (main/www/index.html, app.js).
  The cassette layout (hub centres, reel-flange radii, three-spoke spindle
  pattern, head + pinch roller + idler-roller positions) follows that
  source so the two projects share a visual language; later iterations on
  this branch will add more playheads, an animated tape loop, and a
  Doobie-tuned amber/teal colour scheme on top of the same geometry.

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace doobie
{
// A vector cassette transport drawn in JUCE Graphics. The internal geometry
// is in a fixed 680 × 307 "design viewbox" matching the source project, then
// scaled (aspect-fit) into whatever bounds the component is given — so all
// proportions stay correct at any size.
//
// Initial Doobie integration shows the cassette idling (reels spin slowly,
// no tape-position concept yet). The setters are wired up so a later
// iteration can map them to engine state — delay-time → reel speed,
// freeze → stop spin, multi-head taps → additional head positions.
class VectorCassette : public juce::Component,
                      private juce::Timer
{
public:
    VectorCassette()
    {
        setOpaque (true);
        startTimerHz (30);
    }

    // Driver hooks for later iterations. None of these are wired into Doobie's
    // engine yet — the cassette currently idles at half-pack with a steady
    // reel rotation while playing == true.
    void setPosition  (float p)    noexcept { position = juce::jlimit (0.0f, 1.0f, p); }
    void setPlaying   (bool on)    noexcept { playing  = on; }
    void setSpeed     (float s)    noexcept { speed    = s; }
    void setRecording (bool on)    noexcept { recording = on; }

    void paint (juce::Graphics& g) override
    {
        // Background: a deep panel-shadow rectangle so the white linework reads.
        g.fillAll (juce::Colour (0xff0d0c0a));

        // Aspect-fit the 680x307 design viewbox into the component bounds.
        const auto bounds = getLocalBounds().toFloat();
        const float sx = bounds.getWidth()  / kViewW;
        const float sy = bounds.getHeight() / kViewH;
        const float s  = std::min (sx, sy);
        const float ox = bounds.getX() + (bounds.getWidth()  - kViewW * s) * 0.5f;
        const float oy = bounds.getY() + (bounds.getHeight() - kViewH * s) * 0.5f;

        const auto tx = [=] (float x) { return ox + (x - kViewX) * s; };
        const auto ty = [=] (float y) { return oy + (y - kViewY) * s; };
        const auto ts = [=] (float v) { return v * s; };

        const auto white = juce::Colours::white;
        const auto rim   = white;
        const auto fill  = juce::Colour (0xffd8d8d8);

        // ---- Both reels ---------------------------------------------------
        const float rPack = packRadius();
        const Reel reels[2] = { kSupply, kTakeup };
        for (int reelIdx = 0; reelIdx < 2; ++reelIdx)
        {
            const auto& reel = reels[reelIdx];
            const float cx = tx (reel.cx), cy = ty (reel.cy);

            // Outer flange.
            g.setColour (rim);
            const float rO = ts (reel.rMax);
            g.drawEllipse (cx - rO, cy - rO, rO * 2.0f, rO * 2.0f, ts (1.4f));

            // Pack: filled disc from rim down to the hub housing, with a small
            // rim gap so the white fill doesn't merge into the flange stroke.
            const float rimGap     = ts (4.0f);
            const float rFill      = std::max (ts (reel.rHub + 1.0f), ts (rPack) - rimGap);
            const float rPackEdgeR = ts (rPack);
            const float rHub       = ts (reel.rHub);
            g.setColour (fill);
            g.fillEllipse (cx - rFill, cy - rFill, rFill * 2.0f, rFill * 2.0f);
            // Punch out the hub housing so the pack reads as an annulus.
            g.setColour (juce::Colour (0xff0d0c0a));
            g.fillEllipse (cx - rHub, cy - rHub, rHub * 2.0f, rHub * 2.0f);

            // Pack edge ring (thin white outline at the current pack radius).
            g.setColour (rim);
            g.drawEllipse (cx - rPackEdgeR, cy - rPackEdgeR,
                           rPackEdgeR * 2.0f, rPackEdgeR * 2.0f, ts (1.4f));
            // Hub housing ring (thicker).
            g.drawEllipse (cx - rHub, cy - rHub, rHub * 2.0f, rHub * 2.0f, ts (2.4f));

            // ---- Spindle decoration (the spinning bit) -------------------
            drawSpindle (g, cx, cy, s, reelIdx == 0 ? supplyAngle : takeupAngle);
        }

        // ---- Lower-row hardware: idler rollers, head, pinch -------------
        for (auto cxView : { 245.0f, 285.0f, 395.0f, 435.0f })
            drawSmallRoller (g, tx (cxView), ty (250.0f), ts (6.0f), s);

        drawHead       (g, tx (kHeadX), ty (kHeadY), s);
        drawPinchRoller(g, tx (kHeadX), ty (pinchY), s);

        // ---- Tape line over the lower hardware --------------------------
        drawTapePath (g, s, ox, oy, rPack);

        // ---- REC indicator (faint when idle, red when recording) --------
        drawRec (g, tx (340.0f), ty (110.0f), s);

        // ---- Top-left badge + centred timecode --------------------------
        // A small "1" in a square — corresponds to Recordy's track-number
        // badge; here it identifies the cassette in the row of visualisers.
        {
            const float bx = tx (20.0f), by = ty (18.0f);
            const float bw = ts (34.0f);
            g.setColour (rim);
            g.drawRect (juce::Rectangle<float> (bx, by, bw, bw), ts (1.4f));
            g.setFont (juce::Font (juce::FontOptions (ts (22.0f))));
            g.drawText ("1", juce::Rectangle<float> (bx, by, bw, bw),
                        juce::Justification::centred);
        }
    }

private:
    // ---- Geometry constants (in the 680x307 design viewbox) -----------------
    static constexpr float kViewX = 0.0f,  kViewY = 12.0f;
    static constexpr float kViewW = 680.0f, kViewH = 307.0f;

    struct Reel { float cx, cy, rHub, rMax; };
    static constexpr Reel kSupply { 180.0f, 160.0f, 22.0f, 84.0f };
    static constexpr Reel kTakeup { 500.0f, 160.0f, 22.0f, 84.0f };

    static constexpr float kHeadX = 340.0f;
    static constexpr float kHeadY = 232.0f;
    static constexpr float kPinchDisengaged = 290.0f;
    static constexpr float kPinchEngaged    = 275.0f;

    // ---- Animated state ----------------------------------------------------
    float position     = 0.5f;
    bool  playing      = true;
    float speed        = 1.0f;
    bool  recording    = false;

    float supplyAngle  = 0.0f;
    float takeupAngle  = 0.0f;
    float pinchY       = kPinchEngaged;
    float tensionPhase = 0.0f;

    // ---- Timer drives the reel rotation + tape-tension wobble ------------
    void timerCallback() override
    {
        const float dt = 1.0f / 30.0f;
        const float target = playing ? kPinchEngaged : kPinchDisengaged;
        pinchY += (target - pinchY) * std::min (1.0f, dt * 10.0f);

        if (playing)
        {
            // Reel angular speed scales by the inverse of the pack radius so
            // the inner / outer fills lock to a constant tape speed — same
            // physics as the source project's tick().
            const float referenceR = 40.0f;
            const float base       = 90.0f * speed;
            const float r          = packRadius();
            const float rS         = std::sqrt ((1.0f - position) * (kSupply.rMax * kSupply.rMax - kSupply.rHub * kSupply.rHub) + kSupply.rHub * kSupply.rHub);
            const float rT         = std::sqrt (position * (kTakeup.rMax * kTakeup.rMax - kTakeup.rHub * kTakeup.rHub) + kTakeup.rHub * kTakeup.rHub);
            juce::ignoreUnused (r);
            supplyAngle = std::fmod (supplyAngle - base * (referenceR / std::max (rS, kSupply.rHub)) * dt, 360.0f);
            takeupAngle = std::fmod (takeupAngle - base * (referenceR / std::max (rT, kTakeup.rHub)) * dt, 360.0f);
            tensionPhase += dt * 4.0f;
        }
        repaint();
    }

    float packRadius() const noexcept
    {
        // Mid-pack as a sensible idle: half of the available pack area.
        // A later iteration can drive this from playhead position once
        // Doobie carries one.
        const float aMax = kSupply.rMax * kSupply.rMax - kSupply.rHub * kSupply.rHub;
        return std::sqrt (0.5f * aMax + kSupply.rHub * kSupply.rHub);
    }

    // ---- Helpers -----------------------------------------------------------
    void drawSpindle (juce::Graphics& g, float cx, float cy, float s, float angleDeg)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (angleDeg), cx, cy));

        const auto white = juce::Colours::white;
        const auto black = juce::Colour (0xff0d0c0a);

        // Three spokes radiating from centre, 120° apart.
        g.setColour (white);
        const float spoke = 20.0f * s;
        const float w = 1.4f * s;
        for (int i = 0; i < 3; ++i)
        {
            const float a = juce::degreesToRadians (-90.0f + (float) i * 120.0f);
            g.drawLine (cx, cy, cx + spoke * std::cos (a), cy + spoke * std::sin (a), w);
        }

        // Inner ring (radius 11).
        const float rRing = 11.0f * s;
        g.setColour (black);
        g.fillEllipse (cx - rRing, cy - rRing, rRing * 2.0f, rRing * 2.0f);
        g.setColour (white);
        g.drawEllipse (cx - rRing, cy - rRing, rRing * 2.0f, rRing * 2.0f, 1.2f * s);

        // Three tiny dots inside the ring, evenly spaced like sprocket holes.
        const float dotR = 1.6f * s;
        const float dotO = 8.0f * s;
        const struct { float x, y; } dots[3] = {
            {  0.0f * s,  dotO },
            {  7.0f * s, -4.0f * s },
            { -7.0f * s, -4.0f * s }
        };
        for (auto& d : dots)
        {
            g.setColour (black);
            g.fillEllipse (cx + d.x - dotR, cy + d.y - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour (white);
            g.drawEllipse (cx + d.x - dotR, cy + d.y - dotR, dotR * 2.0f, dotR * 2.0f, 0.8f * s);
        }

        // Centre dot.
        const float ctrR = 2.0f * s;
        g.setColour (white);
        g.fillEllipse (cx - ctrR, cy - ctrR, ctrR * 2.0f, ctrR * 2.0f);
    }

    void drawSmallRoller (juce::Graphics& g, float cx, float cy, float r, float s)
    {
        g.setColour (juce::Colour (0xff0d0c0a));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f * s);
        const float c = 1.5f * s;
        g.fillEllipse (cx - c, cy - c, c * 2.0f, c * 2.0f);
    }

    void drawHead (juce::Graphics& g, float cx, float cy, float s)
    {
        const auto white = juce::Colours::white;
        g.setColour (white);
        g.drawLine (cx - 7.0f * s, cy - 30.0f * s, cx - 7.0f * s, cy - 8.0f * s, 1.2f * s);
        g.drawLine (cx + 7.0f * s, cy - 30.0f * s, cx + 7.0f * s, cy - 8.0f * s, 1.2f * s);
        const float r = 1.2f * s;
        g.fillEllipse (cx - 7.0f * s - r, cy - 20.0f * s - r, r * 2.0f, r * 2.0f);
        g.fillEllipse (cx + 7.0f * s - r, cy - 20.0f * s - r, r * 2.0f, r * 2.0f);
        // Head body rectangle.
        const juce::Rectangle<float> headBox (cx - 9.0f * s, cy - 8.0f * s, 18.0f * s, 16.0f * s);
        g.setColour (juce::Colour (0xff0d0c0a));
        g.fillRect (headBox);
        g.setColour (white);
        g.drawRect (headBox, 1.4f * s);
    }

    void drawPinchRoller (juce::Graphics& g, float cx, float cy, float s)
    {
        const float r = 9.0f * s;
        g.setColour (juce::Colour (0xff0d0c0a));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f * s);
        const float c = 1.5f * s;
        g.fillEllipse (cx - c, cy - c, c * 2.0f, c * 2.0f);
    }

    void drawRec (juce::Graphics& g, float cx, float cy, float s)
    {
        const float r = 7.0f * s;
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f * s);
        if (recording)
        {
            const float rd = 4.5f * s;
            g.setColour (juce::Colour (0xffff2a2a));
            g.fillEllipse (cx - rd, cy - rd, rd * 2.0f, rd * 2.0f);
        }
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (9.0f * s)));
        g.drawText ("REC", juce::Rectangle<float> (cx - 20.0f * s, cy + 14.0f * s, 40.0f * s, 12.0f * s),
                    juce::Justification::centred);
    }

    // ---- Tape path through the lower hardware -----------------------------
    // Builds the white line that threads from the supply reel, around the
    // idler rollers, across the head + pinch roller, and back up to the
    // takeup reel. The math mirrors the source project's externalTangent /
    // tangentFromPoint / lowerTangent helpers: each pulley contributes an
    // entry tangent, an arc around its surface, and an exit tangent.
    struct Pt { float x, y; };
    static float hypot2 (float dx, float dy) { return std::sqrt (dx * dx + dy * dy); }

    static Pt lowerTangent (float cx, float cy, float r, float px, float py)
    {
        const float dx = px - cx, dy = py - cy, d = hypot2 (dx, dy);
        if (d <= r) return { cx, cy + r };
        const float a0 = std::atan2 (dy, dx), a1 = std::acos (r / d);
        const Pt p1 { cx + r * std::cos (a0 + a1), cy + r * std::sin (a0 + a1) };
        const Pt p2 { cx + r * std::cos (a0 - a1), cy + r * std::sin (a0 - a1) };
        return p1.y > p2.y ? p1 : p2;
    }

    static Pt tangentFromPoint (Pt P, Pt c, float r, char side)
    {
        const float dx = c.x - P.x, dy = c.y - P.y, d = hypot2 (dx, dy);
        if (d <= r) return { c.x, c.y + (side == 'b' ? r : -r) };
        const float a1 = std::acos (r / d), from = std::atan2 (-dy, -dx);
        const Pt p1 { c.x + r * std::cos (from + a1), c.y + r * std::sin (from + a1) };
        const Pt p2 { c.x + r * std::cos (from - a1), c.y + r * std::sin (from - a1) };
        if (side == 't') return p1.y < p2.y ? p1 : p2;
        return p1.y > p2.y ? p1 : p2;
    }

    struct TangentPair { Pt p1, p2; };
    static TangentPair externalTangent (Pt c1, float r1, char s1, Pt c2, float r2, char s2)
    {
        const float dx = c2.x - c1.x, dy = c2.y - c1.y, d = hypot2 (dx, dy);
        const float ba = std::atan2 (dy, dx);
        const float a = s1 == 't' ? -1.0f : 1.0f;
        float t1, t2;
        if (s1 == s2)
        {
            const float dr = r1 - r2;
            const float phi = std::fabs (dr) <= d ? std::asin (dr / d) : 0.0f;
            t1 = ba + a * (juce::MathConstants<float>::halfPi - phi);
            t2 = t1;
        }
        else
        {
            const float sum = r1 + r2;
            if (sum >= d)
            {
                t1 = ba + a * juce::MathConstants<float>::halfPi;
                t2 = t1 + juce::MathConstants<float>::pi;
            }
            else
            {
                const float phi = std::asin (sum / d);
                t1 = ba + a * (juce::MathConstants<float>::halfPi - phi);
                t2 = t1 + juce::MathConstants<float>::pi;
            }
        }
        return { { c1.x + r1 * std::cos (t1), c1.y + r1 * std::sin (t1) },
                 { c2.x + r2 * std::cos (t2), c2.y + r2 * std::sin (t2) } };
    }

    void drawTapePath (juce::Graphics& g, float s, float ox, float oy, float rPack)
    {
        const auto V = [=] (float x, float y) { return Pt { ox + (x - kViewX) * s, oy + (y - kViewY) * s }; };

        // Roller / pulley waypoints in viewbox coords, with the side (top /
        // bottom) the tape wraps around each one. The "head" contributes two
        // virtual points pinning the tape to the head's top edge.
        struct W { Pt c; float r; char side; };
        const bool engaged = pinchY < (kPinchEngaged + 4.0f);
        const float headH  = 16.0f, halfW = 9.0f;

        std::vector<W> P;
        P.push_back ({ V (245.0f, 250.0f), 6.0f * s, 'b' });
        P.push_back ({ V (285.0f, 250.0f), 6.0f * s, 't' });
        if (engaged)
        {
            P.push_back ({ V (kHeadX - halfW, kHeadY + headH * 0.5f), 0.5f * s, 't' });
            P.push_back ({ V (kHeadX,         pinchY),                9.0f * s, 'b' });
            P.push_back ({ V (kHeadX + halfW, kHeadY + headH * 0.5f), 0.5f * s, 't' });
        }
        P.push_back ({ V (395.0f, 250.0f), 6.0f * s, 't' });
        P.push_back ({ V (435.0f, 250.0f), 6.0f * s, 'b' });

        const float supplyCx = ox + (kSupply.cx - kViewX) * s;
        const float supplyCy = oy + (kSupply.cy - kViewY) * s;
        const float takeupCx = ox + (kTakeup.cx - kViewX) * s;
        const float takeupCy = oy + (kTakeup.cy - kViewY) * s;
        const float rPackS   = rPack * s;

        const Pt tS = lowerTangent (supplyCx, supplyCy, rPackS, P.front().c.x, P.front().c.y);
        const Pt tT = lowerTangent (takeupCx, takeupCy, rPackS, P.back().c.x,  P.back().c.y);

        const int n = (int) P.size();
        std::vector<Pt> entry ((size_t) n), exitPt ((size_t) n);
        entry[0] = tangentFromPoint (tS, P[0].c, P[0].r, P[0].side);
        for (int i = 0; i < n - 1; ++i)
        {
            const auto t = externalTangent (P[(size_t) i].c,    P[(size_t) i].r,    P[(size_t) i].side,
                                            P[(size_t) (i+1)].c, P[(size_t) (i+1)].r, P[(size_t) (i+1)].side);
            exitPt[(size_t) i]     = t.p1;
            entry [(size_t) (i+1)] = t.p2;
        }
        exitPt[(size_t) (n - 1)] = tangentFromPoint (tT, P[(size_t) (n - 1)].c, P[(size_t) (n - 1)].r, P[(size_t) (n - 1)].side);

        juce::Path tape;
        tape.startNewSubPath (tS.x, tS.y);
        for (int i = 0; i < n; ++i)
        {
            tape.lineTo (entry[(size_t) i].x, entry[(size_t) i].y);
            // Arc around the pulley from entry to exit.
            const auto& w = P[(size_t) i];
            const auto&  e = entry [(size_t) i];
            const auto&  x = exitPt[(size_t) i];
            const float a0 = std::atan2 (e.y - w.c.y, e.x - w.c.x);
            const float a1 = std::atan2 (x.y - w.c.y, x.x - w.c.x);
            // Sweep direction picked so the arc hugs the pulley on the correct side.
            // For tops we sweep CCW (negative), for bottoms CW (positive). Use addArc
            // and let it interpolate the short-way path.
            const bool clockwise = w.side == 'b';
            float d = a1 - a0;
            if (clockwise)
            {
                while (d < 0.0f) d += juce::MathConstants<float>::twoPi;
            }
            else
            {
                while (d > 0.0f) d -= juce::MathConstants<float>::twoPi;
            }
            tape.addCentredArc (w.c.x, w.c.y, w.r, w.r, 0.0f, a0, a0 + d);
            tape.lineTo (exitPt[(size_t) i].x, exitPt[(size_t) i].y);
        }
        tape.lineTo (tT.x, tT.y);

        g.setColour (juce::Colours::white);
        g.strokePath (tape, juce::PathStrokeType (1.4f * s));
    }
};
} // namespace doobie
