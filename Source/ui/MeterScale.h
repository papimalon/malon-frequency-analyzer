// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Metering scales for the Out meter and the RMS statistic, after Voxengo SPAN's "Metering" selector:
//  DBFS / DBFS.30 / DBFS.15 : no biasing, meter range 60 / 30 / 15 dB.
//  DBFS+3                   : +3.01 dB on RMS (a sine's RMS reads its peak level, AES17); peaks unchanged.
//  K-20 / K-14 / K-12 (+ C) : Bob Katz K-system. Unweighted RMS with a fixed 600 ms window, sine-referenced
//                             (+3.01 dB), shown relative to the K reference: 0 K-20 = -20 dBFS RMS (AES17),
//                             so a -20 dBFS sine reads 0 and digital full scale is +20 at the top.
//                             "C" (calibration) modes use the same biasing; SPAN's guide says no weighting
//                             is used in any K mode. The C label marks a monitor-calibrated listening setup.
//  LUFS-ML / LUFS-SL        : ITU-R BS.1770-4 momentary (400 ms) / short-term (3 s) loudness on the meter.
//  LU-ML / LU-SL            : the same relative to the EBU R128 target: LU = LUFS + 23.
#pragma once
#include <cmath>

namespace malon
{
/** Out-meter / statistics metering scales, like SPAN's "Metering" selector (index = value of "meterScale"). */
enum MeterScaleIndex { msDbfs = 0, msDbfsPlus3, msK20, msK14, msK12, msK20C, msK14C, msK12C, msDbfs30, msDbfs15,
                       msLufsML, msLufsSL, msLuML, msLuSL, msCount };
inline constexpr const char* kMeterScaleNames[msCount] = { "DBFS", "DBFS+3", "K-20", "K-14", "K-12", "K-20 C", "K-14 C", "K-12 C",
                                                          "DBFS.30", "DBFS.15", "LUFS-ML", "LUFS-SL", "LU-ML", "LU-SL" };

struct MeterScaleSpec
{
    double top, bottom;      // meter range in display units
    double redAt;            // display value at/above which the hold tick turns red
    int    tick;             // grid step
    const char* unit;
    bool   loudness;         // meter shows LUFS/LU instead of RMS + peak
    bool   shortTerm;        // loudness: short-term (3 s) instead of momentary (400 ms)
    bool   kWindow;          // RMS from the fixed 600 ms K-system window
    double rmsBias, peakBias;
};

inline constexpr double kSineRmsBias = 3.0103;   // 20*log10(sqrt 2)

inline MeterScaleSpec meterScaleSpec (int i)
{
    auto k = [] (double n, const char* u) { return MeterScaleSpec { n, n - 60.0, n, 4, u, false, false, true, kSineRmsBias + n, n }; };
    switch (i)
    {
        case msDbfsPlus3: return { 0, -60, 0, 6, "dBFS+3", false, false, false, kSineRmsBias, 0 };
        case msK20:  return k (20, "K-20");
        case msK14:  return k (14, "K-14");
        case msK12:  return k (12, "K-12");
        case msK20C: return k (20, "K-20 C");
        case msK14C: return k (14, "K-14 C");
        case msK12C: return k (12, "K-12 C");
        case msDbfs30: return { 0, -30, 0, 3, "dBFS", false, false, false, 0, 0 };
        case msDbfs15: return { 0, -15, 0, 3, "dBFS", false, false, false, 0, 0 };
        case msLufsML: return { 0, -60, 1e9, 6, "LUFS", true, false, false, 0, 0 };
        case msLufsSL: return { 0, -60, 1e9, 6, "LUFS", true, true, false, 0, 0 };
        case msLuML:   return { 9, -36, 1e9, 3, "LU", true, false, false, 23.0, 0 };
        case msLuSL:   return { 9, -36, 1e9, 3, "LU", true, true, false, 23.0, 0 };
        default:       return { 0, -60, 0, 6, "dBFS", false, false, false, 0, 0 };
    }
}

/** Value on the meter for a given scale. rmsFast: 300 ms RMS dBFS, rmsK: 600 ms RMS dBFS, peak: dBTP,
    momentary / shortTerm: LUFS. Returns {bar, peak} in display units (peak = -inf for loudness scales). */
struct MeterValues { double bar, peak; };
inline MeterValues meterValues (const MeterScaleSpec& s, double rmsFast, double rmsK, double peak, double momentary, double shortTerm)
{
    if (s.loudness) return { (s.shortTerm ? shortTerm : momentary) + s.rmsBias, -INFINITY };
    return { (s.kWindow ? rmsK : rmsFast) + s.rmsBias, peak + s.peakBias };
}
} // namespace malon
