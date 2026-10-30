// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Input hygiene for the analysis paths. A single NaN/inf from a host or a broken plugin upstream would
// otherwise poison every recursive filter state forever. The check works on the bit pattern, so it
// still works when a compiler is told to assume finite maths.
#pragma once
#include <cstdint>
#include <cstring>

namespace malon
{
/** Non-finite -> 0, absurd values beyond +-1000 (+60 dBFS, only produced by a broken upstream) -> 0,
    denormal-scale values -> 0. */
inline double sanitizeSample (double x) noexcept
{
    std::uint64_t b; std::memcpy (&b, &x, sizeof b);
    if (((b >> 52) & 0x7ffu) == 0x7ffu) return 0.0;            // NaN or +-inf
    if (x > 1.0e3 || x < -1.0e3) return 0.0;
    if (x > -1.0e-30 && x < 1.0e-30) return 0.0;               // below -600 dB: denormal junk
    return x;
}

/** Any display value below this is shown as -inf. */
constexpr double kDisplayFloorDb = -150.0;
} // namespace malon
