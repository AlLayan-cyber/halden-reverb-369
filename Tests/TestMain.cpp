// Offline test runner for Halden Reverb 369.
//   HaldenTests            -> DSP checks
//   HaldenTests --shots D  -> renders the editor to PNG files in folder D
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <functional>

using P = HaldenReverbAudioProcessor;
static int failures = 0;

static void check (bool ok, const juce::String& what, const juce::String& detail)
{
    std::printf ("%s  %-46s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8(), detail.toRawUTF8());
    if (! ok) ++failures;
}

static void setP (P& p, const char* id, float v)
{
    auto* q = p.apvts.getParameter (id);
    q->setValueNotifyingHost (q->convertTo0to1 (v));
}

struct Out { std::vector<float> L, R; };

// Renders `secs` seconds; `input(n, ch)` supplies the input sample; `onBlock(pos)` can change parameters.
static Out render (P& p, double sr, double secs, std::function<float (int, int)> input,
                   std::function<void (int)> onBlock = {}, int block = 256)
{
    p.prepareToPlay (sr, block);
    juce::MidiBuffer m;
    const int N = static_cast<int> (sr * secs);
    Out o; o.L.resize ((size_t) N); o.R.resize ((size_t) N);
    juce::AudioBuffer<float> b (2, block);
    for (int pos = 0; pos < N; pos += block)
    {
        const int n = std::min (block, N - pos);
        if (onBlock) onBlock (pos);
        b.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, input (pos + i, 0)); b.setSample (1, i, input (pos + i, 1)); }
        p.processBlock (b, m);
        for (int i = 0; i < n; ++i) { o.L[(size_t) (pos + i)] = b.getSample (0, i); o.R[(size_t) (pos + i)] = b.getSample (1, i); }
    }
    return o;
}

static auto impulse = [] (int n, int) { return n == 0 ? 1.0f : 0.0f; };

static float rt60 (const std::vector<float>& x, double sr, int skip)
{
    std::vector<double> e (x.size() + 1, 0.0);
    for (int i = (int) x.size() - 1; i >= skip; --i) e[(size_t) i] = e[(size_t) i + 1] + (double) x[(size_t) i] * x[(size_t) i];
    const double e0 = e[(size_t) skip];
    if (e0 <= 0) return 0;
    int t5 = -1, t25 = -1;
    for (size_t i = (size_t) skip; i < x.size(); ++i)
    {
        const double d = 10 * std::log10 (e[i] / e0 + 1e-30);
        if (t5 < 0 && d < -5) t5 = (int) i;
        if (t25 < 0 && d < -25) { t25 = (int) i; break; }
    }
    if (t5 < 0 || t25 < 0) return -1;
    return (float) ((t25 - t5) / sr * 3.0);
}

static std::vector<float> lowpass (std::vector<float> x, double sr, float f)
{
    haldendsp::BiquadFilter a, b; a.setLowPass (f, (float) sr); b.setLowPass (f, (float) sr);
    for (auto& s : x) s = b.process (a.process (s));
    return x;
}

static double energy (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0; for (size_t i = a; i < std::min (b, x.size()); ++i) e += (double) x[i] * x[i]; return e;
}

static float peakOf (const Out& o, bool& finite)
{
    float pk = 0; finite = true;
    for (size_t i = 0; i < o.L.size(); ++i)
    {
        if (! std::isfinite (o.L[i]) || ! std::isfinite (o.R[i])) finite = false;
        pk = std::max (pk, std::max (std::abs (o.L[i]), std::abs (o.R[i])));
    }
    return pk;
}

static float noiseBurst (int n, double sr, float secs)
{
    static juce::Random r (7);
    return n < (int) (sr * secs) ? (r.nextFloat() * 2.0f - 1.0f) * 0.3f : 0.0f;
}

// Synthetic sung phrase (A3 C#4 E4 A4, vibrato, "ah" formants), 4.4 s loop with 2.4 s rest.
struct Voice
{
    double sr = 48000.0, ph = 0.0;
    int lastNote = -1;
    std::array<haldendsp::BiquadFilter, 3> form;
    haldendsp::BiquadFilter tilt, breathHp;
    juce::Random rng { 11 };
    explicit Voice (double s) : sr (s)
    {
        tilt.setLowPass (3500.0f, (float) sr, 0.7f);
        breathHp.setHighPass (2000.0f, (float) sr);
    }
    void setVowel (int k)
    {
        // a, i, u, e
        static const float F[4][3] = { { 800, 1150, 2900 }, { 300, 2300, 3000 }, { 350, 800, 2400 }, { 530, 1850, 2500 } };
        for (int i = 0; i < 3; ++i) form[(size_t) i].setPeak (F[k & 3][i], (float) sr, 4.0f, 12.0f - 3.0f * (float) i);
    }
    float next (int n, bool legatoLoop = false)
    {
        const double t = n / sr;
        const double cyc = legatoLoop ? std::fmod (t, 2.0) : std::fmod (t, 4.4);
        static const double notes[] = { 220.0, 277.18, 329.63, 440.0 };
        const int k = (int) (cyc / 0.5);
        const double nt = cyc - k * 0.5;
        if (k >= 4 || nt >= 0.46) return 0.0f;
        if (k != lastNote) { setVowel (k + (int) (t / 2.0)); lastNote = k; }
        const double f0 = notes[k] * (1.0 + 0.012 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t));
        ph += f0 / sr; ph -= std::floor (ph);
        const double env = std::min (1.0, nt / 0.03) * std::min (1.0, (0.46 - nt) / 0.05);
        float x = (float) (2.0 * ph - 1.0) * 0.25f;
        for (auto& f : form) x = f.process (x);
        x = tilt.process (x) + breathHp.process ((rng.nextFloat() * 2.0f - 1.0f) * 0.01f);
        return 0.5f * (float) env * x;
    }
};

static int runDsp()
{
    const double sr = 48000.0;

    { // 1. Mix 100% is fully wet
        P p; setP (p, "mix", 1.0f); setP (p, "preDelayMs", 100.0f);
        auto o = render (p, sr, 0.5, impulse);
        check (std::abs (o.L[0]) < 1e-6f, "Mix 100% has no dry signal", "sample0=" + juce::String (o.L[0], 6));
    }
    { // 2. Mix 0% is fully dry
        P p; setP (p, "mix", 0.0f);
        auto o = render (p, sr, 0.5, impulse);
        check (std::abs (o.L[0] - 1.0f) < 1e-4f && energy (o.L, 10, o.L.size()) < 1e-8, "Mix 0% is clean dry", "");
    }
    { // 3. Decay calibration
        bool ok = true; juce::String s;
        for (float d : { 1.0f, 2.0f, 5.0f })
        {
            P p; setP (p, "algorithm", 2); setP (p, "mix", 1); setP (p, "bassMult", 1.0f); setP (p, "decay", d);
            auto o = render (p, sr, d * 2.5 + 2, impulse);
            const float m = rt60 (o.L, sr, (int) (0.3 * sr));
            ok = ok && std::abs (m - d) / d < 0.12f;
            s << d << "s->" << juce::String (m, 2) << "s ";
        }
        check (ok, "Decay matches setting (+-12%)", s);
    }
    { // 4. Ambience low end no longer booms
        P p; setP (p, "algorithm", 6); setP (p, "mix", 1);
        auto o = render (p, sr, 8, impulse);
        const float full = rt60 (o.L, sr, (int) (0.05 * sr)), low = rt60 (lowpass (o.L, sr, 300), sr, (int) (0.05 * sr));
        check (low < 1.6f, "Ambience low-end tail is short", "full=" + juce::String (full, 2) + "s <300Hz=" + juce::String (low, 2) + "s");
    }
    { // 5. Mono input feeds both sides
        P p;
        P::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::stereo());
        p.setBusesLayout (l);
        setP (p, "mix", 0.3f);
        auto o = render (p, sr, 2, [] (int n, int ch) { return ch == 0 ? (n < 4800 ? std::sin (n * 0.05f) * 0.5f : 0.0f) : 0.37f; });
        const double eL = energy (o.L, 0, o.L.size()), eR = energy (o.R, 0, o.R.size());
        check (std::abs (10 * std::log10 (eL / eR)) < 1.0, "Mono input: L and R balanced", "L/R=" + juce::String (10 * std::log10 (eL / eR), 2) + " dB");
    }
    { // 6. Parameter jumps do not click
        P p; setP (p, "mix", 1);
        float maxJump = 0, base = 0;
        auto o = render (p, sr, 4, [&] (int n, int) { return noiseBurst (n, sr, 0.3f); }, [&] (int pos)
        {
            if (pos == (int) (1.0 * sr) / 256 * 256) { setP (p, "size", 0.3f); setP (p, "preDelayMs", 200.0f); }
            if (pos == (int) (2.0 * sr) / 256 * 256) setP (p, "algorithm", 4);
            if (pos == (int) (3.0 * sr) / 256 * 256) { setP (p, "mix", 0.2f); setP (p, "width", 0.0f); }
        });
        const auto d2 = [&] (size_t i) { return std::abs (o.L[i] - 2 * o.L[i - 1] + o.L[i - 2]); };
        for (size_t i = (size_t) (0.5 * sr); i < (size_t) (0.95 * sr); ++i) base = std::max (base, d2 (i));
        for (size_t i = (size_t) (1.0 * sr) - 512; i < o.L.size(); ++i) maxJump = std::max (maxJump, d2 (i));
        check (maxJump < base * 1.5f, "No clicks on size/pre-delay/algo/mix jumps", "max d2=" + juce::String (maxJump, 4) + " vs steady " + juce::String (base, 4));
    }
    { // 7. Stability with everything at the extremes
        P p; setP (p, "algorithm", 1); setP (p, "mix", 1); setP (p, "size", 1.5f); setP (p, "decay", 15); setP (p, "bassMult", 2.5f);
        setP (p, "diffusion", 1); setP (p, "spin", 1); setP (p, "wander", 1); setP (p, "highCutHz", 18000);
        for (auto* id : { "pumpOn", "shimOn", "riserOn", "monoOn" }) setP (p, id, 1);
        setP (p, "shimMix", 1); setP (p, "riserAmt", 1);
        auto o = render (p, sr, 40, [&] (int n, int) { return noiseBurst (n, sr, 0.5f); });
        bool fin; const float pk = peakOf (o, fin);
        const double first = energy (o.L, (size_t) (1 * sr), (size_t) (3 * sr)), last = energy (o.L, (size_t) (37 * sr), (size_t) (39 * sr));
        check (fin && pk < 4.0f && last < first, "Extreme settings stable (40 s)", "peak=" + juce::String (pk, 2) + " decayed " + juce::String (10 * std::log10 (first / (last + 1e-30)), 1) + " dB");
    }
    { // 8. Shimmer every interval stays stable and shifts pitch
        juce::String s; bool ok = true;
        for (int pi = 0; pi < 4; ++pi)
            for (int glide = 0; glide < 2; ++glide)
            {
                P p; setP (p, "mix", 1); setP (p, "decay", 8); setP (p, "shimOn", 1); setP (p, "shimPitch", (float) pi); setP (p, "shimMix", 1); setP (p, "shimGlide", (float) glide);
                auto o = render (p, sr, 20, [&] (int n, int) { return n < 9600 ? 0.4f * std::sin (n * juce::MathConstants<float>::twoPi * 440.0f / 48000.0f) : 0.0f; });
                bool fin; const float pk = peakOf (o, fin);
                ok = ok && fin && pk < 2.0f;
            }
        check (ok, "Shimmer -12/-7/+7/+12 (+glide) stable", "");
    }
    { // 9. Shimmer -12 moves energy an octave down
        const auto bandRatio = [&] (bool shim)
        {
            P p; setP (p, "mix", 1); setP (p, "decay", 4); setP (p, "shimOn", shim ? 1.0f : 0.0f); setP (p, "shimPitch", 0); setP (p, "shimMix", 0.8f); setP (p, "monoOn", 0);
            auto o = render (p, sr, 3, [&] (int n, int) { return n < 4800 ? 0.4f * std::sin (n * juce::MathConstants<float>::twoPi * 880.0f / 48000.0f) : 0.0f; });
            std::vector<float> tail (o.L.begin() + (int) (1.0 * sr), o.L.begin() + (int) (2.0 * sr));
            const auto band = [&] (float f) { haldendsp::BiquadFilter a, b; a.setLowPass (f * 1.2f, (float) sr, 4); b.setHighPass (f / 1.2f, (float) sr, 4); double e = 0; for (float x : tail) { float y = b.process (a.process (x)); e += y * y; } return e; };
            return 10 * std::log10 (band (440) / band (880));
        };
        const double off = bandRatio (false), on = bandRatio (true);
        check (on - off > 6.0, "Shimmer -12 adds octave-down energy", "440/880 Hz: " + juce::String (off, 1) + " -> " + juce::String (on, 1) + " dB");
    }
    { // 10. Freeze holds the tail
        P p; setP (p, "mix", 1); setP (p, "decay", 1.0f); setP (p, "freezeFade", 0.05f);
        auto o = render (p, sr, 12, [&] (int n, int) { return noiseBurst (n, sr, 0.5f); }, [&] (int pos) { if (pos >= (int) (0.6 * sr)) setP (p, "freezeOn", 1); });
        const double a = energy (o.L, (size_t) (1 * sr), (size_t) (2 * sr)), b = energy (o.L, (size_t) (10 * sr), (size_t) (11 * sr));
        check (10 * std::log10 (a / b) < 3.0, "Freeze holds tail (decay 1 s, 10 s later)", "loss " + juce::String (10 * std::log10 (a / b), 2) + " dB");
    }
    { // 10b. Glide bends the tail an octave down after the phrase
        P p; setP (p, "mix", 1); setP (p, "decay", 3); setP (p, "shimOn", 1); setP (p, "shimPitch", 0); setP (p, "shimMix", 1); setP (p, "shimGlide", 1);
        auto o = render (p, sr, 3.8, [&] (int n, int) { return n < 19200 ? 0.5f * std::sin (n * juce::MathConstants<float>::twoPi * 440.0f / 48000.0f) : 0.0f; });
        std::vector<float> tail (o.L.begin() + (int) (2.8 * sr), o.L.begin() + (int) (3.6 * sr));
        const auto band = [&] (float f) { haldendsp::BiquadFilter a, b; a.setLowPass (f * 1.1f, (float) sr, 4); b.setHighPass (f / 1.1f, (float) sr, 4); double e = 0; for (float x : tail) { float y = b.process (a.process (x)); e += y * y; } return e; };
        const double r = 10 * std::log10 (band (220) / band (440));
        check (r > 20.0, "Glide -12 moves the tail an octave down", juce::String (r, 1) + " dB (220 vs 440 Hz)");
    }
    { // 10b2. Glide leaves the tail alone while the phrase is still sounding (same spectrum as no glide)
        const auto ratio = [&] (bool glide)
        {
            P p; setP (p, "mix", 1); setP (p, "decay", 3); setP (p, "shimOn", glide ? 1.0f : 0.0f); setP (p, "shimPitch", 0); setP (p, "shimMix", 1); setP (p, "shimGlide", 1); setP (p, "monoOn", 1);
            auto o = render (p, sr, 2.0, [&] (int n, int) { return 0.4f * std::sin (n * juce::MathConstants<float>::twoPi * 440.0f / 48000.0f); });
            std::vector<float> tail (o.L.begin() + (int) (1.0 * sr), o.L.begin() + (int) (2.0 * sr));
            const auto band = [&] (float f) { haldendsp::BiquadFilter a, b; a.setLowPass (f * 1.1f, (float) sr, 4); b.setHighPass (f / 1.1f, (float) sr, 4); double e = 0; for (float x : tail) { float y = b.process (a.process (x)); e += y * y; } return e; };
            return 10 * std::log10 (band (440) / band (220));
        };
        const double plain = ratio (false), glide = ratio (true);
        check (std::abs (glide - plain) < 1.5, "Glide does not bend while singing", "440/220 Hz: plain " + juce::String (plain, 1) + " dB, glide " + juce::String (glide, 1) + " dB");
    }
    { // 10c. Gate opens on quiet material too (-36 dBFS)
        P p; setP (p, "mix", 1); setP (p, "decay", 4); setP (p, "gateOn", 1); setP (p, "gateLen", 1); setP (p, "gateRelease", 10);
        auto o = render (p, sr, 2, [&] (int n, int) { const int ph = n % 24000; return ph < 3800 ? 0.016f * std::sin (n * 0.0432f) : 0.0f; });
        const double open = energy (o.L, (size_t) (1.02 * sr), (size_t) (1.2 * sr)), closed = energy (o.L, (size_t) (1.46 * sr), (size_t) (1.50 * sr));
        check (open > 1e-6 && 10 * std::log10 (open / 4.5 / (closed + 1e-30)) > 30, "Gate triggers on quiet input", juce::String (10 * std::log10 (open / (closed + 1e-30)), 1) + " dB open/closed");
    }
    { // 10d. Gate stays open through a legato phrase
        P p; setP (p, "mix", 1); setP (p, "decay", 4); setP (p, "gateOn", 1); setP (p, "gateLen", 0); setP (p, "gateRelease", 20);
        auto o = render (p, sr, 2.5, [&] (int n, int) { const float f = n < 24000 ? 220.0f : n < 48000 ? 277.0f : n < 72000 ? 330.0f : 0.0f; return f > 0 ? 0.3f * std::sin (n * juce::MathConstants<float>::twoPi * f / 48000.0f) : 0.0f; });
        double minDb = 0;
        for (double t0 = 0.3; t0 < 1.45; t0 += 0.05)
        {
            const double e = energy (o.L, (size_t) (t0 * sr), (size_t) ((t0 + 0.05) * sr)) / (0.05 * sr);
            minDb = std::min (minDb, 10 * std::log10 (e + 1e-30));
        }
        check (minDb > -40.0, "Gate open through legato notes", "quietest 50 ms: " + juce::String (minDb, 1) + " dBFS");
    }
    { // 11. Gate closes the tail after length + release
        P p; setP (p, "mix", 1); setP (p, "decay", 6); setP (p, "gateOn", 1); setP (p, "gateLen", 1); setP (p, "gateRelease", 30);
        auto o = render (p, sr, 2, [&] (int n, int) { return noiseBurst (n, sr, 0.05f); });
        const double open = energy (o.L, (size_t) (0.05 * sr), (size_t) (0.2 * sr)) / (0.15 * sr), closed = energy (o.L, (size_t) (0.6 * sr), (size_t) (1.5 * sr)) / (0.9 * sr);
        check (10 * std::log10 (open / (closed + 1e-30)) > 40, "Gate (1/8 @120bpm) cuts the tail", juce::String (10 * std::log10 (open / (closed + 1e-30)), 1) + " dB down");
    }
    { // 12. Pump modulates at the tempo (1/4 @120 = 2 Hz)
        P p; setP (p, "mix", 1); setP (p, "decay", 10); setP (p, "pumpOn", 1); setP (p, "pumpDepth", 1);
        auto o = render (p, sr, 4, [&] (int n, int) { return noiseBurst (n, sr, 0.2f); });
        const size_t q = (size_t) (sr * 0.5);
        double lo = 1e9, hi = 0;
        for (size_t s = (size_t) (1.0 * sr); s + q <= (size_t) (3.0 * sr); s += q)
        {
            const double dip = energy (o.L, s, s + q / 8), body = energy (o.L, s + q / 2, s + q);
            lo = std::min (lo, dip / (q / 8.0)); hi = std::max (hi, body / (q / 2.0));
        }
        check (10 * std::log10 (hi / lo) > 12, "Pump dips on every beat", juce::String (10 * std::log10 (hi / lo), 1) + " dB depth");
    }
    { // 13. Ducking reports reduction and the in/out gain works
        P p; setP (p, "ducking", 1); setP (p, "mix", 0.5f);
        render (p, sr, 1, [] (int n, int) { return 0.5f * std::sin (n * 0.03f); });
        const float gr = p.duckReductionDb.load();
        check (gr > 15.0f && gr < 25.0f, "Ducking reduction reported", juce::String (gr, 1) + " dB");

        P q; setP (q, "mix", 0); setP (q, "outGain", -6.0f);
        auto o = render (q, sr, 0.3, [] (int, int) { return 0.5f; });
        check (std::abs (o.L.back() - 0.5f * 0.501187f) < 1e-3f, "Output gain -6 dB", juce::String (o.L.back(), 4));
    }
    { // 14. Tail length follows decay
        P p; setP (p, "decay", 15); setP (p, "bassMult", 2.5f);
        check (p.getTailLengthSeconds() > 60.0, "Tail length follows decay x bass", juce::String (p.getTailLengthSeconds(), 1) + " s");
    }
    { // 15. CPU
        P p; for (auto* id : { "pumpOn", "shimOn", "riserOn", "monoOn", "gateOn", "eqEnabled" }) setP (p, id, 1); setP (p, "riserAmt", 0.5f);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        render (p, sr, 20, [&] (int n, int) { return 0.1f * std::sin (n * 0.01f); });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (ms < 20000 * 0.25, "CPU, all FX on (20 s audio)", juce::String (ms, 0) + " ms = " + juce::String (ms / 200.0, 1) + "% of one core");
    }

    { // 15b. CPU in HaalAi (analyser + 3 dynamic bells + static curve)
        P p; setP (p, "algorithm", 7);
        Voice v (sr);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        render (p, sr, 20, [&] (int n, int ch) { static float last = 0; if (ch == 1) return last; last = v.next (n, true); return last; });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (ms < 20000 * 0.25, "CPU, HaalAi (20 s audio)", juce::String (ms, 0) + " ms = " + juce::String (ms / 200.0, 1) + "% of one core");
    }
    { // 16. Turning Size during a tail does not bend the pitch of the notes
        const auto tonalShare = [&] (const std::vector<float>& x, int from)
        {
            constexpr int order = 15, N = 1 << order;
            juce::dsp::FFT f (order);
            std::vector<float> buf (2 * N, 0.0f);
            for (int i = 0; i < N && from + i < (int) x.size(); ++i)
                buf[(size_t) i] = x[(size_t) (from + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
            f.performFrequencyOnlyForwardTransform (buf.data());
            const double bin = sr / N; double c = 0, w = 0;
            for (int k = (int) (300 / bin); k <= (int) (650 / bin); ++k) { const double e = (double) buf[(size_t) k] * buf[(size_t) k]; w += e; if (std::abs (k * bin - 440.0) <= 6.0) c += e; }
            return c / w;
        };
        const auto run = [&] (bool sweep)
        {
            P p; setP (p, "mix", 1); setP (p, "decay", 6); setP (p, "spin", 0); setP (p, "wander", 0); setP (p, "algorithm", 2);
            auto o = render (p, sr, 3.0, [&] (int n, int) { return n < 48000 ? 0.4f * std::sin (n * juce::MathConstants<float>::twoPi * 440.0f / 48000.0f) : 0.0f; },
                             [&] (int pos) { if (sweep && pos >= 48000 && pos < 96000) setP (p, "size", 0.5f + 0.7f * (float) (pos - 48000) / 48000.0f); });
            return tonalShare (o.L, 52000);
        };
        const double still = run (false), moving = run (true);
        check (moving > 0.8 * still, "Size sweep keeps the note in tune", juce::String (100.0 * moving, 1) + " % at 440 Hz while turning vs " + juce::String (100.0 * still, 1) + " % still");
    }
    { // 17. HaalAi learns the voice and cuts its boom and ringing resonance
        P p; setP (p, "algorithm", 7); setP (p, "mix", 1);
        Voice v (sr);
        haldendsp::BiquadFilter boom, ring;
        boom.setPeak (350.0f, (float) sr, 3.0f, 8.0f); ring.setPeak (2800.0f, (float) sr, 6.0f, 9.0f);
        render (p, sr, 9.0, [&] (int n, int ch) { static float last = 0; if (ch == 1) return last; last = ring.process (boom.process (v.next (n, true))); return last; });
        const int nb = p.haalNumStatic.load();
        bool boomCut = false, ringCut = false; juce::String s;
        for (int b = 0; b < nb; ++b)
        {
            const float f = p.haalStaticF[(size_t) b].load(), g = p.haalStaticG[(size_t) b].load();
            s << juce::roundToInt (f) << "Hz " << juce::String (g, 1) << "dB  ";
            if (std::abs (std::log2 (f / 350.0f)) < 0.35f && g < -2.0f) boomCut = true;
            if (std::abs (std::log2 (f / 2800.0f)) < 0.3f && g < -2.0f) ringCut = true;
        }
        check (p.haalProgress.load() >= 1.0f && boomCut && ringCut, "HaalAi curve cuts 350 Hz boom and 2.8 kHz ring", s);

        // the learned curve survives saving and reopening the project
        juce::MemoryBlock mb; p.getStateInformation (mb);
        P q; q.setStateInformation (mb.getData(), (int) mb.getSize()); setP (q, "algorithm", 7);
        render (q, sr, 0.2, [] (int, int) { return 0.0f; });
        check (q.haalNumStatic.load() == nb && ! q.haalLearning.load(), "HaalAi curve is restored with the project", juce::String (q.haalNumStatic.load()) + " bands");
    }
    { // 18. D1-D3 follow a harsh resonance that comes and goes, max 2 dB
        P p; setP (p, "algorithm", 7); setP (p, "mix", 1);
        Voice v (sr);
        haldendsp::BiquadFilter harsh;
        float harshHz = 3500.0f;
        double onHit = 0, onTotal = 0, offRelease = 0, offTotal = 0; float maxCut = 0;
        const auto stepFn = [&] (int pos)
        {
            const double t = pos / sr;
            if (t < 6.0) return;
            const bool on = std::fmod (t, 1.0) < 0.5;
            harshHz = 4200.0f * std::pow (2.0f, 0.2f * (float) std::sin (t * 0.9));   // the resonance wanders
            harsh.setPeak (harshHz, (float) sr, 8.0f, on ? 14.0f : 0.0f);
            const double ph = std::fmod (t, 0.5);
            bool tracked = false; float cut = 0;
            for (int b = 0; b < 3; ++b)
            {
                const float f = p.haalDynF[(size_t) b].load(), g = p.haalDynG[(size_t) b].load();
                maxCut = std::min (maxCut, g);
                if (std::abs (std::log2 (f / harshHz)) < 0.2f && g < -0.8f) tracked = true;
                if (std::abs (std::log2 (f / harshHz)) < 0.2f) cut = std::min (cut, g);
            }
            if (ph > 0.15)
            {
                if (on) { onTotal++; if (tracked) onHit++; }
                else    { offTotal++; if (cut > -0.5f) offRelease++; }
            }
        };
        render (p, sr, 12.0, [&] (int n, int ch) { static float last = 0; if (ch == 1) return last; last = harsh.process (v.next (n, true)); return last; }, stepFn);
        check (onHit / std::max (1.0, onTotal) > 0.8, "D bands lock onto the wandering resonance", juce::String (100.0 * onHit / std::max (1.0, onTotal), 0) + " % of the time");
        check (offRelease / std::max (1.0, offTotal) > 0.8, "D bands let go when it disappears", juce::String (100.0 * offRelease / std::max (1.0, offTotal), 0) + " % of the time");
        check (maxCut >= -2.01f, "D bands never cut more than 2 dB", juce::String (maxCut, 2) + " dB");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

// ------------------------------------------------------------------ screenshots
struct Feeder : juce::Thread
{
    explicit Feeder (P& p) : juce::Thread ("feeder"), proc (p) {}
    void run() override
    {
        juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer m; int n = 0;
        Voice v (48000.0);
        haldendsp::BiquadFilter boom, harsh;
        boom.setPeak (330.0f, 48000.0f, 2.5f, 6.0f);
        while (! threadShouldExit())
        {
            harsh.setPeak (3300.0f * std::pow (2.0f, 0.2f * std::sin (n / 48000.0f * 1.3f)), 48000.0f, 7.0f, std::fmod (n / 48000.0f, 1.0f) < 0.6f ? 12.0f : 0.0f);
            for (int i = 0; i < 480; ++i, ++n)
            {
                const float x = 2.0f * harsh.process (boom.process (v.next (n, true)));
                b.setSample (0, i, x); b.setSample (1, i, x * 0.9f);
            }
            proc.processBlock (b, m);
            juce::Thread::sleep (9);
        }
    }
    P& proc;
};

static void shoot (juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

static int runShots (const juce::String& dir)
{
    juce::File out (dir); out.createDirectory();
    P p; p.prepareToPlay (48000.0, 480);
    setP (p, "ducking", 0.35f); setP (p, "pumpOn", 1); setP (p, "shimOn", 1); setP (p, "shimPitch", 0); setP (p, "monoOn", 1); setP (p, "eqEnabled", 0);

    const auto settle = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    Feeder feeder (p); feeder.startThread();

    p.apvts.state.setProperty ("uiDrawerOpen", false, nullptr);
    p.apvts.state.setProperty ("uiRackOpen", false, nullptr);
    {
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        e->setVisible (true); settle (5000);
        shoot (*e, out.getChildFile ("1_main.png"));
    }
    p.apvts.state.setProperty ("uiDrawerOpen", true, nullptr);
    p.apvts.state.setProperty ("uiRackOpen", true, nullptr);
    {
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        e->setVisible (true); settle (4000);
        shoot (*e, out.getChildFile ("2_full.png"));
        // touch Decay -> LCD shows the decay envelope
        for (auto* c : e->getChildComponent (0)->getChildren())
            if (auto* k = dynamic_cast<hui::Knob*> (c))
                if (std::abs (k->slider.getMaximum() - 15.0) < 0.01)
                    k->slider.setValue (4.2);
        settle (700);
        shoot (*e, out.getChildFile ("3_decay_touch.png"));
    }

    {
        // HaalAi: let it learn the feeder for a few seconds, then show the curve and D1-D3
        setP (p, "algorithm", 7); setP (p, "shimOn", 0); setP (p, "pumpOn", 0);
        p.apvts.state.setProperty ("uiDrawerOpen", false, nullptr);
        p.apvts.state.setProperty ("uiRackOpen", false, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        e->setVisible (true); settle (2500);
        shoot (*e, out.getChildFile ("4_haalai_learning.png"));
        settle (5000);
        shoot (*e, out.getChildFile ("5_haalai.png"));
        std::printf ("editor size %dx%d\n", e->getWidth(), e->getHeight());
        for (int b = 0; b < p.haalNumStatic.load(); ++b)
            std::printf ("static band %d: %.0f Hz %.1f dB Q %.2f\n", b, p.haalStaticF[(size_t) b].load(), p.haalStaticG[(size_t) b].load(), p.haalStaticQ[(size_t) b].load());
        std::printf ("air shelf %.1f dB\n", p.haalShelf.load());
    }
    feeder.stopThread (2000);
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 3 && juce::String (argv[1]) == "--shots")
        return runShots (argv[2]);
    return runDsp();
}
