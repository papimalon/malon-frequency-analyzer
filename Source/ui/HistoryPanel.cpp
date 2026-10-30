// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "HistoryPanel.h"

namespace malon
{
using namespace juce;

static constexpr float kTop = 0.0f, kBottom = -42.0f;       // graph range (dB / LUFS)

HistoryPanel::HistoryPanel (MalonProcessor& p) : proc (p)
{
    for (auto& h : hist) h.fill (-std::numeric_limits<float>::infinity());
    const char* tips[3] = { "Momentary loudness (LUFS, 400 ms) over the last 10 s",
                            "RMS level (dBFS) over the last 10 s", "True peak (dBTP) over the last 10 s" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[i];
        b.setClickingTogglesState (false);
        b.setTooltip (tips[i]);
        b.setConnectedEdges ((i > 0 ? Button::ConnectedOnLeft : 0) | (i < 2 ? Button::ConnectedOnRight : 0));
        b.onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter (ids::historyMode))
            { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); }
            setMode (i);
        };
        addAndMakeVisible (b);
    }
    proc.apvts.addParameterListener (ids::historyMode, this);
    setMode ((int) proc.apvts.getRawParameterValue (ids::historyMode)->load());
    setOpaque (false);
}

HistoryPanel::~HistoryPanel() { proc.apvts.removeParameterListener (ids::historyMode, this); }

void HistoryPanel::parameterChanged (const String&, float v)
{
    const int m = jlimit (0, 2, roundToInt (v));
    MessageManager::callAsync ([sp = SafePointer<HistoryPanel> (this), m] { if (sp) sp->setMode (m); });
}

void HistoryPanel::setMode (int m)
{
    mode = jlimit (0, 2, m);
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setToggleState (i == mode, dontSendNotification);
        modeButtons[i].setColour (TextButton::buttonOnColourId, palette::blue);
    }
    repaint();
}

void HistoryPanel::clear()
{
    for (auto& h : hist) h.fill (-std::numeric_limits<float>::infinity());
    head = filled = 0; corrMin = 1.0;
    repaint();
}

void HistoryPanel::push (double lufsM, double rmsDb, double peakDb, bool valid, double corr)
{
    const int m = jlimit (0, 2, roundToInt (proc.apvts.getRawParameterValue (ids::historyMode)->load()));
    if (m != mode) setMode (m);           // follows host automation / state recall
    hist[0][(size_t) head] = (float) lufsM;
    hist[1][(size_t) head] = (float) rmsDb;
    hist[2][(size_t) head] = (float) peakDb;
    head = (head + 1) % kPoints;
    filled = jmin (filled + 1, kPoints);
    corrValid = valid;
    if (valid)
    {
        corrShown += 0.35 * (corr - corrShown);              // a little visual smoothing
        corrMin = jmin (corrMin, corr);
    }
    repaint();
}

void HistoryPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 8);
    corrArea = r.removeFromRight (jlimit (150, 220, r.getWidth() / 4));
    r.removeFromRight (18);
    auto sel = r.removeFromTop (20);
    const int bw = 50;
    for (int i = 0; i < 3; ++i) modeButtons[i].setBounds (sel.getX() + 70 + i * bw, sel.getY(), bw, 20);
    r.removeFromTop (6);
    graphArea = r;
}

static String fmtDb (double v) { return std::isfinite (v) ? String (v, 1) : String ("-inf"); }

void HistoryPanel::paint (Graphics& g)
{
    using namespace palette;
    g.setColour (hairline);
    g.drawHorizontalLine (0, 12.0f, (float) getWidth() - 12.0f);

    // ---- header ----
    g.setColour (dim);
    g.setFont (uiFont (10.0f).withExtraKerningFactor (0.16f));
    g.drawText ("HISTORY", graphArea.getX(), modeButtons[0].getY(), 66, 20, Justification::centredLeft, false);

    const auto& h = hist[(size_t) mode];
    const float latest = filled > 0 ? h[(size_t) ((head - 1 + kPoints) % kPoints)] : -std::numeric_limits<float>::infinity();
    const char* unit = mode == 0 ? "LUFS M" : mode == 1 ? "dBFS" : "dBTP";
    g.setColour (text);
    g.setFont (uiFont (12.0f, true));
    g.drawText (fmtDb (latest) + "  " + unit, modeButtons[2].getRight() + 12, modeButtons[0].getY(),
                graphArea.getRight() - modeButtons[2].getRight() - 12, 20, Justification::centredRight, false);

    // ---- graph ----
    auto gr = graphArea.toFloat();
    g.setColour (Colour (0xFF0E1118));
    g.fillRoundedRectangle (gr, 4.0f);
    auto yOf = [&] (double v) { return gr.getY() + gr.getHeight() * (float) ((kTop - jlimit ((double) kBottom, (double) kTop, v)) / (kTop - kBottom)); };
    g.setFont (uiFont (9.0f));
    for (int lv : { -6, -12, -18, -24, -30, -36 })
    {
        const float y = std::round (yOf (lv)) + 0.5f;
        g.setColour (hairline);
        g.drawHorizontalLine ((int) y, gr.getX() + 22.0f, gr.getRight());
        g.setColour (dim.withAlpha (0.8f));
        g.drawText (String (lv), Rectangle<float> (gr.getX() + 2.0f, y - 6.0f, 20.0f, 12.0f), Justification::centredLeft, false);
    }
    if (mode == 0)    // EBU R128 target line
    {
        g.setColour (red.withAlpha (0.45f));
        const float y = std::round (yOf (-23.0)) + 0.5f;
        for (float x = gr.getX() + 22.0f; x < gr.getRight(); x += 6.0f) g.drawHorizontalLine ((int) y, x, jmin (x + 3.0f, gr.getRight()));
    }

    if (filled > 1)
    {
        const float x0 = gr.getX() + 22.0f, w = gr.getRight() - x0;
        Path line, fill;
        bool started = false;
        for (int i = 0; i < filled; ++i)
        {
            const int idx = (head - filled + i + kPoints) % kPoints;
            const float v = h[(size_t) idx];
            const float x = x0 + w * (float) (kPoints - filled + i) / (float) (kPoints - 1);
            const float y = std::isfinite (v) ? yOf (v) : gr.getBottom();
            if (! started) { line.startNewSubPath (x, y); fill.startNewSubPath (x, gr.getBottom()); fill.lineTo (x, y); started = true; }
            else { line.lineTo (x, y); fill.lineTo (x, y); }
            if (i == filled - 1) { fill.lineTo (x, gr.getBottom()); fill.closeSubPath(); }
        }
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gr.toNearestInt());
        g.setGradientFill (ColourGradient (blue.withAlpha (0.28f), 0, gr.getY(), blue.withAlpha (0.0f), 0, gr.getBottom(), false));
        g.fillPath (fill);
        g.setColour (blue.brighter (0.25f));
        g.strokePath (line, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ---- correlation ----
    {
        auto c = corrArea;
        g.setColour (dim);
        g.setFont (uiFont (10.0f).withExtraKerningFactor (0.16f));
        g.drawText ("CORRELATION", c.removeFromTop (20), Justification::centredLeft, false);
        c.removeFromTop (6);
        const double v = corrValid ? corrShown : 0.0;
        g.setColour (corrValid ? (v < 0.0 ? red : text) : dim);
        g.setFont (uiFont (22.0f, true));
        g.drawText (corrValid ? (v >= 0 ? "+" : "") + String (v, 2) : String ("--"), c.removeFromTop (26), Justification::centredLeft, false);

        auto bar = Rectangle<float> ((float) c.getX(), (float) c.getY() + 6.0f, (float) c.getWidth(), 4.0f);
        auto toX = [&] (double x) { return bar.getX() + bar.getWidth() * (float) ((jlimit (-1.0, 1.0, x) + 1.0) * 0.5); };
        g.setColour (Colour (0xFF1B1F2A));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (text.withAlpha (0.35f));
        g.fillRect (toX (0.0), bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
        if (corrValid)
        {
            const float xv = toX (v), xc = toX (0.0);
            g.setColour (v < 0.0 ? red : blue);
            g.fillRoundedRectangle (Rectangle<float> (jmin (xv, xc), bar.getY(), jmax (2.0f, std::abs (xv - xc)), bar.getHeight()), 2.0f);
            g.setColour (text);
            g.fillRect (xv - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
        }
        g.setColour (dim.withAlpha (0.8f));
        g.setFont (uiFont (9.0f));
        auto lab = Rectangle<float> (bar.getX(), bar.getBottom() + 4.0f, bar.getWidth(), 11.0f);
        g.drawText ("-1", lab, Justification::centredLeft, false);
        g.drawText ("0", lab, Justification::centred, false);
        g.drawText ("+1", lab, Justification::centredRight, false);
    }
}

} // namespace malon
