// Malon Frequency Analyzer
// Copyright (C) 2026 MUSICA MALON. Licensed under the GNU AGPL v3 (see LICENSE).
//
// The filter-bank design in this file is a portable C++ port of the DSP core of
// "Spectrum Analyzer" by Max Mikhailov (Seven Phases), which is MIT licensed:
//
//   Copyright (c) 2008-2015 Max Mikhailov
//   (MIT licence text: see LICENSE-SEVEN-PHASES-MIT and THIRD_PARTY_NOTICES.md)
//
// What is kept from the original: the structure of the bank (a cascade-free set
// of two-pole resonant low-pass filters whose neighbouring outputs are
// subtracted to form band-pass bands), the Q / edge-warp / emphasis constants,
// the band layout rules and the peak / mean-square accumulation scheme.
// What changed: SSE intrinsics and Windows-only code are replaced by plain
// portable C++ (compilers auto-vectorise it, so it also runs on Apple Silicon),
// state and coefficients are double precision (so 22 Hz stays accurate at 192 kHz),
// and the audio-to-UI hand-off is a non-blocking try-lock instead of a blocking lock.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace malon
{

constexpr int kMaxFilters = 72;   // nBands + 1 filters must fit in here
constexpr int kMaxBands   = kMaxFilters - 1;

enum class ChannelMode { Left = 0, Right = 1, Both = 2, Mix = 3, Mid = 4, Side = 5 };

/** What the UI thread gets: accumulated squared peak and summed squares per output.
    Output 0 is the "everything below the first band" low-pass remainder (not drawn);
    band i (0-based) is output i + 1. Values are already emphasis-compensated. */
struct Readout
{
    int    numBands = 0;
    double freqMin  = 0.0;   // centre of first band, Hz
    double freqMax  = 0.0;   // centre of last band, Hz
    double bandsPerOctave = 0.0;
    double sampleRate = 0.0;
    double samples  = 0.0;   // number of samples summed in 'sumSq' (all channels)
    std::array<double, kMaxFilters> peakSq {};  // max of (emphasised signal)^2
    std::array<double, kMaxFilters> sumSq  {};  // sum of (emphasised signal)^2
};

class FilterBank
{
public:
    FilterBank();

    /** (Re)design the bank. Resets all state. bandsPerOctave must be 3, 4 or 6. */
    void prepare (double sampleRate, int bandsPerOctave);

    /** Clears filter state and accumulators. */
    void reset();

    /** Feed audio. 'right' may be null / equal to left for mono. Never allocates or blocks. */
    void process (const float* left, const float* right, int numSamples, ChannelMode mode) noexcept;
    void process (const double* left, const double* right, int numSamples, ChannelMode mode) noexcept;

    /** UI thread: copies and clears what has been accumulated since the last call.
        Returns false if nothing new has arrived. */
    bool read (Readout& out) noexcept;

    int    getNumBands() const noexcept      { return numBands; }
    double getFreqMin() const noexcept       { return freqMin; }
    double getFreqMax() const noexcept       { return freqMax; }
    double getSampleRate() const noexcept    { return fs; }
    int    getBandsPerOctave() const noexcept { return bpo; }

    static constexpr int frameSize = 256;

private:
    template <typename T>
    void processImpl (const T* left, const T* right, int numSamples, ChannelMode mode) noexcept;

    void processFrame (const double* in0, const double* in1, int numChannels, int n) noexcept;
    void publish (bool force) noexcept;
    static double emphasis (int bpo, double f, double fedg) noexcept;

    // design
    double fs = 0.0, freqMin = 0.0, freqMax = 0.0;
    int    bpo = 0, numBands = 0, numFilters = 0;
    alignas (32) double k0[kMaxFilters] {}, k1[kMaxFilters] {}, k2[kMaxFilters] {};
    alignas (32) double emphSq[kMaxFilters] {};

    // audio-thread state
    alignas (32) double z0[2][kMaxFilters] {}, z1[2][kMaxFilters] {};
    double zlp[2] {};
    alignas (32) double accPeak[kMaxFilters] {}, accSum[kMaxFilters] {};
    double accSamples = 0.0;
    bool   pendingReset = true;

    // hand-off
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
    Readout shared;
    bool    sharedClear = true;
    bool    sharedFresh = false;
};

} // namespace malon
