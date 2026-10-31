# v1.1 (build 2) verification report: HI-RES 48/oct, Modes, metering scales

Produced by `Tests/verify/verify.py build/MalonVerifyRender` (renders through the plugin's own DSP). Voxengo SPAN cannot run on the Linux build box, so values are checked against published references: ITU-R BS.1770-4, EBU Tech 3341, Bob Katz's K-system (sine-referenced RMS, 600 ms, 0 K-20 = -20 dBFS), SPAN's user guide (Metering selector), sine math and numpy/scipy/pyloudnorm.

Notes:
- SPAN's guide says K 'C' modes are calibration modes and that no weighting is used in any K mode, so K-20 C / K-14 C / K-12 C use the same biasing as K-20 / K-14 / K-12.
- DBFS+3 adds 3.01 dB to RMS only (peaks unchanged), as in SPAN's guide.
- Average line: the bars are peak envelopes, and on noise a peak envelope sits higher above the true power the wider the band (constant-Q bands widen with frequency), which tilted the long-term average +0.7 (6/oct) .. +0.9 (48/oct) dB/oct on pink noise. The average now removes that measured bias (one curve for every resolution, by band bandwidth), scaled by how noise-like the band currently is (rms of poll-to-poll bar changes), so steady tones are untouched. Pink noise is flat again within ±0.3 dB/oct (tight tolerance restored), and tones at 60 Hz, 1 kHz and 8 kHz keep the average on the bar.

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
| Pink noise (L/R indep.) | Avg line slope at Slope 3 (dB/oct) | -0.01 | 0.00 | ±0.3 | PASS | SPAN: pink flat at 3 |
| Pink noise (L/R indep.) | Avg line slope at Slope 4.5 (dB/oct) | 1.49 | 1.50 | ±0.3 | PASS | SPAN: +1.5 dB/oct |
| Pink noise (L/R indep.) | Avg line slope at Slope 0 (dB/oct) | -3.01 | -3.00 | ±0.3 | PASS | SPAN: -3 dB/oct |
| Pink noise (L/R indep.) | Correlation (uncorrelated) | 0.03 | 0.00 | ±0.1 | PASS | Pearson |
| Pink noise (L/R indep.) | Bars minus Average line (time-mean dB) | -0.26 | 0.00 | ±1 | PASS | avg sits in the bars |
| Noise L = R (mono) | Correlation | 1.00 | 1.00 | ±0.01 | PASS | Pearson |
| Noise L = -R | Correlation | -1.00 | -1.00 | ±0.01 | PASS | Pearson |
| Full mix (dembow premaster) | RMS L (integrated) | -10.83 | -10.83 | ±0.05 | PASS | numpy |
| Full mix (dembow premaster) | RMS R (integrated) | -11.21 | -11.21 | ±0.05 | PASS | numpy |
| Full mix (dembow premaster) | True peak dBTP | 8.58 | 8.58 | ±0.2 | PASS | scipy 8x |
| Full mix (dembow premaster) | LUFS integrated | -8.89 | -8.93 | ±0.2 | PASS | pyloudnorm |
| Full mix (dembow premaster) | Bars minus v1.0 power Average 1-16 kHz (old) | 7.93 | - | - | INFO | v1.0 method, for comparison |
| Full mix (dembow premaster) | Bars minus Average 1-16 kHz (new) | 0.86 | 0.00 | ±1 | PASS | avg sits in the bars |
| Metering: 1k sine -20 dBFS | DBFS meter value | -23.01 | -23.01 | ±0.1 | PASS | math |
| Metering: 1k sine -20 dBFS | DBFS+3 meter value | -20.00 | -20.00 | ±0.1 | PASS | math |
| Metering: 1k sine -20 dBFS | K-20 meter value | 0.00 | 0.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | K-14 meter value | -6.00 | -6.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | K-12 meter value | -8.00 | -8.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | K-20 C meter value | 0.00 | 0.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | K-14 C meter value | -6.00 | -6.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | K-12 C meter value | -8.00 | -8.00 | ±0.1 | PASS | Katz K-system (-20 dBFS sine = 0 K-20) |
| Metering: 1k sine -20 dBFS | DBFS.30 meter value | -23.01 | -23.01 | ±0.1 | PASS | math |
| Metering: 1k sine -20 dBFS | DBFS.15 meter value | -23.01 | -23.01 | ±0.1 | PASS | math |
| Metering: 1k sine -20 dBFS | LUFS-ML meter value | -19.99 | -20.00 | ±0.1 | PASS | BS.1770-4 / R128 |
| Metering: 1k sine -20 dBFS | LUFS-SL meter value | -19.99 | -20.00 | ±0.1 | PASS | BS.1770-4 / R128 |
| Metering: 1k sine -20 dBFS | LU-ML meter value | 3.01 | 3.00 | ±0.1 | PASS | BS.1770-4 / R128 |
| Metering: 1k sine -20 dBFS | LU-SL meter value | 3.01 | 3.00 | ±0.1 | PASS | BS.1770-4 / R128 |
| Metering: 1k sine -20 dBFS | K-20 peak (dBFS -20 -> K units) | 0.00 | 0.00 | ±0.1 | PASS | K-20: 0 dBFS = +20 |
| Metering: 1k sine -20 dBFS | K-20 meter top (= 0 dBFS) | 20.00 | 20.00 | ±0.01 | PASS | Katz |
| Metering: 1k sine -20 dBFS | K-14 meter top (= 0 dBFS) | 14.00 | 14.00 | ±0.01 | PASS | Katz |
| Metering: 1k sine -20 dBFS | DBFS.30 / DBFS.15 range | -30.15 | -30.15 | ±0.01 | PASS | SPAN guide (30 / 15 dB range) |
| Metering: EBU 3341 #3/#4 (1k -23 dBFS) | LU-ML | 0.01 | 0.00 | ±0.1 | PASS | EBU Tech 3341: momentary = -23 LUFS = 0 LU |
| Metering: EBU 3341 #3/#4 (1k -23 dBFS) | LU-SL | 0.01 | 0.00 | ±0.1 | PASS | EBU Tech 3341: short-term = 0 LU |
| Metering: pink noise -23.01 dBFS RMS | K-20 meter value | 0.30 | 0.00 | ±0.6 | PASS | Katz: -20 dBFS (AES17) pink = 0 K-20 |
| Metering: pink noise -23.01 dBFS RMS | LUFS-SL vs pyloudnorm short-term | -21.69 | -21.55 | ±0.6 | PASS | pyloudnorm, last 3 s (L=R approx) |
| HI-RES 48/oct: A1 55 Hz + A#1 58.27 Hz | Separate peaks between 50 and 64 Hz | 2.00 | 2.00 | ±0 | PASS | semitone apart = 4 bands at 48/oct |
| HI-RES 48/oct: 1 kHz sine -20 dBFS | Peak band level (slope 0) | -20.25 | -20.00 | ±0.5 | PASS | sine reads its amplitude |
| HI-RES 48/oct: pink noise | Band power slope, raw bank (dB/oct) | 0.04 | 0.00 | ±0.2 | PASS | pink = equal power/oct |
| HI-RES 48/oct: pink noise | Avg line slope at Slope 3 (dB/oct) | -0.02 | 0.00 | ±0.3 | PASS | SPAN: pink flat at 3 |
| HI-RES 48/oct: 60 Hz sine -20 dBFS | Average line vs bar | 0.03 | 0.00 | ±0.5 | PASS | avg = long-term bar level |
| HI-RES 48/oct: 8000 Hz sine -20 dBFS | Average line vs bar | 0.00 | 0.00 | ±0.5 | PASS | avg = long-term bar level |
62/62 pass
