#!/usr/bin/env python3
"""Verification report: renders test signals through the plugin's DSP (MalonVerifyRender) and compares
with published references (ITU-R BS.1770-4 / EBU Tech 3341, sine RMS math, pink-noise slope) and with
independent numpy/scipy/pyloudnorm measurements for a real mix."""
import json, subprocess, sys, os, numpy as np, soundfile as sf, pyloudnorm as pyln
from scipy.signal import resample_poly
BIN = sys.argv[1] if len(sys.argv) > 1 else 'build/MalonVerifyRender'
MIX = '/workspace/dembow-demo/_premaster_v2.wav'
rng = np.random.default_rng(1)
FS = 48000

def run(x, fs=FS, bpo=6):
    x = np.ascontiguousarray(x, dtype=np.float32)
    x.tofile('/tmp/v.f32')
    return json.loads(subprocess.check_output([BIN, '/tmp/v.f32', str(fs), str(bpo)]))

def db(v): return 20*np.log10(v)
def pink(n):
    w = rng.standard_normal(n); W = np.fft.rfft(w); f = np.arange(len(W)); f[0] = 1
    p = np.fft.irfft(W/np.sqrt(f), n); return p/np.sqrt(np.mean(p**2))*10**(-20/20)
def st(a, b=None): return np.stack([a, a if b is None else b], 1)
def t(sec, fs=FS): return np.arange(int(sec*fs))/fs

rows = []
def row(test, metric, ours, exp, tol, src):
    if tol is None:
        rows.append((test, metric, f'{ours:.2f}', '-', '-', 'INFO', src)); return
    ok = abs(ours-exp) <= tol
    rows.append((test, metric, f'{ours:.2f}', f'{exp:.2f}', f'±{tol:g}', 'PASS' if ok else 'FAIL', src))

def ref_rms(x): return [db(np.sqrt(np.mean(x[:, c].astype(np.float64)**2))) for c in range(2)]
def ref_tp(x, fs):
    return db(max(np.max(np.abs(resample_poly(x[:, c].astype(np.float64), 8, 1))) for c in range(2)))
def ref_lufs(x, fs): return pyln.Meter(fs).integrated_loudness(x.astype(np.float64))
def bandslope(o, key, fmin=50, fmax=10000, which='avg'):
    f = o['freqMin']*2**(np.arange(o['bands'])/o['bpo']); y = np.array(o[key][which] if key else o[which])
    m = (f >= fmin) & (f <= fmax); return np.polyfit(np.log2(f[m]), y[m], 1)[0]

# 1) -20 dBFS 1 kHz sine, L = R
s = 10**(-20/20)*np.sin(2*np.pi*1000*t(20)); o = run(st(s))
row('1 kHz sine -20 dBFS', 'RMS L (integrated)', o['rmsIntL'], -23.01, 0.05, 'math: A/sqrt2')
row('1 kHz sine -20 dBFS', 'RMS R (integrated)', o['rmsIntR'], -23.01, 0.05, 'math')
row('1 kHz sine -20 dBFS', 'RMS 300 ms', o['rms300L'], -23.01, 0.05, 'math')
row('1 kHz sine -20 dBFS', 'Sample peak dBFS', o['samplePeak'], -20.0, 0.05, 'math')
row('1 kHz sine -20 dBFS', 'True peak dBTP', o['truePeak'], -20.0, 0.1, 'BS.1770-4')
row('1 kHz sine -20 dBFS', 'LUFS integrated', o['lufsI'], -20.0, 0.1, 'Tech 3341 (1k stereo: LUFS = dBFS)')
row('1 kHz sine -20 dBFS', 'Spectrum 1k band, peak (slope 0)', max(o['s0']['peak']), -20.0, 0.5, 'sine reads its amplitude')
b = int(np.argmax(o['s0']['peak']))
row('1 kHz sine -20 dBFS', 'Average line vs bar (1k band)', o['s0']['avg'][b]-o['s0']['peak'][b], 0.0, 0.5, 'avg = long-term bar level')
row('1 kHz sine -20 dBFS', 'Correlation (L=R)', o['corrLast'], 1.0, 0.01, 'Pearson')
# 2) EBU Tech 3341 cases 1 and 2
for lvl in (-23, -33):
    o = run(st(10**(lvl/20)*np.sin(2*np.pi*1000*t(20))))
    row(f'EBU 3341 #{1 if lvl==-23 else 2}: 1k {lvl} dBFS', 'LUFS integrated', o['lufsI'], lvl, 0.1, 'EBU Tech 3341')
# 3) true-peak: fs/4 sine, 45 deg phase, amplitude 0.5 -> samples at -9.03, true peak -6.02
x = 0.5*np.sin(2*np.pi*(FS/4)*t(5)+np.pi/4); o = run(st(x))
row('fs/4 sine 45°, A=0.5', 'Sample peak dBFS', o['samplePeak'], -9.03, 0.05, 'math')
row('fs/4 sine 45°, A=0.5', 'True peak dBTP', o['truePeak'], -6.02, 0.3, 'BS.1770-4 (inter-sample)')
# 4) -6 dBFS log sweep 20 Hz-20 kHz
tt = t(20); f0, f1, T = 20, 20000, 20
sw = 10**(-6/20)*np.sin(2*np.pi*f0*T/np.log(f1/f0)*(np.exp(tt/T*np.log(f1/f0))-1)); o = run(st(sw))
row('Sine sweep -6 dBFS', 'RMS L (integrated)', o['rmsIntL'], -9.03, 0.05, 'math')
row('Sine sweep -6 dBFS', 'Sample peak dBFS', o['samplePeak'], -6.0, 0.05, 'math')
row('Sine sweep -6 dBFS', 'True peak dBTP', o['truePeak'], -6.0, 0.1, 'math (sine amplitude)')
row('Sine sweep -6 dBFS', 'LUFS integrated', o['lufsI'], ref_lufs(st(sw), FS), 0.2, 'pyloudnorm (BS.1770)')
hold = np.array(o['hold']);  f = o['freqMin']*2**(np.arange(o['bands'])/6); m = (f > 40) & (f < 16000)
row('Sine sweep -6 dBFS', 'Max-hold flatness 40 Hz-16 kHz, slope 3 (spread dB)', float(np.ptp(hold[m])), 0.0, 1.5, 'flat response')
row('Sine sweep -6 dBFS', 'Max-hold level, mean 40 Hz-16 kHz (dB)', float(np.mean(hold[m])), -6.0, 1.0, 'sine reads its amplitude')
# 5) pink noise, independent L/R, 60 s
pl, pr = pink(60*FS), pink(60*FS); x = st(pl, pr); o = run(x)
r = ref_rms(x)
row('Pink noise (L/R indep.)', 'RMS L (integrated)', o['rmsIntL'], r[0], 0.05, 'numpy')
row('Pink noise (L/R indep.)', 'LUFS integrated', o['lufsI'], ref_lufs(x, FS), 0.2, 'pyloudnorm')
row('Pink noise (L/R indep.)', 'Band power slope, raw bank (dB/oct)', bandslope(o, None, which='meanOld'), 0.0, 0.2, 'pink = equal power/oct')
row('Pink noise (L/R indep.)', 'Avg line slope at Slope 3 (dB/oct)', bandslope(o, 's3'), 0.0, 0.75, 'SPAN: pink flat at 3')
row('Pink noise (L/R indep.)', 'Avg line slope at Slope 4.5 (dB/oct)', bandslope(o, 's45'), 1.5, 0.75, 'SPAN: +1.5 dB/oct')
row('Pink noise (L/R indep.)', 'Avg line slope at Slope 0 (dB/oct)', bandslope(o, 's0'), -3.0, 0.75, 'SPAN: -3 dB/oct')
row('Pink noise (L/R indep.)', 'Correlation (uncorrelated)', o['corrLast'], 0.0, 0.1, 'Pearson')
gap = np.mean(np.array(o['meanBar']) - np.array(o['meanAvg']))
row('Pink noise (L/R indep.)', 'Bars minus Average line (time-mean dB)', gap, 0.0, 1.0, 'avg sits in the bars')
# 6) correlation edge cases
n = rng.standard_normal(10*FS)*0.1
o = run(st(n)); row('Noise L = R (mono)', 'Correlation', o['corrLast'], 1.0, 0.01, 'Pearson')
o = run(st(n, -n)); row('Noise L = -R', 'Correlation', o['corrLast'], -1.0, 0.01, 'Pearson')
# 7) full mix
mix, mfs = sf.read(MIX, dtype='float32'); o = run(mix, mfs)
r = ref_rms(mix)
row('Full mix (dembow premaster)', 'RMS L (integrated)', o['rmsIntL'], r[0], 0.05, 'numpy')
row('Full mix (dembow premaster)', 'RMS R (integrated)', o['rmsIntR'], r[1], 0.05, 'numpy')
row('Full mix (dembow premaster)', 'True peak dBTP', o['truePeak'], ref_tp(mix, mfs), 0.2, 'scipy 8x')
row('Full mix (dembow premaster)', 'LUFS integrated', o['lufsI'], ref_lufs(mix, mfs), 0.2, 'pyloudnorm')
ff = o['freqMin']*2**(np.arange(o['bands'])/6); mh = (ff >= 1000) & (ff <= 16000)
oldgap = np.mean(np.array(o['meanBar'])[mh] - np.array(o['meanOld'])[mh])
newgap = np.mean(np.array(o['meanBar'])[mh] - np.array(o['meanAvg'])[mh])
row('Full mix (dembow premaster)', 'Bars minus v1.0 power Average 1-16 kHz (old)', oldgap, 0.0, None, 'v1.0 method, for comparison')
extra = {'mix_rms300_end': (o['rms300L'], o['rms300R']), 'mix_corr_range': (o['corrMin'], o['corrMax']),
         'mix_bar_minus_oldavg_1k_16k': oldgap, 'mix_bar_minus_newavg_1k_16k': newgap}
row('Full mix (dembow premaster)', 'Bars minus Average 1-16 kHz (new)', newgap, 0.0, 1.0, 'avg sits in the bars')

w = max(len(r_[0]) for r_ in rows)
print('| Test | Metric | Ours | Expected | Tol | Result | Reference |'); print('|---|---|---|---|---|---|---|')
for r_ in rows: print('| ' + ' | '.join(r_) + ' |')
print(json.dumps(extra))
print(f"{sum(r_[5]=='PASS' for r_ in rows)}/{sum(r_[5]!='INFO' for r_ in rows)} pass")
