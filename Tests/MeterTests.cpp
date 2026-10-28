// Malon Frequency Analyzer - pro-metering tests (true peak, RMS, LUFS, LRA). No JUCE needed.
// References: ITU-R BS.1770-4 (K-weighting, gating), EBU Tech 3341 (test signals 1-5, 9-23),
// EBU Tech 3342 (LRA test signals 1-4 and the MATLAB reference algorithm).
// The EBU signals are synthesised here from the descriptions in the specs (the EBU download is not
// bundled); signals 6 (5.0 channels), 7/8 and Tech 3342 #5/#6 (authentic programme) cannot be generated.
#include "../Source/dsp/LevelMeter.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace malon;
static const double kPi = 3.14159265358979323846;
static int g_checks = 0, g_fail = 0;
#define CHECK_MSG(cond, ...) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf ("  FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)
#define CHECK(cond) CHECK_MSG (cond, "%s", "")
static bool near (double a, double b, double tol) { return std::fabs (a - b) <= tol; }
static double db (double lin) { return 20.0 * std::log10 (lin); }
static double lin (double dB) { return std::pow (10.0, dB / 20.0); }

using Sig = std::vector<std::vector<double>>;   // [channel][sample]

static Sig makeSig (int ch, size_t n) { return Sig ((size_t) ch, std::vector<double> (n, 0.0)); }

static void append (Sig& s, const Sig& t) { for (size_t c = 0; c < s.size(); ++c) s[c].insert (s[c].end(), t[c].begin(), t[c].end()); }

// stereo (or n-channel) sine, 'seconds' long, amplitude in dBFS peak, same signal in all channels
static Sig tone (double fs, double f, double dBFS, double seconds, int ch = 2, double phase0 = 0.0)
{
    const size_t n = (size_t) std::llround (seconds * fs);
    Sig s = makeSig (ch, n);
    const double a = lin (dBFS);
    for (size_t i = 0; i < n; ++i) { const double v = a * std::sin (2.0 * kPi * f * (double) i / fs + phase0); for (int c = 0; c < ch; ++c) s[(size_t) c][i] = v; }
    return s;
}
static Sig silence (double fs, double seconds, int ch = 2) { return makeSig (ch, (size_t) std::llround (seconds * fs)); }

// feed in chunks, calling 'afterChunk' after each one (for time-series checks)
static void run (LevelMeter& m, const Sig& s, int chunk = 480, const std::function<void (size_t)>& afterChunk = {})
{
    const int ch = (int) s.size();
    std::vector<float> tmp[2];
    const size_t n = s[0].size();
    for (size_t pos = 0; pos < n; pos += (size_t) chunk)
    {
        const int len = (int) std::min<size_t> ((size_t) chunk, n - pos);
        const double* p[2] = { s[0].data() + pos, s[(size_t) std::min (1, ch - 1)].data() + pos };
        m.process (p, ch, len);                                    // double path
        if (afterChunk) afterChunk (pos + (size_t) len);
    }
}
static LevelMeter::Readout measure (const Sig& s, double fs = 48000.0, int chunk = 480)
{
    LevelMeter m; m.prepare (fs, (int) s.size()); run (m, s, chunk); return m.read();
}

// ---------------------------------------------------------------------------------------------

static void testKWeightingCoefficients()
{
    std::printf ("[K-weighting] BS.1770-4 coefficients at 48 kHz\n");
    const auto k = LevelMeter::designKWeighting (48000.0);
    // ITU-R BS.1770-4, Table 1 / Table 2
    const double pb[3] = { 1.53512485958697, -2.69169618940638, 1.19839281085285 };
    const double pa[2] = { -1.69065929318241, 0.73248077421585 };
    const double rb[3] = { 1.0, -2.0, 1.0 };
    const double ra[2] = { -1.99004745483398, 0.99007225036621 };
    CHECK_MSG (near (k[0].b0, pb[0], 1e-7) && near (k[0].b1, pb[1], 1e-7) && near (k[0].b2, pb[2], 1e-7), "pre-filter b: %.9f %.9f %.9f", k[0].b0, k[0].b1, k[0].b2);
    CHECK_MSG (near (k[0].a1, pa[0], 1e-7) && near (k[0].a2, pa[1], 1e-7), "pre-filter a: %.9f %.9f", k[0].a1, k[0].a2);
    CHECK (k[1].b0 == rb[0] && k[1].b1 == rb[1] && k[1].b2 == rb[2]);
    CHECK_MSG (near (k[1].a1, ra[0], 1e-6) && near (k[1].a2, ra[1], 1e-6), "RLB a: %.9f %.9f", k[1].a1, k[1].a2);
    std::printf ("  pre-filter b0 %.9f (spec %.9f), RLB a1 %.9f (spec %.9f)\n", k[0].b0, pb[0], k[1].a1, ra[0]);
}

static void testSineCalibration()
{
    std::printf ("[calibration] 997 Hz / 1 kHz sines: RMS and LUFS\n");
    // 997 Hz, -20 dBFS peak, stereo in phase: RMS = -23.01 dBFS per channel and combined; loudness -20.0 LUFS
    auto r = measure (tone (48000, 997, -20.0, 8.0));
    std::printf ("  997 Hz -20 dBFS stereo: RMS L %.2f R %.2f all %.2f dBFS | M %.2f S %.2f I %.2f LUFS | TP %.2f dBTP\n",
                r.rmsDb[0], r.rmsDb[1], r.rmsCombinedDb, r.momentary, r.shortTerm, r.integrated, r.truePeakMaxDb);
    CHECK (near (r.rmsDb[0], -23.01, 0.02) && near (r.rmsDb[1], -23.01, 0.02) && near (r.rmsCombinedDb, -23.01, 0.02));
    CHECK (near (r.momentary, -20.0, 0.1) && near (r.shortTerm, -20.0, 0.1) && near (r.integrated, -20.0, 0.1));
    CHECK (near (r.truePeakMaxDb, -20.0, 0.1));
    CHECK (! r.clipped);
    // EBU Tech 3341 section 2.9: 1 kHz stereo, -18 dBFS peak must read -18.0 LUFS
    r = measure (tone (48000, 1000, -18.0, 10.0));
    CHECK_MSG (near (r.integrated, -18.0, 0.1), "-18 dBFS 1 kHz reads %.3f LUFS", r.integrated);
    // 0 dBFS mono sine: -3.01 LUFS (BS.1770: a full-scale mono 997 Hz sine reads -3.01 LUFS)
    r = measure (tone (48000, 997, 0.0, 10.0, 1));
    std::printf ("  997 Hz 0 dBFS mono: I %.3f LUFS (BS.1770: -3.01), RMS %.2f dBFS (-3.01)\n", r.integrated, r.rmsDb[0]);
    CHECK (near (r.integrated, -3.01, 0.05) && near (r.rmsDb[0], -3.01, 0.02));
    // sample-rate independence of the K-weighting design
    for (double fs : { 32000.0, 44100.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        r = measure (tone (fs, 1000, -23.0, 10.0), fs);
        std::printf ("  fs %6.0f: I %.3f LUFS\n", fs, r.integrated);
        CHECK_MSG (near (r.integrated, -23.0, 0.1) && near (r.momentary, -23.0, 0.1), "fs %.0f reads %.3f", fs, r.integrated);
    }
}

static void testChannels()
{
    std::printf ("[channels] mono vs stereo handling\n");
    auto st = measure (tone (48000, 1000, -23.0, 6.0, 2));
    auto mo = measure (tone (48000, 1000, -23.0, 6.0, 1));
    std::printf ("  stereo %.2f LUFS | mono %.2f LUFS (one channel, weight 1.0: 3 LU lower than dual-mono)\n", st.integrated, mo.integrated);
    CHECK (near (st.integrated, -23.0, 0.1));
    CHECK (near (mo.integrated, -26.0, 0.1));
    CHECK (near (mo.rmsDb[0], -26.01, 0.02) && near (mo.rmsCombinedDb, mo.rmsDb[0], 0.001));
    CHECK (mo.channels == 1 && st.channels == 2);

    // tone in the left channel only
    Sig s = makeSig (2, 48000 * 6);
    auto t = tone (48000, 1000, -23.0, 6.0, 1); s[0] = t[0];
    auto lo = measure (s);
    std::printf ("  left only: RMS L %.2f R %.2f all %.2f | I %.2f | TP L %.2f R %.2f\n", lo.rmsDb[0], lo.rmsDb[1], lo.rmsCombinedDb, lo.integrated, lo.truePeakDb[0], lo.truePeakDb[1]);
    CHECK (near (lo.rmsDb[0], -26.01, 0.02));
    CHECK (std::isinf (lo.rmsDb[1]) && lo.rmsDb[1] < 0);
    CHECK (near (lo.rmsCombinedDb, -29.02, 0.02));     // (power of L + 0) / 2
    CHECK (near (lo.integrated, -26.0, 0.1));
    CHECK (near (lo.truePeakDb[0], -23.0, 0.1) && std::isinf (lo.truePeakDb[1]));
    // channels at different levels: per-channel RMS / TP are independent
    Sig d = makeSig (2, 48000 * 6); d[0] = tone (48000, 1000, -20.0, 6.0, 1)[0]; d[1] = tone (48000, 1000, -30.0, 6.0, 1)[0];
    auto dd = measure (d);
    CHECK (near (dd.rmsDb[0], -23.01, 0.02) && near (dd.rmsDb[1], -33.01, 0.02));
    CHECK (near (dd.truePeakDb[0], -20.0, 0.1) && near (dd.truePeakDb[1], -30.0, 0.1));
}

static void testSilenceAndReset()
{
    std::printf ("[silence/reset/clip]\n");
    auto r = measure (silence (48000, 6.0));
    CHECK (std::isinf (r.truePeakMaxDb) && r.truePeakMaxDb < 0);
    CHECK (std::isinf (r.rmsCombinedDb) && r.rmsCombinedDb < 0);
    CHECK (std::isinf (r.momentary) && std::isinf (r.shortTerm) && std::isinf (r.integrated) && std::isinf (r.loudnessRange));
    CHECK (! r.clipped);
    // very quiet signal below the absolute gate (-70 LUFS): integrated stays undefined
    r = measure (tone (48000, 1000, -80.0, 8.0));
    CHECK_MSG (std::isinf (r.integrated), "-80 dBFS gated: %.2f", r.integrated);
    CHECK (r.momentary < -70.0);

    // clip latch + held values + reset
    LevelMeter m; m.prepare (48000, 2);
    run (m, tone (48000, 1000, -6.0, 2.0));
    auto a = m.read();
    CHECK (! a.clipped && near (a.truePeakMaxDb, -6.0, 0.15) && std::isfinite (a.integrated));
    auto hot = tone (48000, 1000, -6.0, 0.5); hot[0][1000] = 1.0;                 // one full-scale sample
    run (m, hot);
    a = m.read();
    CHECK (a.clipped && a.truePeakMaxDb >= -0.001 && near (a.samplePeakMaxDb, 0.0, 1e-6));
    run (m, tone (48000, 1000, -30.0, 1.0));
    a = m.read();
    CHECK_MSG (a.clipped && a.truePeakMaxDb >= -0.001, "held values must persist (%.2f)", a.truePeakMaxDb);
    m.requestReset();
    run (m, tone (48000, 1000, -30.0, 1.0));
    a = m.read();
    CHECK (! a.clipped);
    CHECK_MSG (near (a.truePeakMaxDb, -30.0, 0.15), "after reset TP max %.2f", a.truePeakMaxDb);
    CHECK_MSG (near (a.integrated, -30.0, 0.15) || std::isinf (a.integrated), "after reset I %.2f", a.integrated);
    CHECK (a.momentaryMax < -29.0);
    double rp[2]; m.consumeRecentPeaks (rp); CHECK (rp[0] > 0.0);
    m.consumeRecentPeaks (rp); CHECK (rp[0] == 0.0 && rp[1] == 0.0);
}

static void testRmsWindows()
{
    std::printf ("[RMS] selectable window\n");
    for (int ms : { 100, 300, 1000, 3000 })
    {
        LevelMeter m; m.prepare (48000, 2); m.setRmsWindowMs (ms);
        run (m, tone (48000, 997, -10.0, 4.0));
        auto r = m.read();
        CHECK_MSG (near (r.rmsCombinedDb, -13.01, 0.03), "window %d ms: %.3f", ms, r.rmsCombinedDb);
        // loud (-10) then quiet (-40, 150 ms): reading blends according to window
        run (m, tone (48000, 997, -40.0, 0.15));
        r = m.read();
        const double w = ms / 1000.0, q = std::min (0.15, w);
        const double expect = 10.0 * std::log10 (((w - q) * 0.5 * lin (-20.0) + q * 0.5 * lin (-80.0) * 0 + 0) / w + (q * 0.5 * std::pow (lin (-40.0), 2)) / w);
        const double loudPart = (w - q) * 0.5 * std::pow (lin (-10.0), 2);
        const double ref = 10.0 * std::log10 ((loudPart + q * 0.5 * std::pow (lin (-40.0), 2)) / w);
        (void) expect;
        std::printf ("  window %4d ms: steady %.2f dBFS, 150 ms after a 30 dB drop %.2f dBFS (expected %.2f)\n", ms, -13.01, r.rmsCombinedDb, ref);
        CHECK_MSG (near (r.rmsCombinedDb, ref, 0.4), "window %d ms: %.2f vs %.2f", ms, r.rmsCombinedDb, ref);
    }
}

// ---- EBU Tech 3341 -------------------------------------------------------------------------

struct Series { std::vector<double> m, s, i; std::vector<double> t; };

static Series trace (const Sig& s, double fs = 48000.0, double* maxM = nullptr, double* maxS = nullptr)
{
    LevelMeter m; m.prepare (fs, (int) s.size());
    Series out;
    const int chunk = (int) std::lround (fs * 0.01);        // 10 ms
    run (m, s, chunk, [&] (size_t pos)
    {
        auto r = m.read();
        out.m.push_back (r.momentary); out.s.push_back (r.shortTerm); out.i.push_back (r.integrated); out.t.push_back ((double) pos / fs);
    });
    auto r = m.read();
    if (maxM) *maxM = r.momentaryMax;
    if (maxS) *maxS = r.shortTermMax;
    return out;
}

static void testEbu3341()
{
    std::printf ("[EBU Tech 3341] loudness test signals (synthesised, 48 kHz stereo)\n");
    const double fs = 48000;
    auto line = [] (const char* name, double got, double want, double tol)
    {
        const bool ok = near (got, want, tol);
        std::printf ("  %-44s %8.3f   expected %7.1f +/-%.1f   %s\n", name, got, want, tol, ok ? "ok" : "FAIL");
        ++g_checks; if (! ok) ++g_fail;
    };

    // 1 and 2: M, S, I
    for (double lvl : { -23.0, -33.0 })
    {
        auto tr = trace (tone (fs, 1000, lvl, 20.0));
        char nm[64]; std::snprintf (nm, sizeof nm, "#%d  M at end (%.0f dBFS)", lvl == -23 ? 1 : 2, lvl); line (nm, tr.m.back(), lvl, 0.1);
        std::snprintf (nm, sizeof nm, "#%d  S at end", lvl == -23 ? 1 : 2); line (nm, tr.s.back(), lvl, 0.1);
        std::snprintf (nm, sizeof nm, "#%d  I at end", lvl == -23 ? 1 : 2); line (nm, tr.i.back(), lvl, 0.1);
    }
    // 3
    { Sig s = tone (fs, 1000, -36.0, 10.0); append (s, tone (fs, 1000, -23.0, 60.0)); append (s, tone (fs, 1000, -36.0, 10.0));
      line ("#3  I (-36/-23/-36 dBFS, 10/60/10 s)", measure (s).integrated, -23.0, 0.1); }
    // 4
    { Sig s = tone (fs, 1000, -72.0, 10.0); append (s, tone (fs, 1000, -36.0, 10.0)); append (s, tone (fs, 1000, -23.0, 60.0));
      append (s, tone (fs, 1000, -36.0, 10.0)); append (s, tone (fs, 1000, -72.0, 10.0));
      line ("#4  I (five tones, abs + rel gate)", measure (s).integrated, -23.0, 0.1); }
    // 5
    { Sig s = tone (fs, 1000, -26.0, 20.0); append (s, tone (fs, 1000, -20.0, 20.1)); append (s, tone (fs, 1000, -26.0, 20.0));
      line ("#5  I (-26/-20/-26 dBFS, 20/20.1/20 s)", measure (s).integrated, -23.0, 0.1); }
    // 9: (1.34 s at -20, 1.66 s at -30) x5 -> S = -23.0 constant after 3 s
    { Sig s; for (int k = 0; k < 5; ++k) { if (k == 0) s = tone (fs, 1000, -20.0, 1.34); else append (s, tone (fs, 1000, -20.0, 1.34)); append (s, tone (fs, 1000, -30.0, 1.66)); }
      auto tr = trace (s); double lo = 99, hi = -99;
      for (size_t k = 0; k < tr.s.size(); ++k) if (tr.t[k] >= 3.0) { lo = std::min (lo, tr.s[k]); hi = std::max (hi, tr.s[k]); }
      std::printf ("  #9  S after 3 s ranges %.3f .. %.3f (expected -23.0 +/-0.1)\n", lo, hi);
      CHECK_MSG (lo > -23.1 - 1e-9 && hi < -22.9 + 1e-9, "S range %.3f..%.3f", lo, hi); }
    // 12: (0.18 s at -20, 0.22 s at -30) x25 -> M = -23.0 constant after 1 s
    { Sig s; for (int k = 0; k < 25; ++k) { if (k == 0) s = tone (fs, 1000, -20.0, 0.18); else append (s, tone (fs, 1000, -20.0, 0.18)); append (s, tone (fs, 1000, -30.0, 0.22)); }
      auto tr = trace (s); double lo = 99, hi = -99;
      for (size_t k = 0; k < tr.m.size(); ++k) if (tr.t[k] >= 1.0) { lo = std::min (lo, tr.m[k]); hi = std::max (hi, tr.m[k]); }
      std::printf ("  #12 M after 1 s ranges %.3f .. %.3f (expected -23.0 +/-0.1)\n", lo, hi);
      CHECK_MSG (lo > -23.1 - 1e-9 && hi < -22.9 + 1e-9, "M range %.3f..%.3f", lo, hi); }
    // 10 (file-based, 20 separate files): max S = -23.0 for each i
    { double worst = 0; for (int i = 0; i < 20; ++i) { Sig s = silence (fs, i * 0.15); append (s, tone (fs, 1000, -23.0, 3.0)); append (s, silence (fs, 1.0));
        double mm, ms; trace (s, fs, &mm, &ms); worst = std::max (worst, std::fabs (ms + 23.0)); }
      std::printf ("  #10 max S over 20 segments: worst deviation %.3f LU (tol 0.1)\n", worst); CHECK_MSG (worst <= 0.1, "worst %.3f", worst); }
    // 13 (file-based): max M for each i
    { double worst = 0; for (int i = 0; i < 20; ++i) { Sig s = silence (fs, i * 0.02); append (s, tone (fs, 1000, -23.0, 0.4)); append (s, silence (fs, 1.0));
        double mm, ms; trace (s, fs, &mm, &ms); worst = std::max (worst, std::fabs (mm + 23.0)); }
      std::printf ("  #13 max M over 20 segments: worst deviation %.3f LU (tol 0.1)\n", worst); CHECK_MSG (worst <= 0.1, "worst %.3f", worst); }
    // 11 (live): successive max S = -38 ... -19
    { Sig s; bool first = true; std::vector<double> segEnd;
      for (int i = 0; i < 20; ++i) { Sig seg = silence (fs, i * 0.15); append (seg, tone (fs, 1000, -38.0 + i, 3.0)); append (seg, silence (fs, 3.0 - i * 0.15));
        if (first) { s = seg; first = false; } else append (s, seg); segEnd.push_back ((double) s[0].size() / fs); }
      auto tr = trace (s); double worst = 0; double t0 = 0;
      for (int i = 0; i < 20; ++i) { double mx = -99; for (size_t k = 0; k < tr.s.size(); ++k) if (tr.t[k] > t0 && tr.t[k] <= segEnd[(size_t) i]) mx = std::max (mx, tr.s[k]);
        worst = std::max (worst, std::fabs (mx - (-38.0 + i))); t0 = segEnd[(size_t) i]; }
      std::printf ("  #11 successive max S: worst deviation %.3f LU (tol 0.1)\n", worst); CHECK_MSG (worst <= 0.1, "worst %.3f", worst); }
    // 14 (live): successive max M = -38 ... -19
    { Sig s; bool first = true; std::vector<double> segEnd;
      for (int i = 0; i < 20; ++i) { Sig seg = silence (fs, i * 0.02); append (seg, tone (fs, 1000, -38.0 + i, 0.4)); append (seg, silence (fs, 0.4 - i * 0.02));
        if (first) { s = seg; first = false; } else append (s, seg); segEnd.push_back ((double) s[0].size() / fs); }
      auto tr = trace (s); double worst = 0; double t0 = 0;
      for (int i = 0; i < 20; ++i) { double mx = -99; for (size_t k = 0; k < tr.m.size(); ++k) if (tr.t[k] > t0 && tr.t[k] <= segEnd[(size_t) i] + 1e-9) mx = std::max (mx, tr.m[k]);
        worst = std::max (worst, std::fabs (mx - (-38.0 + i))); t0 = segEnd[(size_t) i]; }
      std::printf ("  #14 successive max M: worst deviation %.3f LU (tol 0.1)\n", worst); CHECK_MSG (worst <= 0.1, "worst %.3f", worst); }
}

// ---- EBU Tech 3342 (LRA) ------------------------------------------------------------------

static void testLra()
{
    std::printf ("[EBU Tech 3342] loudness range test signals\n");
    const double fs = 48000;
    auto seq = [&] (std::initializer_list<double> levels)
    { Sig s; bool first = true; for (double l : levels) { if (first) { s = tone (fs, 1000, l, 20.0); first = false; } else append (s, tone (fs, 1000, l, 20.0)); } return s; };
    struct Case { const char* name; Sig sig; double want; };
    std::vector<Case> cases;
    cases.push_back ({ "#1 -20 / -30 dBFS (10 LU)", seq ({ -20, -30 }), 10.0 });
    cases.push_back ({ "#2 -20 / -15 dBFS (5 LU)", seq ({ -20, -15 }), 5.0 });
    cases.push_back ({ "#3 -40 / -20 dBFS (20 LU)", seq ({ -40, -20 }), 20.0 });
    cases.push_back ({ "#4 -50/-35/-20/-35/-50 (15 LU)", seq ({ -50, -35, -20, -35, -50 }), 15.0 });
    for (auto& c : cases)
    {
        LevelMeter m; m.prepare (fs, 2); run (m, c.sig);
        append (const_cast<Sig&> (c.sig), silence (fs, 1.5)); run (m, silence (fs, 1.5));   // flush the 3 s window as the spec suggests
        const double got = m.read().loudnessRange;
        const bool ok = near (got, c.want, 1.0);
        std::printf ("  %-36s LRA %7.3f LU   expected %4.1f +/-1.0   %s\n", c.name, got, c.want, ok ? "ok" : "FAIL");
        ++g_checks; if (! ok) ++g_fail;
    }
    // repeating the whole signal must not change the result ("unchanged if repeated")
    { LevelMeter m; m.prepare (fs, 2); Sig s = seq ({ -20, -30 }); run (m, s); run (m, s); const double got = m.read().loudnessRange; CHECK_MSG (near (got, 10.0, 1.0), "repeated: %.3f", got); }
}

// ---- Independent batch reference (BS.1770 + Tech 3342 MATLAB algorithm) vs the streaming meter ---

static void testAgainstBatchReference()
{
    std::printf ("[reference] streaming meter vs. independent batch implementation, 90 s of synthetic programme\n");
    const double fs = 48000; const size_t N = (size_t) (90 * fs);
    std::mt19937 rng (4242); std::normal_distribution<double> nd (0, 1);
    Sig s = makeSig (2, N);
    double lp = 0, lp2 = 0;
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / fs;
        // slowly varying level (up to ~25 dB swings), a silent gap, a loud section, tonal + noisy content
        double envDb = -30.0 + 12.0 * std::sin (2 * kPi * t / 23.0) + 8.0 * std::sin (2 * kPi * t / 7.0);
        if (t > 40 && t < 47) envDb = -90.0;                           // gap (below the gate)
        if (t > 60 && t < 70) envDb += 10.0;
        const double w = nd (rng); lp = 0.9 * lp + 0.1 * w; lp2 = 0.999 * lp2 + 0.001 * w;
        const double x = lin (envDb) * (0.6 * lp * 3.0 + 0.5 * std::sin (2 * kPi * 220 * t) + 0.3 * lp2 * 20.0);
        s[0][i] = std::max (-1.0, std::min (1.0, x));
        s[1][i] = std::max (-1.0, std::min (1.0, 0.8 * x + 0.2 * lin (envDb) * nd (rng)));
    }
    // batch: spec 48 kHz coefficients, direct form I
    const double pb[3] = { 1.53512485958697, -2.69169618940638, 1.19839281085285 }, pa[2] = { -1.69065929318241, 0.73248077421585 };
    const double ra[2] = { -1.99004745483398, 0.99007225036621 };
    std::vector<double> k0 (N), k1 (N);
    auto filt = [&] (const std::vector<double>& x, std::vector<double>& y)
    {
        std::vector<double> a (N);
        for (size_t n = 0; n < N; ++n) a[n] = pb[0] * x[n] + (n > 0 ? pb[1] * x[n - 1] - pa[0] * a[n - 1] : 0) + (n > 1 ? pb[2] * x[n - 2] - pa[1] * a[n - 2] : 0);
        for (size_t n = 0; n < N; ++n) y[n] = a[n] - (n > 0 ? 2 * a[n - 1] : 0) + (n > 1 ? a[n - 2] : 0) - (n > 0 ? ra[0] * y[n - 1] : 0) - (n > 1 ? ra[1] * y[n - 2] : 0);
    };
    filt (s[0], k0); filt (s[1], k1);
    const size_t hop = 4800, blk = 19200;        // 100 ms hop, 400 ms block, 3 s window = 144000
    std::vector<double> z;                          // block mean-square (L + R)
    for (size_t st = 0; st + blk <= N; st += hop) { double a = 0; for (size_t n = st; n < st + blk; ++n) a += k0[n] * k0[n] + k1[n] * k1[n]; z.push_back (a / blk); }
    auto L = [] (double v) { return -0.691 + 10 * std::log10 (v); };
    std::vector<double> g; for (double v : z) if (L (v) > -70) g.push_back (v);
    double mean = 0; for (double v : g) mean += v; mean /= (double) g.size();
    const double gr = L (mean) - 10; double sum = 0; size_t cnt = 0; for (double v : g) if (L (v) > gr) { sum += v; ++cnt; }
    const double refI = L (sum / (double) cnt);
    // LRA (MATLAB algorithm), short-term values every 100 ms from full 3 s windows
    std::vector<double> stl;
    for (size_t st = 0; st + 144000 <= N; st += hop) { double a = 0; for (size_t n = st; n < st + 144000; ++n) a += k0[n] * k0[n] + k1[n] * k1[n]; stl.push_back (L (a / 144000)); }
    std::vector<double> ag; for (double v : stl) if (v >= -70) ag.push_back (v);
    double pw = 0; for (double v : ag) pw += std::pow (10.0, v / 10.0); pw /= (double) ag.size();
    const double integ = 10 * std::log10 (pw); std::vector<double> rg; for (double v : ag) if (v >= integ - 20) rg.push_back (v);
    std::sort (rg.begin(), rg.end());
    const size_t n = rg.size();
    const double refLra = rg[(size_t) std::llround ((double) (n - 1) * 0.95)] - rg[(size_t) std::llround ((double) (n - 1) * 0.10)];

    LevelMeter m; m.prepare (fs, 2); run (m, s, 512);
    auto r = m.read();
    std::printf ("  integrated: meter %.3f vs reference %.3f LUFS | LRA: meter %.3f vs reference %.3f LU\n", r.integrated, refI, r.loudnessRange, refLra);
    CHECK_MSG (near (r.integrated, refI, 0.05), "I %.3f vs %.3f", r.integrated, refI);
    CHECK_MSG (near (r.loudnessRange, refLra, 0.25), "LRA %.3f vs %.3f", r.loudnessRange, refLra);

    // chunk size must not matter
    for (int chunk : { 1, 37, 480, 4096 })
    {
        LevelMeter m2; m2.prepare (fs, 2);
        Sig part = makeSig (2, 0); for (int c = 0; c < 2; ++c) part[(size_t) c].assign (s[(size_t) c].begin(), s[(size_t) c].begin() + (size_t) (30 * fs));
        run (m2, part, chunk);
        LevelMeter m3; m3.prepare (fs, 2); run (m3, part, 960);
        CHECK_MSG (near (m2.read().integrated, m3.read().integrated, 1e-6) && near (m2.read().truePeakMaxDb, m3.read().truePeakMaxDb, 1e-6), "chunk %d differs", chunk);
    }
}

// ---- True peak -------------------------------------------------------------------------------

static void fade (std::vector<double>& x, double fs, double seconds)
{
    const size_t n = (size_t) (seconds * fs);
    for (size_t i = 0; i < n && i < x.size(); ++i) { const double g = 0.5 - 0.5 * std::cos (kPi * (double) i / (double) n); x[i] *= g; x[x.size() - 1 - i] *= g; }
}

static double refTruePeakDb (const std::vector<double>& x, int os = 16)
{
    // slow, high-precision reference: Kaiser-windowed sinc, 128 taps/side, evaluated at os fractional positions
    const int half = 64; double mx = 0;
    const double beta = 10.0;
    auto i0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 50; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
    const double d = i0 (beta);
    for (size_t n = (size_t) half; n + (size_t) half < x.size(); ++n)
        for (int p = 0; p < os; ++p)
        {
            const double f = (double) p / os; double acc = 0;
            for (int k = -half + 1; k <= half; ++k)
            {
                const double u = (double) k - f; const double sinc = std::fabs (u) < 1e-12 ? 1.0 : std::sin (kPi * u) / (kPi * u);
                const double r = u / half; const double w = std::fabs (r) >= 1.0 ? 0.0 : i0 (beta * std::sqrt (1 - r * r)) / d;
                acc += x[n + (size_t) k] * sinc * w;
            }
            mx = std::max (mx, std::fabs (acc));
        }
    return db (std::max (mx, 1e-12));
}

static double tpOf (const std::vector<double>& x, double fs, bool* clipped = nullptr, double* spDb = nullptr)
{
    Sig s (2, x); LevelMeter m; m.prepare (fs, 2); run (m, s, 480);
    auto r = m.read(); if (clipped) *clipped = r.clipped; if (spDb) *spDb = r.samplePeakMaxDb; return r.truePeakMaxDb;
}

static void testTruePeak()
{
    std::printf ("[true peak] EBU Tech 3341 signals 15-23 (tolerance +0.2 / -0.4 dBTP), BS.1770-4 4x oversampling\n");
    auto check = [] (const char* name, double got, double want, double ref = 999)
    {
        const bool ok = got <= want + 0.2 + 1e-9 && got >= want - 0.4 - 1e-9;
        if (ref < 900) std::printf ("  %-52s %7.3f dBTP  expected %5.1f (+0.2/-0.4)  [16x reference %.3f]  %s\n", name, got, want, ref, ok ? "ok" : "FAIL");
        else           std::printf ("  %-52s %7.3f dBTP  expected %5.1f (+0.2/-0.4)  %s\n", name, got, want, ok ? "ok" : "FAIL");
        ++g_checks; if (! ok) ++g_fail;
    };
    for (double fs : { 48000.0, 44100.0, 96000.0 })
    {
        std::printf ("  -- fs = %.0f Hz\n", fs);
        const size_t n = (size_t) (0.1 * fs);
        struct S { const char* nm; double div; double amp; double ph; double want; };
        const S t[] = { { "#15 fs/4, 0.50 FFS, phase 0", 4, 0.5, 0.0, -6.0 }, { "#16 fs/4, 0.50 FFS, phase 45", 4, 0.5, 45.0, -6.0 },
                        { "#17 fs/6, 0.50 FFS, phase 60", 6, 0.5, 60.0, -6.0 }, { "#18 fs/8, 0.50 FFS, phase 67.5", 8, 0.5, 67.5, -6.0 },
                        { "#19 fs/4, 1.41 FFS, phase 45 (above full scale)", 4, 1.41, 45.0, 3.0 } };
        for (auto& c : t)
        {
            std::vector<double> x (n);
            for (size_t i = 0; i < n; ++i) x[i] = c.amp * std::sin (2 * kPi * (fs / c.div) * (double) i / fs + c.ph * kPi / 180.0);
            fade (x, fs, 0.010);
            check (c.nm, tpOf (x, fs), c.want, fs == 48000.0 ? refTruePeakDb (x) : 999);
        }
    }
    // 20-23: fs/6 sine with a single fs/4 period (amplitude 1.0), made at 4 fs, low-passed, decimated by 4 with offset 0..3
    {
        const double fs = 48000, fs4 = 4 * fs;
        const size_t n4 = (size_t) (0.4 * fs4);
        std::vector<double> y (n4);
        const size_t burstAt = (size_t) (n4 / 2) / 16 * 16;                      // start on a zero crossing of fs/6 (multiple of its 4fs period: 24 samples)
        const size_t b0 = burstAt - burstAt % 24, perBurst = 16;                 // one fs/4 period = 16 samples at 4fs
        for (size_t i = 0; i < n4; ++i)
        {
            if (i >= b0 && i < b0 + perBurst) y[i] = 1.0 * std::sin (2 * kPi * (double) (i - b0) / (double) perBurst);
            else { const size_t j = i < b0 ? i : i - perBurst; y[i] = 0.5 * std::sin (2 * kPi * (double) (j % 24) / 24.0 + 0.0 * j); }
        }
        // low-pass at fs/2 (of the target rate) = 0.125 of the 4 fs rate: Kaiser sinc, 1023 taps
        const int H = 511; std::vector<double> h ((size_t) (2 * H + 1)); const double fc = 0.5 * fs / fs4 * 0.98, beta = 9.0;
        auto i0 = [] (double v) { double s = 1, tt = 1; for (int k = 1; k < 60; ++k) { tt *= (v / (2 * k)) * (v / (2 * k)); s += tt; } return s; };
        double sum = 0; for (int k = -H; k <= H; ++k) { const double a = k == 0 ? 2 * fc : std::sin (2 * kPi * fc * k) / (kPi * k); const double r = (double) k / (H + 1); const double w = i0 (beta * std::sqrt (1 - r * r)) / i0 (beta); h[(size_t) (k + H)] = a * w; sum += a * w; }
        for (auto& v : h) v /= sum;
        std::vector<double> f (n4, 0.0);
        for (size_t i = (size_t) H; i + (size_t) H < n4; ++i) { double a = 0; for (int k = -H; k <= H; ++k) a += h[(size_t) (k + H)] * y[i - (size_t) k]; f[i] = a; }
        double mx = 0; for (double v : f) mx = std::max (mx, std::fabs (v));
        std::printf ("  -- synthesised at 4 fs, low-passed: continuous-time peak of the source = %.3f dBFS\n", db (mx));
        for (int off = 0; off < 4; ++off)
        {
            std::vector<double> x; for (size_t i = (size_t) off + 4 * 128; i + 4 * 128 < n4; i += 4) x.push_back (f[i]);
            fade (x, fs, 0.003);
            char nm[80]; std::snprintf (nm, sizeof nm, "#%d single fs/4 period in fs/6 sine, offset %d", 20 + off, off);
            check (nm, tpOf (x, fs), 0.0, refTruePeakDb (x));
        }
    }
    // inter-sample peaks: sample peak under-reads, true peak does not; clip indicator follows the true peak
    {
        const double fs = 48000; std::vector<double> x ((size_t) fs);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 1.0 * std::sin (2 * kPi * (fs / 4) * (double) i / fs + kPi / 4 + 0.0);
        fade (x, fs, 0.010);
        // amplitude 1.0 at 45 deg: samples are +/-0.7071 -> sample peak -3.01 dBFS, true peak 0 dBTP
        bool clip = false; double sp = 0; const double tp = tpOf (x, fs, &clip, &sp);
        std::printf ("  fs/4 @45 deg, amplitude 1.0: sample peak %.2f dBFS, true peak %.2f dBTP (expected 0.0), clip latch %d\n", sp, tp, (int) clip);
        CHECK (near (sp, -3.01, 0.02) && near (tp, 0.0, 0.1) && tp > sp + 2.5);
        std::vector<double> y (x.size()); for (size_t i = 0; i < y.size(); ++i) y[i] = 0.9 * std::sin (2 * kPi * (fs / 4) * (double) i / fs + kPi / 4);
        const double tp2 = tpOf (y, fs, &clip, &sp);
        CHECK_MSG (! clip && near (tp2, db (0.9), 0.2), "0.9 amplitude: tp %.2f clip %d", tp2, (int) clip);
        // Accuracy of the 4x interpolator on sines from 1 to 20 kHz, against an 8x reference. A 4x grid can only
        // under-read (by up to ~0.3 dB for a 16 kHz sine at 48 kHz, which BS.1770 Annex 2 acknowledges); it must never over-read.
        double under = 0, over = 0; int underF = 0;
        for (int fk = 1000; fk <= 20000; fk += 1000)
            for (double ph : { 0.3, 0.9, 1.7, 2.6 })
            {
                std::vector<double> z ((size_t) 3000);
                for (size_t i = 0; i < z.size(); ++i) z[i] = 0.5 * std::sin (2 * kPi * fk * (double) i / fs + ph);
                fade (z, fs, 0.01);
                const double dev = tpOf (z, fs) - refTruePeakDb (z, 8);
                if (dev < under) { under = dev; underF = fk; }
                over = std::max (over, dev);
            }
        std::printf ("  4x interpolator vs 8x reference, sines 1-20 kHz @48 kHz: worst under-read %.3f dB (at %d Hz), worst over-read %.3f dB\n", under, underF, over);
        CHECK_MSG (under > -0.4 && over < 0.05, "under %.3f over %.3f", under, over);
    }
}

int main()
{
    testKWeightingCoefficients();
    testSineCalibration();
    testChannels();
    testSilenceAndReset();
    testRmsWindows();
    testEbu3341();
    testLra();
    testAgainstBatchReference();
    testTruePeak();
    std::printf ("\n%d checks, %d failures\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
