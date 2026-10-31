// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// SPAN-style "Out" level meter (right edge of the graph) and correlation bar (bottom of the graph).
// Pure display: the editor feeds them values from LevelMeter every UI tick.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MalonLookAndFeel.h"
#include <cmath>
#include <functional>

namespace malon
{

/** Vertical L/R meter: RMS bar (solid), peak bar (lighter, behind), peak-hold tick, dB scale, clip LED. */
class OutMeter : public juce::Component
{
public:
    static constexpr int preferredWidth = 52;
    static constexpr double topDb = 0.0, bottomDb = -60.0;

    /** peakDb: live (decaying) true peak per channel; rmsDb: 300 ms RMS per channel. */
    void setLevels (const double peakDb[2], const double rmsDb[2], int channels, bool clipped, double dtMs)
    {
        nch = juce::jlimit (1, 2, channels);
        for (int c = 0; c < 2; ++c)
        {
            pk[c] = peakDb[c]; rms[c] = rmsDb[c];
            holdLeft[c] -= dtMs;
            if (holdLeft[c] <= 0.0) hold[c] -= 20.0 * dtMs / 1000.0;          // 2 s hold, then 20 dB/s fall
            if (std::isfinite (pk[c]) && pk[c] >= hold[c]) { hold[c] = pk[c]; holdLeft[c] = 2000.0; }
            hold[c] = std::max (hold[c], -200.0);
        }
        clip = clipped;
        repaint();
    }
    void reset() { for (int c = 0; c < 2; ++c) { hold[c] = -200.0; holdLeft[c] = 0.0; } repaint(); }

    std::function<void()> onClipClicked;
    void mouseUp (const juce::MouseEvent& e) override { if (ledArea().contains (e.position) && onClipClicked) onClipClicked(); }

    double getHoldDb (int c) const { return hold[c]; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (palette::ink.brighter (0.06f)); g.fillRoundedRectangle (b, 4.0f);

        // header: OUT + clip LED
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.12f));
        g.setColour (palette::dim);
        g.drawText ("OUT", b.withHeight (14.0f).withTrimmedLeft (4.0f), Justification::centredLeft, false);
        auto led = ledArea();
        g.setColour (clip ? palette::red : palette::red.withAlpha (0.18f));
        g.fillEllipse (led);

        const auto bars = barArea();
        const float bw = (bars.getWidth() - 2.0f) / 2.0f;
        g.setFont (uiFont (8.5f));
        for (int db = 0; db >= (int) bottomDb; db -= 6)            // scale on the left
        {
            const float y = dbToY ((double) db, bars);
            g.setColour (Colours::white.withAlpha (db == 0 ? 0.25f : 0.08f));
            g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
            if (db % 12 == 0)
            {
                g.setColour (palette::dim);
                g.drawText (String (db), Rectangle<float> (0, y - 6, bars.getX() - 2, 12), Justification::centredRight, false);
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            const int src = nch > 1 ? c : 0;
            auto col = Rectangle<float> (bars.getX() + (float) c * (bw + 2.0f), bars.getY(), bw, bars.getHeight());
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (col);
            auto fill = [&] (double db, Colour cl)
            {
                if (! std::isfinite (db) || db <= bottomDb) return;
                const float y = dbToY (db, bars);
                g.setColour (cl); g.fillRect (col.withTop (y));
            };
            fill (pk[src],  palette::blue.withAlpha (0.45f));         // peak, light
            fill (rms[src], palette::blue);                           // RMS, solid blue (house colour)
            if (hold[src] > bottomDb)
            {
                const float y = dbToY (hold[src], bars);
                g.setColour (hold[src] >= 0.0 ? palette::red : Colours::white.withAlpha (0.9f));
                g.fillRect (col.getX(), y - 1.0f, col.getWidth(), 2.0f);
            }
            g.setColour (palette::dim);
            g.drawText (c == 0 ? "L" : "R", Rectangle<float> (col.getX(), bars.getBottom() + 1, col.getWidth(), 11), Justification::centred, false);
        }
        // numeric peak hold (max of L/R)
        const double hmax = std::max (hold[0], nch > 1 ? hold[1] : hold[0]);
        g.setColour (hmax >= 0.0 ? palette::red : palette::text);
        g.setFont (uiFont (9.0f, true));
        g.drawText (hmax <= -150.0 ? String ("-inf") : String (hmax, 1), b.removeFromBottom (13.0f), Justification::centred, false);
    }

private:
    juce::Rectangle<float> ledArea() const { return { (float) getWidth() - 13.0f, 3.0f, 8.0f, 8.0f }; }
    juce::Rectangle<float> barArea() const
    {
        auto b = getLocalBounds().toFloat();
        return b.withTrimmedTop (20.0f).withTrimmedBottom (27.0f).withTrimmedLeft (22.0f).withTrimmedRight (4.0f);
    }
    float dbToY (double db, juce::Rectangle<float> r) const
    {
        const double t = (topDb - juce::jlimit (bottomDb, topDb, db)) / (topDb - bottomDb);
        return r.getY() + (float) t * r.getHeight();
    }

    double pk[2] { -200.0, -200.0 }, rms[2] { -200.0, -200.0 }, hold[2] { -200.0, -200.0 }, holdLeft[2] {};
    int nch = 2;
    bool clip = false;
};

/** Horizontal correlation bar, -1 .. +1, 0 in the middle, red zone below 0,
    smoothed value (meter: Pearson L/R, ~300 ms) plus a 1.5 s min/max hold and a number. */
class CorrelationBar : public juce::Component
{
public:
    static constexpr int preferredHeight = 20;

    void setValue (bool valid, double v, double dtMs)
    {
        ok = valid; val = juce::jlimit (-1.0, 1.0, v);
        if (! ok) { mn = mx = 0.0; repaint(); return; }
        tLeft -= dtMs;
        if (tLeft <= 0.0) { mn = mx = val; tLeft = 1500.0; }
        mn = std::min (mn, val); mx = std::max (mx, val);
        repaint();
    }
    double getMin() const { return mn; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setFont (uiFont (9.0f, true).withExtraKerningFactor (0.1f));
        g.setColour (palette::dim);
        g.drawText ("CORRELATION", b.removeFromLeft (84.0f), Justification::centredLeft, false);
        auto num = b.removeFromRight (40.0f);
        g.setColour (ok && val < 0.0 ? palette::red : palette::text);
        g.drawText (ok ? (val >= 0 ? "+" : "") + String (val, 2) : String ("--"), num, Justification::centredRight, false);

        auto bar = b.reduced (4.0f, 5.0f);
        auto xOf = [&] (double c) { return bar.getX() + (float) ((c + 1.0) * 0.5) * bar.getWidth(); };
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (bar);
        g.setColour (palette::red.withAlpha (0.22f)); g.fillRect (bar.withRight (xOf (0.0)));       // red zone
        g.setColour (Colours::white.withAlpha (0.25f));
        for (double t : { -0.5, 0.0, 0.5 }) g.drawVerticalLine ((int) xOf (t), bar.getY() - 2, bar.getBottom() + 2);
        if (ok)
        {
            const float x0 = xOf (0.0), x = xOf (val);
            g.setColour (val < 0.0 ? palette::red : palette::blue);
            g.fillRect (Rectangle<float>::leftTopRightBottom (std::min (x0, x), bar.getY(), std::max (x0, x), bar.getBottom()));
            g.setColour (Colours::white.withAlpha (0.8f));
            g.fillRect (xOf (mn) - 1.0f, bar.getY() - 2, 2.0f, bar.getHeight() + 4);     // min hold
            g.setColour (Colours::white.withAlpha (0.4f));
            g.fillRect (xOf (mx) - 1.0f, bar.getY() - 2, 2.0f, bar.getHeight() + 4);     // max hold
        }
        g.setFont (uiFont (8.0f)); g.setColour (palette::dim);
        g.drawText ("-1", bar.withWidth (14).translated (-1, 0), Justification::centredLeft, false);
        g.drawText ("+1", bar.withTrimmedLeft (bar.getWidth() - 14), Justification::centredRight, false);
    }

private:
    bool ok = false;
    double val = 0.0, mn = 0.0, mx = 0.0, tLeft = 0.0;
};

} // namespace malon
