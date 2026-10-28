// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace malon
{
namespace ids
{
    inline constexpr const char* channel      = "channel";
    inline constexpr const char* peakOn       = "peakOn";
    inline constexpr const char* peakDecay    = "peakDecay";
    inline constexpr const char* avgOn        = "avgOn";
    inline constexpr const char* avgTime      = "avgTime";
    inline constexpr const char* avgShow      = "avgShow";
    inline constexpr const char* avgSize      = "avgSize";
    inline constexpr const char* avgSlope     = "avgSlope";
    inline constexpr const char* holdOn       = "holdOn";
    inline constexpr const char* holdInfinite = "holdInfinite";
    inline constexpr const char* holdTime     = "holdTime";
    inline constexpr const char* holdDecay    = "holdDecay";
    inline constexpr const char* holdShow     = "holdShow";
    inline constexpr const char* holdSize     = "holdSize";
    inline constexpr const char* ceiling      = "ceiling";
    inline constexpr const char* range        = "range";
    inline constexpr const char* grid         = "grid";
    inline constexpr const char* freqGrid     = "freqGrid";
    inline constexpr const char* bands        = "bands";
    inline constexpr const char* meterOn      = "meterOn";
    inline constexpr const char* rmsWindow    = "rmsWindow";
}

/** RMS window choices (ms), index = value of the "rmsWindow" parameter. */
inline constexpr int kRmsWindowsMs[4] = { 100, 300, 1000, 3000 };

enum class ShowAs { Bars = 0, Curve = 1, CurveFill = 2 };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace malon
