// Malon Frequency Analyzer - see FilterBank.h for licence and attribution.
#include "FilterBank.h"
#include "Sanitize.h"
#include <algorithm>
#include <thread>
#include <vector>

namespace malon
{

namespace
{
    constexpr double kPi   = 3.14159265358979323846;
    constexpr double kLn2  = 0.69314718055994530942;
    constexpr double kAdn  = 1e-10;   // anti-denormal offset (-200 dB), as in the original

    struct SpinGuard
    {
        explicit SpinGuard (std::atomic_flag& f) : flag (f) { while (flag.test_and_set (std::memory_order_acquire)) std::this_thread::yield(); }
        ~SpinGuard() { flag.clear (std::memory_order_release); }
        std::atomic_flag& flag;
    };
}

FilterBank::FilterBank() { reset(); }

double FilterBank::emphasis (int bpo, double f, double fedg) noexcept
{
    const double base = 1.025;           // +0.21 dB
    f = f * (1.0 / fedg);
    if (bpo == 6)
        return base + 2.2 * f * f * f - 1.25 * f * f * f * f * f;
    return base + f * f;
}

void FilterBank::prepare (double sampleRate, int bandsPerOctave)
{
    fs  = sampleRate;
    bpo = bandsPerOctave;

    const double fmin = 22.0;
    const double fmax = 29000.0;

    double f = 15.625;
    const double k = std::exp (kLn2 * (1.0 / bpo));
    while (f < fmin)
        f *= k;
    freqMin = f;

    const double fanc = 1000.0;
    const double fedg = 0.94 * 0.5 * fs;

    const int    i   = int (0.5 + std::log (fedg / (fanc * std::sqrt (k))) / std::log (k));
    const double fli = 1.0 / (fanc * std::pow (k, i));                    // 1 / flast
    const double r   = (1.0 - (fedg / std::sqrt (k)) * fli) * fli * fli;

    double ferr = 1.0;
    switch (bpo)
    {
        case 3: ferr = 0.904; break;
        case 4: ferr = 0.905; break;
        case 6: ferr = 0.909; break;
        case 12: ferr = 0.910; break;   // Hi-Res: calibrated so band centres read 0 dB up to ~20 kHz
        case 24: ferr = 0.908; break;
        case 48: ferr = 0.908; break;
        default: break;
    }

    std::memset (k0, 0, sizeof (k0)); std::memset (k1, 0, sizeof (k1)); std::memset (k2, 0, sizeof (k2));
    std::fill (std::begin (emphSq), std::end (emphSq), 1.0);

    int n = 0;
    numBands = 0;
    freqMax  = f;
    double ff = (f / std::sqrt (k)) * (1.0 - r * f * f);
    for (;;)
    {
        if (n >= kMaxFilters) break;   // safety; never reached for supported rates

        // two-pole resonant low-pass: centre ff, Q = 2 * bandsPerOctave
        const double q = 2.0 * bpo;
        const double w = 2.0 * kPi * ff / fs;
        const double y = std::sin (w) / (q * (1.0 + std::cos (w * ferr)));
        const double a = 1.0 / (1.0 + y);
        k2[n] = a * (y - 1.0);
        k1[n] = a * 2.0 * std::cos (w);
        k0[n] = (0.5 / q) * (1.0 - k1[n] - k2[n]);

        const double e = emphasis (bpo, ff, fedg);
        emphSq[n] = e * e;                 // we use squared peak and average

        ff = f * std::sqrt (k) * (1.0 - r * f * f);
        if ((ff > (fedg + 1.0)) || (ff > fmax) || (n + 2 >= kMaxFilters))
            break;

        numBands = ++n;
        freqMax  = f;
        f *= k;
    }
    numFilters = numBands + 1;
    reset();

    // Publish the new layout right away (so a UI that polls before any audio has arrived
    // already knows the band count). Never wait: if the UI holds the lock, publish() will do it.
    if (! lock.test_and_set (std::memory_order_acquire))
    {
        shared = Readout();
        shared.numBands = numBands; shared.freqMin = freqMin; shared.freqMax = freqMax;
        shared.bandsPerOctave = double (bpo); shared.sampleRate = fs;
        sharedClear = true; sharedFresh = false;
        lock.clear (std::memory_order_release);
    }
}

void FilterBank::reset()
{
    std::memset (z0, 0, sizeof (z0));
    std::memset (z1, 0, sizeof (z1));
    zlp[0] = zlp[1] = 0.0;
    std::memset (accPeak, 0, sizeof (accPeak));
    std::memset (accSum,  0, sizeof (accSum));
    accSamples   = 0.0;
    pendingReset = true;   // next publish replaces (rather than merges into) the shared data
}

void FilterBank::processFrame (const double* in0, const double* in1, int numChannels, int n) noexcept
{
    const int nf = numFilters;
    alignas (32) double x[kMaxFilters];

    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            const double raw = (c == 0 ? in0[i] : in1[i]) + kAdn;
            const double s   = raw + zlp[c];       // zero at Nyquist ("ZeroLP")
            zlp[c] = raw;

            double* const a0 = z0[c];
            double* const a1 = z1[c];

            // two-pole resonators; the *delayed* output (state before update) is used,
            // exactly like the original (the delay is identical for all bands)
            for (int j = 0; j < nf; ++j)
            {
                const double out = a0[j];
                x[j]  = out;
                a0[j] = s * k0[j] + out * k1[j] + a1[j] * k2[j];
                a1[j] = out;
            }

            // band = difference of neighbouring low-passes
            {
                const double y = x[0];
                const double y2 = y * y;
                accPeak[0] = std::max (accPeak[0], y2);
                accSum[0] += y2;
            }
            for (int j = 1; j < nf; ++j)
            {
                const double y  = x[j] - x[j - 1];
                const double y2 = y * y;
                accPeak[j] = std::max (accPeak[j], y2);
                accSum[j] += y2;
            }
        }
    }
    accSamples += double (n) * numChannels;
}

template <typename T>
void FilterBank::processImpl (const T* left, const T* right, int numSamples, ChannelMode mode) noexcept
{
    if (numFilters == 0 || left == nullptr) return;
    if (right == nullptr) right = left;

    double bufA[frameSize], bufB[frameSize];
    auto S = [] (T v) noexcept { return sanitizeSample (double (v)); };
    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (frameSize, numSamples - pos);
        switch (mode)
        {
            case ChannelMode::Left:
                for (int i = 0; i < n; ++i) bufA[i] = S (left[pos + i]);
                processFrame (bufA, nullptr, 1, n);
                break;
            case ChannelMode::Right:
                for (int i = 0; i < n; ++i) bufA[i] = S (right[pos + i]);
                processFrame (bufA, nullptr, 1, n);
                break;
            case ChannelMode::Mix:
            case ChannelMode::Mid:          // mid = (L + R) / 2: what's in the centre
                for (int i = 0; i < n; ++i) bufA[i] = 0.5 * (S (left[pos + i]) + S (right[pos + i]));
                processFrame (bufA, nullptr, 1, n);
                break;
            case ChannelMode::Side:         // side = (L - R) / 2: what's only on the sides
                for (int i = 0; i < n; ++i) bufA[i] = 0.5 * (S (left[pos + i]) - S (right[pos + i]));
                processFrame (bufA, nullptr, 1, n);
                break;
            case ChannelMode::Both:
            default:
                for (int i = 0; i < n; ++i) { bufA[i] = S (left[pos + i]); bufB[i] = S (right[pos + i]); }
                processFrame (bufA, bufB, 2, n);
                break;
        }
        pos += n;
        publish (false);
    }
}

void FilterBank::process (const float* l, const float* r, int n, ChannelMode m) noexcept  { processImpl (l, r, n, m); }
void FilterBank::process (const double* l, const double* r, int n, ChannelMode m) noexcept { processImpl (l, r, n, m); }

void FilterBank::publish (bool) noexcept
{
    // Never block the audio thread: if the UI is reading right now, keep accumulating
    // locally and merge on a later frame.
    if (lock.test_and_set (std::memory_order_acquire))
        return;

    if (pendingReset || sharedClear)
    {
        for (int j = 0; j < numFilters; ++j)
        {
            shared.peakSq[j] = accPeak[j] * emphSq[j];
            shared.sumSq[j]  = accSum[j]  * emphSq[j];
        }
        for (int j = numFilters; j < kMaxFilters; ++j) shared.peakSq[j] = shared.sumSq[j] = 0.0;
        shared.samples = accSamples;
    }
    else
    {
        for (int j = 0; j < numFilters; ++j)
        {
            shared.peakSq[j] = std::max (shared.peakSq[j], accPeak[j] * emphSq[j]);
            shared.sumSq[j] += accSum[j] * emphSq[j];
        }
        shared.samples += accSamples;
    }
    shared.numBands = numBands;
    shared.freqMin = freqMin;
    shared.freqMax = freqMax;
    shared.bandsPerOctave = double (bpo);
    shared.sampleRate = fs;
    sharedClear = false;
    sharedFresh = true;
    pendingReset = false;

    std::memset (accPeak, 0, sizeof (accPeak));
    std::memset (accSum,  0, sizeof (accSum));
    accSamples = 0.0;

    lock.clear (std::memory_order_release);
}

bool FilterBank::read (Readout& out) noexcept
{
    SpinGuard g (lock);
    out = shared;                       // layout is always valid; data only if fresh
    if (! sharedFresh)
    {
        out.samples = 0.0;
        out.peakSq.fill (0.0);
        out.sumSq.fill (0.0);
        return false;
    }
    sharedClear = true;
    sharedFresh = false;
    return true;
}

} // namespace malon
