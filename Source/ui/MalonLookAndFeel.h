// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Our own minimal, dark, bold look. No third-party artwork.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace malon
{

namespace palette
{
    inline const juce::Colour ink       { 0xFF07080C };   // window
    inline const juce::Colour panel     { 0xFF0C0E14 };
    inline const juce::Colour hairline  { 0x1FFFFFFF };
    inline const juce::Colour text      { 0xFFE9ECF2 };
    inline const juce::Colour dim       { 0xFF7C8394 };
    inline const juce::Colour red       { 0xFFFF2D55 };   // neon palm
    inline const juce::Colour blue      { 0xFF2F7BFF };   // blue wave
}

inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

class MalonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MalonLookAndFeel()
    {
        using namespace palette;
        setColour (juce::ResizableWindow::backgroundColourId, ink);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF12151D));
        setColour (juce::ComboBox::outlineColourId, hairline);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xFF12151D));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, blue);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF12151D));
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x40FFFFFF));
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, juce::Colour (0xFFB5BBC8));
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return uiFont (12.5f); }
    juce::Font getPopupMenuFont() override                       { return uiFont (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (12.0f, true); }
    juce::Font getLabelFont (juce::Label&) override              { return uiFont (12.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = new juce::Label();
        l->setJustificationType (juce::Justification::centredRight);
        l->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
        l->setFont (uiFont (12.0f));
        l->setColour (juce::Label::textColourId, palette::text);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xFF12151D));
        l->setColour (juce::Label::textWhenEditingColourId, palette::text);
        l->setColour (juce::Label::outlineWhenEditingColourId, palette::blue);
        l->setColour (juce::TextEditor::highlightColourId, palette::blue);
        juce::ignoreUnused (s);
        return l;
    }

    void drawCornerResizer (juce::Graphics& g, int w, int h, bool, bool) override
    {
        g.setColour (juce::Colour (0x40FFFFFF));
        for (int i = 0; i < 3; ++i)
        {
            const float o = 4.0f + 4.0f * (float) i;
            g.drawLine ((float) w - o, (float) h - 2.0f, (float) w - 2.0f, (float) h - o, 1.2f);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace palette;
        const float cy = y + h * 0.5f;
        const float x0 = (float) x + 2.0f, x1 = (float) (x + w) - 2.0f;
        g.setColour (juce::Colour (0xFF1B1F2A));
        g.fillRoundedRectangle (x0, cy - 1.5f, x1 - x0, 3.0f, 1.5f);
        const bool on = s.isEnabled();
        g.setColour (on ? blue : dim.withAlpha (0.4f));
        g.fillRoundedRectangle (x0, cy - 1.5f, juce::jmax (0.0f, pos - x0), 3.0f, 1.5f);
        g.setColour (on ? juce::Colours::white : dim);
        g.fillEllipse (pos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        using namespace palette;
        auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (30.0f, 16.0f).withX ((float) b.getWidth() - 32.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? red : juce::Colour (0xFF1B1F2A));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? juce::Colours::white : dim);
        const float d = 10.0f;
        g.fillEllipse (on ? r.getRight() - d - 3.0f : r.getX() + 3.0f, r.getCentreY() - d * 0.5f, d, d);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (palette::hairline);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        juce::Path p;
        const float cx = (float) w - 12.0f, cy = h * 0.5f;
        p.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
        g.setColour (palette::dim);
        g.fillPath (p);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool toggled = b.getToggleState();
        auto fill = toggled ? palette::red : juce::Colour (0xFF12151D);
        if (over) fill = fill.brighter (0.12f);
        if (down) fill = fill.brighter (0.25f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (toggled ? palette::red : palette::hairline);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
};

} // namespace malon
