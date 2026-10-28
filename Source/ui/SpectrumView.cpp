// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Own drawing code. Band layout / grid frequencies follow the original analyzer's behaviour
// (bars evenly spaced = log-frequency, decade or octave grid); art and colours are ours.
#include "SpectrumView.h"
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


    // pointer read-out
    if (hasMouse && plot.contains (mouse) && model->freqMax > model->freqMin)
    {
        const float x0 = plot.getX() + 0.5f * slot, span = plot.getWidth() - slot;
        const double t = juce::jlimit (0.0, 1.0, (double) ((mouse.x - x0) / span));
        const double hz = model->freqMin * std::exp (std::log (model->freqMax / model->freqMin) * t);
        const double db = settings.ceiling - (mouse.y - plot.getY()) / (plot.getHeight() / settings.range);
        g.setColour (colors[ColorScheme::GridLabel].withAlpha (0.5f));
        g.drawVerticalLine ((int) mouse.x, plot.getY(), plot.getBottom());
        g.drawHorizontalLine ((int) mouse.y, plot.getX(), plot.getRight());
        const String text = String (roundToInt (hz)) + " Hz   " + String (db, 1) + " dB";
        auto r = Rectangle<float> (plot.getRight() - 128, plot.getY() + 6, 122, 18);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.setFont (uiFont (11.0f));
        g.drawText (text, r, Justification::centred, false);
    }

    if (settings.frozen)
    {
        g.setColour (palette::red);
        g.setFont (uiFont (10.5f, true));
        g.drawText ("FROZEN", Rectangle<float> (plot.getX() + 8, plot.getY() + 6, 80, 14), Justification::centredLeft, false);
    }
}

} // namespace malon
