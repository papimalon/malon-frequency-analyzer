# Third-party notices

## Spectrum Analyzer by Max Mikhailov (Seven Phases) - MIT License

The filter-bank engine (`Source/dsp/FilterBank.*`) and the meter / averager ballistics
(`Source/dsp/Ballistics.h`) are ported from
<https://github.com/seven-phases/spectrum-analyzer> (commit b3638de), which is released under the MIT
License. The code was rewritten as portable C++ (no SSE intrinsics, no Windows code, double precision,
non-blocking hand-off to the UI). The design constants (Q = 2 x bands/octave, edge warp, emphasis,
band layout rules) are kept so the analyzer behaves like the original.

The original user interface, bitmaps, icons, screenshot and colour presets are **not** used. The UI,
colours and branding in this project are new.

The MIT License (MIT)

Copyright (c) 2008-2015 Max Mikhailov

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

## JUCE 8

<https://juce.com> - used under the GNU AGPL v3 option of the JUCE 8 licence. This is why Malon Frequency
Analyzer is itself released under the AGPL v3 with source code available.

## Steinberg VST 3 SDK (bundled with JUCE)

MIT licensed (VST 3 SDK 3.8). "VST" is a trademark of Steinberg Media Technologies GmbH. The product name
does not contain it. "Audio Units" is a trademark of Apple Inc.
