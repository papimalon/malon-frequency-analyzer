// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "MalonLookAndFeel.h"

namespace malon
{

/** Full-size level-over-time graph (LUFS momentary / RMS / true peak). Shares the main graph area
    with the spectrum; the editor chooses which one is visible. Always records, so switching is instant. */
class HistoryView : public juce::Component
{
public:
    static constexpr int kPoints = 310;                     // ~10 s at the 31 Hz UI rate
    static constexpr double kSecondsShown = kPoints / 31.0;

    HistoryView() { setOpaque (true); clear(); }

    /** 0 = LUFS, 1 = RMS, 2 = peak */
    void setMode (int m) { mode = juce::jlimit (0, 2, m); repaint(); }
    int  getMode() const noexcept { return mode; }
    void setMetersOn (bool on) { metersOn = on; repaint(); }

    /** One UI tick: momentary LUFS, RMS (dBFS), peak since last tick (dBTP). */
    void push (double lufsM, double rmsDb, double peakDb);
    void clear();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override { mouse = e.position; hasMouse = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { hasMouse = false; repaint(); }
    void simulateHover (juce::Point<float> p) { mouse = p; hasMouse = true; repaint(); }
    juce::Rectangle<float> getPlotArea() const;

private:
    float valueAt (int age) const;                          // age 0 = newest
    std::array<std::array<float, kPoints>, 3> hist {};
    int head = 0, filled = 0, mode = 0;
    bool metersOn = true, hasMouse = false;
    juce::Point<float> mouse;
};

} // namespace malon
