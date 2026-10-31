// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ColorScheme.h"
#include "Parameters.h"
#include "dsp/Ballistics.h"
#include "dsp/LevelMeter.h"

namespace malon
{

class MalonProcessor : public juce::AudioProcessor,
                       public juce::ChangeBroadcaster,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::AsyncUpdater
{
public:
    MalonProcessor();
    ~MalonProcessor() override;

    /** Apply a newly chosen Mode / switch to Off after a manual change, now (normally async). */
    void syncModeNow() { handleUpdateNowIfNeeded(); }

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Malon Frequency Analyzer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // analysis + settings
    FilterBank& getBank() noexcept { return bank; }
    LevelMeter& getMeter() noexcept { return meter; }
    juce::AudioProcessorValueTreeState apvts;

    DisplaySettings getDisplaySettings() const;

    // colours (message thread)
    ColorScheme getColors() const { return colors; }
    void setColors (const ColorScheme& c);

    // window size & panel state are part of the saved state
    juce::Point<int> getSavedEditorSize() const;
    void saveEditorSize (int w, int h);
    bool getSettingsOpen() const;
    void setSettingsOpen (bool open);

    // "save as default": a small XML file in the user's app-data folder
    static juce::File getDefaultsFile();
    bool saveAsDefault();
    bool clearDefault();
    bool loadDefault();

    // for tests / offline rendering: don't pick up the user's default file
    static bool& useUserDefault() { static bool v = true; return v; }

private:
    template <typename Sample> void processImpl (juce::AudioBuffer<Sample>&);
    void applyStateTree (const juce::ValueTree&);
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    int lastAppliedMode = kModeDefault;

    FilterBank bank;
    LevelMeter meter;
    std::atomic<float>* pMeterOn = nullptr;
    std::atomic<float>* pRmsWindow = nullptr;
    bool meterWasOn = false;
    bool wasPlaying = false;
    std::atomic<float>* pChannel = nullptr;
    std::atomic<float>* pBands   = nullptr;
    int  currentBpo = 0;
    double currentFs = 0.0;
    ColorScheme colors = ColorScheme::defaults();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MalonProcessor)
};

} // namespace malon
