// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Own drawing code. Band layout / grid frequencies follow the original analyzer's behaviour
// (bars evenly spaced = log-frequency, decade or octave grid); art and colours are ours.
#include "SpectrumView.h"
#include "TooltipPlacement.h"
#include "MalonLookAndFeel.h"

namespace malon
{
using namespace juce;

namespace
{
    struct GridLine { double hz; bool labelled; };
    const GridLine decadeGrid[] =
    {   {25, 1}, {40, 0}, {50, 1}, {60, 0}, {70, 0}, {80, 0}, {90, 0}, {100, 1}, {200, 1}, {300, 0}, {400, 0},
        {500, 1}, {600, 0}, {700, 0}, {800, 0}, {900, 0}, {1000, 1}, {2000, 1}, {3000, 0}, {4000, 0}, {5000, 1},
        {6000, 0}, {7000, 0}, {8000, 0}, {9000, 0}, {10000, 1}, {20000, 1} };
    const GridLine octaveGrid[] =
    {   {31.25, 1}, {62.5, 1}, {125, 1}, {250, 1}, {500, 1}, {1000, 1}, {2000, 1}, {4000, 1}, {8000, 1}, {16000, 1} };

    String freqText (double hz)
    {
        if (hz < 1000.0) return String (roundToInt (hz));
        const int rem = int (0.01 * std::fmod (hz, 1000.0));
        return rem ? String (int (hz / 1000.0)) + "k" + String (rem) : String (int (hz / 1000.0)) + "k";
    }
}

Rectangle<float> SpectrumView::getPlotArea() const
{
    const float padL = 36.0f, padR = 36.0f, padT = 16.0f, padB = 24.0f;
    return getLocalBounds().toFloat().withTrimmedLeft (padL).withTrimmedRight (padR)
                                     .withTrimmedTop (padT).withTrimmedBottom (padB);
}

float SpectrumView::levelToY (double db, Rectangle<float> plot) const
{
    return plot.getY() + float ((settings.ceiling - db) * (plot.getHeight() / settings.range));
}

float SpectrumView::bandX (int i, int n, Rectangle<float> plot) const
{
    const float slot = plot.getWidth() / (float) n;
    return plot.getX() + ((float) i + 0.5f) * slot;
}

void SpectrumView::drawGrid (Graphics& g, Rectangle<float> plot, int n)
{
    const auto labelFont = uiFont (10.5f);
    g.setFont (labelFont);

    // horizontal level lines
    const int top = (int) settings.ceiling, grid = juce::jmax (1, (int) settings.grid), range = (int) settings.range;
    int level = top - top % grid;
    if (level > top) level -= grid;
    while (level > top - range)
    {
        const float y = std::round (levelToY ((double) level, plot)) + 0.5f;
        if (level < top)
        {
            g.setColour (colors[ColorScheme::GridLine]);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
        g.setColour (colors[ColorScheme::GridLabel]);
        g.drawText (String (level), Rectangle<float> (0, y - 7, plot.getX() - 6, 14), Justification::centredRight, false);
        g.drawText (String (level), Rectangle<float> (plot.getRight() + 6, y - 7, 30, 14), Justification::centredLeft, false);
        level -= grid;
    }

    // vertical frequency lines
    if (n > 1 && model->freqMax > model->freqMin)
    {
        const double fmin = model->freqMin, fmax = model->freqMax;
        const float slot = plot.getWidth() / (float) n;
        const float x0 = plot.getX() + 0.5f * slot, span = plot.getWidth() - slot;
        const auto* grid2 = settings.octaveGrid ? octaveGrid : decadeGrid;
        const int count = settings.octaveGrid ? (int) std::size (octaveGrid) : (int) std::size (decadeGrid);
        for (int i = 0; i < count; ++i)
        {
            const double f = grid2[i].hz;
            if (f < fmin * 0.97 || f > fmax * 1.03) continue;
            const float x = std::round (x0 + span * (float) (std::log (f / fmin) / std::log (fmax / fmin))) + 0.5f;
            g.setColour (colors[ColorScheme::GridLine]);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (grid2[i].labelled)
            {
                g.setColour (colors[ColorScheme::GridLabel]);
                g.drawText (freqText (f), Rectangle<float> (x - 22, plot.getBottom() + 5, 44, 14), Justification::centred, false);
            }
        }
    }
}

void SpectrumView::drawCurve (Graphics& g, const double* level, int n, Rectangle<float> plot,
                              Colour colour, int size, bool fill) const
{
    if (n < 2) return;
    std::vector<Point<float>> p ((size_t) n);
    for (int i = 0; i < n; ++i)
        p[(size_t) i] = { bandX (i, n, plot), levelToY (juce::jlimit (-400.0, 400.0, level[i]), plot) };

    // Catmull-Rom (cardinal, tension 0.5) through the band points -> cubic Beziers
    Path path;
    path.startNewSubPath (p[0]);
    for (int i = 0; i < n - 1; ++i)
    {
        const auto p0 = p[(size_t) juce::jmax (0, i - 1)], p1 = p[(size_t) i], p2 = p[(size_t) i + 1],
                   p3 = p[(size_t) juce::jmin (n - 1, i + 2)];
        path.cubicTo (p1 + (p2 - p0) * (1.0f / 6.0f), p2 - (p3 - p1) * (1.0f / 6.0f), p2);
    }

    if (fill)
    {
        Path f (path);
        f.lineTo (p.back().x, plot.getBottom() + 4);
        f.lineTo (p.front().x, plot.getBottom() + 4);
        f.closeSubPath();
        g.setColour (colour.withMultipliedAlpha (0.5f));
        g.fillPath (f);
    }
    g.setColour (colour);
    g.strokePath (path, PathStrokeType (1.0f + 0.75f * (float) (size - 1), PathStrokeType::curved, PathStrokeType::rounded));
}

void SpectrumView::paint (Graphics& g)
{
    // background
    g.setGradientFill (ColourGradient (colors[ColorScheme::BackgroundTop].withAlpha (1.0f), 0, 0,
                                       colors[ColorScheme::BackgroundBottom].withAlpha (1.0f), 0, (float) getHeight(), false));
    g.fillAll();
    if (model == nullptr) return;

    const int n = model->numBands;
    const auto plot = getPlotArea();
    if (n < 2 || plot.getWidth() < 40 || plot.getHeight() < 30) return;

    drawGrid (g, plot, n);

    // frame
    g.setColour (colors[ColorScheme::GridBorder]);
    g.drawRect (plot.expanded (0.5f), 1.0f);

    // data (clipped to the plot)
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.toNearestIntEdges());

    const float slot = plot.getWidth() / (float) n;
    const float gap  = juce::jmax (1.0f, std::round (slot * 0.16f));
    const float barW = juce::jmax (1.0f, slot - gap);

    auto drawBars = [&] (const double* lv, Colour c, bool fromBottom, int thickness)
    {
        g.setColour (c);
        for (int i = 0; i < n; ++i)
        {
            const float y = levelToY (juce::jlimit (-400.0, 400.0, lv[i]), plot);
            const float x = plot.getX() + (float) i * slot + 0.5f * gap;
            if (fromBottom) g.fillRect (x, y, barW, plot.getBottom() - y);
            else            g.fillRect (x, y, barW, (float) thickness);
        }
    };

    const auto cPeak = colors[ColorScheme::Peak], cHold = colors[ColorScheme::Hold], cAvg = colors[ColorScheme::Average];

    // like the original: peak goes on top of a filled hold curve, below it otherwise
    if (settings.holdShow != ShowAs::CurveFill && settings.peakOn) drawBars (model->peak, cPeak, true, 0);
    if (settings.holdOn)
    {
        if (settings.holdShow == ShowAs::Bars) drawBars (model->hold, cHold, false, settings.holdSize);
        else drawCurve (g, model->hold, n, plot, cHold, settings.holdSize, settings.holdShow == ShowAs::CurveFill);
    }
    if (settings.holdShow == ShowAs::CurveFill && settings.peakOn) drawBars (model->peak, cPeak, true, 0);
    if (settings.avgOn)
    {
        if (settings.avgShow == ShowAs::Bars) drawBars (model->avg, cAvg, false, settings.avgSize);
        else drawCurve (g, model->avg, n, plot, cAvg, settings.avgSize, settings.avgShow == ShowAs::CurveFill);
    }


    // legend: which colour is which (only the traces that are on)
    {
        struct Item { const char* name; Colour c; bool bar; };
        Item items[3]; int count = 0;
        if (settings.peakOn) items[count++] = { "PEAK", cPeak, true };
        if (settings.holdOn) items[count++] = { "HOLD", cHold, false };
        if (settings.avgOn)  items[count++] = { "AVERAGE", cAvg, false };
        float x = plot.getX() + 10.0f;
        const float y = plot.getY() + 8.0f;
        if (settings.frozen) x += 62.0f;
        g.setFont (uiFont (9.5f).withExtraKerningFactor (0.12f));
        for (int i = 0; i < count; ++i)
        {
            g.setColour (items[i].c.withAlpha (1.0f));
            if (items[i].bar) g.fillRect (x, y + 1.0f, 8.0f, 8.0f);
            else              g.fillRoundedRectangle (x, y + 4.0f, 12.0f, 2.0f, 1.0f);
            const float sw = items[i].bar ? 8.0f : 12.0f;
            g.setColour (colors[ColorScheme::GridLabel]);
            const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), items[i].name);
            g.drawText (items[i].name, Rectangle<float> (x + sw + 5.0f, y - 1.0f, tw + 4.0f, 12.0f), Justification::centredLeft, false);
            x += sw + 5.0f + tw + 14.0f;
        }
    }

    // pointer read-out: frequency of the band under the cursor and its levels
    if (hasMouse && plot.contains (mouse) && model->freqMax > model->freqMin)
    {
        const int band = juce::jlimit (0, n - 1, (int) ((mouse.x - plot.getX()) / slot));
        const double hz = model->freqMin * std::pow (model->freqMax / model->freqMin, (double) band / (double) juce::jmax (1, n - 1));
        const double db = settings.ceiling - (mouse.y - plot.getY()) / (plot.getHeight() / settings.range);
        auto dbText = [] (double v) { return v <= -150.0 || ! std::isfinite (v) ? String ("-inf") : String (v, 1); };

        g.setColour (colors[ColorScheme::GridLabel].withAlpha (0.10f));
        g.fillRect (plot.getX() + (float) band * slot, plot.getY(), slot, plot.getHeight());
        g.setColour (colors[ColorScheme::GridLabel].withAlpha (0.5f));
        g.drawHorizontalLine ((int) mouse.y, plot.getX(), plot.getRight());

        const String hzText = hz < 1000.0 ? String (roundToInt (hz)) + " Hz" : String (hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz";
        StringArray lines;
        lines.add (hzText + "    cursor " + dbText (db) + " dB");
        String levels;
        if (settings.peakOn) levels << "peak " << dbText (model->peak[band]) << "   ";
        if (settings.holdOn) levels << "hold " << dbText (model->hold[band]) << "   ";
        if (settings.avgOn)  levels << "avg " << dbText (model->avg[band]);
        if (levels.isNotEmpty()) lines.add (levels.trimEnd() + " dB");

        const float w = 232.0f, h = 8.0f + 15.0f * (float) lines.size();
        // keep the box off the traces: find the top of whatever is drawn under each x
        auto traceTop = [&] (float x)
        {
            const int b = juce::jlimit (0, n - 1, (int) ((x - plot.getX()) / slot));
            double top = -1e9;
            if (settings.peakOn) top = juce::jmax (top, model->peak[b]);
            if (settings.holdOn) top = juce::jmax (top, model->hold[b]);
            if (settings.avgOn)  top = juce::jmax (top, model->avg[b]);
            return top < -1e8 ? plot.getBottom() + 10.0f : levelToY (juce::jlimit (-400.0, 400.0, top), plot);
        };
        const auto legendArea = Rectangle<float> (plot.getX(), plot.getY(), 260.0f, 22.0f);
        auto r = placeReadout (mouse, w, h, plot, traceTop, legendArea);
        g.setColour (Colour (0xF00C0E14));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        for (int i = 0; i < lines.size(); ++i)
        {
            g.setColour (i == 0 ? Colours::white.withAlpha (0.95f) : colors[ColorScheme::GridLabel].brighter (0.4f));
            g.setFont (uiFont (11.0f, i == 0));
            g.drawText (lines[i], r.withTrimmedLeft (10.0f).withTrimmedTop (4.0f + 15.0f * (float) i).withHeight (15.0f), Justification::centredLeft, false);
        }
    }

    if (settings.frozen)
    {
        g.setColour (palette::red);
        g.setFont (uiFont (10.5f, true));
        g.drawText ("FROZEN", Rectangle<float> (plot.getX() + 8, plot.getY() + 6, 80, 14), Justification::centredLeft, false);
    }
}

} // namespace malon
