// Malon Frequency Analyzer - plugin-level tests + headless screenshots.
// Runs the real processor and the real editor without a DAW and without a screen
// (use xvfb-run on Linux; JUCE needs a display connection for fonts only).
//   MalonPluginTests --out <folder>      writes PNG screenshots + runs the checks
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>
#include "../Source/Modes.h"
#include "../Source/ui/MeterScale.h"

using namespace juce;
using namespace malon;

static int failures = 0, checks = 0;
#define CHECK_MSG(cond, ...) do { ++checks; if (!(cond)) { ++failures; std::printf ("  FAIL line %d: %s -- ", __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)
#define CHECK(cond) CHECK_MSG(cond, "%s", "")

static void setParam (MalonProcessor& p, const char* id, float plainValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}
static float getParam (MalonProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

// ---------------------------------------------------------------------------

static void testPassthroughAndLayouts()
{
    std::printf ("[processor] pass-through is bit exact, mono + stereo, float + double\n");
    MalonProcessor::useUserDefault() = false;
    MalonProcessor p;
    p.setPlayConfigDetails (2, 2, 48000, 512);
    p.prepareToPlay (48000, 512);

    Random rnd (7);
    for (int meterOn : { 1, 0, 1 })
    for (int ch : { 1, 2 })
        for (int n : { 1, 7, 64, 255, 256, 257, 480, 1000, 4096 })
        {
            setParam (p, ids::meterOn, (float) meterOn);
            AudioBuffer<float> b (ch, n), ref;
            // +-2.0 on purpose: over full scale must pass untouched too (the meter only reads)
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) b.setSample (c, i, (rnd.nextFloat() * 2.0f - 1.0f) * 2.0f);
            ref.makeCopyOf (b);
            MidiBuffer m; p.processBlock (b, m);
            bool same = true;
            for (int c = 0; c < ch; ++c) same &= std::memcmp (b.getReadPointer (c), ref.getReadPointer (c), sizeof (float) * (size_t) n) == 0;
            CHECK_MSG (same, "float %d ch %d samples altered", ch, n);

            AudioBuffer<double> bd (ch, n), refd;
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) bd.setSample (c, i, (rnd.nextDouble() * 2.0 - 1.0) * 2.0);
            refd.makeCopyOf (bd);
            p.processBlock (bd, m);
            same = true;
            for (int c = 0; c < ch; ++c) same &= std::memcmp (bd.getReadPointer (c), refd.getReadPointer (c), sizeof (double) * (size_t) n) == 0;
            CHECK_MSG (same, "double %d ch %d samples altered", ch, n);
        }

    auto layout = [] (const AudioChannelSet& in, const AudioChannelSet& out)
    { AudioProcessor::BusesLayout l; l.inputBuses.add (in); l.outputBuses.add (out); return l; };
    const auto mono = layout (AudioChannelSet::mono(), AudioChannelSet::mono());
    const auto stereo = layout (AudioChannelSet::stereo(), AudioChannelSet::stereo());
    const auto bad = layout (AudioChannelSet::mono(), AudioChannelSet::stereo());
    CHECK (p.isBusesLayoutSupported (mono)); CHECK (p.isBusesLayoutSupported (stereo)); CHECK (! p.isBusesLayoutSupported (bad));
}

static void testParametersAndState()
{
    std::printf ("[state] defaults, ranges, save/restore\n");
    MalonProcessor::useUserDefault() = false;
    MalonProcessor a;
    // defaults/ranges from the original settings table
    CHECK (getParam (a, ids::channel) == 2);   CHECK (getParam (a, ids::peakDecay) == 15);
    CHECK (getParam (a, ids::avgTime) == 3000); CHECK (getParam (a, ids::holdTime) == 2000);
    CHECK (getParam (a, ids::holdDecay) == 3);  CHECK (getParam (a, ids::ceiling) == 3);
    CHECK (getParam (a, ids::range) == 51);     CHECK (getParam (a, ids::grid) == 5);
    CHECK (getParam (a, ids::bands) == 2);      CHECK (getParam (a, ids::avgSize) == 3);
    CHECK (getParam (a, ids::meterOn) == 1);    CHECK (getParam (a, ids::rmsWindow) == 4);   // meters on, 300 ms
    CHECK (getParam (a, ids::slope) == 2);      // 4.5 dB/oct by default, like SPAN
    CHECK (getParam (a, ids::holdOn) == 1);     CHECK (getParam (a, ids::avgOn) == 1);
    CHECK (getParam (a, ids::holdSize) == 2);   CHECK (getParam (a, ids::avgSlope) == 0);
    auto rng = [&] (const char* id) { return dynamic_cast<RangedAudioParameter*> (a.apvts.getParameter (id))->getNormalisableRange(); };
    CHECK (rng (ids::avgTime).start == 100 && rng (ids::avgTime).end == 20000 && rng (ids::avgTime).interval == 100);
    CHECK (rng (ids::avgSlope).start == -6 && rng (ids::avgSlope).end == 12);
    CHECK (rng (ids::range).start == 10 && rng (ids::range).end == 120);
    CHECK (rng (ids::ceiling).start == -80 && rng (ids::ceiling).end == 20);

    setParam (a, ids::channel, 3); setParam (a, ids::avgTime, 7500); setParam (a, ids::holdShow, 2);
    setParam (a, ids::bands, 0); setParam (a, ids::avgSlope, 4); setParam (a, ids::range, 80); setParam (a, ids::holdInfinite, 1);
    setParam (a, ids::meterOn, 0); setParam (a, ids::rmsWindow, 3);
    auto cs = builtInSchemes()[3].scheme; cs.c[2] = Colour (0x80123456);
    a.setColors (cs); a.saveEditorSize (900, 500); a.setSettingsOpen (true);
    MemoryBlock blob; a.getStateInformation (blob);

    MalonProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (getParam (b, ids::channel) == 3);     CHECK (getParam (b, ids::avgTime) == 7500);
    CHECK (getParam (b, ids::holdShow) == 2);    CHECK (getParam (b, ids::bands) == 0);
    CHECK (getParam (b, ids::avgSlope) == 4);    CHECK (getParam (b, ids::range) == 80);
    CHECK (getParam (b, ids::holdInfinite) == 1);
    CHECK (getParam (b, ids::meterOn) == 0);     CHECK (getParam (b, ids::rmsWindow) == 3);
    CHECK (b.getColors().toString() == cs.toString());
    CHECK (b.getSavedEditorSize().x == 900 && b.getSavedEditorSize().y == 500 && b.getSettingsOpen());

    // garbage state must not crash or change anything
    MalonProcessor c; const char junk[] = "not a state";
    c.setStateInformation (junk, (int) sizeof (junk));
    CHECK (getParam (c, ids::avgTime) == 3000);

    // Save-as-default round trip (writes into the real app-data folder, so redirect HOME for the test)
    auto oldHome = SystemStats::getEnvironmentVariable ("HOME", "");
    auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("malon-fa-test-home");
    tmp.deleteRecursively(); tmp.createDirectory();
   #if JUCE_LINUX
    setenv ("HOME", tmp.getFullPathName().toRawUTF8(), 1);
    CHECK (a.saveAsDefault());
    MalonProcessor::useUserDefault() = true;
    MalonProcessor d;
    CHECK_MSG (getParam (d, ids::avgTime) == 7500 && d.getColors().toString() == cs.toString(), "default not applied to new instance");
    CHECK (d.clearDefault());
    MalonProcessor::useUserDefault() = false;
    setenv ("HOME", oldHome.toRawUTF8(), 1);
   #endif
    tmp.deleteRecursively();
}

static void testEndToEnd()
{
    std::printf ("[end-to-end] 1 kHz tone through the processor shows up in the 1 kHz band; bands/octave switch\n");
    MalonProcessor::useUserDefault() = false;
    MalonProcessor p; p.setPlayConfigDetails (2, 2, 48000, 480); p.prepareToPlay (48000, 480);
    for (int bpoIdx : { 2, 1, 0, 3, 4, 2 })
    {
        setParam (p, ids::bands, (float) bpoIdx);
        const double bpo = bpoIdx == 0 ? 3 : bpoIdx == 1 ? 4 : bpoIdx == 2 ? 6 : bpoIdx == 3 ? 12 : 24;
        Readout r;
        double ph = 0;
        for (int blk = 0; blk < 60; ++blk)
        {
            AudioBuffer<float> b (2, 480);
            for (int i = 0; i < 480; ++i, ph += 2.0 * 3.14159265358979 * 1000.0 / 48000.0) { auto s = (float) std::sin (ph); b.setSample (0, i, s); b.setSample (1, i, s); }
            MidiBuffer m; p.processBlock (b, m);
            p.getBank().read (r);
        }
        CHECK_MSG (r.bandsPerOctave == bpo, "bpo %g", r.bandsPerOctave);
        int best = 0; for (int i = 0; i < r.numBands; ++i) if (r.peakSq[i + 1] > r.peakSq[best + 1]) best = i;
        const double fc = r.freqMin * std::pow (2.0, best / bpo);
        CHECK_MSG (std::fabs (std::log2 (fc / 1000.0)) < 0.5 / bpo + 0.01, "bpo %g: peak at %.0f Hz", bpo, fc);
    }
}

static void testMeterInProcessor()
{
    std::printf ("[meter] 997 Hz at -20 dBFS through the real processor: loudness, RMS, true peak, mono/stereo, reset, on/off\n");
    MalonProcessor::useUserDefault() = false;
    auto near = [] (double a, double b, double tol) { return std::fabs (a - b) <= tol; };
    const double fs = 48000.0;

    auto feed = [&] (MalonProcessor& p, int numCh, double seconds, double amp, const std::vector<int>& blocks)
    {
        double ph = 0.0; size_t bi = 0; long long left = (long long) (seconds * fs);
        while (left > 0)
        {
            const int n = (int) std::min<long long> (left, blocks[bi++ % blocks.size()]);
            AudioBuffer<float> b (numCh, n);
            for (int i = 0; i < n; ++i, ph += 2.0 * MathConstants<double>::pi * 997.0 / fs)
                for (int c = 0; c < numCh; ++c) b.setSample (c, i, (float) (amp * std::sin (ph)));
            MidiBuffer m; p.processBlock (b, m);
            left -= n;
        }
    };

    {   // stereo, odd block sizes
        MalonProcessor p; p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
        feed (p, 2, 8.0, 0.1, { 1, 17, 480, 511, 64, 1024 });
        auto r = p.getMeter().read();
        std::printf ("  stereo: M %.3f S %.3f I %.3f LUFS, RMS %.3f dBFS, TP %.3f dBTP, LRA %.3f\n", r.momentary, r.shortTerm, r.integrated, r.rmsCombinedDb, r.truePeakMaxDb, r.loudnessRange);
        CHECK (r.channels == 2);
        CHECK (near (r.momentary, -20.0, 0.05) && near (r.shortTerm, -20.0, 0.05) && near (r.integrated, -20.0, 0.05));
        CHECK (near (r.rmsCombinedDb, -23.01, 0.05) && near (r.rmsDb[0], -23.01, 0.05) && near (r.rmsDb[1], -23.01, 0.05));
        CHECK (near (r.truePeakMaxDb, -20.0, 0.1) && ! r.clipped);
        CHECK (r.loudnessRange < 0.1);

        // reset clears everything, then it measures again
        p.getMeter().requestReset();
        feed (p, 2, 0.05, 0.0, { 480 });
        r = p.getMeter().read();
        CHECK (! std::isfinite (r.integrated) && ! std::isfinite (r.truePeakMaxDb) && ! r.clipped);
        feed (p, 2, 4.0, 0.1, { 480 });
        CHECK (near (p.getMeter().read().integrated, -20.0, 0.05));

        // RMS window selectable through the parameter
        setParam (p, ids::rmsWindow, 0);   // 100 ms
        feed (p, 2, 0.5, 0.1, { 480 });
        CHECK (near (p.getMeter().read().rmsCombinedDb, -23.01, 0.05));

        // clip latch: a full-scale sample
        {
            AudioBuffer<float> b (2, 480); b.clear();
            for (int i = 0; i < 480; ++i) { b.setSample (0, i, (i % 2) ? 1.0f : -1.0f); b.setSample (1, i, 0.0f); }
            MidiBuffer m; p.processBlock (b, m);
        }
        r = p.getMeter().read();
        CHECK_MSG (r.clipped && r.samplePeakMaxDb > -0.01, "clip not latched (%.2f)", r.samplePeakMaxDb);
        CHECK (r.truePeakMaxDb >= -0.01);
        feed (p, 2, 0.1, 0.01, { 480 });
        CHECK (p.getMeter().read().clipped);                          // stays until reset
        p.getMeter().requestReset(); feed (p, 2, 0.05, 0.01, { 480 });
        CHECK (! p.getMeter().read().clipped);

        // switching off stops measuring; switching on starts from a clean slate
        setParam (p, ids::meterOn, 0);
        feed (p, 2, 0.5, 0.5, { 480 });
        CHECK (near (p.getMeter().read().truePeakMaxDb, -40.0, 0.2) || p.getMeter().read().truePeakMaxDb < -39.0);   // unchanged by the 0.5 burst
        setParam (p, ids::meterOn, 1);
        feed (p, 2, 1.0, 0.1, { 480 });
        CHECK (near (p.getMeter().read().truePeakMaxDb, -20.0, 0.1));
    }
    {   // mono: one channel measured as one channel (-3 LU vs the same level on two channels)
        MalonProcessor p; p.setPlayConfigDetails (1, 1, fs, 480); p.prepareToPlay (fs, 480);
        feed (p, 1, 6.0, 0.1, { 480 });
        auto r = p.getMeter().read();
        std::printf ("  mono:   I %.3f LUFS, RMS %.3f dBFS\n", r.integrated, r.rmsCombinedDb);
        CHECK (r.channels == 1);
        CHECK (near (r.integrated, -23.01, 0.05) && near (r.rmsCombinedDb, -23.01, 0.05));
    }
    {   // silence
        MalonProcessor p; p.setPlayConfigDetails (2, 2, fs, 480); p.prepareToPlay (fs, 480);
        feed (p, 2, 4.0, 0.0, { 480 });
        auto r = p.getMeter().read();
        CHECK (! std::isfinite (r.integrated) && ! std::isfinite (r.momentary) && ! std::isfinite (r.truePeakMaxDb) && ! std::isfinite (r.rmsCombinedDb) && ! r.clipped);
    }
    {   // other sample rates + prepare again
        for (double rate : { 44100.0, 96000.0 })
        {
            MalonProcessor p; p.setPlayConfigDetails (2, 2, rate, 480); p.prepareToPlay (rate, 480);
            double ph = 0.0; for (int k = 0; k < (int) (6 * rate / 480); ++k)
            {
                AudioBuffer<float> b (2, 480);
                for (int i = 0; i < 480; ++i, ph += 2.0 * MathConstants<double>::pi * 997.0 / rate) for (int c = 0; c < 2; ++c) b.setSample (c, i, (float) (0.1 * std::sin (ph)));
                MidiBuffer m; p.processBlock (b, m);
            }
            CHECK_MSG (near (p.getMeter().read().integrated, -20.0, 0.05), "rate %.0f: I %.3f", rate, p.getMeter().read().integrated);
        }
    }
}

// ---- v1.2: SPAN-style Modes + metering scales -------------------------------

static void testModesAndScales()
{
    std::printf ("[modes] spectrum Mode presets, manual change -> Off, state; metering scales\n");
    MalonProcessor::useUserDefault() = false;
    {
        MalonProcessor p;
        CHECK_MSG (roundToInt (getParam (p, ids::specMode)) == kModeDefault, "default mode %d", roundToInt (getParam (p, ids::specMode)));
        CHECK_MSG (modeMatches (p.apvts, kModeDefault), "factory settings must equal the Default mode");
        CHECK (roundToInt (getParam (p, ids::slope)) == 2);       // 4.5 dB/oct
        for (int m = 1; m < 8; ++m)
        {
            setParam (p, ids::specMode, (float) m); p.syncModeNow();
            CHECK_MSG (roundToInt (getParam (p, ids::specMode)) == m && modeMatches (p.apvts, m), "mode %d not applied", m);
        }
        setParam (p, ids::specMode, 3); p.syncModeNow();
        CHECK (roundToInt (getParam (p, ids::bands)) == kBandsHiResIndex && roundToInt (getParam (p, ids::peakShow)) == 2);
        setParam (p, ids::peakDecay, 30); p.syncModeNow();            // hand-made change
        CHECK_MSG (roundToInt (getParam (p, ids::specMode)) == 0, "manual change did not switch to Off");
        CHECK (std::abs (getParam (p, ids::peakDecay) - 30.0f) < 0.01f);  // and the change itself stays
        setParam (p, ids::ceiling, 0); p.syncModeNow();               // not a Mode setting: Off stays Off, nothing applied
        CHECK (roundToInt (getParam (p, ids::specMode)) == 0);
        // a session saved in Off/custom restores exactly, never re-applying a preset
        MemoryBlock mb; p.getStateInformation (mb);
        MalonProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize()); q.syncModeNow();
        CHECK (roundToInt (getParam (q, ids::specMode)) == 0 && std::abs (getParam (q, ids::peakDecay) - 30.0f) < 0.01f);
        // a session saved in Avg+Max restores as Avg+Max
        setParam (p, ids::specMode, 7); p.syncModeNow();
        p.getStateInformation (mb);
        MalonProcessor r; r.setStateInformation (mb.getData(), (int) mb.getSize()); r.syncModeNow();
        CHECK (roundToInt (getParam (r, ids::specMode)) == 7 && getParam (r, ids::holdInfinite) > 0.5f && getParam (r, ids::peakOn) < 0.5f);
        // an old (v1.1) session without the Mode setting but with custom values: values kept, Mode -> Off
        auto t = p.apvts.copyState(); t.setProperty ("dummy", 1, nullptr);
        for (int i = t.getNumChildren(); --i >= 0;)
            if (t.getChild (i).getProperty ("id").toString() == ids::specMode) t.removeChild (i, nullptr);
        if (auto xml = t.createXml())
        {
            MemoryBlock old; AudioProcessor::copyXmlToBinary (*xml, old);
            MalonProcessor o; o.setStateInformation (old.getData(), (int) old.getSize()); o.syncModeNow();
            CHECK_MSG (roundToInt (getParam (o, ids::specMode)) == 0 && getParam (o, ids::holdInfinite) > 0.5f, "old session: mode %d", roundToInt (getParam (o, ids::specMode)));
        }
    }
    // metering scales: standard values (sine-referenced K-system, BS.1770 LUFS, LU = LUFS + 23)
    {
        const double sineRms = -23.0103;     // a -20 dBFS peak sine
        auto mv = [&] (int sc) { return meterValues (meterScaleSpec (sc), sineRms, sineRms, -20.0, -20.0, -20.0); };
        CHECK (std::abs (mv (msDbfs).bar - sineRms) < 1e-9 && std::abs (mv (msDbfs).peak + 20.0) < 1e-9);
        CHECK (std::abs (mv (msDbfsPlus3).bar + 20.0) < 1e-3 && std::abs (mv (msDbfsPlus3).peak + 20.0) < 1e-9);
        CHECK (std::abs (mv (msK20).bar) < 1e-3 && std::abs (mv (msK20C).bar) < 1e-3);        // -20 dBFS sine = 0 on K-20
        CHECK (std::abs (mv (msK14).bar + 6.0) < 1e-3 && std::abs (mv (msK12).bar + 8.0) < 1e-3);
        CHECK (std::abs (meterScaleSpec (msK20).top - 20.0) < 1e-9 && std::abs (meterScaleSpec (msK14).top - 14.0) < 1e-9);  // top = 0 dBFS
        CHECK (std::abs (mv (msLufsML).bar + 20.0) < 1e-9 && std::abs (mv (msLuSL).bar - 3.0) < 1e-9);
        CHECK (std::abs (meterScaleSpec (msDbfs30).bottom + 30.0) < 1e-9 && std::abs (meterScaleSpec (msDbfs15).bottom + 15.0) < 1e-9);
        // in the processor: 600 ms K window of a -20 dBFS sine reads -23.01 dBFS
        MalonProcessor p; p.setPlayConfigDetails (2, 2, 48000, 480); p.prepareToPlay (48000, 480);
        double ph = 0;
        for (int k = 0; k < 200; ++k)
        {
            AudioBuffer<float> b (2, 480);
            for (int i = 0; i < 480; ++i, ph += 2.0 * MathConstants<double>::pi * 1000.0 / 48000.0) for (int c = 0; c < 2; ++c) b.setSample (c, i, (float) (0.1 * std::sin (ph)));
            MidiBuffer m; p.processBlock (b, m);
        }
        const auto ro = p.getMeter().read();
        std::printf ("  -20 dBFS sine: K window RMS %.3f dBFS -> K-20 %.3f, momentary %.3f LUFS -> LU %.3f\n", ro.rmsKDb[0],
                     meterValues (meterScaleSpec (msK20), ro.rmsFastDb[0], ro.rmsKDb[0], 0, ro.momentary, ro.shortTerm).bar,
                     ro.momentary, meterValues (meterScaleSpec (msLuML), 0, 0, 0, ro.momentary, ro.shortTerm).bar);
        CHECK_MSG (std::abs (ro.rmsKDb[0] - sineRms) < 0.02, "K RMS %.3f", ro.rmsKDb[0]);
    }
}

// ---- screenshots ------------------------------------------------------------

struct Synth
{
    // pink-ish bass-heavy "music": noise with -4.5 dB/oct tilt, kick tone, a few tones, hi-hat bursts
    std::mt19937 rng { 99 };
    std::normal_distribution<float> nd { 0.0f, 1.0f };
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    double n = 0;
    float next()
    {
        const double x = nd (rng);
        b0 = 0.99886 * b0 + x * 0.0555179; b1 = 0.99332 * b1 + x * 0.0750759; b2 = 0.96900 * b2 + x * 0.1538520;
        b3 = 0.86650 * b3 + x * 0.3104856; b4 = 0.55000 * b4 + x * 0.5329522; b5 = -0.7616 * b5 - x * 0.0168980;
        const double pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362) * 0.05; b6 = x * 0.115926;
        const double t = n / 48000.0; n += 1;
        const double beat = std::fmod (t, 0.5);
        const double kick = std::exp (-beat * 14.0) * std::sin (2 * 3.14159265358979 * (48.0 + 90.0 * std::exp (-beat * 40.0)) * beat) * 0.55;
        const double bass = 0.18 * std::sin (2 * 3.14159265358979 * 55.0 * t) + 0.07 * std::sin (2 * 3.14159265358979 * 110.0 * t);
        const double lead = 0.06 * std::sin (2 * 3.14159265358979 * 880.0 * t) + 0.04 * std::sin (2 * 3.14159265358979 * 2637.0 * t);
        const double hat = (std::fmod (t, 0.25) < 0.04) ? 0.12 * nd (rng) * (1.0 - std::fmod (t, 0.25) / 0.04) : 0.0;
        // song-like dynamics: an 8-bar phrase where the drums drop out for a 2 s breakdown and the
        // mix swells, so the LUFS / RMS / peak histories move like real music, not a flat line
        const double phrase = std::fmod (t, 8.0);
        const bool breakdown = phrase >= 6.0;
        const double swell = 0.75 + 0.25 * std::sin (2 * 3.14159265358979 * t / 8.0);
        const double drums = breakdown ? 0.0 : 1.0;
        return (float) (swell * (pink * (breakdown ? 0.5 : 1.0) + drums * (kick + hat) + bass + lead));
    }
};

static void runAudio (MalonProcessor& p, MalonEditor& ed, Synth& syn, double seconds)
{
    const int blk = 1536;   // = 32 ms at 48 kHz (the display's nominal poll interval)
    for (int k = 0; k < int (seconds * 48000 / blk); ++k)
    {
        AudioBuffer<float> b (2, blk);
        for (int i = 0; i < blk; ++i) { const float s = syn.next(); b.setSample (0, i, s * 0.6f); b.setSample (1, i, s * 0.54f); }
        MidiBuffer m; p.processBlock (b, m);
        ed.pollNow (32.0);
    }
}

static bool save (Component& c, const File& file, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    FileOutputStream out (file);
    if (! out.openedOk()) return false;
    PNGImageFormat png;
    return png.writeImageToStream (img, out);
}

static Image snap (Component& c) { return c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f); }

static int countPixels (const Image& img, Rectangle<int> area, std::function<bool (Colour)> pred)
{
    int n = 0; Image::BitmapData d (img, Image::BitmapData::readOnly);
    area = area.getIntersection (img.getBounds());
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x) if (pred (d.getPixelColour (x, y))) ++n;
    return n;
}

static void screenshots (const File& dir)
{
    std::printf ("[ui] headless screenshots -> %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
    MalonProcessor::useUserDefault() = false;

    MalonProcessor p; p.setPlayConfigDetails (2, 2, 48000, 1536); p.prepareToPlay (48000, 1536);
    p.saveEditorSize (780, 480);
    auto ed = std::unique_ptr<MalonEditor> (static_cast<MalonEditor*> (p.createEditor()));
    ed->haltTimer();
    CHECK (ed->getWidth() == 780 && ed->getHeight() == 480);
    Synth syn;
    runAudio (p, *ed, syn, 5.0);

    // v1.1: view selector (spectrum / LUFS / RMS / peak history), legend, hover read-outs, correlation, mid/side
    {
        ed->setSize (780, 500);
        runAudio (p, *ed, syn, 11.0);
        CHECK (ed->getViewMode() == 0 && ! ed->getHistory().isVisible());
        auto pa = ed->getView().getPlotArea();
        ed->getView().simulateHover ({ pa.getX() + pa.getWidth() * 0.42f, pa.getY() + pa.getHeight() * 0.45f });
        CHECK (save (*ed, dir.getChildFile ("v11-mode-1-spectrum.png")));
        // Hi-Res (24 bands/octave): kick fundamental, 55 Hz bass and its 110 Hz harmonic resolve as separate peaks
        setParam (p, ids::bands, 4); runAudio (p, *ed, syn, 4.0);
        CHECK (ed->getModel().numBands > 200);
        ed->getView().simulateHover ({ pa.getX() + pa.getWidth() * 0.17f, pa.getY() + pa.getHeight() * 0.30f });
        CHECK (save (*ed, dir.getChildFile ("v11-mode-1c-spectrum-hires.png")));
        setParam (p, ids::bands, 2); runAudio (p, *ed, syn, 2.0);
        ed->getView().simulateHover ({ -100.0f, -100.0f });
        const char* names[3] = { "v11-mode-2-lufs.png", "v11-mode-3-rms.png", "v11-mode-4-peak.png" };
        for (int m = 1; m <= 3; ++m)
        {
            setParam (p, ids::viewMode, (float) m); runAudio (p, *ed, syn, 0.3);
            CHECK_MSG (ed->getViewMode() == m && ed->getHistory().isVisible() && ! ed->getView().isVisible() && ed->getHistory().getMode() == m - 1, "mode %d", m);
            auto hp = ed->getHistory().getPlotArea();
            ed->getHistory().simulateHover ({ hp.getX() + hp.getWidth() * 0.6f, hp.getY() + hp.getHeight() * 0.3f });
            CHECK (save (*ed, dir.getChildFile (names[m - 1])));
        }
        // state round trip keeps the view mode
        { MemoryBlock mb; p.getStateInformation (mb); MalonProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
          CHECK (roundToInt (q.apvts.getRawParameterValue (ids::viewMode)->load()) == 3); }
        setParam (p, ids::viewMode, 0); setParam (p, ids::channel, 5); runAudio (p, *ed, syn, 1.0);
        CHECK (ed->getViewMode() == 0 && ed->getView().isVisible());
        CHECK (save (*ed, dir.getChildFile ("v11-mode-1b-spectrum-side.png")));
        {
            AudioBuffer<float> b (2, 1536); b.clear();
            for (int i = 0; i < 1536; ++i) { b.setSample (0, i, std::nanf ("")); b.setSample (1, i, INFINITY); }
            MidiBuffer m; p.processBlock (b, m); ed->pollNow (32.0);
        }
        setParam (p, ids::channel, 2);
        runAudio (p, *ed, syn, 4.0);
        const auto ro = p.getMeter().read();
        std::printf ("  after NaN/inf block: M %.2f S %.2f RMS %.2f TP %.2f corr %.2f\n", ro.momentary, ro.shortTerm, ro.rmsCombinedDb, ro.truePeakMaxDb, ro.correlation);
        CHECK (std::isfinite (ro.momentary) && std::isfinite (ro.shortTerm) && std::isfinite (ro.rmsCombinedDb) && std::isfinite (ro.truePeakMaxDb));
    }

    // 1. default view
    ed->setSize (780, 480);
    CHECK (save (*ed, dir.getChildFile ("01-default.png")));
    auto img = snap (*ed);
    auto bounds = img.getBounds();
    const int blue = countPixels (img, bounds, [] (Colour c) { return c.getBlue() > 150 && c.getRed() < 90 && c.getGreen() > 70 && c.getGreen() < 170; });
    const int red  = countPixels (img, bounds, [] (Colour c) { return c.getRed() > 200 && c.getGreen() < 90 && c.getBlue() < 130; });
    const int white = countPixels (img, bounds, [] (Colour c) { return c.getRed() > 200 && c.getGreen() > 200 && c.getBlue() > 200; });
    std::printf ("  default view pixels: peak-blue %d, average-red %d, hold/white %d\n", blue, red, white);
    CHECK_MSG (blue > 1500, "peak bars not drawn (%d)", blue);
    CHECK_MSG (red > 300, "average curve not drawn (%d)", red);
    CHECK_MSG (white > 100, "hold curve / text not drawn (%d)", white);
    // dark and mostly empty = 'lots of negative space'
    const int dark = countPixels (img, bounds, [] (Colour c) { return c.getBrightness() < 0.12f; });
    CHECK_MSG (dark > bounds.getWidth() * bounds.getHeight() / 2, "UI not predominantly dark (%d)", dark);

    // 1b. meters visible and showing real numbers; meters off hides the strip
    CHECK (ed->isMeterVisible());
    {
        auto ro = p.getMeter().read();
        std::printf ("  meter after 5 s of synth music: TP %.1f dBTP, RMS %.1f dBFS, I %.1f LUFS, LRA %.1f LU\n", ro.truePeakMaxDb, ro.rmsCombinedDb, ro.integrated, ro.loudnessRange);
        CHECK (std::isfinite (ro.truePeakMaxDb) && std::isfinite (ro.rmsCombinedDb) && std::isfinite (ro.integrated) && std::isfinite (ro.shortTerm));
        const int stripBlue = countPixels (img, { 0, ed->getHeight() - MeterStrip::preferredHeight, ed->getWidth(), MeterStrip::preferredHeight },
                                           [] (Colour c) { return c.getBrightness() > 0.7f; });
        CHECK_MSG (stripBlue > 300, "meter strip text not drawn (%d)", stripBlue);
    }
    setParam (p, ids::meterOn, 0);
    ed->pollNow (32.0);
    CHECK (! ed->isMeterVisible());
    CHECK (save (*ed, dir.getChildFile ("01b-meters-off.png")));
    setParam (p, ids::meterOn, 1);
    ed->pollNow (32.0);
    CHECK (ed->isMeterVisible());
    runAudio (p, *ed, syn, 5.0);

    // 2. settings drawer open
    ed->setSize (1060, 640);
    ed->setSettingsOpen (true);
    runAudio (p, *ed, syn, 0.5);
    CHECK (save (*ed, dir.getChildFile ("02-settings-open.png")));
    ed->scrollSettingsTo (2000);
    CHECK (save (*ed, dir.getChildFile ("02b-settings-colors.png")));
    ed->scrollSettingsTo (0);

    // 3. display modes
    ed->setSettingsOpen (false);
    ed->setSize (780, 480);
    struct Mode { const char* name; float avgShow, holdShow; };
    const Mode modes[] = { { "03-bars.png", 0, 0 }, { "04-curve-fill.png", 2, 2 } };
    for (auto& m : modes)
    {
        setParam (p, ids::avgShow, m.avgShow); setParam (p, ids::holdShow, m.holdShow);
        runAudio (p, *ed, syn, 2.0);
        CHECK (save (*ed, dir.getChildFile (m.name)));
    }
    setParam (p, ids::avgShow, 1); setParam (p, ids::holdShow, 1);

    // 4. bands per octave and octave grid
    for (int idx : { 0, 1 })
    {
        setParam (p, ids::bands, (float) idx); setParam (p, ids::freqGrid, idx == 0 ? 1.0f : 0.0f);
        runAudio (p, *ed, syn, 2.0);
        CHECK (save (*ed, dir.getChildFile (idx == 0 ? "05-3-bands-octave-grid.png" : "06-4-bands.png")));
    }
    setParam (p, ids::bands, 2); setParam (p, ids::freqGrid, 0);

    // 5. colour scheme
    p.setColors (builtInSchemes()[1].scheme);
    runAudio (p, *ed, syn, 1.0);
    CHECK (save (*ed, dir.getChildFile ("07-scheme-neon-palm.png")));
    p.setColors (builtInSchemes()[0].scheme);

    // 6. tiny and big
    ed->setSize (680, 380); runAudio (p, *ed, syn, 0.5);
    CHECK (save (*ed, dir.getChildFile ("08-small.png")));
    ed->setSize (1600, 800); runAudio (p, *ed, syn, 0.5);
    CHECK (save (*ed, dir.getChildFile ("09-large.png")));

    // 6b. meters with a hot signal: inter-sample overs, clip light
    {
        ed->setSize (780, 480);
        p.getMeter().requestReset();
        runAudio (p, *ed, syn, 2.0);
        for (int blk = 0; blk < 4; ++blk)
        {
            AudioBuffer<float> b (2, 1536); b.clear();
            for (int i = 0; i < 1536; ++i) { const float v = (float) std::sin (2.0 * 3.14159265358979 * 12000.0 * i / 48000.0 + 0.785398); b.setSample (0, i, v); b.setSample (1, i, 0.5f * v); }
            b.setSample (0, 700, 1.0f);
            MidiBuffer m; p.processBlock (b, m); ed->pollNow (32.0);
        }
        runAudio (p, *ed, syn, 1.0);
        auto ro = p.getMeter().read();
        std::printf ("  hot signal: TP %.2f dBTP, sample peak %.2f dBFS, clip %d\n", ro.truePeakMaxDb, ro.samplePeakMaxDb, (int) ro.clipped);
        CHECK (ro.clipped && ro.truePeakMaxDb > 0.0);
        CHECK (save (*ed, dir.getChildFile ("11-meters-clip.png")));
        // the Reset button on the strip clears it
        ed->getMeterStrip().onReset();
        runAudio (p, *ed, syn, 0.2);
        CHECK (! p.getMeter().read().clipped);
        // narrow window with the settings drawer open: the strip must not overlap anything
        ed->setSize (620, 400); ed->setSettingsOpen (true);
        runAudio (p, *ed, syn, 0.5);
        CHECK (save (*ed, dir.getChildFile ("12-meters-with-settings.png")));
        ed->setSettingsOpen (false);
        ed->setSize (780, 480);
    }

    // 7. about box (MIT notice + credit)
    ed->setSize (780, 460);
    ed->showAbout (true);
    CHECK (save (*ed, dir.getChildFile ("10-about.png")));
    ed->showAbout (false);

    // freeze really freezes
    ed->setSize (780, 480);
    runAudio (p, *ed, syn, 0.5);
    ed->setFrozen (true);
    const double before = ed->getModel().peak[20];
    runAudio (p, *ed, syn, 1.0);
    CHECK_MSG (ed->getModel().peak[20] == before, "frozen display moved");
    ed->setFrozen (false);

    // v1.2: HI-RES (48 bands/oct filled curve), Mode menu, metering scales
    {
        ed->setSize (780, 500);
        setParam (p, ids::specMode, 0); p.syncModeNow();
        setParam (p, ids::specMode, 1); p.syncModeNow();
        CHECK (ed->getModeBox().isVisible());
        setParam (p, ids::specMode, 3); p.syncModeNow();               // View: High Res (replaces the HI-RES button)
        CHECK (! ed->getHiResButton().isVisible());
        CHECK_MSG (roundToInt (getParam (p, ids::specMode)) == 3 && roundToInt (getParam (p, ids::bands)) == kBandsHiResIndex, "View High Res -> mode %d", roundToInt (getParam (p, ids::specMode)));
        runAudio (p, *ed, syn, 8.5);   // (lands outside the synth's drum breakdown)
        CHECK (ed->getModel().numBands > 400);
        CHECK (save (*ed, dir.getChildFile ("v12-1-hires-48.png")));
        setParam (p, ids::specMode, 1); p.syncModeNow();
        CHECK (roundToInt (getParam (p, ids::specMode)) == 1 && roundToInt (getParam (p, ids::bands)) == kBandsDefaultIndex);
        const char* shots[] = { nullptr, nullptr, "v12-2-mode-slower.png", nullptr, nullptr, "v12-3-mode-master.png", "v12-4-mode-average.png", "v12-5-mode-avgmax.png" };
        for (int m : { 2, 5, 6, 7 })
        {
            setParam (p, ids::specMode, (float) m); p.syncModeNow(); runAudio (p, *ed, syn, 4.0);
            CHECK (save (*ed, dir.getChildFile (shots[m])));
        }
        setParam (p, ids::specMode, 1); p.syncModeNow();
        const struct { int sc; const char* f; } sc[] = { { msK20, "v12-6-meter-k20.png" }, { msLuSL, "v12-7-meter-lu-sl.png" }, { msDbfs15, "v12-8-meter-dbfs15.png" } };
        for (auto& x : sc)
        {
            setParam (p, ids::meterScale, (float) x.sc); runAudio (p, *ed, syn, 3.5);
            CHECK (std::string (ed->getOutMeter().getScale().unit) == kMeterScaleNames[x.sc] || x.sc != msK20);
            CHECK (save (*ed, dir.getChildFile (x.f)));
        }
        setParam (p, ids::meterScale, 0);
        ed->setSize (1060, 640); ed->setSettingsOpen (true); runAudio (p, *ed, syn, 0.5);
        CHECK (save (*ed, dir.getChildFile ("v12-9-settings.png")));
        ed->setSettingsOpen (false);
        ed->setSize (680, 380); runAudio (p, *ed, syn, 0.5);
        // smallest window: View, Slope and the full correlation meter must still be there, nothing overlapping
        CHECK (ed->getModeBox().isVisible() && ! ed->getCorrelationBar().isCompactMode());
        CHECK (ed->getModeBox().getX() > 0);
        CHECK (save (*ed, dir.getChildFile ("v12-10-small.png")));
        ed->setSize (780, 480);
    }

    // resizing keeps the saved size in the state
    CHECK (p.getSavedEditorSize().x == 780);
    ed.reset();
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    File out = File::getCurrentWorkingDirectory().getChildFile ("screenshots");
    for (int i = 1; i + 1 < argc; ++i) if (String (argv[i]) == "--out") out = File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);

    testPassthroughAndLayouts();
    testParametersAndState();
    testEndToEnd();
    testMeterInProcessor();
    testModesAndScales();
    screenshots (out);
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
