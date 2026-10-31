# v1.1 verification report

Produced by `Tests/verify/verify.py` (renders through the plugin's own DSP via `Tests/verify/VerifyRender.cpp`). Voxengo SPAN could not be run headless on the build box (Linux, no Windows/macOS plugin host), so results are checked against published references (ITU-R BS.1770-4, EBU Tech 3341, sine math) and independent numpy / scipy / pyloudnorm measurements. Slope expectations use SPAN's documented behaviour (pink noise flat at 3 dB/oct).

| Test | Metric | Ours | Expected | Tol | Result | Reference |
|---|---|---|---|---|---|---|
| 1 kHz sine -20 dBFS | RMS L (integrated) | -23.01 | -23.01 | ±0.05 | PASS | math: A/sqrt2 |
| 1 kHz sine -20 dBFS | RMS R (integrated) | -23.01 | -23.01 | ±0.05 | PASS | math |
| 1 kHz sine -20 dBFS | RMS 300 ms | -23.01 | -23.01 | ±0.05 | PASS | math |
| 1 kHz sine -20 dBFS | Sample peak dBFS | -20.00 | -20.00 | ±0.05 | PASS | math |
| 1 kHz sine -20 dBFS | True peak dBTP | -20.00 | -20.00 | ±0.1 | PASS | BS.1770-4 |
| 1 kHz sine -20 dBFS | LUFS integrated | -19.99 | -20.00 | ±0.1 | PASS | Tech 3341 (1k stereo: LUFS = dBFS) |
| 1 kHz sine -20 dBFS | Spectrum 1k band, peak (slope 0) | -20.23 | -20.00 | ±0.5 | PASS | sine reads its amplitude |
| 1 kHz sine -20 dBFS | Average line vs bar (1k band) | 0.00 | 0.00 | ±0.5 | PASS | avg = long-term bar level |
| 1 kHz sine -20 dBFS | Correlation (L=R) | 1.00 | 1.00 | ±0.01 | PASS | Pearson |
| EBU 3341 #1: 1k -23 dBFS | LUFS integrated | -22.99 | -23.00 | ±0.1 | PASS | EBU Tech 3341 |
| EBU 3341 #2: 1k -33 dBFS | LUFS integrated | -32.99 | -33.00 | ±0.1 | PASS | EBU Tech 3341 |
| fs/4 sine 45°, A=0.5 | Sample peak dBFS | -9.03 | -9.03 | ±0.05 | PASS | math |
| fs/4 sine 45°, A=0.5 | True peak dBTP | -6.02 | -6.02 | ±0.3 | PASS | BS.1770-4 (inter-sample) |
| Sine sweep -6 dBFS | RMS L (integrated) | -9.01 | -9.03 | ±0.05 | PASS | math |
| Sine sweep -6 dBFS | Sample peak dBFS | -6.00 | -6.00 | ±0.05 | PASS | math |
| Sine sweep -6 dBFS | True peak dBTP | -5.99 | -6.00 | ±0.1 | PASS | math (sine amplitude) |
| Sine sweep -6 dBFS | LUFS integrated | -5.05 | -5.09 | ±0.2 | PASS | pyloudnorm (BS.1770) |
| Sine sweep -6 dBFS | Max-hold flatness 40 Hz-16 kHz, slope 3 (spread dB) | 0.97 | 0.00 | ±1.5 | PASS | flat response |
| Sine sweep -6 dBFS | Max-hold level, mean 40 Hz-16 kHz (dB) | -5.50 | -6.00 | ±1 | PASS | sine reads its amplitude |
| Pink noise (L/R indep.) | RMS L (integrated) | -20.00 | -20.00 | ±0.05 | PASS | numpy |
| Pink noise (L/R indep.) | LUFS integrated | -19.39 | -19.43 | ±0.2 | PASS | pyloudnorm |
| Pink noise (L/R indep.) | Band power slope, raw bank (dB/oct) | -0.00 | 0.00 | ±0.2 | PASS | pink = equal power/oct |
| Pink noise (L/R indep.) | Avg line slope at Slope 3 (dB/oct) | 0.66 | 0.00 | ±0.75 | PASS | SPAN: pink flat at 3 |
| Pink noise (L/R indep.) | Avg line slope at Slope 4.5 (dB/oct) | 2.16 | 1.50 | ±0.75 | PASS | SPAN: +1.5 dB/oct |
| Pink noise (L/R indep.) | Avg line slope at Slope 0 (dB/oct) | -2.34 | -3.00 | ±0.75 | PASS | SPAN: -3 dB/oct |
| Pink noise (L/R indep.) | Correlation (uncorrelated) | 0.03 | 0.00 | ±0.1 | PASS | Pearson |
| Pink noise (L/R indep.) | Bars minus Average line (time-mean dB) | -0.00 | 0.00 | ±1 | PASS | avg sits in the bars |
| Noise L = R (mono) | Correlation | 1.00 | 1.00 | ±0.01 | PASS | Pearson |
| Noise L = -R | Correlation | -1.00 | -1.00 | ±0.01 | PASS | Pearson |
| Full mix (dembow premaster) | RMS L (integrated) | -10.83 | -10.83 | ±0.05 | PASS | numpy |
| Full mix (dembow premaster) | RMS R (integrated) | -11.21 | -11.21 | ±0.05 | PASS | numpy |
| Full mix (dembow premaster) | True peak dBTP | 8.58 | 8.58 | ±0.2 | PASS | scipy 8x |
| Full mix (dembow premaster) | LUFS integrated | -8.89 | -8.93 | ±0.2 | PASS | pyloudnorm |
| Full mix (dembow premaster) | Bars minus v1.0 power Average 1-16 kHz (old) | 7.93 | - | - | INFO | v1.0 method, for comparison |
| Full mix (dembow premaster) | Bars minus Average 1-16 kHz (new) | -0.68 | 0.00 | ±1 | PASS | avg sits in the bars |

34/34 pass
