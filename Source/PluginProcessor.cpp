// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Modes.h"

namespace malon
{
using namespace juce;

static const Identifier kColors ("colors"), kEdW ("editorW"), kEdH ("editorH"), kPanel ("settingsOpen");

MalonProcessor::MalonProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MalonFrequencyAnalyzer", createParameterLayout())
{
    pChannel = apvts.getRawParameterValue (ids::channel);
    pBands   = apvts.getRawParameterValue (ids::bands);
    pMeterOn = apvts.getRawParameterValue (ids::meterOn);
    pRmsWindow = apvts.getRawParameterValue (ids::rmsWindow);
    lastAppliedMode = roundToInt (apvts.getRawParameterValue (ids::specMode)->load());
    if (useUserDefault()) loadDefault();
}

MalonProcessor::~MalonProcessor() = default;

// Message thread. A new Mode choice applies its preset; a hand-made change to a setting the Mode
// controls switches the Mode to Off (custom), like SPAN.
void MalonProcessor::syncModeNow()
{
    const int m = jlimit (0, 7, roundToInt (apvts.getRawParameterValue (ids::specMode)->load()));
    if (m != lastAppliedMode)
    {
        lastAppliedMode = m;
        applyMode (apvts, m);
    }
    else if (m != 0 && ! modeMatches (apvts, m))
    {
        lastAppliedMode = 0;
        setParam (apvts, ids::specMode, 0.0f);
    }
}

bool MalonProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::stereo() || in == AudioChannelSet::mono());
}

static int bpoFromIndex (int i) { static const int v[] = { 3, 4, 6, 12, 24, 48 }; return v[jlimit (0, 5, i)]; }

void MalonProcessor::prepareToPlay (double sampleRate, int)
{
    meter.prepare (sampleRate, jlimit (1, 2, getTotalNumInputChannels()));
    meterWasOn = false;
    currentFs  = sampleRate;
    currentBpo = bpoFromIndex ((int) pBands->load());
    bank.prepare (sampleRate, currentBpo);
}

template <typename Sample>
void MalonProcessor::processImpl (AudioBuffer<Sample>& buffer)
{
    ScopedNoDenormals noDenormals;

    // The audio passes through untouched (we only read it).
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0) return;

    const int bpo = bpoFromIndex ((int) pBands->load());
    if (bpo != currentBpo && currentFs > 0.0)
    {
        currentBpo = bpo;
        bank.prepare (currentFs, bpo);     // pure arithmetic on fixed arrays: no allocation, no waiting
    }

    const auto mode = (ChannelMode) jlimit (0, 5, (int) pChannel->load());
    const Sample* l = buffer.getReadPointer (0);
    const Sample* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
    bank.process (l, r, n, mode);

    // pro metering: read-only as well. Off = not measured at all; switching it on starts from a clean slate.
    if (pMeterOn->load() > 0.5f)
    {
        if (! meterWasOn) { meter.reset(); meterWasOn = true; }
        // like SPAN's Statistics: integrated RMS restarts when the host starts playing
        bool playing = wasPlaying;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition()) playing = pos->getIsPlaying();
        if (playing && ! wasPlaying) meter.requestRmsIntegratedReset();
        wasPlaying = playing;
        const int rw = jlimit (0, kRmsIntegratedIndex, (int) pRmsWindow->load());
        meter.setRmsIntegrated (rw == kRmsIntegratedIndex);
        if (rw < kRmsIntegratedIndex) meter.setRmsWindowMs (kRmsWindowsMs[rw]);
        meter.process (buffer.getArrayOfReadPointers(), jmin (numCh, 2), n);
    }
    else
    {
        meterWasOn = false;
    }
}

void MalonProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)  { processImpl (b); }
void MalonProcessor::processBlock (AudioBuffer<double>& b, MidiBuffer&) { processImpl (b); }

DisplaySettings MalonProcessor::getDisplaySettings() const
{
    auto v = [this] (const char* id) { return (double) apvts.getRawParameterValue (id)->load(); };
    DisplaySettings s;
    s.channel      = (ChannelMode) jlimit (0, 5, (int) v (ids::channel));
    s.peakOn       = v (ids::peakOn) > 0.5;
    s.peakDecay    = v (ids::peakDecay);
    s.avgOn        = v (ids::avgOn) > 0.5;
    s.avgTimeMs    = v (ids::avgTime);
    s.avgSlope     = v (ids::avgSlope);
    s.slope        = kSlopes[jlimit (0, 2, (int) v (ids::slope))];
    s.holdOn       = v (ids::holdOn) > 0.5;
    s.holdInfinite = v (ids::holdInfinite) > 0.5;
    s.holdTimeMs   = v (ids::holdTime);
    s.holdDecay    = v (ids::holdDecay);
    return s;
}

AudioProcessorEditor* MalonProcessor::createEditor() { return new MalonEditor (*this); }

// ---- state -----------------------------------------------------------------

void MalonProcessor::setColors (const ColorScheme& c)
{
    colors = c;
    apvts.state.setProperty (kColors, c.toString(), nullptr);
    sendChangeMessage();
}

Point<int> MalonProcessor::getSavedEditorSize() const
{
    return { (int) apvts.state.getProperty (kEdW, 780), (int) apvts.state.getProperty (kEdH, 500) };
}
void MalonProcessor::saveEditorSize (int w, int h)
{
    apvts.state.setProperty (kEdW, w, nullptr);
    apvts.state.setProperty (kEdH, h, nullptr);
}
bool MalonProcessor::getSettingsOpen() const { return (bool) apvts.state.getProperty (kPanel, false); }
void MalonProcessor::setSettingsOpen (bool open) { apvts.state.setProperty (kPanel, open, nullptr); }

void MalonProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MalonProcessor::applyStateTree (const ValueTree& t)
{
    if (! t.isValid() || ! t.hasType (apvts.state.getType())) return;
    apvts.replaceState (t);
    // a restored session keeps its own values: never re-apply the Mode preset over them
    lastAppliedMode = jlimit (0, 7, roundToInt (apvts.getRawParameterValue (ids::specMode)->load()));
    colors = ColorScheme::fromString (apvts.state.getProperty (kColors).toString(), ColorScheme::defaults());
    sendChangeMessage();
}

void MalonProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        applyStateTree (ValueTree::fromXml (*xml));
}

File MalonProcessor::getDefaultsFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("MALON").getChildFile ("Frequency Analyzer").getChildFile ("default.xml");
}

bool MalonProcessor::saveAsDefault()
{
    auto f = getDefaultsFile();
    if (! f.getParentDirectory().createDirectory()) return false;
    auto state = apvts.copyState();
    state.removeProperty (kEdW, nullptr); state.removeProperty (kEdH, nullptr);   // size is per instance
    state.setProperty (kColors, colors.toString(), nullptr);
    if (auto xml = state.createXml()) return xml->writeTo (f);
    return false;
}

bool MalonProcessor::clearDefault() { return getDefaultsFile().deleteFile(); }

bool MalonProcessor::loadDefault()
{
    auto f = getDefaultsFile();
    if (! f.existsAsFile()) return false;
    if (auto xml = parseXML (f))
    {
        applyStateTree (ValueTree::fromXml (*xml));
        return true;
    }
    return false;
}

} // namespace malon
