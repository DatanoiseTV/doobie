#include "SpringTankView.h"
#include "LookAndFeel.h"

namespace doobie
{
SpringTankView::SpringTankView()
{
    startTimerHz (30);
}

void SpringTankView::timerCallback()
{
    const float lvl = source ? source() : 0.0f;
    excite += 0.3f * (lvl - excite);
    phase  += 0.07f;
    if (phase > 1.0e6f) phase = 0.0f;
    repaint();
}

void SpringTankView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (colours::line());
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

    const auto inner = b.reduced (10.0f, 8.0f);
    constexpr int springs = 3;
    constexpr int steps   = 90;

    for (int s = 0; s < springs; ++s)
    {
        const float cy    = inner.getY() + ((float) s + 0.5f) * inner.getHeight() / (float) springs;
        const int   coils = 10 + s * 3;
        const float amp   = 2.5f + excite * 10.0f + 1.5f * std::sin (phase * 0.7f + (float) s);

        juce::Path p;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float x = inner.getX() + t * inner.getWidth();
            // Taper the coil amplitude toward the suspended end-points.
            const float env = std::sin (t * juce::MathConstants<float>::pi);
            const float y = cy + amp * env * std::sin ((float) coils * juce::MathConstants<float>::twoPi * t
                                                        + phase + (float) s);
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }

        if (excite > 0.02f)
        {
            g.setColour (colours::teal().withAlpha (juce::jlimit (0.0f, 0.5f, excite)));
            g.strokePath (p, juce::PathStrokeType (3.0f));
        }
        g.setColour (colours::metal().brighter (0.6f).withAlpha (0.9f));
        g.strokePath (p, juce::PathStrokeType (1.3f));

        // Anchor lugs at each end.
        g.setColour (colours::metal().darker (0.3f));
        g.fillEllipse (inner.getX() - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        g.fillEllipse (inner.getRight() - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }
}
} // namespace doobie
