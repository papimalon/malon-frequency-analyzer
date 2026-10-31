// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "PluginEditor.h"
#include "ui/Credits.h"
#include "Modes.h"

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
    : AudioProcessorEditor (&p), proc (p), content (p), strip (p)
{
    setLookAndFeel (&laf);
    view.setData (&model);
    addAndMakeVisible (view);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addChildComponent (viewport);

    freezeButton.setClickingTogglesState (true);
    freezeButton.getProperties().set ("neutral", true);   // one-shot style; FROZEN shows on the graph
    freezeButton.onClick = [this] { setFrozen (freezeButton.getToggleState()); };
    resetButton.onClick = [this] { resetAll(); };
    aboutButton.getProperties().set ("neutral", true);
    settingsButton.setClickingTogglesState (true);
    settingsButton.onClick = [this] { setSettingsOpen (settingsButton.getToggleState()); };
    aboutButton.onClick = [this] { showAbout (true); };
    for (auto* b : { &freezeButton, &resetButton, &settingsButton, &aboutButton, &metersButton }) addAndMakeVisible (b);
    metersAtt = std::make_unique<ButtonParameterAttachment> (*proc.apvts.getParameter (ids::meterOn), metersButton);
    metersButton.setClickingTogglesState (true);
    metersButton.setTooltip ("Show the pro meters: true peak, RMS, loudness (M)");
    addChildComponent (strip);
    addChildComponent (history);
    const char* modeTips[4] = { "Frequency spectrum", "Momentary loudness (LUFS) over the last 10 s",
                                "RMS level over the last 10 s", "True peak over the last 10 s" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeButtons[i];
        b.setTooltip (modeTips[i]);
        b.getProperties().set ("neutral", true);     // tabs: selected = neutral highlight, not the accent
        b.onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter (ids::viewMode))
            { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); }
            updateViewMode();
        };
        addAndMakeVisible (b);
    }
    hiResButton.setClickingTogglesState (true);
    hiResButton.setTooltip ("High resolution: 48 bands per octave drawn as a filled curve, so kicks, 808 notes and close tones show as sharp, separate peaks");
    hiResButton.onClick = [this]
    {
        const bool on = hiResButton.getToggleState();
        const int m = roundToInt (proc.apvts.getRawParameterValue (ids::specMode)->load());
        // Default <-> High Res go through the Mode menu; in any other Mode / Off only the resolution changes
        if (on && m == 1)                    setParam (proc.apvts, ids::specMode, 3.0f);
        else if (! on && (m == 3 || m == 4)) setParam (proc.apvts, ids::specMode, 1.0f);
        else setParam (proc.apvts, ids::bands, (float) (on ? kBandsHiResIndex : kBandsDefaultIndex));
        proc.syncModeNow();
    };
    // HI-RES lives in the View menu now (View: High Res); the button is kept only for old tests/hosts, hidden
    addChildComponent (hiResButton);
    for (auto* b : { &holdButton, &avgButton }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    holdButton.setTooltip ("Show / hide the white peak-hold line");
    avgButton.setTooltip ("Show / hide the red average line");
    holdAtt = std::make_unique<ButtonParameterAttachment> (*proc.apvts.getParameter (ids::holdOn), holdButton);
    avgAtt  = std::make_unique<ButtonParameterAttachment> (*proc.apvts.getParameter (ids::avgOn), avgButton);
    slopeBox.addItemList ({ "Slope 0", "Slope 3", "Slope 4.5" }, 1);
    slopeBox.setTooltip ("Spectrum slope in dB/octave around 1 kHz, same scale as SPAN. 4.5 (SPAN default): a balanced mix looks flat. 3: pink noise flat, tones read their true level at any frequency. 0: SPAN raw view");
    addAndMakeVisible (slopeBox);
    for (int i = 0; i < 8; ++i) specModeBox.addItem (String ("View: ") + kModeNames[i], i + 1);
    specModeBox.setTooltip ("Spectrum view preset, like SPAN's Mode: Default, Slower (calmer), High Res (48 bands/oct curve, sharp low end), 96 kHz, Master (slow tonal balance), Average (long-term only), Avg+Max (average + all-time max). Changing a setting by hand switches to Custom");
    addAndMakeVisible (specModeBox);
    specModeAtt = std::make_unique<ComboBoxParameterAttachment> (*proc.apvts.getParameter (ids::specMode), specModeBox);
    slopeAtt = std::make_unique<ComboBoxParameterAttachment> (*proc.apvts.getParameter (ids::slope), slopeBox);
    addAndMakeVisible (outMeter);
    outMeter.onClipClicked = [this] { resetMeters(); };
    channelBox.addItemList ({ "Left", "Right", "Stereo", "Mix", "Mid", "Side" }, 1);
    channelBox.setTooltip ("What the spectrum shows: Left, Right, both (Stereo), L+R Mix, Mid (centre) or Side (stereo width)");
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<ComboBoxParameterAttachment> (*proc.apvts.getParameter (ids::channel), channelBox);
    strip.onReset = [this] { resetMeters(); };
    freezeButton.setTooltip ("Freeze the display (F)");
    resetButton.setTooltip ("Clear peaks, holds and averages (R)");

    addChildComponent (about);
    about.onClose = [this] { showAbout (false); };

    setWantsKeyboardFocus (true);
    constrainer.setMinimumSize (680, 380);   // smallest size where View, Slope and correlation/balance all fit
    constrainer.setMaximumSize (4096, 2400);
    setConstrainer (&constrainer);
    setResizable (true, true);

    settingsOpen = proc.getSettingsOpen();
    settingsButton.setToggleState (settingsOpen, dontSendNotification);
    viewport.setVisible (settingsOpen);

    sendLookAndFeelChange();      // controls built before they had a parent picked up the default look

    auto sz = proc.getSavedEditorSize();
    setSize (jmax (680, sz.x), jmax (380, sz.y));   // older sessions may have saved a smaller window

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
    if (getWidth() > 700)
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
    top.removeFromRight (6);
    metersButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (6);
    channelBox.setVisible (top.getWidth() >= 190);
    channelBox.setBounds (top.removeFromRight (90).reduced (0, 2));

    if (strip.isVisible())
        strip.setBounds (r.removeFromBottom (MeterStrip::preferredHeight));

    {   // view selector, aligned with the plot's left edge
        auto row = r.removeFromTop (28).withTrimmedLeft (8 + 36).withTrimmedBottom (4);
        const int widths[4] = { 82, 56, 52, 56 };
        for (int i = 0; i < 4; ++i) { modeButtons[i].setBounds (row.removeFromLeft (widths[i])); row.removeFromLeft (4); }
        row.removeFromRight (8);
        avgButton.setBounds (row.removeFromRight (46 + 4).withTrimmedRight (4));
        holdButton.setBounds (row.removeFromRight (52 + 4).withTrimmedRight (4));
        const bool spectrum = viewMode <= 0;
        slopeBox.setVisible (spectrum && row.getWidth() >= 100);
        slopeBox.setBounds (row.removeFromRight (92 + 6).withTrimmedRight (6));
        specModeBox.setVisible (spectrum && row.getWidth() >= 140);
        specModeBox.setBounds (row.removeFromRight (140 + 6).withTrimmedRight (6));
    }

    if (settingsOpen)
    {
        const int pw = juce::jlimit (240, 320, r.getWidth() / 2);
        auto pr = r.removeFromRight (pw);
        viewport.setBounds (pr);
        content.setSize (pr.getWidth() - viewport.getScrollBarThickness(), 3000);
        content.resized();
        content.setSize (pr.getWidth() - viewport.getScrollBarThickness(), content.getContentHeight());
    }
    {
        auto g = r.withTrimmedLeft (8).withTrimmedBottom (8).withTrimmedRight (8);
        outMeter.setBounds (g.removeFromRight (OutMeter::preferredWidth));
        g.removeFromRight (4);
        view.setBounds (g);
    }
    history.setBounds (view.getBounds());
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

void MalonEditor::resetMeters()
{
    proc.getMeter().requestReset();
    history.clear();
    double dump[2]; proc.getMeter().consumeRecentPeaks (dump);
    outMeter.reset();
    liveDb[0] = liveDb[1] = -200.0;
}

void MalonEditor::updateMeterVisibility()
{
    const bool on = proc.apvts.getRawParameterValue (ids::meterOn)->load() > 0.5f;
    if (on != strip.isVisible())
    {
        strip.setVisible (on);
        resized();
        repaint();
    }
}

bool MalonEditor::keyPressed (const KeyPress& k)
{
    if (k.getTextCharacter() == 'm' || k.getTextCharacter() == 'M')
    {
        if (auto* prm = proc.apvts.getParameter (ids::meterOn))
        { prm->beginChangeGesture(); prm->setValueNotifyingHost (strip.isVisible() ? 0.0f : 1.0f); prm->endChangeGesture(); }
        return true;
    }
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
    s.peakShow = (ShowAs) (int) v (ids::peakShow);
    s.holdSize = (int) v (ids::holdSize);  s.avgSize = (int) v (ids::avgSize);
    s.ceiling = v (ids::ceiling);  s.grid = v (ids::grid);
    s.range = jmax (10.0, s.ceiling - kFloors[jlimit (0, 2, (int) v (ids::dbFloor))]);
    s.octaveGrid = (int) v (ids::freqGrid) == 1;
    s.frozen = model.frozen;
    s.slope = kSlopes[jlimit (0, 2, (int) v (ids::slope))];
    view.setSettings (s);
    view.setColors (proc.getColors());
    updateMeterVisibility();
    updateViewMode();
}

void MalonEditor::changeListenerCallback (ChangeBroadcaster*)
{
    content.refreshColors();
    applyAllSettings();
}

void MalonEditor::pollNow (double dtMs)
{
    proc.syncModeNow();
    Readout r;
    proc.getBank().read (r);         // always fills the layout; zero data when no audio arrived
    model.update (r, dtMs, proc.getDisplaySettings());
    applyAllSettings();

    {
        double pk[2];
        proc.getMeter().consumeRecentPeaks (pk);
        for (int ch = 0; ch < 2; ++ch)
        {
            liveDb[ch] = jmax (liveDb[ch] - 24.0 * dtMs / 1000.0, -200.0);                       // 24 dB/s fall
            if (pk[ch] > 0.0) liveDb[ch] = jmax (liveDb[ch], 20.0 * std::log10 (pk[ch]));
        }
        const auto ro = proc.getMeter().read();
        const bool meterOn = proc.apvts.getRawParameterValue (ids::meterOn)->load() > 0.5f;
        if (! model.frozen)
        {
            const double off[2] = { -200.0, -200.0 };
            const auto sp = meterScaleSpec (roundToInt (proc.apvts.getRawParameterValue (ids::meterScale)->load()));
            outMeter.setScale (sp);
            double bar[2], pkv[2];
            for (int c = 0; c < 2; ++c)
            {
                const auto mv = meterValues (sp, ro.rmsFastDb[c], ro.rmsKDb[c], liveDb[c], ro.momentary, ro.shortTerm);
                bar[c] = mv.bar; pkv[c] = mv.peak;
            }
            outMeter.setLevels (meterOn ? pkv : off, meterOn ? bar : off, ro.channels, meterOn && ro.clipped, dtMs);
        }
        if (strip.isVisible())
        {
            strip.setReadout (ro, liveDb[0], liveDb[1], dtMs, meterOn && ! model.frozen);
            const double pkMax = jmax (pk[0], pk[1]);
            const double pkDb = pkMax > 0.0 ? 20.0 * std::log10 (pkMax) : -std::numeric_limits<double>::infinity();
            if (! model.frozen) history.push (ro.momentary, ro.rmsFastCombinedDb, pkDb < -150.0 ? -std::numeric_limits<double>::infinity() : pkDb);
        }
    }
}

void MalonEditor::updateHiResButton()
{
    const bool hi = roundToInt (proc.apvts.getRawParameterValue (ids::bands)->load()) >= 3;
    if (hiResButton.getToggleState() != hi) hiResButton.setToggleState (hi, dontSendNotification);
}

void MalonEditor::updateViewMode()
{
    updateHiResButton();
    const int m = jlimit (0, 3, roundToInt (proc.apvts.getRawParameterValue (ids::viewMode)->load()));
    history.setMetersOn (strip.isVisible());
    if (m == viewMode) return;
    viewMode = m;
    for (int i = 0; i < 4; ++i) modeButtons[i].setToggleState (i == m, dontSendNotification);
    view.setVisible (m == 0);
    history.setVisible (m != 0);
    if (m != 0) history.setMode (m - 1);
    channelBox.setEnabled (m == 0);
    hiResButton.setVisible (false);
    holdButton.setVisible (m == 0); avgButton.setVisible (m == 0);
    resized();
}

void MalonEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (1.0, 250.0, now - lastTickMs);
    lastTickMs = now;
    pollNow (dt);
}

} // namespace malon
