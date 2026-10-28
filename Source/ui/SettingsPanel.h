// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "../PluginProcessor.h"
#include "MalonLookAndFeel.h"

namespace malon
{

class ColorSwatch : public juce::Component
{
public:
    std::function<void (juce::Colour)> onChange;
    ColorSwatch() = default;
    void setColour (juce::Colour c) { colour = c; repaint(); }
    juce::Colour getColour() const { return colour; }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Colour colour;
};

/** The scrolling content of the settings drawer. */
class SettingsContent : public juce::Component
{
public:
    explicit SettingsContent (MalonProcessor&);
    void resized() override;
    int  getContentHeight() const { return contentHeight; }
    void refreshColors();

private:
    struct Row { juce::String label; std::unique_ptr<juce::Component> control; std::unique_ptr<juce::Label> name; };

    void addHeader (const juce::String& title);
    juce::Slider*      addSlider (const char* id, const juce::String& label, const juce::String& suffix, int decimals = 0);
    juce::ToggleButton* addToggle (const char* id, const juce::String& label);
    juce::ComboBox*    addCombo (const char* id, const juce::String& label);
    void layoutRows (int width);

    MalonProcessor& proc;
    std::vector<Row> rows;
    std::vector<std::unique_ptr<juce::Label>> headers;
    std::vector<int> headerBefore;     // index in 'rows' each header precedes

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> bAtt;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> cAtt;

    juce::ComboBox schemeBox;
    juce::TextButton saveDefaultButton { "Save as default" }, resetDefaultButton { "Reset default" };
    std::array<ColorSwatch, ColorScheme::NumSlots> swatches;
    std::array<juce::Label, ColorScheme::NumSlots> swatchNames;
    juce::Label schemeLabel;
    int colorsTop = 0, contentHeight = 0;
};

} // namespace malon
