#pragma once

// HaalAi voice analyser for Halden Reverb 369.
//
// 1. Learning (first 4 s of sung input): a long-term average spectrum is taken and compared to the
//    voice's own overall tilt. Regions that stick out (boom / boxiness / ringing resonances) become a
//    static cut curve for the reverb send, plus a gentle air shelf when the voice is dull up top.
// 2. Tracking (always on in HaalAi): every 256 samples the current spectrum is smoothed to formant
//    resolution (so single harmonics are never chased) and compared with a wider envelope. The three
//    worst bumps, weighted towards the harsh 2-6 kHz zone, drive three dynamic bells (D1-D3) of at
//    most 2 dB that glide after them. The reverb's pre-delay gives the look-ahead.

#include "DSP.h"
#include <array>
#include <vector>

namespace haldendsp
{
struct CurveBand
{
    float freq = 1000.0f, gain = 0.0f, q = 1.0f;
};

class VoiceAnalyzer
{
public:
    static constexpr int kOrder = 11;
    static constexpr int kN = 1 << kOrder;   // 2048
    static constexpr int kBins = kN / 2 + 1;
    static constexpr int kHop = 256;
    static constexpr int kMaxStatic = 5;
    static constexpr int kDyn = 3;
    static constexpr float kLearnSeconds = 4.0f;
    static constexpr float kMaxDynCut = 2.0f;

    struct DynBand
    {
        float logf = std::log2 (3000.0f);    // where the band sits (log2 Hz)
        float targetLogf = std::log2 (3000.0f);
        float targetGain = 0.0f;              // <= 0 dB
        bool assigned = false;
    };

    void prepare (double sampleRate)
    {
        sr = static_cast<float> (sampleRate);
        ring.assign (kN, 0.0f);
        fftBuf.assign (2 * kN, 0.0f);
        powr.assign (kBins, 0.0f);
        ltas.assign (kBins, 0.0f);
        e1.assign (kBins, -200.0f);
        e2.assign (kBins, -200.0f);
        prefix.assign (kBins + 1, 0.0);
        win.resize (kN);
        for (int i = 0; i < kN; ++i)
            win[static_cast<size_t> (i)] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (kN));
        pos = hopCount = 0;
        resetDynamics();
    }

    void resetDynamics()
    {
        const float defaults[kDyn] = { 1800.0f, 3200.0f, 5500.0f };
        for (int b = 0; b < kDyn; ++b)
        {
            dyn[static_cast<size_t> (b)] = {};
            dyn[static_cast<size_t> (b)].logf = dyn[static_cast<size_t> (b)].targetLogf = std::log2 (defaults[b]);
        }
    }

    void startLearning()
    {
        std::fill (ltas.begin(), ltas.end(), 0.0f);
        learnFrames = 0;
        learning = true;
    }

    bool isLearning() const noexcept { return learning; }
    bool hasCurve() const noexcept { return learned; }
    float learnProgress() const noexcept
    {
        return learning ? juce::jlimit (0.0f, 1.0f, static_cast<float> (learnFrames * kHop) / (kLearnSeconds * sr)) : (learned ? 1.0f : 0.0f);
    }

    void setCurve (const std::array<CurveBand, kMaxStatic>& bands, int num, float shelf)
    {
        staticBands = bands;
        numStatic = juce::jlimit (0, kMaxStatic, num);
        shelfGain = shelf;
        learned = true;
        learning = false;
    }

    // x: the reverb send after the static curve; sounding: input is singing right now
    inline bool push (float x, bool sounding) noexcept
    {
        ring[static_cast<size_t> (pos)] = x;
        pos = (pos + 1) & (kN - 1);
        if (++hopCount < kHop) return false;
        hopCount = 0;
        analyse (sounding);
        return true;
    }

    std::array<CurveBand, kMaxStatic> staticBands {};
    int numStatic = 0;
    float shelfGain = 0.0f;
    std::array<DynBand, kDyn> dyn {};

private:
    float sr = 48000.0f;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> ring, fftBuf, powr, ltas, e1, e2, win;
    std::vector<double> prefix;
    int pos = 0, hopCount = 0, learnFrames = 0;
    bool learning = false, learned = false;

    float binHz() const noexcept { return sr / static_cast<float> (kN); }

    // power spectrum smoothed with a window of max(fraction of an octave, minHz), result in dB
    void smoothDb (const std::vector<float>& p, std::vector<float>& out, float octaves, float minHz)
    {
        prefix[0] = 0.0;
        for (int k = 0; k < kBins; ++k)
            prefix[static_cast<size_t> (k) + 1] = prefix[static_cast<size_t> (k)] + static_cast<double> (p[static_cast<size_t> (k)]);
        const float half = std::pow (2.0f, octaves * 0.5f);
        const float bh = binHz();
        for (int k = 1; k < kBins; ++k)
        {
            const float f = static_cast<float> (k) * bh;
            const float width = std::max (f * (half - 1.0f / half), minHz);
            const int lo = juce::jlimit (1, kBins - 1, static_cast<int> ((f - width * 0.5f) / bh));
            const int hi = juce::jlimit (lo, kBins - 1, static_cast<int> ((f + width * 0.5f) / bh));
            const double mean = (prefix[static_cast<size_t> (hi) + 1] - prefix[static_cast<size_t> (lo)]) / static_cast<double> (hi - lo + 1);
            out[static_cast<size_t> (k)] = 10.0f * static_cast<float> (std::log10 (mean + 1.0e-20));
        }
        out[0] = out[1];
    }

    // moving average of a dB curve over max(octaves, minHz)
    void averageDb (const std::vector<float>& in, std::vector<float>& out, float octaves, float minHz)
    {
        prefix[0] = 0.0;
        for (int k = 0; k < kBins; ++k)
            prefix[static_cast<size_t> (k) + 1] = prefix[static_cast<size_t> (k)] + static_cast<double> (in[static_cast<size_t> (k)]);
        const float half = std::pow (2.0f, octaves * 0.5f);
        const float bh = binHz();
        for (int k = 1; k < kBins; ++k)
        {
            const float f = static_cast<float> (k) * bh;
            const float width = std::max (f * (half - 1.0f / half), minHz);
            const int lo = juce::jlimit (1, kBins - 1, static_cast<int> ((f - width * 0.5f) / bh));
            const int hi = juce::jlimit (lo, kBins - 1, static_cast<int> ((f + width * 0.5f) / bh));
            out[static_cast<size_t> (k)] = static_cast<float> ((prefix[static_cast<size_t> (hi) + 1] - prefix[static_cast<size_t> (lo)]) / static_cast<double> (hi - lo + 1));
        }
        out[0] = out[1];
    }

    static float harshWeight (float f) noexcept
    {
        if (f >= 1800.0f && f <= 6000.0f) return 1.0f;                       // harsh / sibilant-ish zone
        if (f < 1800.0f) return 0.6f + 0.4f * (f - 700.0f) / 1100.0f;       // nasal / honk zone
        return std::max (0.6f, 1.0f - (f - 6000.0f) / 8000.0f);             // keep the air
    }

    void analyse (bool sounding)
    {
        for (int i = 0; i < kN; ++i)
            fftBuf[static_cast<size_t> (i)] = ring[static_cast<size_t> ((pos + i) & (kN - 1))] * win[static_cast<size_t> (i)];
        std::fill (fftBuf.begin() + kN, fftBuf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data());

        float maxP = 0.0f;
        for (int k = 0; k < kBins; ++k)
        {
            const float m = fftBuf[static_cast<size_t> (k)];
            powr[static_cast<size_t> (k)] = m * m;
            maxP = std::max (maxP, m * m);
        }
        const float norm = static_cast<float> (kN) * 0.25f;
        const bool hasSignal = maxP > (1.0e-4f * norm) * (1.0e-4f * norm);   // above about -80 dBFS

        if (learning && sounding && hasSignal)
        {
            for (int k = 0; k < kBins; ++k) ltas[static_cast<size_t> (k)] += powr[static_cast<size_t> (k)];
            if (++learnFrames * kHop >= static_cast<int> (kLearnSeconds * sr))
                computeStaticCurve();
        }

        trackResonances (hasSignal);
    }

    void trackResonances (bool hasSignal)
    {
        for (auto& d : dyn) d.assigned = false;
        if (! hasSignal)
        {
            for (auto& d : dyn) d.targetGain = 0.0f;
            return;
        }

        // formant-level envelope vs. a wider envelope: what sticks out is a resonance
        smoothDb (powr, e1, 1.0f / 6.0f, 350.0f);
        smoothDb (powr, e2, 1.0f, 900.0f);

        struct Cand { float f, score; };
        std::array<Cand, 32> cands {};
        int nc = 0;
        const float bh = binHz();
        const int kLo = static_cast<int> (700.0f / bh), kHi = std::min (kBins - 2, static_cast<int> (10000.0f / bh));
        float topDb = -200.0f;
        for (int k = kLo; k <= kHi; ++k) topDb = std::max (topDb, e1[static_cast<size_t> (k)]);

        for (int k = kLo + 1; k < kHi && nc < static_cast<int> (cands.size()); ++k)
        {
            const float v = e1[static_cast<size_t> (k)];
            if (v < e1[static_cast<size_t> (k) - 1] || v < e1[static_cast<size_t> (k) + 1]) continue;
            if (v < topDb - 30.0f) continue;   // too quiet to matter
            const float f = static_cast<float> (k) * bh;
            const float score = (v - e2[static_cast<size_t> (k)]) * harshWeight (f);
            if (score > 2.5f) cands[static_cast<size_t> (nc++)] = { f, score };
        }
        std::sort (cands.begin(), cands.begin() + nc, [] (const Cand& a, const Cand& b) { return a.score > b.score; });

        // keep the strongest three that are at least 1/3 octave apart
        std::array<Cand, kDyn> pick {};
        int np = 0;
        for (int i = 0; i < nc && np < kDyn; ++i)
        {
            bool ok = true;
            for (int j = 0; j < np; ++j)
                if (std::abs (std::log2 (cands[static_cast<size_t> (i)].f / pick[static_cast<size_t> (j)].f)) < 1.0f / 3.0f) ok = false;
            if (ok) pick[static_cast<size_t> (np++)] = cands[static_cast<size_t> (i)];
        }

        // assign each pick to the nearest band so bands follow their resonance instead of hopping
        for (int i = 0; i < np; ++i)
        {
            const float lf = std::log2 (pick[static_cast<size_t> (i)].f);
            int best = -1;
            float bestDist = 1.0e9f;
            for (int b = 0; b < kDyn; ++b)
            {
                if (dyn[static_cast<size_t> (b)].assigned) continue;
                const float dist = std::abs (dyn[static_cast<size_t> (b)].targetLogf - lf);
                if (dist < bestDist) { bestDist = dist; best = b; }
            }
            if (best < 0) break;
            auto& d = dyn[static_cast<size_t> (best)];
            d.assigned = true;
            d.targetLogf = lf;
            d.targetGain = -std::min (kMaxDynCut, 0.35f + (pick[static_cast<size_t> (i)].score - 2.5f) * 0.4f);
        }
        for (auto& d : dyn)
            if (! d.assigned) d.targetGain = 0.0f;
    }

    void computeStaticCurve()
    {
        learning = false;
        if (learnFrames <= 0) return;

        std::vector<float> s6 (kBins), sw (kBins);
        smoothDb (ltas, s6, 1.0f / 3.0f, 250.0f);   // wide enough to merge the harmonics of any voice
        averageDb (s6, sw, 2.0f, 400.0f);           // the voice's own broad shape (mean in dB)
        const float bh = binHz();

        // what sticks out locally above the broad shape = resonance
        std::vector<float> ex (kBins, 0.0f);
        for (int k = 1; k < kBins; ++k) ex[static_cast<size_t> (k)] = s6[static_cast<size_t> (k)] - sw[static_cast<size_t> (k)];

        const auto meanDb = [&] (float f0, float f1)
        {
            double e = 0; int n = 0;
            for (int k = static_cast<int> (f0 / bh); k <= static_cast<int> (f1 / bh) && k < kBins; ++k) { e += ltas[static_cast<size_t> (k)]; ++n; }
            return 10.0f * static_cast<float> (std::log10 (e / std::max (1, n) + 1.0e-20));
        };
        struct Pk { int k; float e; };
        std::vector<Pk> peaks;
        const int kLo = static_cast<int> (150.0f / bh), kHi = static_cast<int> (6500.0f / bh);
        for (int k = kLo + 1; k < kHi; ++k)
            if (ex[static_cast<size_t> (k)] > 2.0f && ex[static_cast<size_t> (k)] >= ex[static_cast<size_t> (k) - 1] && ex[static_cast<size_t> (k)] >= ex[static_cast<size_t> (k) + 1])
                peaks.push_back ({ k, ex[static_cast<size_t> (k)] });
        std::sort (peaks.begin(), peaks.end(), [] (const Pk& a, const Pk& b) { return a.e > b.e; });

        std::array<CurveBand, kMaxStatic> bands {};
        int nb = 0;
        for (const auto& p : peaks)
        {
            if (nb >= kMaxStatic) break;
            const float f = static_cast<float> (p.k) * bh;
            bool ok = true;
            for (int j = 0; j < nb; ++j)
                if (std::abs (std::log2 (f / bands[static_cast<size_t> (j)].freq)) < 0.5f) ok = false;
            if (! ok) continue;

            // width where the bump falls to half its height
            int lo = p.k, hi = p.k;
            while (lo > 1 && ex[static_cast<size_t> (lo)] > p.e * 0.5f) --lo;
            while (hi < kBins - 2 && ex[static_cast<size_t> (hi)] > p.e * 0.5f) ++hi;
            const float bw = juce::jlimit (0.15f, 2.5f, std::log2 (static_cast<float> (std::max (hi, lo + 1)) / static_cast<float> (std::max (1, lo))));
            const float r = std::pow (2.0f, bw);
            const float q = juce::jlimit (0.7f, 6.0f, std::sqrt (r) / (r - 1.0f));

            const float cut = std::min (6.0f, 0.8f * p.e);
            bands[static_cast<size_t> (nb++)] = { f, -cut, q };
        }

        // boom / mud: low-mids (180-500 Hz) more than 6 dB above the presence region (1-3 kHz)
        const float boomExcess = meanDb (180.0f, 500.0f) - meanDb (1000.0f, 3000.0f) - 6.0f;
        if (boomExcess > 0.5f)
        {
            int kMax = static_cast<int> (180.0f / bh);
            for (int k = kMax; k <= static_cast<int> (500.0f / bh); ++k)
                if (s6[static_cast<size_t> (k)] > s6[static_cast<size_t> (kMax)]) kMax = k;
            const float f = static_cast<float> (kMax) * bh;
            const float cut = std::min (5.0f, 0.7f * boomExcess + 1.0f);
            int near = -1;
            for (int j = 0; j < nb; ++j)
                if (std::abs (std::log2 (f / bands[static_cast<size_t> (j)].freq)) < 0.5f) near = j;
            if (near >= 0)
                bands[static_cast<size_t> (near)].gain = -std::min (6.0f, std::max (-bands[static_cast<size_t> (near)].gain, cut));
            else if (nb < kMaxStatic)
                bands[static_cast<size_t> (nb++)] = { f, -cut, 1.0f };
            else
                bands[static_cast<size_t> (kMaxStatic - 1)] = { f, -cut, 1.0f };   // boom matters more than the weakest bump
        }

        // air: if 6-10 kHz sits well below the presence region, lift it a little
        const float airGap = meanDb (1500.0f, 4000.0f) - meanDb (6000.0f, 10000.0f);
        const float shelf = juce::jlimit (0.0f, 2.5f, (airGap - 12.0f) * 0.25f);

        setCurve (bands, nb, shelf);
    }
};
} // namespace haldendsp
