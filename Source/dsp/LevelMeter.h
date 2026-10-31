// Malon Frequency Analyzer
// Copyright (C) 2026 MUSICA MALON. Licensed under the GNU AGPL v3 (see LICENSE).
//
// Pro metering: true peak (ITU-R BS.1770-4 Annex 2, 4x oversampled), RMS with selectable window,
// loudness per ITU-R BS.1770-4 / EBU R128 (momentary 400 ms, short-term 3 s, integrated with absolute
// and relative gate) and loudness range per EBU Tech 3342.
//
// Written from the published specifications (BS.1770-4 K-weighting, EBU Tech 3341 / 3342); no code
// from other meters is used. Real-time safe: all memory is allocated in prepare(); the audio thread
// publishes results through atomics and never waits for the UI.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace malon
{

class LevelMeter
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr double kNegInf = -std::numeric_limits<double>::infinity();

    /** Everything the UI shows. All levels in dB; -inf when there is nothing to report yet. */
    struct Readout
    {
        int    channels = 2;
        double truePeakMaxDb   = kNegInf;                   // held, both channels, dBTP
        double truePeakDb[2]   = { kNegInf, kNegInf };      // held per channel, dBTP
        double samplePeakMaxDb = kNegInf;                   // held, dBFS
        bool   clipped = false;                             // latched until reset
        double rmsDb[2]        = { kNegInf, kNegInf };      // per channel, dBFS
        double rmsCombinedDb   = kNegInf;                   // both channels together, dBFS
        double rmsFastDb[2]    = { kNegInf, kNegInf };      // fixed 300 ms window per channel (Out meter, history)
        double rmsFastCombinedDb = kNegInf;
        double rmsKDb[2]       = { kNegInf, kNegInf };      // fixed 600 ms window per channel (K-system metering)
        bool   rmsIntegrated   = false;                     // rmsDb / rmsCombinedDb are integrated since reset
        double momentary = kNegInf, shortTerm = kNegInf;    // LUFS
        double integrated = kNegInf;                        // LUFS (gated)
        double loudnessRange = kNegInf;                     // LU (kNegInf until 3 s of audio)
        double momentaryMax = kNegInf, shortTermMax = kNegInf;
        double correlation = 0.0;                           // -1 .. +1 (stereo only)
        bool   correlationValid = false;                    // false for mono or silence
    };

    LevelMeter();

    /** Allocates and resets everything. Not real-time safe. channels: 1 (mono) or 2 (stereo). */
    void prepare (double sampleRate, int channels);

    /** Window for the RMS readout, in milliseconds (1 .. 3000). Audio thread (or before processing). */
    void setRmsWindowMs (int ms) noexcept;

    /** true: rmsDb is the power integrated since the last reset (Voxengo SPAN "Statistics RMS"). */
    void setRmsIntegrated (bool on) noexcept { rmsIntegratedMode = on; }

    /** Any thread: restart only the integrated RMS (e.g. on host playback start, like SPAN). */
    void requestRmsIntegratedReset() noexcept { rmsIntResetRequests.fetch_add (1, std::memory_order_release); }

    /** Any thread: ask the audio thread to clear held values, integrated loudness, LRA and the clip latch. */
    void requestReset() noexcept { resetRequests.fetch_add (1, std::memory_order_release); }

    /** Audio thread only: clear now (also done automatically when requestReset() was called). */
    void reset() noexcept;

    /** Feed audio (read only). 'numChannels' is 1 or 2; mono input is measured as a single channel. */
    void process (const float* const* in, int numChannels, int numSamples) noexcept;
    void process (const double* const* in, int numChannels, int numSamples) noexcept;

    /** Any thread: latest published values. */
    Readout read() const noexcept;

    /** UI thread: highest true peak per channel since the last call (linear), then cleared. */
    void consumeRecentPeaks (double out[2]) noexcept;

    /** BS.1770-4 K-weighting biquads for a sample rate: [0] = high shelf, [1] = RLB high-pass.
        b0,b1,b2,a1,a2 (a0 = 1). Exposed for the tests. */
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    static std::array<Biquad, 2> designKWeighting (double sampleRate);

    /** Mean square -> LUFS; anything below -150 is reported as -inf (no "-2134 LUFS" on silence). */
    static double lufs (double meanSquare) noexcept
    {
        if (! (meanSquare > 0.0)) return kNegInf;
        const double l = -0.691 + 10.0 * std::log10 (meanSquare);
        return l < -150.0 ? kNegInf : l;
    }

private:
    template <typename T> void processImpl (const T* const* in, int numChannels, int numSamples) noexcept;
    void finishLoudnessBlock() noexcept;
    void finishRmsBin() noexcept;
    void publish() noexcept;
    double integratedFromHistogram() const noexcept;
    double rangeFromHistogram() const noexcept;
    void recomputeRmsSums() noexcept;

    // ---- design ----
    double fs = 48000.0;
    int channels = 2;
    std::array<Biquad, 2> kw {};
    static constexpr int kTpTaps = 24;                      // taps per phase (96 total)
    double tpCoef[4][kTpTaps] {};

    // ---- audio-thread state ----
    double kz[kMaxChannels][2][2] {};                       // K filters (transposed direct form II)
    double tpHist[kMaxChannels][2 * kTpTaps] {};
    int    tpIdx = 0;
    int    tpSkip = 0;                                      // after a reset: ignore interpolation windows that straddle old audio
    double tpHeld[kMaxChannels] {}, samplePeakHeld = 0.0;
    bool   clipLatch = false;

    static constexpr int kLoudBins = 300;                   // 10 ms bins in the 3 s window
    int    blockLen = 480, blockPos = 0;                    // 10 ms base bin
    double blockAcc[kMaxChannels] {};
    double blockRing[kLoudBins] {};                         // energy per 10 ms bin, zero-initialised
    int    blockHead = 0;
    long long blocksTotal = 0;
    double momentaryMaxLin = 0.0, shortTermMaxLin = 0.0;
    double lastMomentary = kNegInf, lastShort = kNegInf, lastIntegrated = kNegInf, lastLra = kNegInf;

    static constexpr int kHistBins = 8000;                  // 0.01 LU bins from -70 to +10 LUFS
    std::vector<uint32_t> gateCount, lraCount;
    std::vector<double>   gateEnergy, lraEnergy;

    int    rmsBinLen = 48, rmsBinPos = 0, rmsWindowBins = 300;
    static constexpr int kRmsMaxBins = 3000;
    double rmsBinAcc[kMaxChannels] {};
    std::vector<double> rmsRing[kMaxChannels];
    int    rmsHead = 0;
    int    rmsBinsFilled = 0;
    double rmsSum[kMaxChannels] {};
    static constexpr int kRmsFastBins = 300;                // 300 ms at 1 ms bins
    double rmsFastSum[kMaxChannels] {};
    static constexpr int kRmsKBins = 600;                   // 600 ms at 1 ms bins (K-system)
    double rmsKSum[kMaxChannels] {};
    double rmsIntSum[kMaxChannels] {};
    double rmsIntN = 0.0;
    bool   rmsIntegratedMode = false;
    std::atomic<int> rmsIntResetRequests { 0 };
    int    handledRmsIntResets = 0;

    int    handledResets = 0;

    // stereo correlation (one-pole averages, ~300 ms)
    double corrCoef = 0.0, corrLR = 0.0, corrLL = 0.0, corrRR = 0.0;

    // ---- published (relaxed atomics; a read may mix two neighbouring updates, which is harmless) ----
    std::atomic<int> resetRequests { 0 };
    std::array<std::atomic<float>, 24> pub;
    std::atomic<float> recentPeak[kMaxChannels];
    std::atomic<int> pubClip { 0 };
};

} // namespace malon
