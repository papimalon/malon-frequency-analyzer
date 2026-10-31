// Malon Frequency Analyzer - offline render for the verification report (not part of the plugin).
// Usage: MalonVerifyRender <in.f32 (interleaved stereo float32)> <sampleRate> <bandsPerOctave>
// Feeds the file through the same DSP the plugin uses (FilterBank + DisplayModel + LevelMeter) in 512-sample
// blocks, polling the display every ~32 ms like the editor (31 Hz timer). Prints JSON.
#include "../../Source/dsp/FilterBank.h"
#include "../../Source/dsp/Ballistics.h"
#include "../../Source/dsp/LevelMeter.h"
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace malon;

int main (int argc, char** argv)
{
    if (argc < 4) return 1;
    const double fs = std::atof (argv[2]); const int bpo = std::atoi (argv[3]);
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 2;
    std::vector<float> d; float buf[4096]; size_t n;
    while ((n = std::fread (buf, sizeof (float), 4096, f)) > 0) d.insert (d.end(), buf, buf + n);
    std::fclose (f);
    const int frames = (int) d.size() / 2;
    std::vector<float> L (frames), R (frames);
    for (int i = 0; i < frames; ++i) { L[i] = d[2 * i]; R[i] = d[2 * i + 1]; }

    FilterBank fb; fb.prepare (fs, bpo);
    LevelMeter lm; lm.prepare (fs, 2); lm.setRmsIntegrated (true);
    LevelMeter lm300; lm300.prepare (fs, 2); lm300.setRmsWindowMs (300);
    const double slopes[3] = { 0.0, 3.0, 4.5 };
    DisplayModel dm[3]; DisplaySettings ds[3];
    for (int k = 0; k < 3; ++k) { ds[k].slope = slopes[k]; ds[k].avgTimeMs = 3000; }
    // power-domain average per band (what v1.0 drew) for the root-cause comparison
    std::vector<double> powSum (kMaxBands, 0.0); double powN = 0.0;
    // time-means (after 3 s) of the drawn bar, the new average line and the v1.0 power average, slope 0
    std::vector<double> mBar (kMaxBands, 0.0), mAvg (kMaxBands, 0.0), mOld (kMaxBands, 0.0); double mN = 0.0;
    std::vector<double> oldEma (kMaxBands, 0.0);
    DisplayModel holdM; DisplaySettings holdS; holdS.slope = 3.0; holdS.holdInfinite = true;   // 3 = true band level
    double corrMin = 1.0, corrMax = -1.0, corrLast = 0.0; bool corrValid = false;
    const int block = 512, pollEvery = int (0.032 * fs);
    int sincePoll = 0;
    for (int pos = 0; pos < frames; pos += block)
    {
        const int m = std::min (block, frames - pos);
        const float* ch[2] = { L.data() + pos, R.data() + pos };
        fb.process (ch[0], ch[1], m, ChannelMode::Both);
        lm.process (ch, 2, m); lm300.process (ch, 2, m);
        sincePoll += m;
        if (sincePoll >= pollEvery)
        {
            const double dt = 1000.0 * sincePoll / fs; sincePoll = 0;
            Readout r; fb.read (r);
            for (int k = 0; k < 3; ++k) dm[k].update (r, dt, ds[k]);
            holdM.update (r, dt, holdS);
            if (r.samples > 0)
            {
                for (int i = 0; i < r.numBands; ++i) powSum[i] += r.sumSq[i + 1] / r.samples;
                powN += 1;
                if (pos > 3.0 * fs)
                {
                    for (int i = 0; i < r.numBands; ++i)
                    {
                        mBar[i] += dm[0].peak[i]; mAvg[i] += dm[0].avg[i];
                        mOld[i] += 3.0 + 10.0 * std::log10 (powSum[i] / powN + 1e-30);
                    }
                    mN += 1;
                }
            }
            auto ro = lm.read();
            if (ro.correlationValid && pos > fs * 0.5) { corrValid = true; corrMin = std::min (corrMin, ro.correlation); corrMax = std::max (corrMax, ro.correlation); }
            corrLast = ro.correlation;
        }
    }
    auto ro = lm.read(); auto r3 = lm300.read();
    std::printf ("{\"rmsIntL\":%.3f,\"rmsIntR\":%.3f,\"rms300L\":%.3f,\"rms300R\":%.3f,\"rmsFastL\":%.3f,"
                 "\"truePeak\":%.3f,\"samplePeak\":%.3f,\"lufsI\":%.3f,\"lufsMmax\":%.3f,"
                 "\"corrValid\":%s,\"corrLast\":%.4f,\"corrMin\":%.4f,\"corrMax\":%.4f,",
                 ro.rmsDb[0], ro.rmsDb[1], r3.rmsDb[0], r3.rmsDb[1], ro.rmsFastDb[0],
                 ro.truePeakMaxDb, ro.samplePeakMaxDb, ro.integrated, ro.momentaryMax,
                 corrValid ? "true" : "false", corrLast, corrMin, corrMax);
    const int nb = dm[0].numBands;
    std::printf ("\"freqMin\":%.4f,\"bpo\":%d,\"bands\":%d", dm[0].freqMin, bpo, nb);
    const char* names[3] = { "s0", "s3", "s45" };
    for (int k = 0; k < 3; ++k)
    {
        std::printf (",\"%s\":{\"peak\":[", names[k]);
        for (int i = 0; i < nb; ++i) std::printf ("%s%.2f", i ? "," : "", dm[k].peak[i]);
        std::printf ("],\"avg\":[");
        for (int i = 0; i < nb; ++i) std::printf ("%s%.2f", i ? "," : "", dm[k].avg[i]);
        std::printf ("]}");
    }
    std::printf (",\"oldPowAvg\":[");
    for (int i = 0; i < nb; ++i) std::printf ("%s%.2f", i ? "," : "", powN > 0 ? 3.0 + 10.0 * std::log10 (powSum[i] / powN + 1e-30) : -200.0);
    std::printf ("],\"hold\":[");
    for (int i = 0; i < nb; ++i) std::printf ("%s%.2f", i ? "," : "", holdM.hold[i]);
    auto pr = [&] (const char* k, std::vector<double>& v)
    { std::printf ("],\"%s\":[", k); for (int i = 0; i < nb; ++i) std::printf ("%s%.2f", i ? "," : "", mN > 0 ? v[i] / mN : -200.0); };
    pr ("meanBar", mBar); pr ("meanAvg", mAvg); pr ("meanOld", mOld);
    std::printf ("]}\n");
}
