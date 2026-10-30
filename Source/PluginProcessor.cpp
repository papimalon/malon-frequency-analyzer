// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    if (useUserDefault()) loadDefault();
}

bool MalonProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::stereo() || in == AudioChannelSet::mono());
}

static int bpoFromIndex (int i) { static const int v[] = { 3, 4, 6 }; return v[jlimit (0, 2, i)]; }

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
        meter.setRmsWindowMs (kRmsWindowsMs[jlimit (0, 3, (int) pRmsWindow->load())]);
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
    return { (int) apvts.state.getProperty (kEdW, 780), (int) apvts.state.getProperty (kEdH, 600) };
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
