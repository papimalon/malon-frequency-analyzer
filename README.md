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
* **Pro meters** (switch on/off with the *Meters* button or `M`): **true peak** (4x oversampled, dBTP, held),
  **RMS** (per channel and combined, 100 ms / 300 ms / 1 s / 3 s window), **loudness** per ITU-R BS.1770-4 /
  EBU R128 (momentary, short-term, gated integrated, loudness range) and a latching **clip** light with one
  **Reset**. Details and test results: [docs/METERING.md](docs/METERING.md).

A sine of amplitude 1.0 (0 dBFS peak) reads 0 dB on the peak *and* average displays.

## Accuracy (measured by the unit tests)

* A sine at any band centre reads within about +/-1.2 dB of its true level (1/3, 1/4, 1/6 octave, 44.1-192 kHz);
  the lowest and highest bands read lower because only half of their band exists. Same design constants as the original.
* Log sweeps give every band the same maximum (flat within 1.3 dB) and the maxima arrive in frequency order.
* White noise rises 3.1 dB/octave (theory 3.01); pink noise is flat within 0.8 dB from 100 Hz to 10 kHz.
* The *Slope* control tilts the average in true dB/octave (the original's slope was about half of that).

## Pro metering accuracy (measured against the published references)

| Check | Reference | Result |
|---|---|---|
| 997 Hz, -20 dBFS, stereo | M = S = I = -20.0 LUFS, RMS -23.01 dBFS | -20.00 LUFS, -23.01 dBFS, true peak -20.00 dBTP |
| 1 kHz, -23 dBFS stereo, 32-192 kHz | -23.0 LUFS | -22.98 ... -23.02 |
| EBU Tech 3341 #1-5, 9-14 (loudness) | within +/-0.1 LU | worst deviation 0.04 LU |
| EBU Tech 3342 #1-4 (loudness range) | within +/-1 LU | 10.0 / 5.0 / 20.0 / 15.0 LU exactly |
| EBU Tech 3341 #15-23 (true peak) | 0 dBTP +0.2 / -0.4 | -6.02 and +2.98 dBTP at 44.1 / 48 / 96 kHz; #20-23 within -0.14 ... -0.11 dBTP |
| Streaming meter vs. an independent batch BS.1770 + EBU R128 implementation, 90 s | identical | I -20.483 / -20.483 LUFS, LRA 22.915 / 22.915 LU |
| Silence | nothing shown | -inf everywhere, clip light off |

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
* `MalonMeterTests` - pure C++, no JUCE: the metering against BS.1770-4 and the EBU Tech 3341 / 3342 test signals
  (see above), inter-sample peaks, mono vs stereo, silence, reset and clip latch, RMS windows, other sample rates.
  Takes about 40 s because it also runs slow reference implementations.
* CI runs [pluginval](https://github.com/Tracktion/pluginval) on the VST3 (Windows, macOS, Linux) and Apple's
  `auval` on the AU.

## Layout

```
Source/dsp/            FilterBank (port of the Seven Phases engine), Ballistics, DisplayModel,
                       LevelMeter (true peak, RMS, BS.1770 loudness)  (no JUCE)
Source/ui/             SpectrumView, MeterStrip, SettingsPanel, look and feel, credits
Source/Plugin*.cpp     processor, editor, plugin entry
Tests/                 DspTests.cpp, MeterTests.cpp, PluginTests.cpp
.github/workflows/     build.yml (Windows + macOS + Linux, VST3 + AU)
```

## Licence and credits

Copyright (C) 2026 MUSICA MALON. Free software under the **GNU AGPL v3** (see `LICENSE`).

The DSP engine is based on *Spectrum Analyzer* by Max Mikhailov (Seven Phases), MIT licensed,
Copyright (c) 2008-2015 Max Mikhailov (see `LICENSE-SEVEN-PHASES-MIT`, `THIRD_PARTY_NOTICES.md`, and the plugin's
About box). The interface, colours and branding are original. Built with JUCE (AGPL v3).
"VST" is a trademark of Steinberg Media Technologies GmbH.
