// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "MeterStrip.h"

namespace malon
{
using namespace juce;

String MeterStrip::fmt (double db, int decimals)
{
    if (std::isnan (db)) return "--.-";
    if (! std::isfinite (db) || db < -150.0) return db > 0 ? "+inf" : "-inf";
    if (db > -0.05 && db < 0.0 && decimals == 1) return "0.0";       // never "-0.0"
    return String (db, decimals);
}

MeterStrip::MeterStrip (MalonProcessor& p)
{
    rmsBox.addItem ("100 ms", 1); rmsBox.addItem ("300 ms", 2); rmsBox.addItem ("1 s", 3); rmsBox.addItem ("3 s", 4);
    rmsBox.setTooltip ("RMS window");
    addAndMakeVisible (rmsBox);
    rmsAtt = std::make_unique<ComboBoxParameterAttachment> (*p.apvts.getParameter (ids::rmsWindow), rmsBox);

    resetButton.setTooltip ("Reset held peaks, integrated loudness, loudness range and the clip light");
    resetButton.onClick = [this] { if (onReset) onReset(); };
    addAndMakeVisible (resetButton);
    setOpaque (false);
}

MeterStrip::Layout MeterStrip::layout() const
{
    const bool narrow = getWidth() < 680;
    auto r = getLocalBounds().reduced (narrow ? 12 : 16, 10);
    Layout l;
    l.ctl = r.removeFromRight (narrow ? 80 : 96);
    const int gap = narrow ? 16 : 22;
    r.removeFromRight (gap);
    const int w = r.getWidth() - 2 * gap;
    const int wTp = jmax (100, (int) (w * 0.25f)), wRms = jmax (118, (int) (w * 0.27f));
    l.tp   = r.removeFromLeft (wTp);   r.removeFromLeft (gap);
    l.rms  = r.removeFromLeft (wRms);  r.removeFromLeft (gap);
    l.loud = r;
    return l;
}

void MeterStrip::resized()
{
    auto l = layout();
    rmsBox.setBounds (l.rms.getRight() - 62, l.rms.getY() - 3, 62, 20);
    resetButton.setBounds (l.ctl.getX(), l.ctl.getY() + 24, l.ctl.getWidth(), 26);
    ledArea = Rectangle<int> (l.ctl.getX(), l.ctl.getY(), l.ctl.getWidth(), 18);
}

void MeterStrip::setReadout (const LevelMeter::Readout& r, double liveL, double liveR)
{
    ro = r; live[0] = liveL; live[1] = liveR;
    repaint();
}

void MeterStrip::mouseUp (const MouseEvent& e)
{
    if (ledArea.contains (e.getPosition()) && onReset) onReset();     // the clip light doubles as a reset target
}

static float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga; ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void MeterStrip::paint (Graphics& g)
{
    using namespace palette;
    auto l = layout();

    // hairline above the strip: the only divider
    g.setColour (hairline);
    g.drawHorizontalLine (0, 12.0f, (float) getWidth() - 12.0f);

    auto header = [&] (Rectangle<int> col, const char* title)
    {
        g.setColour (dim);
        g.setFont (uiFont (10.0f).withExtraKerningFactor (0.16f));
        g.drawText (title, col.getX(), col.getY(), col.getWidth(), 14, Justification::centredLeft, false);
    };
    auto big = [&] (Rectangle<int> col, double value, const char* unit, Colour c)
    {
        const auto txt = fmt (value);
        const auto f = uiFont (26.0f, true);
        g.setColour (c);
        g.setFont (f);
        g.drawText (txt, col.getX(), col.getY() + 16, col.getWidth(), 32, Justification::centredLeft, false);
        g.setColour (dim);
        g.setFont (uiFont (10.5f).withExtraKerningFactor (0.08f));
        g.drawText (unit, col.getX() + (int) textWidth (f, txt) + 6, col.getY() + 16, 150, 32, Justification::centredLeft, false);
    };
    auto small = [&] (Rectangle<int> area, const String& text, Colour c = dim)
    {
        g.setColour (c);
        g.setFont (uiFont (11.0f));
        g.drawText (text, area, Justification::centredLeft, false);
    };

    const bool stereo = ro.channels > 1;

    // ---- true peak ----
    {
        header (l.tp, "TRUE PEAK");
        const bool over = std::isfinite (ro.truePeakMaxDb) && ro.truePeakMaxDb > 0.0;
        big (l.tp, ro.truePeakMaxDb, "dBTP", over ? red : text);

        // live level: one thin bar, -60 .. +3 dBTP, tick = held maximum
        auto bar = Rectangle<float> ((float) l.tp.getX(), (float) l.tp.getY() + 54.0f, (float) l.tp.getWidth(), 4.0f);
        auto toX = [&] (double db) { return bar.getX() + bar.getWidth() * (float) jlimit (0.0, 1.0, (db + 60.0) / 63.0); };
        g.setColour (Colour (0xFF1B1F2A));
        g.fillRoundedRectangle (bar, 2.0f);
        const double lv = jmax (live[0], stereo ? live[1] : live[0]);
        if (std::isfinite (lv) && lv > -60.0)
        {
            const float x1 = toX (lv);
            g.setColour (lv > -1.0 ? red : blue);
            g.fillRoundedRectangle (bar.withRight (jmax (bar.getX() + 3.0f, x1)), 2.0f);
        }
        g.setColour (text.withAlpha (0.35f));
        g.fillRect (toX (0.0), bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);                // 0 dBTP
        if (std::isfinite (ro.truePeakMaxDb) && ro.truePeakMaxDb > -60.0)
        {
            g.setColour (text);
            g.fillRect (toX (ro.truePeakMaxDb) - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
        }
        small (Rectangle<int> (l.tp.getX(), l.tp.getY() + 62, l.tp.getWidth(), 16),
               stereo ? "L " + fmt (ro.truePeakDb[0]) + "     R " + fmt (ro.truePeakDb[1]) : "M " + fmt (ro.truePeakDb[0]));
    }

    // ---- RMS ----
    {
        header (l.rms, "RMS");
        big (l.rms, ro.rmsCombinedDb, "dBFS", text);
        small (Rectangle<int> (l.rms.getX(), l.rms.getY() + 54, l.rms.getWidth(), 16),
               stereo ? "L " + fmt (ro.rmsDb[0]) + "     R " + fmt (ro.rmsDb[1]) : "M " + fmt (ro.rmsDb[0]));
    }

    // ---- loudness ----
    {
        header (l.loud, "LOUDNESS");
        const int cw = l.loud.getWidth() / 3;
        const bool tight = cw < 82;
        big (l.loud, ro.integrated, tight ? "LUFS  INT" : "LUFS  INTEGRATED", text);
        struct Cell { const char* name; String value; };
        const Cell cells[3] = { { tight ? "MOM" : "MOMENTARY", fmt (ro.momentary) },
                                { tight ? "SHORT" : "SHORT-TERM", fmt (ro.shortTerm) },
                                { tight ? "LRA" : "RANGE", fmt (ro.loudnessRange) + (std::isfinite (ro.loudnessRange) ? " LU" : "") } };
        for (int i = 0; i < 3; ++i)
        {
            auto c = Rectangle<int> (l.loud.getX() + i * cw, l.loud.getY() + 52, cw, 30);
            g.setColour (dim.withAlpha (0.8f));
            g.setFont (uiFont (9.0f).withExtraKerningFactor (0.14f));
            g.drawText (cells[i].name, c.removeFromTop (12), Justification::centredLeft, false);
            g.setColour (text);
            g.setFont (uiFont (13.0f, true));
            g.drawText (cells[i].value, c, Justification::centredLeft, false);
        }
    }

    // ---- stereo correlation (under the reset button) ----
    {
        auto c = Rectangle<int> (l.ctl.getX(), l.ctl.getY() + 58, l.ctl.getWidth(), 14);
        g.setColour (dim);
        g.setFont (uiFont (9.0f).withExtraKerningFactor (0.14f));
        g.drawText ("CORR", c, Justification::centredLeft, false);
        const bool ok = ro.correlationValid && stereo;
        const double v = ok ? ro.correlation : 0.0;
        g.setColour (ok ? (v < 0.0 ? red : text) : dim);
        g.setFont (uiFont (12.0f, true));
        g.drawText (ok ? (v >= 0.0 ? "+" : "") + String (v, 2) : String ("--"), c, Justification::centredRight, false);
        auto bar = Rectangle<float> ((float) c.getX(), (float) c.getBottom() + 5.0f, (float) c.getWidth(), 3.0f);
        auto toX = [&] (double x) { return bar.getX() + bar.getWidth() * (float) ((jlimit (-1.0, 1.0, x) + 1.0) * 0.5); };
        g.setColour (Colour (0xFF1B1F2A));
        g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (text.withAlpha (0.35f));
        g.fillRect (toX (0.0), bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
        if (ok)
        {
            const float xv = toX (v), xc = toX (0.0);
            g.setColour (v < 0.0 ? red : blue);
            g.fillRoundedRectangle (Rectangle<float> (jmin (xv, xc), bar.getY(), jmax (2.0f, std::abs (xv - xc)), bar.getHeight()), 1.5f);
            g.setColour (text);
            g.fillRect (xv - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
        }
    }

    // ---- clip light ----
    {
        const float d = 10.0f;
        const float cx = (float) ledArea.getX() + d * 0.5f + 1.0f, cy = (float) ledArea.getCentreY();
        if (ro.clipped)
        {
            g.setColour (red.withAlpha (0.22f));
            g.fillEllipse (cx - d, cy - d, d * 2.0f, d * 2.0f);
            g.setColour (red);
            g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);
        }
        else
        {
            g.setColour (Colour (0xFF1B1F2A));
            g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);
        }
        g.setColour (ro.clipped ? red : dim);
        g.setFont (uiFont (10.0f).withExtraKerningFactor (0.16f));
        g.drawText ("CLIP", (int) (cx + d + 3.0f), ledArea.getY(), 50, ledArea.getHeight(), Justification::centredLeft, false);
    }
}

} // namespace malon
