// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#include "LevelMeter.h"
#include <algorithm>
#include <cstring>

namespace malon
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    // indices into the published array
    enum Pub { TpMax, TpL, TpR, SpMax, RmsL, RmsR, RmsAll, Mom, Short, Integ, Lra, MomMax, ShortMax, NumPub };

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 60; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; if (term < 1e-18 * sum) break; }
        return sum;
    }

    inline float toDbF (double lin) { return lin > 0.0 ? (float) (20.0 * std::log10 (lin)) : -std::numeric_limits<float>::infinity(); }
    inline float msToDbF (double ms) { return ms > 0.0 ? (float) (10.0 * std::log10 (ms)) : -std::numeric_limits<float>::infinity(); }
}

LevelMeter::LevelMeter()
{
    for (auto& p : pub) p.store (-std::numeric_limits<float>::infinity());
    for (auto& r : recentPeak) r.store (0.0f);
    prepare (48000.0, 2);          // safe defaults if audio arrives before prepareToPlay
}

std::array<LevelMeter::Biquad, 2> LevelMeter::designKWeighting (double fs)
{
    std::array<Biquad, 2> out;
    {   // stage 1: high shelf ("pre-filter"), ~ +4 dB above 2 kHz
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K  = std::tan (kPi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        out[0].b0 = (Vh + Vb * K / Q + K * K) / a0;
        out[0].b1 = 2.0 * (K * K - Vh) / a0;
        out[0].b2 = (Vh - Vb * K / Q + K * K) / a0;
        out[0].a1 = 2.0 * (K * K - 1.0) / a0;
        out[0].a2 = (1.0 - K / Q + K * K) / a0;
    }
    {   // stage 2: RLB high-pass at ~38 Hz
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K  = std::tan (kPi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        out[1].b0 = 1.0; out[1].b1 = -2.0; out[1].b2 = 1.0;
        out[1].a1 = 2.0 * (K * K - 1.0) / a0;
        out[1].a2 = (1.0 - K / Q + K * K) / a0;
    }
    return out;
}

void LevelMeter::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::max (1, std::min (kMaxChannels, numChannels));
    kw = designKWeighting (fs);

    // true-peak interpolator: windowed-sinc, 4 phases x 24 taps, Kaiser window (beta 6).
    // Phase 0 is the identity, so the original samples are always included.
    const double beta = 6.0, denom = besselI0 (beta);
    for (int p = 0; p < 4; ++p)
    {
        const double f = p / 4.0;
        for (int j = 0; j < kTpTaps; ++j)
        {
            const double k = kTpTaps / 2 - j;           // sample c + k, with c = n - T/2
            const double u = k - f;
            double sinc = std::fabs (u) < 1e-12 ? 1.0 : std::sin (kPi * u) / (kPi * u);
            const double r = u / (kTpTaps / 2.0);
            const double w = std::fabs (r) >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / denom;
            tpCoef[p][j] = sinc * w;
        }
    }

    blockLen   = std::max (1, (int) std::lround (fs * 0.01));            // 10 ms base bins
    rmsBinLen  = std::max (1, (int) std::lround (fs / 1000.0));
    for (int c = 0; c < kMaxChannels; ++c) rmsRing[c].assign ((size_t) kRmsMaxBins + 2, 0.0);
    gateCount.assign (kHistBins, 0); lraCount.assign (kHistBins, 0);
    gateEnergy.assign (kHistBins, 0.0); lraEnergy.assign (kHistBins, 0.0);

    std::memset (kz, 0, sizeof (kz));
    std::memset (tpHist, 0, sizeof (tpHist));
    tpIdx = 0;
    rmsBinPos = 0; rmsHead = 0; rmsBinsFilled = 0;
    for (int c = 0; c < kMaxChannels; ++c) { rmsBinAcc[c] = 0; rmsSum[c] = 0; }
    setRmsWindowMs (300);
    reset();
    handledResets = resetRequests.load (std::memory_order_acquire);
    publish();
}

void LevelMeter::setRmsWindowMs (int ms) noexcept
{
    const int bins = std::max (1, std::min (kRmsMaxBins, (int) std::lround (ms * fs / 1000.0 / rmsBinLen)));
    if (bins == rmsWindowBins) return;
    rmsWindowBins = bins;
    recomputeRmsSums();
}

void LevelMeter::recomputeRmsSums() noexcept
{
    const int cap = kRmsMaxBins + 2;
    for (int c = 0; c < channels; ++c)
    {
        double s = 0.0;
        for (int i = 1; i <= rmsWindowBins; ++i) s += rmsRing[c][(size_t) ((rmsHead - i + cap * 2) % cap)];
        rmsSum[c] = s;
    }
}

void LevelMeter::reset() noexcept
{
    for (int c = 0; c < kMaxChannels; ++c) { tpHeld[c] = 0.0; blockAcc[c] = 0.0; recentPeak[c].store (0.0f, std::memory_order_relaxed); }
    tpSkip = kTpTaps - 1;
    samplePeakHeld = 0.0; clipLatch = false; pubClip.store (0, std::memory_order_relaxed);
    std::fill (std::begin (blockRing), std::end (blockRing), 0.0);
    blockHead = 0; blockPos = 0; blocksTotal = 0;
    momentaryMaxLin = shortTermMaxLin = 0.0;
    lastMomentary = lastShort = lastIntegrated = lastLra = kNegInf;
    if (! gateCount.empty())
    {
        std::fill (gateCount.begin(), gateCount.end(), 0u); std::fill (lraCount.begin(), lraCount.end(), 0u);
        std::fill (gateEnergy.begin(), gateEnergy.end(), 0.0); std::fill (lraEnergy.begin(), lraEnergy.end(), 0.0);
    }
    publish();
}

void LevelMeter::finishRmsBin() noexcept
{
    const int cap = kRmsMaxBins + 2;
    for (int c = 0; c < channels; ++c)
    {
        double& slot = rmsRing[c][(size_t) rmsHead];
        // running window: add the new bin, drop the one that just fell out of the window
        const double leaving = rmsRing[c][(size_t) ((rmsHead - rmsWindowBins + cap * 2) % cap)];
        rmsSum[c] += rmsBinAcc[c] - leaving;
        if (rmsSum[c] < 0.0) rmsSum[c] = 0.0;
        slot = rmsBinAcc[c];
        rmsBinAcc[c] = 0.0;
    }
    rmsHead = (rmsHead + 1) % cap;
    rmsBinsFilled = std::min (rmsBinsFilled + 1, kRmsMaxBins);
}

void LevelMeter::finishLoudnessBlock() noexcept
{
    // One 10 ms bin is complete. Momentary (40 bins = 400 ms) and short-term (300 bins = 3 s) are evaluated
    // every 10 ms (EBU Tech 3341 test 13/14 needs a 20 ms resolution); gating blocks (400 ms, 75 % overlap)
    // and the LRA's short-term values are taken every 100 ms, as in BS.1770 / Tech 3342.
    double e = 0.0;
    for (int c = 0; c < channels; ++c) { e += blockAcc[c] / blockLen; blockAcc[c] = 0.0; }   // channel weight 1.0 (L, R)
    blockRing[blockHead] = e;
    blockHead = (blockHead + 1) % kLoudBins;
    ++blocksTotal;

    // Before a window is full the missing part counts as silence, exactly like measuring a signal that starts at t = 0.
    double m = 0.0, s = 0.0;
    for (int i = 1; i <= kLoudBins; ++i)
    {
        const double v = blockRing[(blockHead - i + 2 * kLoudBins) % kLoudBins];
        s += v; if (i <= 40) m += v;
    }
    m /= 40.0; s /= (double) kLoudBins;
    lastMomentary = lufs (m);
    lastShort = lufs (s);
    momentaryMaxLin = std::max (momentaryMaxLin, m);
    shortTermMaxLin = std::max (shortTermMaxLin, s);

    if (blocksTotal % 10 != 0) return;

    auto addToHistogram = [] (std::vector<uint32_t>& cnt, std::vector<double>& en, double z)
    {
        const double l = lufs (z);
        if (! (l > -70.0)) return;                       // absolute gate
        int idx = (int) std::floor ((l + 70.0) / 0.01);
        idx = std::max (0, std::min (kHistBins - 1, idx));
        cnt[(size_t) idx] += 1; en[(size_t) idx] += z;
    };

    if (blocksTotal >= 40)         addToHistogram (gateCount, gateEnergy, m);
    if (blocksTotal >= kLoudBins)  addToHistogram (lraCount, lraEnergy, s);      // full 3 s windows only
    lastIntegrated = integratedFromHistogram();
    lastLra = blocksTotal >= kLoudBins ? rangeFromHistogram() : kNegInf;
}

double LevelMeter::integratedFromHistogram() const noexcept
{
    double n0 = 0, s0 = 0;
    for (int i = 0; i < kHistBins; ++i) { n0 += gateCount[(size_t) i]; s0 += gateEnergy[(size_t) i]; }
    if (n0 <= 0) return kNegInf;
    const double gammaR = lufs (s0 / n0) - 10.0;          // relative gate, -10 LU
    int first = (int) std::ceil ((gammaR + 70.0) / 0.01 - 1e-9);
    first = std::max (0, first);
    double n = 0, s = 0;
    for (int i = first; i < kHistBins; ++i) { n += gateCount[(size_t) i]; s += gateEnergy[(size_t) i]; }
    return n > 0 ? lufs (s / n) : kNegInf;
}

double LevelMeter::rangeFromHistogram() const noexcept
{
    double n0 = 0, s0 = 0;
    for (int i = 0; i < kHistBins; ++i) { n0 += lraCount[(size_t) i]; s0 += lraEnergy[(size_t) i]; }
    if (n0 <= 0) return kNegInf;
    const double gammaR = lufs (s0 / n0) - 20.0;          // relative gate, -20 LU (Tech 3342)
    int first = std::max (0, (int) std::ceil ((gammaR + 70.0) / 0.01 - 1e-9));
    double n = 0;
    for (int i = first; i < kHistBins; ++i) n += lraCount[(size_t) i];
    if (n <= 0) return kNegInf;
    auto percentile = [&] (double pct)
    {
        const double rank = std::round ((n - 1.0) * pct / 100.0);       // 0-based index in the sorted list
        double seen = 0;
        for (int i = first; i < kHistBins; ++i)
        {
            const double c = lraCount[(size_t) i];
            if (c > 0 && rank < seen + c) return lufs (lraEnergy[(size_t) i] / c);
            seen += c;
        }
        return kNegInf;
    };
    return percentile (95.0) - percentile (10.0);
}

template <typename T>
void LevelMeter::processImpl (const T* const* in, int numCh, int n) noexcept
{
    if (in == nullptr || n <= 0) return;
    if (resetRequests.load (std::memory_order_acquire) != handledResets)
    {
        handledResets = resetRequests.load (std::memory_order_acquire);
        reset();
    }
    const int nc = std::min (channels, std::max (1, numCh));
    constexpr int T2 = kTpTaps;
    double blockPeak[kMaxChannels] = { 0.0, 0.0 };

    for (int i = 0; i < n; ++i)
    {
        const bool tpValid = (tpSkip == 0);
        if (tpSkip > 0) --tpSkip;
        // history ring is stored twice so a contiguous window is always available
        tpIdx = (tpIdx + T2 - 1) % T2;                   // newest sample lives at tpIdx, older ones follow
        for (int c = 0; c < nc; ++c)
        {
            const double x = (double) in[c][i];
            const double ax = std::fabs (x);

            // ---- true peak: 4x oversampling ----
            tpHist[c][tpIdx] = x; tpHist[c][tpIdx + T2] = x;
            const double* h = tpHist[c] + tpIdx;         // h[j] = x[n - j]
            double pk = ax;                              // phase 0 == the sample itself
            for (int p = 1; p < 4 && tpValid; ++p)
            {
                double acc = 0.0; const double* k = tpCoef[p];
                for (int j = 0; j < T2; ++j) acc += k[j] * h[j];
                pk = std::max (pk, std::fabs (acc));
            }
            if (pk > blockPeak[c]) blockPeak[c] = pk;
            if (ax > samplePeakHeld) samplePeakHeld = ax;

            // ---- K-weighting for loudness ----
            double y = x;
            for (int s = 0; s < 2; ++s)
            {
                const Biquad& q = kw[(size_t) s];
                const double out = q.b0 * y + kz[c][s][0];
                kz[c][s][0] = q.b1 * y - q.a1 * out + kz[c][s][1];
                kz[c][s][1] = q.b2 * y - q.a2 * out;
                y = out;
            }
            blockAcc[c] += y * y;
            rmsBinAcc[c] += x * x;
        }
        if (++blockPos >= blockLen) { blockPos = 0; finishLoudnessBlock(); }
        if (++rmsBinPos >= rmsBinLen) { rmsBinPos = 0; finishRmsBin(); }
    }

    for (int c = 0; c < nc; ++c)
    {
        if (blockPeak[c] > tpHeld[c]) tpHeld[c] = blockPeak[c];
        // recent peak for the live display (UI consumes and clears)
        float old = recentPeak[c].load (std::memory_order_relaxed);
        while ((float) blockPeak[c] > old && ! recentPeak[c].compare_exchange_weak (old, (float) blockPeak[c], std::memory_order_relaxed)) {}
        if (blockPeak[c] >= 1.0) clipLatch = true;
    }
    if (samplePeakHeld >= 1.0) clipLatch = true;
    publish();
}

void LevelMeter::process (const float* const* in, int c, int n) noexcept  { processImpl (in, c, n); }
void LevelMeter::process (const double* const* in, int c, int n) noexcept { processImpl (in, c, n); }

void LevelMeter::publish() noexcept
{
    auto put = [this] (int i, float v) { pub[(size_t) i].store (v, std::memory_order_relaxed); };
    const double tpAll = std::max (tpHeld[0], channels > 1 ? tpHeld[1] : 0.0);
    put (TpMax, toDbF (tpAll));
    put (TpL, toDbF (tpHeld[0]));
    put (TpR, toDbF (channels > 1 ? tpHeld[1] : tpHeld[0]));
    put (SpMax, toDbF (samplePeakHeld));
    const double win = (double) rmsWindowBins * rmsBinLen;
    const double l = rmsSum[0] / win, r = channels > 1 ? rmsSum[1] / win : l;
    put (RmsL, msToDbF (l));
    put (RmsR, msToDbF (r));
    put (RmsAll, msToDbF (channels > 1 ? 0.5 * (l + r) : l));
    put (Mom, (float) lastMomentary);
    put (Short, (float) lastShort);
    put (Integ, (float) lastIntegrated);
    put (Lra, lastLra == kNegInf ? -std::numeric_limits<float>::infinity()
                                 : (float) lastLra);
    put (MomMax, (float) lufs (momentaryMaxLin));
    put (ShortMax, (float) lufs (shortTermMaxLin));
    pubClip.store (clipLatch ? 1 : 0, std::memory_order_relaxed);
}

LevelMeter::Readout LevelMeter::read() const noexcept
{
    auto get = [this] (int i) { return (double) pub[(size_t) i].load (std::memory_order_relaxed); };
    Readout r;
    r.channels = channels;
    r.truePeakMaxDb = get (TpMax); r.truePeakDb[0] = get (TpL); r.truePeakDb[1] = get (TpR);
    r.samplePeakMaxDb = get (SpMax);
    r.clipped = pubClip.load (std::memory_order_relaxed) != 0;
    r.rmsDb[0] = get (RmsL); r.rmsDb[1] = get (RmsR); r.rmsCombinedDb = get (RmsAll);
    r.momentary = get (Mom); r.shortTerm = get (Short); r.integrated = get (Integ);
    r.loudnessRange = get (Lra);
    r.momentaryMax = get (MomMax); r.shortTermMax = get (ShortMax);
    return r;
}

void LevelMeter::consumeRecentPeaks (double out[2]) noexcept
{
    for (int c = 0; c < kMaxChannels; ++c) out[c] = (double) recentPeak[c].exchange (0.0f, std::memory_order_relaxed);
}

} // namespace malon
