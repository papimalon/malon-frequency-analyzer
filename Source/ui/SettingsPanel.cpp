// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "SettingsPanel.h"

namespace malon
{
using namespace juce;

void ColorSwatch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    // checkerboard under translucent colours
    g.saveState();
    Path clip; clip.addRoundedRectangle (r, 4.0f);
    g.reduceClipRegion (clip);
    g.setColour (Colour (0xFF2A2E39)); g.fillRect (r);
    g.setColour (Colour (0xFF3A3F4C));
    for (int y = 0; y < (int) r.getHeight(); y += 6)
        for (int x = ((y / 6) & 1) * 6; x < (int) r.getWidth(); x += 12)
            g.fillRect (r.getX() + (float) x, r.getY() + (float) y, 6.0f, 6.0f);
    g.setColour (colour);
    g.fillRect (r);
    g.restoreState();
    g.setColour (palette::hairline.brighter (0.4f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void ColorSwatch::mouseUp (const MouseEvent& e)
{
    if (! contains (e.getPosition())) return;
    auto sel = std::make_unique<ColourSelector> (ColourSelector::showAlphaChannel | ColourSelector::showColourAtTop
                                                 | ColourSelector::showSliders | ColourSelector::showColourspace);
    sel->setCurrentColour (colour, dontSendNotification);
    sel->setSize (260, 300);
    struct Listener : ChangeListener
    {
        Listener (ColorSwatch& s, ColourSelector& c) : swatch (s), sel (c) { sel.addChangeListener (this); }
        ~Listener() override { sel.removeChangeListener (this); }
        void changeListenerCallback (ChangeBroadcaster*) override
        { swatch.setColour (sel.getCurrentColour()); if (swatch.onChange) swatch.onChange (sel.getCurrentColour()); }
        ColorSwatch& swatch; ColourSelector& sel;
    };
    struct Holder : Component
    {
        Holder (ColorSwatch& s, std::unique_ptr<ColourSelector> c) : sel (std::move (c)), l (s, *sel)
        { addAndMakeVisible (*sel); setSize (sel->getWidth(), sel->getHeight()); }
        void resized() override { sel->setBounds (getLocalBounds()); }
        std::unique_ptr<ColourSelector> sel; Listener l;
    };
    CallOutBox::launchAsynchronously (std::make_unique<Holder> (*this, std::move (sel)), getScreenBounds(), nullptr);
}

// ---------------------------------------------------------------------------

static void styleName (Label& l, const String& text)
{
    l.setText (text, dontSendNotification);
    l.setFont (uiFont (12.0f));
    l.setColour (Label::textColourId, palette::dim);
    l.setJustificationType (Justification::centredLeft);
    l.setInterceptsMouseClicks (false, false);
}

void SettingsContent::addHeader (const String& title)
{
    auto h = std::make_unique<Label>();
    h->setText (title.toUpperCase(), dontSendNotification);
    h->setFont (uiFont (10.5f, true).withExtraKerningFactor (0.18f));
    h->setColour (Label::textColourId, palette::red);
    h->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*h);
    headerBefore.push_back ((int) rows.size());
    headers.push_back (std::move (h));
}

Slider* SettingsContent::addSlider (const char* id, const String& label, const String& suffix, int decimals)
{
    auto s = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::TextBoxRight);
    s->setTextBoxStyle (Slider::TextBoxRight, false, 62, 20);
    s->setTextValueSuffix (suffix.isEmpty() ? String() : " " + suffix);
    s->setNumDecimalPlacesToDisplay (decimals);
    auto* raw = s.get();
    sAtt.push_back (std::make_unique<SliderParameterAttachment> (*proc.apvts.getParameter (id), *s));
    Row r; r.label = label; r.name = std::make_unique<Label>(); styleName (*r.name, label);
    addAndMakeVisible (*r.name); addAndMakeVisible (*s);
    r.control = std::move (s);
    rows.push_back (std::move (r));
    return raw;
}

ToggleButton* SettingsContent::addToggle (const char* id, const String& label)
{
    auto t = std::make_unique<ToggleButton>();
    auto* raw = t.get();
    bAtt.push_back (std::make_unique<ButtonParameterAttachment> (*proc.apvts.getParameter (id), *t));
    Row r; r.label = label; r.name = std::make_unique<Label>(); styleName (*r.name, label);
    addAndMakeVisible (*r.name); addAndMakeVisible (*t);
    r.control = std::move (t);
    rows.push_back (std::move (r));
    return raw;
}

ComboBox* SettingsContent::addCombo (const char* id, const String& label)
{
    auto c = std::make_unique<ComboBox>();
    auto* param = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id));
    c->addItemList (param->choices, 1);
    auto* raw = c.get();
    cAtt.push_back (std::make_unique<ComboBoxParameterAttachment> (*param, *c));
    Row r; r.label = label; r.name = std::make_unique<Label>(); styleName (*r.name, label);
    addAndMakeVisible (*r.name); addAndMakeVisible (*c);
    r.control = std::move (c);
    rows.push_back (std::move (r));
    return raw;
}

SettingsContent::SettingsContent (MalonProcessor& p) : proc (p)
{
    addHeader ("Input");
    addCombo (ids::channel, "Channel");

    addHeader ("Peak");
    addToggle (ids::peakOn, "Show");
    addSlider (ids::peakDecay, "Decay", "dB/s");

    addHeader ("Average");
    addToggle (ids::avgOn, "Show");
    addSlider (ids::avgTime, "Time", "ms");
    addCombo (ids::avgShow, "Show as");
    addSlider (ids::avgSize, "Size", "px");
    addSlider (ids::avgSlope, "Slope", "dB/oct");

    addHeader ("Peak Hold");
    addToggle (ids::holdOn, "Show");
    addToggle (ids::holdInfinite, "Infinite");
    addSlider (ids::holdTime, "Time", "ms");
    addSlider (ids::holdDecay, "Decay", "dB/s");
    addCombo (ids::holdShow, "Show as");
    addSlider (ids::holdSize, "Size", "px");

    addHeader ("Level");
    addSlider (ids::ceiling, "Ceiling", "dB");
    addSlider (ids::range, "Range", "dB");
    addSlider (ids::grid, "Grid", "dB");

    addHeader ("Frequency");
    addCombo (ids::bands, "Bands / octave");
    addCombo (ids::freqGrid, "Grid");

    addHeader ("Colors");
    schemeLabel.setText ("Scheme", dontSendNotification);
    styleName (schemeLabel, "Scheme");
    addAndMakeVisible (schemeLabel);
    schemeBox.addItem ("Custom", 100);
    int id = 1;
    for (auto& s : builtInSchemes()) schemeBox.addItem (s.name, id++);
    schemeBox.onChange = [this]
    {
        const int idx = schemeBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) builtInSchemes().size())
            proc.setColors (builtInSchemes()[(size_t) idx].scheme);
    };
    addAndMakeVisible (schemeBox);

    for (int i = 0; i < ColorScheme::NumSlots; ++i)
    {
        styleName (swatchNames[(size_t) i], ColorScheme::slotName (i));
        swatchNames[(size_t) i].setFont (uiFont (11.0f));
        addAndMakeVisible (swatchNames[(size_t) i]);
        addAndMakeVisible (swatches[(size_t) i]);
        swatches[(size_t) i].onChange = [this, i] (Colour c)
        {
            auto cs = proc.getColors(); cs.c[(size_t) i] = c; proc.setColors (cs);
            schemeBox.setSelectedId (100, dontSendNotification);
        };
    }

    saveDefaultButton.onClick = [this] { proc.saveAsDefault(); };
    resetDefaultButton.onClick = [this] { proc.clearDefault(); };
    saveDefaultButton.setTooltip ("New instances of the plugin start with the current settings and colors.");
    addAndMakeVisible (saveDefaultButton);
    addAndMakeVisible (resetDefaultButton);

    refreshColors();
}

void SettingsContent::refreshColors()
{
    auto cs = proc.getColors();
    for (int i = 0; i < ColorScheme::NumSlots; ++i) swatches[(size_t) i].setColour (cs[i]);
    int match = 100, id = 1;
    for (auto& s : builtInSchemes()) { if (s.scheme.toString() == cs.toString()) match = id; ++id; }
    schemeBox.setSelectedId (match, dontSendNotification);
}

void SettingsContent::layoutRows (int w)
{
    const int pad = 18, rowH = 26, headH = 34;
    int y = 8;
    size_t hi = 0;
    for (size_t i = 0; i <= rows.size(); ++i)
    {
        while (hi < headers.size() && headerBefore[hi] == (int) i)
        {
            headers[hi]->setBounds (pad, y + 8, w - pad * 2, 18);
            y += headH; ++hi;
        }
        if (i == rows.size()) break;
        auto& r = rows[i];
        r.name->setBounds (pad, y, 92, rowH);
        auto* c = r.control.get();
        if (dynamic_cast<ToggleButton*> (c))  c->setBounds (pad + 92, y, w - pad * 2 - 92, rowH);
        else if (dynamic_cast<ComboBox*> (c)) c->setBounds (pad + 92, y + 2, w - pad * 2 - 92, rowH - 4);
        else                                  c->setBounds (pad + 88, y, w - pad * 2 - 88, rowH);
        y += rowH;
    }

    // colours block (headers for it was already placed at the end by the loop)
    schemeLabel.setBounds (pad, y, 92, rowH);
    schemeBox.setBounds (pad + 92, y + 2, w - pad * 2 - 92, rowH - 4);
    y += rowH + 6;
    const int colW = (w - pad * 2) / 2;
    for (int i = 0; i < ColorScheme::NumSlots; ++i)
    {
        const int cx = pad + (i % 2) * colW, cy = y + (i / 2) * 28;
        swatches[(size_t) i].setBounds (cx, cy + 3, 26, 20);
        swatchNames[(size_t) i].setBounds (cx + 34, cy, colW - 40, 26);
    }
    y += 4 * 28 + 10;
    saveDefaultButton.setBounds (pad, y, (w - pad * 2) / 2 - 4, 26);
    resetDefaultButton.setBounds (pad + (w - pad * 2) / 2 + 4, y, (w - pad * 2) / 2 - 4, 26);
    y += 26 + 24;
    contentHeight = y;
}

void SettingsContent::resized() { layoutRows (getWidth()); }

} // namespace malon
