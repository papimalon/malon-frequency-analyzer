// Malon Frequency Analyzer - DSP unit tests (no JUCE needed).
// These are how the filter bank is verified without anybody listening: known tones,
// sweeps and noise go in, band levels are checked numerically.
#include "../Source/dsp/Ballistics.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <thread>
#include <chrono>

using namespace malon;

static int g_failures = 0, g_checks = 0;
static bool g_verbose = false;

#define CHECK_MSG(cond, ...) do { ++g_checks; if (!(cond)) { ++g_failures; std::printf("  FAIL %s:%d: %s  -- ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define CHECK(cond) CHECK_MSG(cond, "%s", "")

static const double kPi = 3.14159265358979323846;

// ---- helpers ---------------------------------------------------------------

struct Run { Readout r; DisplayModel m; };

static void feed (FilterBank& fb, const std::vector<float>& l, const std::vector<float>& rr, ChannelMode mode, int blockSize = 512)
{
    for (size_t pos = 0; pos < l.size(); pos += (size_t) blockSize)
    {
        const int n = (int) std::min<size_t> ((size_t) blockSize, l.size() - pos);
        fb.process (l.data() + pos, rr.data() + pos, n, mode);
    }
}

static std::vector<float> sine (double fs, double f, double amp, double seconds)
{
    std::vector<float> v ((size_t) (fs * seconds));
    for (size_t i = 0; i < v.size(); ++i) v[i] = float (amp * std::sin (2.0 * kPi * f * double (i) / fs));
    return v;
}

static double bandCentre (const Readout& r, int i) { return r.freqMin * std::pow (2.0, double (i) / r.bandsPerOctave); }

static double peakDb (const Readout& r, int band) { return 0.5 * gainToDb (r.peakSq[band + 1] + 1e-20); }
static double avgDb  (const Readout& r, int band) { return 3.0 + 0.5 * gainToDb (r.sumSq[band + 1] / r.samples + 1e-20); }

// Steady-state reading for a sine: skip first 0.5 s (transient), then read for 1 s.
static Readout measureSine (double fs, int bpo, double f, double amp = 1.0, ChannelMode mode = ChannelMode::Both)
{
    FilterBank fb; fb.prepare (fs, bpo);
    auto warm = sine (fs, f, amp, 0.6);
    fb.process (warm.data(), warm.data(), (int) warm.size(), mode);
    Readout dump; fb.read (dump);
    // continue phase-continuously
    std::vector<float> s ((size_t) fs);
    const size_t off = warm.size();
    for (size_t i = 0; i < s.size(); ++i) s[i] = float (amp * std::sin (2.0 * kPi * f * double (i + off) / fs));
    fb.process (s.data(), s.data(), (int) s.size(), mode);
    Readout r; fb.read (r);
    return r;
}

// ---- tests -----------------------------------------------------------------

static void testBandLayout()
{
    std::printf("[layout] band counts and ranges\n");
    const double rates[] = { 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
    for (int bpo : { 3, 4, 6 })
        for (double fs : rates)
        {
            FilterBank fb; fb.prepare (fs, bpo);
            const int n = fb.getNumBands();
            const double ratio = fb.getFreqMax() / fb.getFreqMin();
            const double expected = std::pow (2.0, double (n - 1) / bpo);
            if (g_verbose) std::printf("  bpo %d fs %6.0f: %2d bands, %.1f .. %.1f Hz\n", bpo, fs, n, fb.getFreqMin(), fb.getFreqMax());
            CHECK_MSG (n >= 3 * bpo, "bpo %d fs %.0f only %d bands", bpo, fs, n);
            CHECK_MSG (n <= kMaxBands, "too many bands %d", n);
            CHECK_MSG (fb.getFreqMin() >= 22.0 && fb.getFreqMin() < 22.0 * std::pow (2.0, 1.0 / bpo) + 1e-9, "fmin %.2f", fb.getFreqMin());
            CHECK_MSG (std::fabs (ratio / expected - 1.0) < 1e-9, "log spacing off");
            CHECK_MSG (fb.getFreqMax() <= 0.47 * fs + 1.0, "top band above 0.47 fs: %.0f", fb.getFreqMax());
            CHECK_MSG (fb.getFreqMax() <= 29000.0 + 1.0, "top band above 29 kHz");
        }
    // at 44.1k / 4 bpo the analyzer should reach ~16 kHz+ and start at ~22 Hz like the original
    FilterBank fb; fb.prepare (44100, 4);
    CHECK_MSG (std::fabs (fb.getFreqMin() - 22.097) < 0.01, "fmin %.3f", fb.getFreqMin());
    CHECK (fb.getFreqMax() > 17000.0);
}

// Each band's own centre frequency must read ~0 dB and be clearly the maximum.
static void testSineAtBandCentres()
{
    std::printf("[sine] amplitude-1 sine at every band centre\n");
    const double fs = 48000;
    for (int bpo : { 3, 4, 6 })
    {
        FilterBank probe; probe.prepare (fs, bpo);
        double worstErr = 0, worstMargin = 1e9;
        int    worstBand = -1;
        const int nb = probe.getNumBands();
        for (int b = 1; b < nb - 1; ++b)           // skip the first/last edge band (partial skirts)
        {
            const double f = probe.getFreqMin() * std::pow (2.0, double (b) / bpo);
            const Readout r = measureSine (fs, bpo, f);
            const double own = peakDb (r, b);
            const double lo  = peakDb (r, b - 1);
            const double hi  = peakDb (r, b + 1);
            double others = -300;
            for (int j = 0; j < nb; ++j) if (std::abs (j - b) >= 2) others = std::max (others, peakDb (r, j));
            const double err = own - 0.0;
            if (std::fabs (err) > std::fabs (worstErr)) { worstErr = err; worstBand = b; }
            worstMargin = std::min (worstMargin, own - std::max (lo, hi));
            CHECK_MSG (std::fabs (err) < 1.5, "bpo %d band %d (%.0f Hz) reads %.2f dB", bpo, b, f, own);
            CHECK_MSG (own > lo && own > hi, "bpo %d band %d is not the maximum (own %.2f lo %.2f hi %.2f)", bpo, b, own, lo, hi);
            CHECK_MSG (own - others > 12.0, "bpo %d band %d: 2+ bands away only %.1f dB down", bpo, b, own - others);
            // average of a sine reads the same level as its peak (A=1 => 0 dB)
            CHECK_MSG (std::fabs (avgDb (r, b) - own) < 0.6, "bpo %d band %d avg %.2f vs peak %.2f", bpo, b, avgDb (r, b), own);
        }
        std::printf("  bpo %d: worst centre error %+.2f dB (band %d), min margin to neighbour %.1f dB\n", bpo, worstErr, worstBand, worstMargin);
    }
}

// Linear scaling: halving amplitude lowers every reading by 6.02 dB.
static void testLinearityAndChannels()
{
    std::printf("[linearity] level scaling and channel modes\n");
    const double fs = 44100;
    const Readout a = measureSine (fs, 6, 1000.0, 1.0);
    const Readout b = measureSine (fs, 6, 1000.0, 0.25);
    int best = 0; for (int i = 0; i < a.numBands; ++i) if (peakDb (a, i) > peakDb (a, best)) best = i;
    CHECK_MSG (std::fabs ((peakDb (a, best) - peakDb (b, best)) - 12.04) < 0.05, "scaling %.3f", peakDb (a, best) - peakDb (b, best));
    CHECK_MSG (std::fabs (bandCentre (a, best) / 1000.0 - 1.0) < 0.07, "1 kHz peaked at %.0f Hz", bandCentre (a, best));

    // channel modes: tone only on the left
    FilterBank fb; fb.prepare (fs, 4);
    auto tone = sine (fs, 1000, 1.0, 1.5);
    std::vector<float> zero (tone.size(), 0.0f);
    auto level = [&] (ChannelMode m, const std::vector<float>& l, const std::vector<float>& r)
    {
        fb.prepare (fs, 4);
        feed (fb, l, r, m); Readout rd; fb.read (rd);
        double mx = -300; for (int i = 0; i < rd.numBands; ++i) mx = std::max (mx, peakDb (rd, i)); return mx;
    };
    const double L = level (ChannelMode::Left,  tone, zero);
    const double R = level (ChannelMode::Right, tone, zero);
    const double B = level (ChannelMode::Both,  tone, zero);
    const double M = level (ChannelMode::Mix,   tone, zero);
    std::printf("  tone on left only: Left %.1f  Right %.1f  Both %.1f  Mix %.1f dB\n", L, R, B, M);
    CHECK (std::fabs (L) < 1.5);
    CHECK_MSG (R < -100.0, "right channel not silent: %.1f", R);
    CHECK (std::fabs (B - L) < 0.1);
    CHECK_MSG (std::fabs (M - (L - 6.02)) < 0.2, "mix should be -6 dB, got %.2f", M - L);
}

// Log sweep 20 Hz..20 kHz: the maximum over time of every band must be ~0 dB (flat), and
// the time of each band's maximum must increase monotonically with frequency.
static void testSweep()
{
    std::printf("[sweep] log sweep 25 Hz -> 18 kHz, 12 s\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int bpo : { 3, 4, 6 })
        {
            const double f0 = 25, f1 = 18000, T = 12.0;
            const size_t N = (size_t) (fs * T);
            std::vector<float> s (N);
            const double Lk = T / std::log (f1 / f0);
            for (size_t i = 0; i < N; ++i)
            {
                const double t = double (i) / fs;
                s[i] = float (std::sin (2.0 * kPi * f0 * Lk * (std::exp (t / Lk) - 1.0)));
            }
            FilterBank fb; fb.prepare (fs, bpo);
            const int nb = fb.getNumBands();
            std::vector<double> mx ((size_t) nb, -300.0); std::vector<double> tmax ((size_t) nb, 0.0);
            const int block = 256;
            for (size_t pos = 0; pos + block <= N; pos += block)
            {
                fb.process (s.data() + pos, s.data() + pos, block, ChannelMode::Both);
                Readout r; fb.read (r);
                for (int b = 0; b < nb; ++b)
                {
                    const double d = peakDb (r, b);
                    if (d > mx[(size_t) b]) { mx[(size_t) b] = d; tmax[(size_t) b] = double (pos) / fs; }
                }
            }
            double lo = 300, hi = -300; int mono = 0;
            // only bands whose centre the sweep really passes through (sweep starts at 25 Hz, ends at 18 kHz)
            for (int b = 1; b < nb - 1; ++b)
            {
                const double fc = fb.getFreqMin() * std::pow (2.0, double (b) / bpo);
                if (fc < f0 * 1.3 || fc > f1 / 1.15) continue;
                lo = std::min (lo, mx[(size_t) b]); hi = std::max (hi, mx[(size_t) b]);
                if (tmax[(size_t) b] < tmax[(size_t) b - 1] - 1e-6) ++mono;
            }
            std::printf("  fs %6.0f bpo %d: band maxima %+.2f .. %+.2f dB, non-monotonic: %d\n", fs, bpo, lo, hi, mono);
            CHECK_MSG (lo > -1.6 && hi < 1.6, "fs %.0f bpo %d not flat: %.2f..%.2f", fs, bpo, lo, hi);
            CHECK_MSG (mono == 0, "fs %.0f bpo %d: band maxima not in frequency order", fs, bpo);
        }
}

// Same tone at different sample rates must read the same.
static void testSampleRateIndependence()
{
    std::printf("[fs] 1 kHz and 100 Hz tone across sample rates\n");
    for (double f : { 100.0, 1000.0, 8000.0 })
    {
        double minv = 300, maxv = -300;
        for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })   // (32 kHz: 8 kHz is half-way to Nyquist, reads ~2 dB low, same as the original)
        {
            const Readout r = measureSine (fs, 4, f);
            double m = -300; for (int i = 0; i < r.numBands; ++i) m = std::max (m, peakDb (r, i));
            minv = std::min (minv, m); maxv = std::max (maxv, m);
        }
        std::printf("  %5.0f Hz: reads %+.2f .. %+.2f dB across rates\n", f, minv, maxv);
        CHECK_MSG (maxv - minv < 1.5 && std::fabs (minv) < 1.6 && std::fabs (maxv) < 1.6, "%.0f Hz spread %.2f", f, maxv - minv);
    }
}

// White noise has +3 dB/octave in constant-Q bands, pink noise is flat.
static void testNoiseSlope()
{
    std::printf("[noise] white noise slope and pink noise flatness\n");
    const double fs = 48000;
    std::mt19937 rng (12345);
    std::normal_distribution<double> nd (0.0, 0.2);
    const size_t N = (size_t) (fs * 20);
    std::vector<float> w (N), p (N);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;  // Paul Kellet's refined pink filter
    for (size_t i = 0; i < N; ++i)
    {
        const double x = nd (rng); w[i] = float (x);
        b0 = 0.99886 * b0 + x * 0.0555179; b1 = 0.99332 * b1 + x * 0.0750759; b2 = 0.96900 * b2 + x * 0.1538520;
        b3 = 0.86650 * b3 + x * 0.3104856; b4 = 0.55000 * b4 + x * 0.5329522; b5 = -0.7616 * b5 - x * 0.0168980;
        p[i] = float ((b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362) * 0.3); b6 = x * 0.115926;
    }
    for (int bpo : { 3, 4, 6 })
    {
        auto bandAvg = [&] (const std::vector<float>& s)
        {
            FilterBank fb; fb.prepare (fs, bpo); feed (fb, s, s, ChannelMode::Left);
            Readout r; fb.read (r);
            std::vector<double> v; for (int i = 0; i < r.numBands; ++i) v.push_back (avgDb (r, i) - 3.0);  // RMS dB
            return std::pair<Readout, std::vector<double>> (r, v);
        };
        auto [rw, vw] = bandAvg (w);
        // least squares slope in dB/octave between 200 Hz and 12 kHz
        double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
        for (int i = 0; i < rw.numBands; ++i)
        {
            const double f = bandCentre (rw, i); if (f < 200 || f > 12000) continue;
            const double x = std::log2 (f); sx += x; sy += vw[(size_t) i]; sxx += x * x; sxy += x * vw[(size_t) i]; ++n;
        }
        const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        // Absolute level: white noise sigma = 0.2 -> power 0.04; a nominal 1/bpo-octave band holds
        // 0.04 * bandwidth / (fs/2). The bank's real noise bandwidth is a little wider than the
        // nominal band (resonant band-pass), so a constant offset of about +2 dB is expected; what must
        // hold is that the offset is the SAME in every band (no ripple from band to band).
        double eLo = 300, eHi = -300;
        for (int i = 0; i < rw.numBands; ++i)
        {
            const double f = bandCentre (rw, i); if (f < 100 || f > 6000) continue;
            const double bw = f * (std::pow (2.0, 0.5 / bpo) - std::pow (2.0, -0.5 / bpo));
            const double err = vw[(size_t) i] - 10.0 * std::log10 (0.04 * bw / (fs / 2.0));
            eLo = std::min (eLo, err); eHi = std::max (eHi, err);
        }
        std::printf("  bpo %d: white-noise slope %.2f dB/oct (theory 3.01); level offset vs nominal band %+.2f..%+.2f dB\n", bpo, slope, eLo, eHi);
        CHECK_MSG (std::fabs (slope - 3.01) < 0.25, "bpo %d slope %.2f", bpo, slope);
        CHECK_MSG (eHi - eLo < 0.8 && eLo > 1.0 && eHi < 3.0, "bpo %d noise level offset %.2f..%.2f", bpo, eLo, eHi);

        auto [rp, vp] = bandAvg (p);
        double lo = 300, hi = -300;
        for (int i = 0; i < rp.numBands; ++i) { const double f = bandCentre (rp, i); if (f < 100 || f > 10000) continue; lo = std::min (lo, vp[(size_t) i]); hi = std::max (hi, vp[(size_t) i]); }
        std::printf("         pink noise spread over 100 Hz..10 kHz: %.2f dB\n", hi - lo);
        CHECK_MSG (hi - lo < 2.5, "bpo %d pink spread %.2f dB", bpo, hi - lo);
    }
}

static void testSilenceAndStability()
{
    std::printf("[stability] silence, extreme input, denormals\n");
    for (double fs : { 44100.0, 192000.0 })
    {
        FilterBank fb; fb.prepare (fs, 6);
        std::vector<float> z ((size_t) fs, 0.0f);
        feed (fb, z, z, ChannelMode::Both);
        Readout r; fb.read (r);
        bool finite = true; double mx = -400;
        for (int i = 0; i < r.numBands; ++i) { const double d = peakDb (r, i); finite &= std::isfinite (d); mx = std::max (mx, d); }
        CHECK (finite);
        CHECK_MSG (mx < -150.0, "silence reads %.1f dB", mx);
        // full-scale square wave then silence: filters ring down, nothing explodes
        std::vector<float> sq ((size_t) fs / 2);
        for (size_t i = 0; i < sq.size(); ++i) sq[i] = ((i / 50) & 1) ? 1.0f : -1.0f;
        feed (fb, sq, sq, ChannelMode::Both); fb.read (r);
        for (int i = 0; i < r.numBands; ++i) CHECK_MSG (peakDb (r, i) < 12.0 && std::isfinite (peakDb (r, i)), "square wave band %d %.1f dB", i, peakDb (r, i));
        for (int k = 0; k < 6; ++k) { feed (fb, z, z, ChannelMode::Both); fb.read (r); }   // 22 Hz band rings for seconds
        mx = -400; for (int i = 0; i < r.numBands; ++i) mx = std::max (mx, peakDb (r, i));
        CHECK_MSG (mx < -120.0, "fs %.0f: after ringing down still %.1f dB", fs, mx);
    }
}

static void testReadoutHandoff()
{
    std::printf("[handoff] read() clears, peaks and counts accumulate between reads\n");
    FilterBank fb; fb.prepare (48000, 4);
    auto s = sine (48000, 1000, 0.5, 0.2);
    Readout r;
    CHECK (! fb.read (r) || true);
    fb.process (s.data(), s.data(), 4800, ChannelMode::Both);
    CHECK (fb.read (r));
    CHECK_MSG (r.samples == 9600.0, "samples %.0f", r.samples);       // 2 channels
    CHECK (! fb.read (r));                                            // nothing new
    fb.process (s.data(), s.data(), 1000, ChannelMode::Left);
    fb.process (s.data(), s.data(), 1000, ChannelMode::Left);
    CHECK (fb.read (r));
    CHECK_MSG (r.samples == 2000.0, "samples after clear %.0f", r.samples);
    // changing bands-per-octave swaps the layout cleanly
    fb.prepare (48000, 6);
    fb.process (s.data(), s.data(), 2048, ChannelMode::Both);
    CHECK (fb.read (r));
    CHECK (r.bandsPerOctave == 6.0 && r.numBands == fb.getNumBands());
}

static void testBallistics()
{
    std::printf("[ballistics] peak decay, hold, averager\n");
    Meter m; Meter::Options o; o.peakDecayDbPerSec = 15; o.holdTimeMs = 2000; o.holdDecayDbPerSec = 3;
    m.tick (-10.0, 32, o);                      // hit
    CHECK (std::fabs (m.peak + 10.0) < 1e-9 && std::fabs (m.hold + 10.0) < 1e-9);
    for (int i = 0; i < 31; ++i) m.tick (-200, 32, o);    // ~1 s of silence
    CHECK_MSG (std::fabs (m.peak - (-10.0 - 15.0 * 0.992)) < 0.2, "peak after ~1 s: %.2f", m.peak);
    CHECK_MSG (std::fabs (m.hold + 10.0) < 1e-9, "hold must still be held after 1 s: %.2f", m.hold);
    for (int i = 0; i < 62; ++i) m.tick (-200, 32, o);    // total ~3 s
    CHECK_MSG (m.hold < -10.0 && m.hold > -10.0 - 3.0 * 1.2, "hold decaying at 3 dB/s after the hold time: %.2f", m.hold);
    Meter inf; o.holdInfinite = true; inf.tick (-6, 32, o);
    for (int i = 0; i < 2000; ++i) inf.tick (-200, 32, o);
    CHECK_MSG (inf.hold == -6.0, "infinite hold moved: %.2f", inf.hold);

    // averager: constant input -> same output, step response reaches 100 % after 'size' ticks
    Averager a (100);
    double out = 0; for (int i = 0; i < 50; ++i) out = a.tick (4.0, 1e-20, 10);
    CHECK_MSG (std::fabs (out - 4.0) < 1e-9, "avg const %.6f", out);
    for (int i = 0; i < 5; ++i) out = a.tick (0.0, 1e-20, 10);
    CHECK_MSG (std::fabs (out - 2.0) < 1e-9, "avg half-step %.6f", out);
    for (int i = 0; i < 5; ++i) out = a.tick (0.0, 1e-20, 10);
    CHECK_MSG (out == 1e-20 || out < 1e-9, "avg after full window %.6g", out);
    out = 0; for (int i = 0; i < 30; ++i) out = a.tick (1.0, 1e-20, 10);
    out = a.tick (1.0, 1e-20, 20);        // grow window: the unknown past is filled with the current mean
    CHECK_MSG (std::fabs (out - 1.0) < 1e-9, "avg after resize %.6f", out);
}

// End-to-end through DisplayModel: sine of amplitude 0.5 => about -6 dB on peak AND average;
// slope control tilts only the average.
static void testDisplayModel()
{
    std::printf("[display] DisplayModel readings and slope\n");
    const double fs = 48000;
    FilterBank fb; fb.prepare (fs, 6);
    DisplayModel dm; DisplaySettings ds; ds.avgTimeMs = 500;
    auto tone = sine (fs, 1000, 0.5, 0.032);
    for (int i = 0; i < 80; ++i)
    {
        fb.process (tone.data(), tone.data(), (int) tone.size(), ChannelMode::Both);
        Readout r; if (fb.read (r)) dm.update (r, 32.0, ds);
    }
    int b = 0; for (int i = 0; i < dm.numBands; ++i) if (dm.peak[i] > dm.peak[b]) b = i;
    CHECK_MSG (std::fabs (dm.peak[b] - gainToDb (0.5)) < 1.5, "peak %.2f", dm.peak[b]);
    CHECK_MSG (std::fabs (dm.avg[b] - dm.peak[b]) < 0.8, "avg %.2f vs peak %.2f", dm.avg[b], dm.peak[b]);
    CHECK_MSG (std::fabs (dm.hold[b] - dm.peak[b]) < 0.2, "hold %.2f", dm.hold[b]);
    const double avgFlat = dm.avg[b];
    ds.avgSlope = 3.0;
    Readout r; fb.process (tone.data(), tone.data(), (int) tone.size(), ChannelMode::Both); fb.read (r);
    dm.update (r, 32.0, ds);
    const double expectedTilt = 3.0 * (double (b) - 0.5 * dm.numBands) / 6.0;
    // the average also moved a hair; compare against the untilted value
    CHECK_MSG (std::fabs ((dm.avg[b] - avgFlat) - expectedTilt) < 0.3, "tilt %.2f expected %.2f", dm.avg[b] - avgFlat, expectedTilt);
    dm.frozen = true; const double keep = dm.peak[b];
    ds.avgSlope = 0; auto loud = sine (fs, 1000, 1.0, 0.032);
    fb.process (loud.data(), loud.data(), (int) loud.size(), ChannelMode::Both); fb.read (r); dm.update (r, 32.0, ds);
    CHECK (dm.peak[b] == keep);
}

// Audio thread processes while another thread keeps reading and re-preparing: no crash, no NaN,
// the audio side never waits (we count how fast it runs), totals add up.
static void testThreads()
{
    std::printf("[threads] audio thread vs. UI reader (+ re-prepare)\n");
    FilterBank fb; fb.prepare (48000, 6);
    std::atomic<bool> stop { false };
    std::atomic<long long> reads { 0 }, bad { 0 };
    std::thread ui ([&]
    {
        Readout r;
        while (! stop.load())
        {
            fb.read (r);
            ++reads;
            for (int i = 0; i < r.numBands; ++i)
                if (! std::isfinite (r.peakSq[(size_t) i + 1]) || ! std::isfinite (r.sumSq[(size_t) i + 1]) || r.peakSq[(size_t) i + 1] < 0) ++bad;
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }
    });
    auto tone = sine (48000, 1000, 0.5, 0.05);
    const auto t0 = std::chrono::steady_clock::now();
    double audioSeconds = 0;
    for (int blk = 0; blk < 4000; ++blk)
    {
        if (blk % 500 == 250) fb.prepare (48000, (blk / 500) % 3 == 0 ? 3 : (blk / 500) % 3 == 1 ? 4 : 6);
        fb.process (tone.data(), tone.data(), 480, ChannelMode::Both);
        audioSeconds += 480.0 / 48000.0;
    }
    const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    stop = true; ui.join();
    std::printf("  %.0f s of audio in %.2f s wall (%.0fx real time, 6 bands/oct stereo), %lld reads, %lld bad values\n",
                audioSeconds, wall, audioSeconds / wall, (long long) reads.load(), (long long) bad.load());
    CHECK (bad.load() == 0);
    CHECK (reads.load() > 10);
   #if ! defined (__SANITIZE_ADDRESS__) && ! defined (__SANITIZE_THREAD__)
    CHECK_MSG (audioSeconds / wall > 20.0, "too slow: %.1fx real time", audioSeconds / wall);   // optimised builds only
   #endif
}


static void testNonFiniteAndMidSide()
{
    std::printf ("[robustness] NaN / inf never lock the filter bank; mid / side modes\n");
    const double fs = 48000.0;
    for (float bad : { std::nanf (""), INFINITY, -INFINITY })
    {
        FilterBank fb; fb.prepare (fs, 6);
        auto s = sine (fs, 1000.0, 0.1, 3.0);
        s[1000] = bad; s[50000] = bad;
        feed (fb, s, s, ChannelMode::Both);
        Readout dump; fb.read (dump);
        auto t = sine (fs, 1000.0, 0.1, 1.0);
        feed (fb, t, t, ChannelMode::Both);
        Readout r; fb.read (r);
        int best = 0; for (int i = 0; i < r.numBands; ++i) if (peakDb (r, i) > peakDb (r, best)) best = i;
        CHECK_MSG (std::isfinite (peakDb (r, best)) && std::fabs (peakDb (r, best) + 20.0) < 1.0, "bad %g: peak %.2f", (double) bad, peakDb (r, best));
        CHECK_MSG (std::fabs (bandCentre (r, best) - 1000.0) < 60.0, "band %.0f", bandCentre (r, best));
    }
    // mid/side: identical L/R -> side silent; inverted -> mid silent
    auto a = sine (fs, 1000.0, 0.1, 1.5), inv = a; for (auto& v : inv) v = -v;
    auto lvl = [&] (ChannelMode m, const std::vector<float>& l, const std::vector<float>& rr)
    {
        FilterBank fb; fb.prepare (fs, 6); feed (fb, l, rr, m); Readout r; fb.read (r);
        double mx = -400; for (int i = 0; i < r.numBands; ++i) mx = std::max (mx, peakDb (r, i)); return mx;
    };
    const double midSame = lvl (ChannelMode::Mid, a, a), sideSame = lvl (ChannelMode::Side, a, a);
    const double midInv = lvl (ChannelMode::Mid, a, inv), sideInv = lvl (ChannelMode::Side, a, inv);
    CHECK_MSG (std::fabs (midSame + 20.0) < 1.0 && sideSame < -100.0, "same: mid %.1f side %.1f", midSame, sideSame);
    CHECK_MSG (std::fabs (sideInv + 20.0) < 1.0 && midInv < -100.0, "inv: mid %.1f side %.1f", midInv, sideInv);
}

int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) if (std::string (argv[i]) == "-v") g_verbose = true;
    testBandLayout();
    testSineAtBandCentres();
    testLinearityAndChannels();
    testSweep();
    testSampleRateIndependence();
    testNoiseSlope();
    testSilenceAndStability();
    testReadoutHandoff();
    testBallistics();
    testDisplayModel();
    testThreads();
    testNonFiniteAndMidSide();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
