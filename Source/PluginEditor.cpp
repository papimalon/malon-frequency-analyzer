// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "PluginEditor.h"
#include "ui/Credits.h"

namespace malon
{
using namespace juce;

// ---- About -----------------------------------------------------------------

AboutOverlay::AboutOverlay()
{
    body.setMultiLine (true, true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (uiFont (12.0f));
    body.setText (credits::about, false);
    addAndMakeVisible (body);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
    setVisible (false);
}

void AboutOverlay::resized()
{
    auto r = getLocalBounds();
    card = r.withSizeKeepingCentre (juce::jmin (560, r.getWidth() - 24), juce::jmin (430, r.getHeight() - 24));
    auto inner = card.reduced (22, 18);
    inner.removeFromTop (54);                       // title block (painted)
    closeButton.setBounds (inner.removeFromBottom (26).removeFromRight (84));
    inner.removeFromBottom (8);
    body.setBounds (inner);
}

void AboutOverlay::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.72f));
    g.setColour (Colour (0xFF0C0E14));
    g.fillRoundedRectangle (card.toFloat(), 10.0f);
    g.setColour (palette::hairline.brighter (0.3f));
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto t = card.reduced (22, 18).removeFromTop (54);
    g.setColour (palette::red);
    g.fillEllipse ((float) t.getX(), (float) t.getY() + 7.0f, 9.0f, 9.0f);
    g.setColour (palette::text);
    g.setFont (uiFont (17.0f, true).withExtraKerningFactor (0.16f));
    g.drawText ("MALON", t.getX() + 18, t.getY(), 120, 24, Justification::centredLeft, false);
    g.setColour (palette::dim);
    g.setFont (uiFont (11.0f).withExtraKerningFactor (0.12f));
    g.drawText (String ("FREQUENCY ANALYZER  -  v") + credits::version, t.getX() + 18, t.getY() + 26, 300, 16, Justification::centredLeft, false);
}

void AboutOverlay::mouseUp (const MouseEvent& e)
{
    if (! card.contains (e.getPosition()) && onClose) onClose();
}

// ---- Editor ----------------------------------------------------------------

MalonEditor::MalonEditor (MalonProcessor& p)
    : AudioProcessorEditor (&p), proc (p), content (p)
{
    setLookAndFeel (&laf);
    view.setData (&model);
    addAndMakeVisible (view);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addChildComponent (viewport);

    freezeButton.setClickingTogglesState (true);
    freezeButton.onClick = [this] { setFrozen (freezeButton.getToggleState()); };
    resetButton.onClick = [this] { resetAll(); };
    settingsButton.setClickingTogglesState (true);
    settingsButton.onClick = [this] { setSettingsOpen (settingsButton.getToggleState()); };
    aboutButton.onClick = [this] { showAbout (true); };
    for (auto* b : { &freezeButton, &resetButton, &settingsButton, &aboutButton }) addAndMakeVisible (b);
    freezeButton.setTooltip ("Freeze the display (F)");
    resetButton.setTooltip ("Clear peaks, holds and averages (R)");

    addChildComponent (about);
    about.onClose = [this] { showAbout (false); };

    setWantsKeyboardFocus (true);
    constrainer.setMinimumSize (480, 260);
    constrainer.setMaximumSize (4096, 2400);
    setConstrainer (&constrainer);
    setResizable (true, true);

    settingsOpen = proc.getSettingsOpen();
    settingsButton.setToggleState (settingsOpen, dontSendNotification);
    viewport.setVisible (settingsOpen);

    sendLookAndFeelChange();      // controls built before they had a parent picked up the default look

    auto sz = proc.getSavedEditorSize();
    setSize (sz.x, sz.y);

    proc.addChangeListener (this);
    applyAllSettings();
    lastTickMs = Time::getMillisecondCounterHiRes();
    startTimerHz (31);
}

MalonEditor::~MalonEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void MalonEditor::paint (Graphics& g)
{
    g.fillAll (palette::ink);

    // wordmark
    auto r = Rectangle<int> (0, 0, getWidth(), topBarH).reduced (16, 0);
    g.setColour (palette::red);
    g.fillEllipse ((float) r.getX(), (float) r.getCentreY() - 4.5f, 9.0f, 9.0f);
    g.setColour (palette::text);
    g.setFont (uiFont (16.0f, true).withExtraKerningFactor (0.16f));
    g.drawText ("MALON", r.getX() + 18, 0, 100, topBarH, Justification::centredLeft, false);
    if (getWidth() > 560)
    {
        g.setColour (palette::dim);
        g.setFont (uiFont (10.5f).withExtraKerningFactor (0.14f));
        g.drawText ("FREQUENCY ANALYZER", r.getX() + 104, 0, 200, topBarH, Justification::centredLeft, false);
    }
}

void MalonEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (topBarH).reduced (14, 8);
    aboutButton.setBounds (top.removeFromRight (30));
    top.removeFromRight (6);
    settingsButton.setBounds (top.removeFromRight (86));
    top.removeFromRight (6);
    resetButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    freezeButton.setBounds (top.removeFromRight (74));

    if (settingsOpen)
    {
        const int pw = juce::jlimit (240, 320, r.getWidth() / 2);
        auto pr = r.removeFromRight (pw);
        viewport.setBounds (pr);
        content.setSize (pr.getWidth() - viewport.getScrollBarThickness(), 3000);
        content.resized();
        content.setSize (pr.getWidth() - viewport.getScrollBarThickness(), content.getContentHeight());
    }
    view.setBounds (r.withTrimmedLeft (8).withTrimmedBottom (8).withTrimmedRight (settingsOpen ? 8 : 8));
    about.setBounds (getLocalBounds());

    if (getWidth() > 0) proc.saveEditorSize (getWidth(), getHeight());
}

void MalonEditor::setSettingsOpen (bool open)
{
    settingsOpen = open;
    settingsButton.setToggleState (open, dontSendNotification);
    viewport.setVisible (open);
    proc.setSettingsOpen (open);
    resized();
}

void MalonEditor::showAbout (bool show)
{
    about.setVisible (show);
    if (show) about.toFront (false);
}

void MalonEditor::setFrozen (bool f)
{
    model.frozen = f;
    freezeButton.setToggleState (f, dontSendNotification);
    applyAllSettings();
}

void MalonEditor::resetAll()
{
    model.reset();
    Readout dump; proc.getBank().read (dump);   // drop what's queued
    view.repaint();
}

bool MalonEditor::keyPressed (const KeyPress& k)
{
    if (k.getTextCharacter() == 'f' || k.getTextCharacter() == 'F') { setFrozen (! model.frozen); return true; }
    if (k.getTextCharacter() == 'r' || k.getTextCharacter() == 'R') { resetAll(); return true; }
    return false;
}

void MalonEditor::applyAllSettings()
{
    auto v = [this] (const char* id) { return (double) proc.apvts.getRawParameterValue (id)->load(); };
    ViewSettings s;
    s.peakOn = v (ids::peakOn) > 0.5;  s.holdOn = v (ids::holdOn) > 0.5;  s.avgOn = v (ids::avgOn) > 0.5;
    s.holdShow = (ShowAs) (int) v (ids::holdShow);  s.avgShow = (ShowAs) (int) v (ids::avgShow);
    s.holdSize = (int) v (ids::holdSize);  s.avgSize = (int) v (ids::avgSize);
    s.ceiling = v (ids::ceiling);  s.range = v (ids::range);  s.grid = v (ids::grid);
    s.octaveGrid = (int) v (ids::freqGrid) == 1;
    s.frozen = model.frozen;
    view.setSettings (s);
    view.setColors (proc.getColors());
}

void MalonEditor::changeListenerCallback (ChangeBroadcaster*)
{
    content.refreshColors();
    applyAllSettings();
}

void MalonEditor::pollNow (double dtMs)
{
    Readout r;
    proc.getBank().read (r);         // always fills the layout; zero data when no audio arrived
    model.update (r, dtMs, proc.getDisplaySettings());
    applyAllSettings();
}

void MalonEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (1.0, 250.0, now - lastTickMs);
    lastTickMs = now;
    pollNow (dt);
}

} // namespace malon
