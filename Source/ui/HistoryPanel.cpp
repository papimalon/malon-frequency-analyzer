// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "HistoryPanel.h"

namespace malon
{
using namespace juce;

static constexpr double kTop = 3.0, kBottom = -54.0;        // same feel as the spectrum's default 3 dB / 57 dB

void HistoryView::clear()
{
    for (auto& h : hist) h.fill (-std::numeric_limits<float>::infinity());
    head = filled = 0;
    repaint();
}

void HistoryView::push (double lufsM, double rmsDb, double peakDb)
{
    hist[0][(size_t) head] = (float) lufsM;
    hist[1][(size_t) head] = (float) rmsDb;
    hist[2][(size_t) head] = (float) peakDb;
    head = (head + 1) % kPoints;
    filled = jmin (filled + 1, kPoints);
    if (isVisible()) repaint();
}

float HistoryView::valueAt (int age) const
{
    if (age < 0 || age >= filled) return -std::numeric_limits<float>::infinity();
    return hist[(size_t) mode][(size_t) ((head - 1 - age + 2 * kPoints) % kPoints)];
}

Rectangle<float> HistoryView::getPlotArea() const
{   // same margins as the spectrum, so switching modes doesn't jump
    return getLocalBounds().toFloat().withTrimmedLeft (36.0f).withTrimmedRight (36.0f).withTrimmedTop (16.0f).withTrimmedBottom (24.0f);
}

static String fmtDb (double v) { return std::isfinite (v) && v > -150.0 ? String (v, 1) : String ("-inf"); }

void HistoryView::paint (Graphics& g)
{
    using namespace palette;
    g.setGradientFill (ColourGradient (Colour (0xFF0B0D13), 0, 0, Colour (0xFF07080C), 0, (float) getHeight(), false));
    g.fillAll();
    const auto plot = getPlotArea();
    if (plot.getWidth() < 40 || plot.getHeight() < 30) return;

    auto yOf = [&] (double v) { return plot.getY() + plot.getHeight() * (float) ((kTop - jlimit (kBottom, kTop, v)) / (kTop - kBottom)); };
    auto xOfAge = [&] (double ageTicks) { return plot.getRight() - plot.getWidth() * (float) (ageTicks / (kPoints - 1)); };

    // grid: 6 dB lines, 1 s lines
    g.setFont (uiFont (10.5f));
    for (int lv = 0; lv > kBottom; lv -= 6)
    {
        const float y = std::round (yOf (lv)) + 0.5f;
        g.setColour (Colour (0x14FFFFFF));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (dim);
        g.drawText (String (lv), Rectangle<float> (0, y - 7, plot.getX() - 6, 14), Justification::centredRight, false);
        g.drawText (String (lv), Rectangle<float> (plot.getRight() + 6, y - 7, 30, 14), Justification::centredLeft, false);
    }
    for (int s = 0; s <= (int) kSecondsShown; ++s)
    {
        const float x = std::round (xOfAge (s * 31.0)) + 0.5f;
        if (x < plot.getX()) break;
        g.setColour (Colour (0x14FFFFFF));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (dim);
        g.drawText (s == 0 ? String ("now") : "-" + String (s) + "s", Rectangle<float> (x - 22, plot.getBottom() + 5, 44, 14), Justification::centred, false);
    }
    g.setColour (Colour (0x26FFFFFF));
    g.drawRect (plot.expanded (0.5f), 1.0f);

    const char* unit = mode == 0 ? "LUFS" : mode == 1 ? "dBFS" : "dBTP";
    const char* title = mode == 0 ? "MOMENTARY LOUDNESS" : mode == 1 ? "RMS" : "TRUE PEAK";

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.toNearestIntEdges());

    if (mode == 0)    // EBU R128 target
    {
        const float y = std::round (yOf (-23.0)) + 0.5f;
        g.setColour (red.withAlpha (0.55f));
        for (float x = plot.getX(); x < plot.getRight(); x += 8.0f) g.drawHorizontalLine ((int) y, x, jmin (x + 4.0f, plot.getRight()));
        g.setFont (uiFont (9.5f));
        g.drawText ("-23 LUFS", Rectangle<float> (plot.getX() + 8, y - 15, 70, 12), Justification::centredLeft, false);
    }

    if (filled > 1)
    {
        Path line, fill;
        bool open = false;
        for (int age = filled - 1; age >= 0; --age)
        {
            const float v = valueAt (age);
            const float x = xOfAge (age), y = std::isfinite (v) ? yOf (v) : plot.getBottom() + 2.0f;
            if (! open) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom() + 2.0f); fill.lineTo (x, y); open = true; }
            else { line.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (plot.getRight(), plot.getBottom() + 2.0f); fill.closeSubPath();
        g.setGradientFill (ColourGradient (blue.withAlpha (0.32f), 0, plot.getY(), blue.withAlpha (0.02f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (blue.brighter (0.3f));
        g.strokePath (line, PathStrokeType (1.75f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // title + latest value, top left (where the spectrum has its legend)
    g.setColour (dim);
    g.setFont (uiFont (9.5f).withExtraKerningFactor (0.12f));
    g.drawText (title, Rectangle<float> (plot.getX() + 10, plot.getY() + 7, 200, 12), Justification::centredLeft, false);
    g.setColour (text);
    g.setFont (uiFont (20.0f, true));
    g.drawText (fmtDb (valueAt (0)) + " " + unit, Rectangle<float> (plot.getX() + 10, plot.getY() + 20, 240, 26), Justification::centredLeft, false);

    if (! metersOn)
    {
        g.setColour (dim);
        g.setFont (uiFont (13.0f));
        g.drawText ("Turn on Meters (M) to record level history", plot, Justification::centred, false);
    }

    // hover: time + level at that time
    if (hasMouse && plot.contains (mouse))
    {
        const int age = jlimit (0, kPoints - 1, roundToInt ((plot.getRight() - mouse.x) / plot.getWidth() * (kPoints - 1)));
        const float v = valueAt (age), x = xOfAge (age);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (std::isfinite (v) && v > kBottom)
        {
            g.setColour (Colours::white);
            g.fillEllipse (x - 3.5f, yOf (v) - 3.5f, 7.0f, 7.0f);
        }
        const String t = age == 0 ? String ("now") : "-" + String (age / 31.0, 1) + " s";
        const String txt = t + "    " + fmtDb (v) + " " + unit;
        const float w = 150.0f, h = 22.0f;
        float bx = mouse.x + 14.0f, by = mouse.y + 14.0f;
        if (bx + w > plot.getRight() - 4.0f) bx = mouse.x - 14.0f - w;
        if (by + h > plot.getBottom() - 4.0f) by = mouse.y - 14.0f - h;
        auto r = Rectangle<float> (bx, by, w, h);
        g.setColour (Colour (0xF00C0E14));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.95f));
        g.setFont (uiFont (11.0f, true));
        g.drawText (txt, r, Justification::centred, false);
    }
}

} // namespace malon
