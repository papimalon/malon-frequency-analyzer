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
        return (sum > inf * size * 10000.0) ? sum / size : inf;
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
        else if (newSize > size)
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
    double avgSlope = 0.0;       // dB per octave, pivot at the middle of the range
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

    void update (const Readout& r, double dtMs, const DisplaySettings& s)
    {
        if (r.numBands != numBands || r.bandsPerOctave != bandsPerOctave) { numBands = r.numBands; reset(); }
        freqMin = r.freqMin; freqMax = r.freqMax; bandsPerOctave = r.bandsPerOctave;
        if (frozen) return;

        const double infG = dbToGain (kInfEdgeDb);
        const double inf  = infG * infG;                    // squared domain

        Meter::Options mo;
        mo.peakDecayDbPerSec = s.peakDecay;
        mo.holdInfinite = s.holdInfinite;
        mo.holdTimeMs = s.holdTimeMs;
        mo.holdDecayDbPerSec = s.holdDecay;

        const double scale = r.samples > 0 ? 1.0 / r.samples : 0.0;
        const int    size  = int (s.avgTimeMs / nominalPollMs);
        const double bpo   = r.bandsPerOctave > 0 ? r.bandsPerOctave : 6.0;

        for (int i = 0; i < numBands; ++i)
        {
            // output 0 is the low-pass remainder, band i is output i + 1
            const double pk = 0.5 * gainToDb (r.peakSq[i + 1] + inf);          // amplitude dB
            meters[i].tick (pk, dtMs, mo);

            // No new audio since the last poll (e.g. host paused): keep the average as it is
            // instead of averaging in a fake silent slot.
            if (r.samples > 0)
            {
                const double ms = r.sumSq[i + 1] * scale;                        // mean square
                lastAvgDb[i] = gainToDb (avgFilters[i].tick (ms, inf, size));    // 20log10(mean square)
            }
            // +3 dB so that a sine reads its peak amplitude (mean square of a sine = A^2/2)
            const double tilt = s.avgSlope * (double (i) - 0.5 * numBands) / bpo;
            avg[i] = 3.0 + 0.5 * lastAvgDb[i] + tilt;

            peak[i] = meters[i].peak;
            hold[i] = meters[i].hold;
        }
    }

    void reset()
    {
        for (int i = 0; i < kMaxBands; ++i)
        {
            meters[i].reset(); avgFilters[i].clear();
            peak[i] = hold[i] = avg[i] = kInfEdgeDb;
            lastAvgDb[i] = 2.0 * kInfEdgeDb;   // mean-square domain
        }
    }

    void resetHold() { for (int i = 0; i < numBands; ++i) { meters[i].resetHold(); hold[i] = meters[i].hold; } }

    bool frozen = false;

private:
    Meter meters[kMaxBands];
    double lastAvgDb[kMaxBands] {};
    Averager avgFilters[kMaxBands];
};

} // namespace malon
