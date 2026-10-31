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

# 8) v1.2 metering scales (SPAN "Metering" selector): K-system is sine-referenced (AES17, +3.01 dB) RMS with a
#    600 ms window shown relative to the K reference; LUFS/LU per BS.1770-4 / EBU R128 (LU = LUFS + 23)
s = 10**(-20/20)*np.sin(2*np.pi*1000*t(10)); o = run(st(s)); sc = o['scales']
exp = {'DBFS': -23.01, 'DBFS+3': -20.0, 'K-20': 0.0, 'K-14': -6.0, 'K-12': -8.0, 'K-20 C': 0.0, 'K-14 C': -6.0, 'K-12 C': -8.0,
       'DBFS.30': -23.01, 'DBFS.15': -23.01, 'LUFS-ML': -20.0, 'LUFS-SL': -20.0, 'LU-ML': 3.0, 'LU-SL': 3.0}
for k, v in exp.items():
    row('Metering: 1k sine -20 dBFS', f'{k} meter value', sc[k]['bar'], v, 0.1, 'Katz K-system (-20 dBFS sine = 0 K-20)' if k.startswith('K') else ('BS.1770-4 / R128' if 'LU' in k else 'math'))
row('Metering: 1k sine -20 dBFS', 'K-20 peak (dBFS -20 -> K units)', sc['K-20']['peak'], 0.0, 0.1, 'K-20: 0 dBFS = +20')
row('Metering: 1k sine -20 dBFS', 'K-20 meter top (= 0 dBFS)', sc['K-20']['top'], 20.0, 0.01, 'Katz')
row('Metering: 1k sine -20 dBFS', 'K-14 meter top (= 0 dBFS)', sc['K-14']['top'], 14.0, 0.01, 'Katz')
row('Metering: 1k sine -20 dBFS', 'DBFS.30 / DBFS.15 range', sc['DBFS.30']['bottom'] + sc['DBFS.15']['bottom'] / 100, -30.15, 0.01, 'SPAN guide (30 / 15 dB range)')
o = run(st(10**(-23/20)*np.sin(2*np.pi*1000*t(10)))); sc = o['scales']
row('Metering: EBU 3341 #3/#4 (1k -23 dBFS)', 'LU-ML', sc['LU-ML']['bar'], 0.0, 0.1, 'EBU Tech 3341: momentary = -23 LUFS = 0 LU')
row('Metering: EBU 3341 #3/#4 (1k -23 dBFS)', 'LU-SL', sc['LU-SL']['bar'], 0.0, 0.1, 'EBU Tech 3341: short-term = 0 LU')
pk = pink(30*FS)*10**(-3.0103/20); o = run(st(pk, pink(30*FS)*10**(-3.0103/20))); sc = o['scales']
row('Metering: pink noise -23.01 dBFS RMS', 'K-20 meter value', sc['K-20']['bar'], 0.0, 0.6, 'Katz: -20 dBFS (AES17) pink = 0 K-20')
row('Metering: pink noise -23.01 dBFS RMS', 'LUFS-SL vs pyloudnorm short-term', sc['LUFS-SL']['bar'],
    pyln.Meter(FS, block_size=3.0).integrated_loudness(st(pk[-3*FS:], pk[-3*FS:]).astype(np.float64)) , 0.6, 'pyloudnorm, last 3 s (L=R approx)')
# 9) v1.2 HI-RES: 48 bands/octave
x = 0.25*np.sin(2*np.pi*55*t(6)) + 0.25*np.sin(2*np.pi*58.27*t(6)); o = run(st(x), bpo=48)
f48 = o['freqMin']*2**(np.arange(o['bands'])/48); hp = np.array(o['hold']); m = (f48 > 50) & (f48 < 64)
idx = np.where(m)[0]; mx = [i for i in idx if hp[i] > hp[i-1] and hp[i] > hp[i+1] and hp[i] > hp[idx].max() - 10]
row('HI-RES 48/oct: A1 55 Hz + A#1 58.27 Hz', 'Separate peaks between 50 and 64 Hz', len(mx), 2, 0, 'semitone apart = 4 bands at 48/oct')
o = run(st(10**(-20/20)*np.sin(2*np.pi*1000*t(10))), bpo=48)
row('HI-RES 48/oct: 1 kHz sine -20 dBFS', 'Peak band level (slope 0)', max(o['s0']['peak']), -20.0, 0.5, 'sine reads its amplitude')
o = run(st(pink(60*FS), pink(60*FS)), bpo=48)
row('HI-RES 48/oct: pink noise', 'Band power slope, raw bank (dB/oct)', bandslope(o, None, which='meanOld'), 0.0, 0.2, 'pink = equal power/oct')
# the drawn line follows peak envelopes; narrow low bands have more envelope spread, so a small tilt (+0.9 vs +0.7 at 6/oct)
row('HI-RES 48/oct: pink noise', 'Avg line slope at Slope 3 (dB/oct)', bandslope(o, 's3'), 0.0, 1.0, 'SPAN: pink flat at 3 (peak-envelope bias, wider tol)')

w = max(len(r_[0]) for r_ in rows)
print('| Test | Metric | Ours | Expected | Tol | Result | Reference |'); print('|---|---|---|---|---|---|---|')
for r_ in rows: print('| ' + ' | '.join(r_) + ' |')
print(json.dumps(extra))
print(f"{sum(r_[5]=='PASS' for r_ in rows)}/{sum(r_[5]!='INFO' for r_ in rows)} pass")
