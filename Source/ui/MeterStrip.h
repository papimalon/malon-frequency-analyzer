// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../PluginProcessor.h"
#include "MalonLookAndFeel.h"
#include "OutMeter.h"

namespace malon
{

/** The pro metering strip: true peak | RMS | loudness | clip + reset.
    Pure display: it paints a LevelMeter::Readout that the editor hands it. */
class MeterStrip : public juce::Component
{
public:
    static constexpr int preferredHeight = 104;

    explicit MeterStrip (MalonProcessor&);

    /** liveL/liveR: decaying live true peak in dBTP (for the thin bar). */
    void setReadout (const LevelMeter::Readout& r, double liveL, double liveR, double dtMs = 33.0, bool updateCorr = true);
    CorrelationBar& getCorrelation() noexcept { return corr; }

    std::function<void()> onReset;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    /** "-23.0", or "--.-" while there is nothing to show. Exposed for tests. */
    static juce::String fmt (double db, int decimals = 1);

private:
    struct Layout { juce::Rectangle<int> tp, rms, loud, corr, ctl; };
    Layout layout() const;

    LevelMeter::Readout ro;
    double live[2] = { -200.0, -200.0 };

    juce::ComboBox rmsBox;
    juce::TextButton resetButton { "Reset" };
    std::unique_ptr<juce::ComboBoxParameterAttachment> rmsAtt;
    juce::Rectangle<int> ledArea;
    CorrelationBar corr;
};

} // namespace malon
