// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "../PluginProcessor.h"
#include "MalonLookAndFeel.h"

namespace malon
{

/** Level over time (LUFS momentary / RMS / true peak, chosen with a selector) + stereo correlation meter. */
class HistoryPanel : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int preferredHeight = 118;
    static constexpr int kPoints = 310;                     // ~10 s at the 31 Hz UI rate

    explicit HistoryPanel (MalonProcessor&);
    ~HistoryPanel() override;

    /** One UI tick: momentary LUFS, RMS (dBFS), peak since last tick (dBTP), correlation. */
    void push (double lufsM, double rmsDb, double peakDb, bool corrValid, double corr);
    void clear();

    int  getMode() const noexcept { return mode; }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String&, float) override;
    void setMode (int m);

    MalonProcessor& proc;
    std::array<std::array<float, kPoints>, 3> hist {};
    int head = 0, filled = 0, mode = 0;
    bool corrValid = false; double corrShown = 0.0, corrMin = 1.0;
    juce::TextButton modeButtons[3] { juce::TextButton ("LUFS"), juce::TextButton ("RMS"), juce::TextButton ("PEAK") };
    juce::Rectangle<int> graphArea, corrArea;
};

} // namespace malon
