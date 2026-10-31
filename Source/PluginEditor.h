// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MalonLookAndFeel.h"
#include "ui/SpectrumView.h"
#include "ui/OutMeter.h"
#include "ui/SettingsPanel.h"
#include "ui/MeterStrip.h"
#include "ui/HistoryPanel.h"

namespace malon
{

class AboutOverlay : public juce::Component
{
public:
    AboutOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onClose;
private:
    juce::TextEditor body;
    juce::TextButton closeButton { "Close" };
    juce::Rectangle<int> card;
};

class MalonEditor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    explicit MalonEditor (MalonProcessor&);
    ~MalonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // exposed for the headless screenshot tool and tests
    void pollNow (double dtMs);               // same work as one timer tick
    void haltTimer() { stopTimer(); }
    DisplayModel& getModel() noexcept { return model; }
    void setSettingsOpen (bool open);
    void showAbout (bool show);
    void setFrozen (bool f);
    void applyAllSettings();
    void scrollSettingsTo (int y) { viewport.setViewPosition (0, y); }
    MeterStrip& getMeterStrip() noexcept { return strip; }
    HistoryView& getHistory() noexcept { return history; }
    int getViewMode() const noexcept { return viewMode; }
    SpectrumView& getView() noexcept { return view; }
    bool isMeterVisible() const noexcept { return strip.isVisible(); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void resetAll();
    void resetMeters();
    void updateMeterVisibility();

    MalonProcessor& proc;
    MalonLookAndFeel laf;
    DisplayModel model;
    double lastTickMs = 0.0;

    SpectrumView view;
    SettingsContent content;
    juce::Viewport viewport;
    juce::TextButton freezeButton { "Freeze" }, resetButton { "Reset" }, settingsButton { "Settings" }, aboutButton { "i" };
    MeterStrip strip;
    HistoryView history;
    juce::TextButton modeButtons[4] { juce::TextButton ("SPECTRUM"), juce::TextButton ("LUFS"), juce::TextButton ("RMS"), juce::TextButton ("PEAK") };
    int viewMode = -1;
    juce::TextButton hiResButton { "HI-RES" };
    juce::TextButton holdButton { "HOLD" }, avgButton { "AVG" };
    std::unique_ptr<juce::ButtonParameterAttachment> holdAtt, avgAtt;
    juce::ComboBox slopeBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> slopeAtt;
    OutMeter outMeter;
    CorrelationBar corrBar;
public:
    OutMeter& getOutMeter() noexcept { return outMeter; }
    CorrelationBar& getCorrelationBar() noexcept { return corrBar; }
private:
    void updateHiResButton();
    void updateViewMode();
    juce::ComboBox channelBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> channelAtt;
    juce::TextButton metersButton { "Meters" };
    std::unique_ptr<juce::ButtonParameterAttachment> metersAtt;
    double liveDb[2] = { -200.0, -200.0 };
    AboutOverlay about;
    juce::ComponentBoundsConstrainer constrainer;

    static constexpr int topBarH = 44;
    bool settingsOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MalonEditor)
};

} // namespace malon
