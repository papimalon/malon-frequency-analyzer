// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// SPAN-style "Out" level meter (right edge of the graph) and correlation bar (bottom of the graph).
// Pure display: the editor feeds them values from LevelMeter every UI tick.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MalonLookAndFeel.h"
#include "MeterScale.h"
#include <cmath>
#include <functional>

namespace malon
{

/** Vertical L/R meter: RMS bar (solid), peak bar (lighter, behind), peak-hold tick, dB scale, clip LED. */
class OutMeter : public juce::Component
{
public:
    static constexpr int preferredWidth = 52;
    void setScale (const MeterScaleSpec& s) { if (juce::String (s.unit) != spec.unit || s.top != spec.top || s.bottom != spec.bottom || s.loudness != spec.loudness) { spec = s; reset(); } }
    const MeterScaleSpec& getScale() const { return spec; }

    /** peakDb: live (decaying) true peak per channel; rmsDb: 300 ms RMS per channel. */
    void setLevels (const double peakDb[2], const double rmsDb[2], int channels, bool clipped, double dtMs)
    {
        nch = juce::jlimit (1, 2, channels);
        for (int c = 0; c < 2; ++c)
        {
            pk[c] = peakDb[c]; rms[c] = rmsDb[c];
            holdLeft[c] -= dtMs;
            if (holdLeft[c] <= 0.0) hold[c] -= 20.0 * dtMs / 1000.0;          // 2 s hold, then 20 dB/s fall
            const double src = spec.loudness ? rms[c] : pk[c];       // loudness scales hold the loudness
            if (std::isfinite (src) && src >= hold[c]) { hold[c] = src; holdLeft[c] = 2000.0; }
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
        const bool plain = juce::String (spec.unit) == "dBFS";
        if (! plain) g.setFont (uiFont (8.5f, true));
        g.drawText (plain ? juce::String ("OUT") : juce::String (spec.unit), b.withHeight (14.0f).withTrimmedLeft (4.0f), Justification::centredLeft, false);
        auto led = ledArea();
        g.setColour (clip ? palette::red : palette::red.withAlpha (0.18f));
        g.fillEllipse (led);

        const auto bars = barArea();
        const float bw = (bars.getWidth() - 2.0f) / 2.0f;
        g.setFont (uiFont (8.5f));
        const int tk = spec.tick, topI = (int) std::floor (spec.top / tk) * tk;
        for (int db = topI; db >= (int) spec.bottom; db -= tk)    // scale on the left
        {
            const float y = dbToY ((double) db, bars);
            g.setColour (Colours::white.withAlpha (db == 0 ? 0.25f : 0.08f));
            g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
            if (db % (2 * tk) == 0)
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
                if (! std::isfinite (db) || db <= spec.bottom) return;
                const float y = dbToY (db, bars);
                g.setColour (cl); g.fillRect (col.withTop (y));
            };
            fill (pk[src],  palette::blue.withAlpha (0.45f));         // peak, light
            fill (rms[src], palette::blue);                           // RMS, solid blue (house colour)
            if (hold[src] > spec.bottom)
            {
                const float y = dbToY (hold[src], bars);
                g.setColour (hold[src] >= spec.redAt ? palette::red : Colours::white.withAlpha (0.9f));
                g.fillRect (col.getX(), y - 1.0f, col.getWidth(), 2.0f);
            }
            g.setColour (palette::dim);
            g.drawText (c == 0 ? "L" : "R", Rectangle<float> (col.getX(), bars.getBottom() + 1, col.getWidth(), 11), Justification::centred, false);
        }
        // numeric peak hold (max of L/R)
        const double hmax = std::max (hold[0], nch > 1 ? hold[1] : hold[0]);
        g.setColour (hmax >= spec.redAt ? palette::red : palette::text);
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
        const double t = (spec.top - juce::jlimit (spec.bottom, spec.top, db)) / (spec.top - spec.bottom);
        return r.getY() + (float) t * r.getHeight();
    }

    double pk[2] { -200.0, -200.0 }, rms[2] { -200.0, -200.0 }, hold[2] { -200.0, -200.0 }, holdLeft[2] {};
    int nch = 2;
    bool clip = false;
    MeterScaleSpec spec = meterScaleSpec (0);
};

/** SPAN-style correlation meter: "Correlation" title, gradient track (-1 red .. 0 yellow .. +1 blue),
    a value indicator with 1.5 s min/max hold, a numeric read-out, a -1.00 .. 1.00 scale and an L/R balance read-out.
    Value = Pearson L/R correlation from LevelMeter (~300 ms). Lives in the meter strip. */
class CorrelationBar : public juce::Component
{
public:
    static constexpr int preferredHeight = 80;

    void setValue (bool valid, double v, double dtMs)
    {
        ok = valid; val = juce::jlimit (-1.0, 1.0, v);
        if (! ok) { mn = mx = 0.0; repaint(); return; }
        tLeft -= dtMs;
        if (tLeft <= 0.0) { mn = mx = val; tLeft = 1500.0; }
        mn = std::min (mn, val); mx = std::max (mx, val);
        repaint();
    }
    /** balance in dB (R minus L), NaN = unknown */
    void setBalance (double db) { bal = db; }
    void setCompact (bool c) { compact = c; }
    double getMin() const { return mn; }
    double getMax() const { return mx; }

    static juce::Colour colourAt (double c)
    {
        using juce::Colour;
        const Colour cr (0xFFD23B3B), cy (0xFFE8C547), cb (0xFF3D8BFF);
        if (c < 0.0) return cr.interpolatedWith (cy, (float) juce::jlimit (0.0, 1.0, c + 1.0));
        return cy.interpolatedWith (cb, (float) juce::jlimit (0.0, 1.0, c));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        auto top = b.removeFromTop (14.0f);
        g.setFont (uiFont (10.0f).withExtraKerningFactor (0.16f));
        g.setColour (palette::dim);
        g.drawText (compact ? "CORR" : "CORRELATION", top, Justification::centredLeft, false);
        g.setFont (uiFont (13.0f, true));
        g.setColour (ok ? colourAt (val).brighter (0.2f) : palette::dim);
        g.drawText (ok ? (val >= 0 ? "+" : "") + String (val, 2) : String ("--"), top.withTrimmedTop (-2.0f), Justification::centredRight, false);

        b.removeFromTop (compact ? 6.0f : 8.0f);
        auto bar = b.removeFromTop (compact ? 6.0f : 12.0f).reduced (compact ? 0.0f : 4.0f, 0.0f);
        auto xOf = [&] (double c) { return bar.getX() + (float) ((jlimit (-1.0, 1.0, c) + 1.0) * 0.5) * bar.getWidth(); };

        ColourGradient grad (colourAt (-1.0).darker (0.6f), bar.getX(), 0, colourAt (1.0), bar.getRight(), 0, false);
        grad.addColour (0.5, colourAt (0.0));
        g.setGradientFill (grad);
        g.setOpacity (ok ? 0.5f : 0.25f);
        g.fillRect (bar);
        g.setOpacity (1.0f);
        if (ok)
        {   // lit span between min and max hold, full colour
            g.setGradientFill (grad);
            const float x0 = xOf (mn) - 1.0f, x1 = xOf (mx) + 1.0f;
            g.fillRect (Rectangle<float>::leftTopRightBottom (x0, bar.getY() + (compact ? 1.0f : 3.0f), x1, bar.getBottom() - (compact ? 1.0f : 3.0f)));
        }
        g.setColour (Colours::black.withAlpha (0.45f));
        g.drawRect (bar, 1.0f);
        for (double t : { -0.5, 0.0, 0.5 })
        {
            g.setColour (Colours::white.withAlpha (t == 0.0 ? 0.45f : 0.22f));
            g.drawVerticalLine ((int) xOf (t), bar.getY(), bar.getBottom());
        }
        if (ok)
        {
            g.setColour (Colours::white.withAlpha (0.55f));
            g.fillRect (xOf (mn) - 1.0f, bar.getY(), 2.0f, bar.getHeight());     // min hold
            g.fillRect (xOf (mx) - 1.0f, bar.getY(), 2.0f, bar.getHeight());     // max hold
            const float x = xOf (val);
            g.setColour (Colours::white);
            g.fillRect (x - 1.5f, bar.getY() - 3.0f, 3.0f, bar.getHeight() + 6.0f);    // current value
        }

        if (compact) return;
        // scale
        auto sc = b.removeFromTop (14.0f);
        g.setFont (uiFont (8.5f)); g.setColour (palette::dim);
        const double ticks[5] = { -1.0, -0.5, 0.0, 0.5, 1.0 };
        const char* lab[5] = { "-1.00", "-0.50", "0.00", "0.50", "1.00" };
        for (int i = 0; i < 5; ++i)
        {
            const float x = xOf (ticks[i]);
            auto j = i == 0 ? Justification::centredLeft : i == 4 ? Justification::centredRight : Justification::centred;
            auto r = i == 0 ? Rectangle<float> (bar.getX() - 4.0f, sc.getY(), 34.0f, sc.getHeight())
                   : i == 4 ? Rectangle<float> (bar.getRight() - 30.0f, sc.getY(), 34.0f, sc.getHeight())
                            : Rectangle<float> (x - 17.0f, sc.getY(), 34.0f, sc.getHeight());
            g.drawText (lab[i], r, j, false);
        }

        // balance
        b.removeFromTop (4.0f);
        auto br = b.removeFromTop (14.0f);
        g.setFont (uiFont (9.0f).withExtraKerningFactor (0.14f));
        g.setColour (palette::dim.withAlpha (0.8f));
        g.drawText ("BAL", br, Justification::centredLeft, false);
        String bt = "--";
        if (std::isfinite (bal))
            bt = std::abs (bal) < 0.05 ? String ("C  0.0 dB") : (bal < 0 ? "L " : "R ") + String (std::abs (bal), 1) + " dB";
        g.setFont (uiFont (11.0f, true)); g.setColour (palette::text);
        g.drawText (bt, br, Justification::centredRight, false);
    }

private:
    bool ok = false, compact = false;
    double val = 0.0, mn = 0.0, mx = 0.0, tLeft = 0.0, bal = std::numeric_limits<double>::quiet_NaN();
};

} // namespace malon
