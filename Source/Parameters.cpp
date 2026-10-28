// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// Ranges and defaults follow the Seven Phases analyzer's settings table.
#include "Parameters.h"

namespace malon
{
using namespace juce;

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };
    auto choice = [&] (const char* s, const char* name, StringArray items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (id (s), name, items, def)); };
    auto toggle = [&] (const char* s, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (id (s), name, def)); };
    auto number = [&] (const char* s, const char* name, float lo, float hi, float step, float def, const char* unit)
    {
        l.add (std::make_unique<AudioParameterFloat> (id (s), name, NormalisableRange<float> (lo, hi, step), def,
               AudioParameterFloatAttributes().withLabel (unit)));
    };

    const StringArray show { "Bars", "Curve", "Curve Fill" };

    choice ("channel", "Channel", { "Left", "Right", "Both", "Mix" }, 2);

    toggle ("peakOn", "Peak", true);
    number ("peakDecay", "Peak Decay", 1, 60, 1, 15, "dB/s");

    toggle ("avgOn", "Average", true);
    number ("avgTime", "Average Time", 100, 20000, 100, 3000, "ms");
    choice ("avgShow", "Average Show As", show, 1);
    number ("avgSize", "Average Size", 1, 4, 1, 3, "px");
    number ("avgSlope", "Average Slope", -6, 12, 1, 0, "dB/oct");

    toggle ("holdOn", "Peak Hold", true);
    toggle ("holdInfinite", "Hold Infinite", false);
    number ("holdTime", "Hold Time", 0, 20000, 100, 2000, "ms");
    number ("holdDecay", "Hold Decay", 1, 60, 1, 3, "dB/s");
    choice ("holdShow", "Hold Show As", show, 1);
    number ("holdSize", "Hold Size", 1, 4, 1, 2, "px");

    number ("ceiling", "Ceiling", -80, 20, 1, 3, "dB");
    number ("range", "Range", 10, 120, 1, 51, "dB");
    number ("grid", "Grid", 2, 12, 1, 5, "dB");
    choice ("freqGrid", "Frequency Grid", { "Decade", "Octave" }, 0);
    choice ("bands", "Bands per Octave", { "3", "4", "6" }, 2);

    // pro metering (true peak, RMS, loudness) - a separate, switchable section
    toggle ("meterOn", "Meters", true);
    choice ("rmsWindow", "RMS Window", { "100 ms", "300 ms", "1 s", "3 s" }, 1);

    return l;
}

} // namespace malon
