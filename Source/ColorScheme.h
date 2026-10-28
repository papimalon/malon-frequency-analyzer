// Malon Frequency Analyzer - Copyright (C) 2026 MUSICA MALON, AGPL-3.0-or-later (see LICENSE)
#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>

namespace malon
{

/** The 8 user-customisable colours (same set as the original analyzer). */
struct ColorScheme
{
    enum Slot { Peak, Hold, Average, GridBorder, GridLine, GridLabel, BackgroundTop, BackgroundBottom, NumSlots };

    std::array<juce::Colour, NumSlots> c;

    static const char* slotName (int i)
    {
        static const char* names[] = { "Peak", "Peak Hold", "Average", "Grid Border", "Grid Line", "Grid Label", "Background A", "Background B" };
        return names[i];
    }

    juce::Colour operator[] (int i) const { return c[(size_t) i]; }

    juce::String toString() const
    {
        juce::StringArray s;
        for (auto& col : c) s.add (col.toString());
        return s.joinIntoString (",");
    }

    static ColorScheme fromString (const juce::String& text, const ColorScheme& fallback)
    {
        auto parts = juce::StringArray::fromTokens (text, ",", "");
        if (parts.size() != NumSlots) return fallback;
        ColorScheme r;
        for (int i = 0; i < NumSlots; ++i) r.c[(size_t) i] = juce::Colour::fromString (parts[i]);
        return r;
    }

    static ColorScheme defaults();
};

struct NamedScheme { const char* name; ColorScheme scheme; };

/** Built-in schemes. Index 0 is the default. All colours are our own. */
inline const std::array<NamedScheme, 5>& builtInSchemes()
{
    using C = juce::Colour;
    static const std::array<NamedScheme, 5> list =
    {{
        // Peak, Hold, Average, GridBorder, GridLine, GridLabel, BgTop, BgBottom
        { "MALON",       {{ C (0xCC2F7BFF), C (0xB8FFFFFF), C (0xF2FF2D55), C (0x2EFFFFFF), C (0x12FFFFFF), C (0x70FFFFFF), C (0xFF0E1118), C (0xFF050609) }} },
        { "Neon Palm",   {{ C (0xCCFF2D55), C (0xB8FFE0E6), C (0xF22F7BFF), C (0x2EFFFFFF), C (0x12FFFFFF), C (0x70FFFFFF), C (0xFF12080D), C (0xFF050304) }} },
        { "Blue Wave",   {{ C (0xCC3C8CFF), C (0xB8BFD9FF), C (0xF2FFFFFF), C (0x30A8C8FF), C (0x14A8C8FF), C (0x78A8C8FF), C (0xFF0A1424), C (0xFF03070F) }} },
        { "Mono",        {{ C (0xCCDADADA), C (0xB8FFFFFF), C (0xF2808080), C (0x30FFFFFF), C (0x12FFFFFF), C (0x70FFFFFF), C (0xFF141414), C (0xFF050505) }} },
        { "Phosphor",    {{ C (0xCC1FD66B), C (0xB8D8FFE6), C (0xF2F5E663), C (0x2EB0FFD0), C (0x12B0FFD0), C (0x70B0FFD0), C (0xFF08130C), C (0xFF020604) }} },
    }};
    return list;
}


inline ColorScheme ColorScheme::defaults() { return builtInSchemes()[0].scheme; }

} // namespace malon
