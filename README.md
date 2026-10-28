# Malon Frequency Analyzer

A free, open-source, real-time **filter-bank spectrum analyzer** plugin (VST3 on Windows, VST3 + AU on macOS,
VST3 on Linux for testing). Not FFT: a bank of parallel band-pass filters in 1/3, 1/4 or 1/6 octave, so you get
instant attack and tidy low-frequency resolution, like a classic hardware RTA.

* **Peak**, **peak hold** and **average** shown at the same time
* Each of hold and average as **bars**, **curve** or **curve fill**
* Bands per octave: 3 / 4 / 6 (30 / 40 / 60 bands), any sample rate (22.05 kHz - 192 kHz tested)
* Ballistics: peak decay 1-60 dB/s, average time 100-20 000 ms, hold time / infinite / decay
* Channel: left / right / both / mix. Level ceiling, range, grid. Decade or octave frequency grid
* Average slope: -6 ... +12 dB/oct tilt (pivot at the middle of the range)
* 8 customisable colours + built-in schemes, **Save as default**
* Resizable from small to full-screen, `F` = freeze, `R` = reset
* Audio passes through bit-exact. Stereo or mono.

A sine of amplitude 1.0 (0 dBFS peak) reads 0 dB on the peak *and* average displays.

## Accuracy (measured by the unit tests)

* A sine at any band centre reads within about +/-1.2 dB of its true level (1/3, 1/4, 1/6 octave, 44.1-192 kHz);
  the lowest and highest bands read lower because only half of their band exists. Same design constants as the original.
* Log sweeps give every band the same maximum (flat within 1.3 dB) and the maxima arrive in frequency order.
* White noise rises 3.1 dB/octave (theory 3.01); pink noise is flat within 0.8 dB from 100 Hz to 10 kHz.
* The *Slope* control tilts the average in true dB/octave (the original's slope was about half of that).

## Build

Needs CMake 3.22+, a C++17 compiler. JUCE 8.0.8 is fetched automatically (or put it in `third_party/JUCE`).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Plugins appear under `build/MalonFrequencyAnalyzer_artefacts/Release/`. Linux needs the usual JUCE packages
(`libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
libxinerama-dev libxrandr-dev libxrender-dev libcurl4-openssl-dev`). Headless UI tests/screenshots need `xvfb-run`.

## Tests (nobody has to listen)

* `MalonDspTests` - pure DSP, no JUCE: band layout at 8 sample rates, sine at every band centre,
  log sweeps, level scaling, channel modes, white / pink noise slope and flatness, silence, ring-down,
  thread hand-off, ballistics and display model.
* `MalonPluginTests` - the real processor + editor: bit-exact pass-through (mono / stereo, float / double),
  parameter ranges and defaults, state save/restore, end-to-end tone test, and headless PNG screenshots
  of every display mode (`build/screenshots`).
* CI runs [pluginval](https://github.com/Tracktion/pluginval) on the VST3 (Windows, macOS, Linux) and Apple's
  `auval` on the AU.

## Layout

```
Source/dsp/            FilterBank (port of the Seven Phases engine), Ballistics, DisplayModel  (no JUCE)
Source/ui/             SpectrumView, SettingsPanel, look and feel, credits
Source/Plugin*.cpp     processor, editor, plugin entry
Tests/                 DspTests.cpp, PluginTests.cpp
.github/workflows/     build.yml (Windows + macOS + Linux, VST3 + AU)
```

## Licence and credits

Copyright (C) 2026 MUSICA MALON. Free software under the **GNU AGPL v3** (see `LICENSE`).

The DSP engine is based on *Spectrum Analyzer* by Max Mikhailov (Seven Phases), MIT licensed,
Copyright (c) 2008-2015 Max Mikhailov (see `LICENSE-SEVEN-PHASES-MIT`, `THIRD_PARTY_NOTICES.md`, and the plugin's
About box). The interface, colours and branding are original. Built with JUCE (AGPL v3).
"VST" is a trademark of Steinberg Media Technologies GmbH.
