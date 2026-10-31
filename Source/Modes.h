// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
// SPAN-style spectrum Modes. Voxengo SPAN's modes are FFT presets (block size, overlap, averaging time,
// secondary "max" spectrum). Our spectrum is a constant-Q filter bank, so the same ideas map to:
//   FFT block size     -> bands per octave (resolution; 48/oct ~ SPAN's big blocks in the low end)
//   averaging / speed  -> peak decay (dB/s) and average time
//   secondary spectrum -> the white Hold line (RT max = timed hold, all-time max = infinite hold)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

namespace malon
{
struct ModePreset
{
    int bands; bool peakOn; float peakDecay; int peakShow;
    bool holdOn; bool holdInfinite; float holdTime; float holdDecay;
    bool avgOn; float avgTime;
};

/** index = specMode value (0 = Off is never applied). */
inline const ModePreset& modePreset (int m)
{
    static const ModePreset p[8] = {
        //  bands  peak  decay show  hold   inf    time   hdec  avg   avgT
        {   2,     true, 15,   0,    true,  false, 2000,  3,    true, 3000  },   // Off (unused)
        {   2,     true, 15,   0,    true,  false, 2000,  3,    true, 3000  },   // Default (= factory settings)
        {   2,     true, 6,    0,    true,  false, 3000,  2,    true, 6000  },   // Slower: calmer, longer average
        {   5,     true, 15,   2,    true,  false, 2000,  3,    true, 3000  },   // High Res: 48 bands/oct, filled curve
        {   5,     true, 15,   2,    true,  false, 2000,  3,    true, 3000  },   // 96 kHz: same resolution (bank is rate-independent)
        {   4,     true, 4,    2,    false, false, 2000,  3,    true, 10000 },   // Master: slow, smooth tonal balance
        {   4,     false,15,   2,    false, false, 2000,  3,    true, 5000  },   // Average: long-term average only
        {   4,     false,15,   2,    true,  true,  2000,  3,    true, 5000  },   // Avg+Max: average + all-time max
    };
    return p[juce::jlimit (0, 7, m)];
}

/** Parameters a Mode controls (changing one by hand -> Mode Off). */
inline const juce::StringArray& modeParamIds()
{
    static const juce::StringArray s { ids::bands, ids::peakOn, ids::peakDecay, ids::peakShow, ids::holdOn,
                                       ids::holdInfinite, ids::holdTime, ids::holdDecay, ids::avgOn, ids::avgTime };
    return s;
}

inline float modeValue (const ModePreset& p, const juce::String& id)
{
    if (id == ids::bands) return (float) p.bands;
    if (id == ids::peakOn) return p.peakOn ? 1.f : 0.f;
    if (id == ids::peakDecay) return p.peakDecay;
    if (id == ids::peakShow) return (float) p.peakShow;
    if (id == ids::holdOn) return p.holdOn ? 1.f : 0.f;
    if (id == ids::holdInfinite) return p.holdInfinite ? 1.f : 0.f;
    if (id == ids::holdTime) return p.holdTime;
    if (id == ids::holdDecay) return p.holdDecay;
    if (id == ids::avgOn) return p.avgOn ? 1.f : 0.f;
    return p.avgTime;
}

inline bool modeMatches (juce::AudioProcessorValueTreeState& s, int m)
{
    if (m <= 0) return true;
    const auto& p = modePreset (m);
    for (auto& id : modeParamIds())
        if (std::abs (s.getRawParameterValue (id)->load() - modeValue (p, id)) > 1e-3f) return false;
    return true;
}

inline void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
{
    if (auto* prm = s.getParameter (id))
    {
        const float n = prm->convertTo0to1 (v);
        if (std::abs (prm->getValue() - n) > 1e-6f)
        { prm->beginChangeGesture(); prm->setValueNotifyingHost (n); prm->endChangeGesture(); }
    }
}

inline void applyMode (juce::AudioProcessorValueTreeState& s, int m)
{
    if (m <= 0) return;
    const auto& p = modePreset (m);
    for (auto& id : modeParamIds()) setParam (s, id, modeValue (p, id));
}
} // namespace malon
