// Malon Frequency Analyzer - plugin-level tests + headless screenshots.
// Runs the real processor and the real editor without a DAW and without a screen
// (use xvfb-run on Linux; JUCE needs a display connection for fonts only).
//   MalonPluginTests --out <folder>      writes PNG screenshots + runs the checks
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>

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
    for (int ch : { 1, 2 })
        for (int n : { 1, 7, 64, 255, 256, 257, 480, 1000, 4096 })
        {
            AudioBuffer<float> b (ch, n), ref;
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) b.setSample (c, i, rnd.nextFloat() * 2.0f - 1.0f);
            ref.makeCopyOf (b);
            MidiBuffer m; p.processBlock (b, m);
            bool same = true;
            for (int c = 0; c < ch; ++c) same &= std::memcmp (b.getReadPointer (c), ref.getReadPointer (c), sizeof (float) * (size_t) n) == 0;
            CHECK_MSG (same, "float %d ch %d samples altered", ch, n);

            AudioBuffer<double> bd (ch, n), refd;
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) bd.setSample (c, i, rnd.nextDouble() * 2.0 - 1.0);
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
    CHECK (getParam (a, ids::holdSize) == 2);   CHECK (getParam (a, ids::avgSlope) == 0);
    auto rng = [&] (const char* id) { return dynamic_cast<RangedAudioParameter*> (a.apvts.getParameter (id))->getNormalisableRange(); };
    CHECK (rng (ids::avgTime).start == 100 && rng (ids::avgTime).end == 20000 && rng (ids::avgTime).interval == 100);
    CHECK (rng (ids::avgSlope).start == -6 && rng (ids::avgSlope).end == 12);
    CHECK (rng (ids::range).start == 10 && rng (ids::range).end == 120);
    CHECK (rng (ids::ceiling).start == -80 && rng (ids::ceiling).end == 20);

    setParam (a, ids::channel, 3); setParam (a, ids::avgTime, 7500); setParam (a, ids::holdShow, 2);
    setParam (a, ids::bands, 0); setParam (a, ids::avgSlope, 4); setParam (a, ids::range, 80); setParam (a, ids::holdInfinite, 1);
    auto cs = builtInSchemes()[3].scheme; cs.c[2] = Colour (0x80123456);
    a.setColors (cs); a.saveEditorSize (900, 500); a.setSettingsOpen (true);
    MemoryBlock blob; a.getStateInformation (blob);

    MalonProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (getParam (b, ids::channel) == 3);     CHECK (getParam (b, ids::avgTime) == 7500);
    CHECK (getParam (b, ids::holdShow) == 2);    CHECK (getParam (b, ids::bands) == 0);
    CHECK (getParam (b, ids::avgSlope) == 4);    CHECK (getParam (b, ids::range) == 80);
    CHECK (getParam (b, ids::holdInfinite) == 1);
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
    for (int bpoIdx : { 2, 1, 0, 2 })
    {
        setParam (p, ids::bands, (float) bpoIdx);
        const double bpo = bpoIdx == 0 ? 3 : bpoIdx == 1 ? 4 : 6;
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
        return (float) (pink + kick + bass + lead + hat);
    }
};

static void runAudio (MalonProcessor& p, MalonEditor& ed, Synth& syn, double seconds)
{
    const int blk = 1536;   // = 32 ms at 48 kHz (the display's nominal poll interval)
    for (int k = 0; k < int (seconds * 48000 / blk); ++k)
    {
        AudioBuffer<float> b (2, blk);
        for (int i = 0; i < blk; ++i) { const float s = syn.next(); b.setSample (0, i, s); b.setSample (1, i, s * 0.9f); }
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
    p.saveEditorSize (780, 400);
    auto ed = std::unique_ptr<MalonEditor> (static_cast<MalonEditor*> (p.createEditor()));
    ed->haltTimer();
    CHECK (ed->getWidth() == 780 && ed->getHeight() == 400);
    Synth syn;
    runAudio (p, *ed, syn, 5.0);

    // 1. default view
    ed->setSize (780, 400);
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
    ed->setSize (780, 400);
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
    ed->setSize (480, 260); runAudio (p, *ed, syn, 0.5);
    CHECK (save (*ed, dir.getChildFile ("08-small.png")));
    ed->setSize (1600, 800); runAudio (p, *ed, syn, 0.5);
    CHECK (save (*ed, dir.getChildFile ("09-large.png")));

    // 7. about box (MIT notice + credit)
    ed->setSize (780, 460);
    ed->showAbout (true);
    CHECK (save (*ed, dir.getChildFile ("10-about.png")));
    ed->showAbout (false);

    // freeze really freezes
    ed->setSize (780, 400);
    runAudio (p, *ed, syn, 0.5);
    ed->setFrozen (true);
    const double before = ed->getModel().peak[20];
    runAudio (p, *ed, syn, 1.0);
    CHECK_MSG (ed->getModel().peak[20] == before, "frozen display moved");
    ed->setFrozen (false);

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
    screenshots (out);
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
