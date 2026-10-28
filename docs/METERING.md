# Pro metering

The strip under the analyzer (button **Meters** or key `M`) shows four things. Everything is measured from
the audio that passes through; the audio itself is never changed. With **Meters** off nothing is measured
(no CPU) and the analyzer fills the window. Switching it on starts from a clean slate.

| Element | What it shows |
|---|---|
| **TRUE PEAK** | Highest inter-sample peak since the last reset, in dBTP (red above 0). The thin bar is the live level (-60 ... +3 dBTP, falls 24 dB/s); the white tick is the held maximum, the faint line is 0 dBTP. Below: held value per channel. |
| **RMS** | Combined RMS of both channels in dBFS (a full-scale sine reads -3.01) and per channel. Window: 100 ms, 300 ms (default), 1 s or 3 s. |
| **LOUDNESS** | Integrated loudness (gated, since the last reset) in LUFS, plus momentary (400 ms), short-term (3 s) and loudness range (LU). |
| **CLIP** + **Reset** | The light latches when any sample reaches full scale (0 dBFS). **Reset** (or clicking the light) clears held peaks, integrated loudness, loudness range and the clip light. |

The meter settings (Meters on/off, RMS window) are plugin parameters and are saved with the project.
Mono tracks are measured as one channel.

## How it is measured

* **True peak**: ITU-R BS.1770-4 Annex 2. 4x oversampling with a 4-phase, 24-taps-per-phase windowed-sinc
  interpolator; the maximum of the original samples and the three interpolated points per sample. The
  interpolator itself is flat to < 0.02 dB up to 0.42 x the sample rate and never reads high. Like any 4x true-peak
  meter it can read low when a high-frequency sine peaks between grid points (worst case about 0.3 dB for a
  16 kHz sine at 48 kHz, 0.5 dB at 20 kHz; measured worst over 1-20 kHz and four phases: 0.21 dB).
* **K-weighting**: the two BS.1770-4 biquads, designed for the actual sample rate (the 48 kHz coefficients match the
  standard's table exactly). `LUFS = -0.691 + 10 log10(sum of channel mean squares)`; left and right have weight 1.0.
* **Momentary / short-term**: 400 ms and 3 s rectangular windows, updated every 10 ms.
* **Integrated**: 400 ms blocks with 75 % overlap, absolute gate -70 LUFS, relative gate -10 LU (EBU R128).
* **Loudness range**: EBU Tech 3342: 3 s short-term values every 100 ms, absolute gate -70 LUFS, relative gate -20 LU,
  difference between the 95th and 10th percentile. Shown after 3 s of audio.
* **RMS**: running mean square over the chosen window (1 ms bins), per channel; combined = mean of the two channels.

The code is in `Source/dsp/LevelMeter.{h,cpp}`: no JUCE, no allocation in the audio thread, results published
to the UI through atomics. It was written from the published specifications (BS.1770-4, EBU Tech 3341 / 3342).

## Tests (`MalonMeterTests`, `MalonPluginTests`)

See the table in the README. The EBU test signals are re-created from the descriptions in Tech 3341 / 3342
(the official audio files could not be downloaded in the build environment), so tests 6-8 (5.0 channel layout,
authentic programme material) and Tech 3342 tests 5-6 are **not** covered. Please run those with the official
files when you can.
