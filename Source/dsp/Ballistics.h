// Malon Frequency Analyzer
// Copyright (C) 2026 MUSICA MALON. Licensed under the GNU AGPL v3 (see LICENSE).
//
// Display-side ballistics: peak decay, peak hold, rectangular-window average.
// The Meter and Averager behaviour is ported from Seven Phases Spectrum Analyzer
// (MIT, Copyright (c) 2008-2015 Max Mikhailov; see THIRD_PARTY_NOTICES.md), with
// real elapsed time instead of a fixed poll constant and a plain ring buffer.
#pragma once

#include "FilterBank.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace malon
{

inline double gainToDb (double v) { return 20.0 * std::log10 (v); }
inline double dbToGain (double v) { return std::pow (10.0, v / 20.0); }

constexpr double kInfEdgeDb = -200.0;
constexpr double kAvgSilenceDb = -150.0;

/** Peak with constant dB/s decay + hold with time, then constant dB/s decay (or infinite). */
struct Meter
{
    struct Options
    {
        double peakDecayDbPerSec = 15.0;
        bool   holdInfinite = false;
        double holdTimeMs   = 2000.0;
        double holdDecayDbPerSec = 3.0;
    };

    double peak = kInfEdgeDb;
    double hold = kInfEdgeDb;
    double holdTimeLeft = 0.0;

    void tick (double valueDb, double dtMs, const Options& o)
    {
        peak -= o.peakDecayDbPerSec * (0.001 * dtMs);
        if (peak < valueDb) peak = valueDb;

        if (holdTimeLeft > 0.0)
            holdTimeLeft -= dtMs;
        else if (! o.holdInfinite)
            hold -= o.holdDecayDbPerSec * (0.001 * dtMs);

        if (hold < valueDb)
        {
            hold = valueDb;
            holdTimeLeft = o.holdTimeMs;
        }
    }

    void reset()     { hold = peak = kInfEdgeDb; holdTimeLeft = 0.0; }
    void resetHold() { hold = peak; holdTimeLeft = 0.0; }
};

/** Rectangular moving average over the last 'size' poll values (mean-square domain). */
class Averager
{
public:
    explicit Averager (int capacity = 1) { setCapacity (capacity); }

    void setCapacity (int capacity)
    {
        cap = std::max (1, capacity) + 2;
        buf.assign ((size_t) cap, 0.0);
        head = 0; count = 0; sum = 0.0; size = 0;
    }

    /** Returns mean of the last 'newSize' inputs (or 'inf' if the sum is ~zero). */
    double tick (double in, double inf, int newSize)
    {
        resize (newSize);
        push (in);
        while (count > size) sum -= pop();
        return (count > 0 && sum > inf * count * 10000.0) ? sum / count : inf;
    }

    void clear() { head = 0; count = 0; sum = 0.0; }

private:
    void push (double v) { buf[(size_t) ((head + count) % cap)] = v; ++count; sum += v; }
    double pop()         { const double v = buf[(size_t) head]; head = (head + 1) % cap; --count; return v; }

    void resize (int newSize)
    {
        newSize = std::max (1, std::min (newSize, cap - 2));
        if (newSize < size)
            while (count > newSize) sum -= pop();
        else if (newSize > size && count > 0)
        {
            const double current = (count > 0) ? sum / count : 0.0;
            while (count < newSize) push (current);
        }
        size = newSize;
    }

    int cap = 3, head = 0, count = 0, size = 0;
    double sum = 0.0;
    std::vector<double> buf;
};

struct DisplaySettings
{
    ChannelMode channel = ChannelMode::Both;
    bool   peakOn = true;
    double peakDecay = 15.0;     // dB/s
    bool   avgOn = true;
    double avgTimeMs = 3000.0;
    bool   holdOn = true;
    bool   holdInfinite = false;
    double holdTimeMs = 2000.0;
    double holdDecay = 3.0;      // dB/s
    double avgSlope = 0.0;       // extra dB/oct on the average only (legacy), pivot 1 kHz
    double slope = 4.5;          // SPAN-equivalent spectrum slope in dB/oct, pivot 1 kHz (peak, hold, average)
};

/** Turns raw Readouts into the dB values that are drawn. UI thread only. */
class DisplayModel
{
public:
    static constexpr double nominalPollMs = 32.0;   // averager bin length

    DisplayModel() { for (auto& a : avgFilters) a.setCapacity (int (20000.0 / nominalPollMs) + 2); reset(); }

    /** peak[i], hold[i], avg[i] in dB for i < numBands (a sine of amplitude 1 reads 0 dB). */
    int    numBands = 0;
    double freqMin = 0, freqMax = 0, bandsPerOctave = 0;
    double peak[kMaxBands] {}, hold[kMaxBands] {}, avg[kMaxBands] {};
    double sigma[kMaxBands] {};   // spread (dB) of each band's bar over the average time

    /** Noise-envelope bias of the bars (dB above the band's true power, Gaussian noise), as a function of
        x = log2 (band noise bandwidth [Hz] * 0.032 s poll). Bars are peak envelopes; the more independent
        envelope samples a band gets per poll (wide, high bands), the higher its peak sits above its power.
        Constant-Q bands get wider with frequency, so on noise this tilts the long-term average up by
        ~0.7 (6/oct) .. 0.9 (48/oct) dB/oct. Measured with white noise through the bank at 6/12/24/48 bpo
        (it is one curve for every resolution). sigmaNoise = the bar's spread on noise at that x. */
    static double noiseBias (double x) noexcept  { return interp (x, biasTab); }
    static double noiseSigma (double x) noexcept { return interp (x, sigTab); }

    void update (const Readout& r, double dtMs, const DisplaySettings& s)
    {
        if (r.numBands != numBands || r.bandsPerOctave != bandsPerOctave) { numBands = r.numBands; freqMin = r.freqMin; bandsPerOctave = r.bandsPerOctave; reset(); }
        freqMin = r.freqMin; freqMax = r.freqMax; bandsPerOctave = r.bandsPerOctave;
        if (frozen) return;

        const double infG = dbToGain (kInfEdgeDb);
        const double inf  = infG * infG;                    // squared domain

        Meter::Options mo;
        mo.peakDecayDbPerSec = s.peakDecay;
        mo.holdInfinite = s.holdInfinite;
        mo.holdTimeMs = s.holdTimeMs;
        mo.holdDecayDbPerSec = s.holdDecay;

        const int    size  = int (s.avgTimeMs / nominalPollMs);
        const double bpo   = r.bandsPerOctave > 0 ? r.bandsPerOctave : 6.0;

        for (int i = 0; i < numBands; ++i)
        {
            // output 0 is the low-pass remainder, band i is output i + 1
            const double pk = 0.5 * gainToDb (r.peakSq[i + 1] + inf);          // amplitude dB
            meters[i].tick (pk, dtMs, mo);

            // Average = long-term mean of the bars themselves (dB domain, over avgTime).
            // v1.0 averaged the band's mean-square power instead. Bars are peak envelopes with a
            // slow decay, so on music (crest factor 10-25 dB in narrow high bands) the power average
            // sat 10-25 dB under the bars. Averaging the bar value keeps the line where the bars live.
            // Silent polls (bar below -150 dB) are skipped so pauses don't drag a dB-domain mean down;
            // until the window has filled, the mean is over what has arrived (no ramp up from -200 dB).
            if (r.samples > 0 && meters[i].peak > kAvgSilenceDb)
                lastAvgDb[i] = kInfEdgeDb + avgFilters[i].tick (meters[i].peak - kInfEdgeDb, 1.0e-12, size);
            // Tilt, pivot 1 kHz, band centre f_i = freqMin * 2^(i / bpo). The slope is SPAN-equivalent:
            // SPAN is an FFT (constant-Hz bins), where pink noise falls 3 dB/oct at slope 0 and is flat at 3.
            // Our bands are constant-Q (1/6 oct), where pink noise is already flat, so we tilt by (slope - 3).
            // Result: a mix looks the same as in SPAN at the same slope setting.
            // how noise-like the band is: rms of poll-to-poll bar changes (blind to slow swells and to a
            // tone building up; a steady tone gives ~0, noise gives noiseSigma)
            if (r.samples > 0 && meters[i].peak > kAvgSilenceDb)
            {
                const double a = std::min (1.0, dtMs / 1000.0);          // ~1 s memory
                const double v = meters[i].peak;
                if (m1[i] > kAvgSilenceDb)
                {
                    const double d = std::max (-6.0, std::min (6.0, v - m1[i]));
                    m2[i] += a * (0.5 * d * d - m2[i]);
                }
                m1[i] = v;
                sigma[i] = std::sqrt (std::max (0.0, m2[i]));
            }
            const double oct = std::log2 (r.freqMin / 1000.0) + double (i) / bpo;
            const double tilt = (s.slope - 3.0) * oct;
            avg[i] = lastAvgDb[i] - biasCorr[i] * std::min (1.0, sigma[i] / std::max (0.1, sigNoise[i])) + tilt + s.avgSlope * oct;
            peak[i] = meters[i].peak + tilt;
            hold[i] = meters[i].hold + tilt;
        }
    }
    void reset()
    {
        for (int i = 0; i < kMaxBands; ++i)
        {
            meters[i].reset(); avgFilters[i].clear();
            peak[i] = hold[i] = avg[i] = kInfEdgeDb;
            lastAvgDb[i] = kInfEdgeDb;
            m1[i] = kInfEdgeDb; m2[i] = 0.0; sigma[i] = 0.0;
        }
        layoutCorrections();
    }

    /** Per band: how much of the noise bias to remove (relative to the mean over 50 Hz..10 kHz, so the
        overall level is unchanged) and the spread noise would have there. */
    void layoutCorrections()
    {
        const double bpo = bandsPerOctave > 0 ? bandsPerOctave : 6.0;
        const double bw = std::pow (2.0, 0.5 / bpo) - std::pow (2.0, -0.5 / bpo);
        double sum = 0; int cnt = 0;
        for (int i = 0; i < kMaxBands; ++i)
        {
            const double f = (freqMin > 0 ? freqMin : 22.0) * std::pow (2.0, i / bpo);
            const double x = std::log2 (f * bw * 0.032);
            biasCorr[i] = noiseBias (x); sigNoise[i] = noiseSigma (x);
            if (i < numBands && f >= 50 && f <= 10000) { sum += biasCorr[i]; ++cnt; }
        }
        const double c = cnt ? sum / cnt : 0.0;
        for (int i = 0; i < kMaxBands; ++i) biasCorr[i] -= c;
    }
    double biasCorr[kMaxBands] {}, sigNoise[kMaxBands] {};

    void resetHold() { for (int i = 0; i < numBands; ++i) { meters[i].resetHold(); hold[i] = meters[i].hold; } }

    bool frozen = false;

private:
    struct Tab { double x0, dx; int n; double v[16]; };
    static double interp (double x, const Tab& t) noexcept
    {
        const double u = (x - t.x0) / t.dx;
        if (u <= 0) return t.v[0];
        if (u >= t.n - 1) return t.v[t.n - 1];
        const int i = (int) u; const double f = u - i;
        return t.v[i] + f * (t.v[i + 1] - t.v[i]);
    }
    static constexpr Tab biasTab { -6.0, 1.0, 12, { -1.77, -0.68, 0.41, 1.61, 2.77, 3.81, 4.73, 5.56, 6.25, 6.75, 7.26, 7.68 } };
    static constexpr Tab sigTab  { -6.0, 1.0, 12, { 0.32, 0.43, 0.55, 0.66, 0.72, 0.73, 0.71, 0.67, 0.63, 0.60, 0.56, 0.53 } };
    double m1[kMaxBands] {}, m2[kMaxBands] {};

    Meter meters[kMaxBands];
    double lastAvgDb[kMaxBands] {};
    Averager avgFilters[kMaxBands];
};

} // namespace malon
