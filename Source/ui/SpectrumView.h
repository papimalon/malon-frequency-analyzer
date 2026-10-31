// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../ColorScheme.h"
#include "../Parameters.h"
#include "../dsp/Ballistics.h"

namespace malon
{

/** Everything the view needs besides the model (read from parameters by the editor). */
struct ViewSettings
{
    bool peakOn = true, holdOn = true, avgOn = true;
    ShowAs holdShow = ShowAs::Curve, avgShow = ShowAs::Curve, peakShow = ShowAs::Bars;
    int holdSize = 2, avgSize = 3;
    double ceiling = 3, range = 51, grid = 5;
    bool octaveGrid = false;
    bool frozen = false;
    double slope = 4.5;   // dB/oct, pivot 1 kHz (for the scale label)
};

class SpectrumView : public juce::Component
{
public:
    SpectrumView() { setOpaque (true); setRepaintsOnMouseActivity (false); }

    void setData (const DisplayModel* m) { model = m; }
    void setSettings (const ViewSettings& s) { settings = s; repaint(); }
    void setColors (const ColorScheme& c)    { colors = c; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override  { mouse = e.position; hasMouse = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { hasMouse = false; repaint(); }

    /** The inner plotting rectangle for the current size. */
    juce::Rectangle<float> getPlotArea() const;

    /** For screenshots/tests: pretend the mouse is at this position. */
    void simulateHover (juce::Point<float> p) { mouse = p; hasMouse = true; repaint(); }

private:
    void drawGrid (juce::Graphics&, juce::Rectangle<float> plot, int numBands);
    void drawCurve (juce::Graphics&, const double* levelDb, int n, juce::Rectangle<float> plot,
                    juce::Colour colour, int size, bool fill, float fillAlpha = 0.5f) const;
    float levelToY (double db, juce::Rectangle<float> plot) const;
    float bandX (int i, int n, juce::Rectangle<float> plot) const;

    const DisplayModel* model = nullptr;
    ViewSettings settings;
    ColorScheme colors = ColorScheme::defaults();
    juce::Point<float> mouse;
    bool hasMouse = false;
};

} // namespace malon
