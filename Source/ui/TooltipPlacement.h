// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Picks where a hover read-out box goes so it does not cover the trace it describes.
#pragma once
#include <juce_graphics/juce_graphics.h>
#include <functional>

namespace malon
{

/** traceTopY(x) returns the highest (smallest y) point any trace reaches at screen x,
    or a value >= plot bottom when nothing is drawn there. Everything below that line is
    trace (bars and filled curves grow up from the bottom), so a box is only "clear" when
    it sits completely above it. Candidates are tried near the cursor first, then pinned
    to the top of the plot; the one covering the fewest trace columns wins. */
inline juce::Rectangle<float> placeReadout (juce::Point<float> mouse, float w, float h,
                                            juce::Rectangle<float> plot,
                                            const std::function<float (float)>& traceTopY,
                                            juce::Rectangle<float> avoid = {})
{
    using R = juce::Rectangle<float>;
    const float gap = 14.0f, m = 4.0f;
    const float right = mouse.x + gap, left = mouse.x - gap - w;
    const float below = mouse.y + gap, above = mouse.y - gap - h;
    const float top = plot.getY() + m;
    const R cands[] = { { right, below, w, h }, { left, below, w, h }, { right, above, w, h }, { left, above, w, h },
                        { right, top, w, h },   { left, top, w, h } };

    auto clampIn = [&] (R r)
    {
        r.setX (juce::jlimit (plot.getX() + m, juce::jmax (plot.getX() + m, plot.getRight() - m - w), r.getX()));
        r.setY (juce::jlimit (plot.getY() + m, juce::jmax (plot.getY() + m, plot.getBottom() - m - h), r.getY()));
        return r;
    };
    auto cost = [&] (R r)
    {
        int c = 0;
        for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
            if (r.getBottom() + 3.0f > traceTopY (x)) ++c;
        if (! avoid.isEmpty() && r.intersects (avoid)) c += 1000;
        if (r.contains (mouse)) c += 2000;
        return c;
    };

    R best; int bestCost = 1 << 30;
    for (auto r : cands)
    {
        r = clampIn (r);
        const int c = cost (r);
        if (c < bestCost) { bestCost = c; best = r; }
    }
    return best;
}

} // namespace malon
