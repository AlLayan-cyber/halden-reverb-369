// FxLab: renders the plugin with given parameters to a WAV for analysis.
// usage: FxLab out.wav seconds input levelDb [param=value ...] [@t:param=value ...]
//   input: sine440 | noise | chord | pulses
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 5) { std::printf ("usage\n"); return 1; }
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const double secs = juce::String (argv[2]).getDoubleValue();
    const juce::String input (argv[3]);
    const float level = juce::Decibels::decibelsToGain (juce::String (argv[4]).getFloatValue());
    const double sr = 48000.0; const int block = 256;

    HaldenReverbAudioProcessor p;
    struct Change { double t; juce::String id; float v; };
    std::vector<Change> changes;
    const auto setP = [&p] (const juce::String& id, float v)
    {
        auto* q = p.apvts.getParameter (id);
        if (q == nullptr) { std::printf ("unknown param %s\n", id.toRawUTF8()); std::exit (2); }
        q->setValueNotifyingHost (q->convertTo0to1 (v));
    };
    for (int i = 5; i < argc; ++i)
    {
        juce::String a (argv[i]);
        if (a.startsWith ("@"))
        {
            const double t = a.substring (1).upToFirstOccurrenceOf (":", false, false).getDoubleValue();
            const auto kv = a.fromFirstOccurrenceOf (":", false, false);
            changes.push_back ({ t, kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
        }
        else setP (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    }

    p.prepareToPlay (sr, block);
    const int N = (int) (sr * secs);
    juce::AudioBuffer<float> all (2, N), b (2, block);
    juce::MidiBuffer m; juce::Random rng (3);
    double vPhase = 0.0;
    haldendsp::BiquadFilter fA, fB, fC;   // vowel "ah" formants
    fA.setLowPass (800.0f, (float) sr, 6.0f); fB.setLowPass (1150.0f, (float) sr, 5.0f); fC.setLowPass (2900.0f, (float) sr, 4.0f);
    for (int pos = 0; pos < N; pos += block)
    {
        const int n = std::min (block, N - pos);
        for (auto& c : changes) if (c.t >= 0 && pos >= (int) (c.t * sr)) { setP (c.id, c.v); c.t = -1; }
        b.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const int s = pos + i; const double t = s / sr; float x = 0;
            if (input == "sine440") x = t < 0.4 ? std::sin (juce::MathConstants<double>::twoPi * 440.0 * t) : 0.0;
            else if (input == "noise") x = t < 0.3 ? rng.nextFloat() * 2 - 1 : 0.0f;
            else if (input == "chord") { if (t < 0.6) for (double f : { 220.0, 277.18, 329.63 }) x += (float) std::sin (juce::MathConstants<double>::twoPi * f * t) / 3; }
            else if (input == "vocal")
            {
                // sung phrase: 4 notes (A3 C#4 E4 A4) with vibrato and formants, then 2.4 s of silence
                const double cyc = std::fmod (t, 4.4);
                static const double notes[] = { 220.0, 277.18, 329.63, 440.0 };
                const int k = (int) (cyc / 0.5);
                const double nt = cyc - k * 0.5;
                if (k < 4 && nt < 0.44)
                {
                    const double f0 = notes[k] * (1.0 + 0.012 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t));
                    vPhase += f0 / sr; vPhase -= std::floor (vPhase);
                    const double env = std::min (1.0, nt / 0.03) * std::min (1.0, (0.44 - nt) / 0.06);
                    float saw = (float) (2.0 * vPhase - 1.0);
                    x = (float) env * (fA.process (saw) * 0.9f + fB.process (saw) * 0.5f + fC.process (saw) * 0.25f);
                }
            }
            else if (input == "drums")
            {
                const double ph = std::fmod (t, 0.5);   // 120 bpm quarter notes, snare on 2 and 4
                const int beat = (int) std::fmod (t / 0.5, 4.0);
                if (beat % 2 == 0) x = (float) (std::sin (juce::MathConstants<double>::twoPi * (50.0 + 120.0 * std::exp (-ph * 40)) * ph) * std::exp (-ph * 18));
                else x = (float) ((rng.nextFloat() * 2 - 1) * std::exp (-ph * 25) * 0.7 + std::sin (juce::MathConstants<double>::twoPi * 190.0 * ph) * std::exp (-ph * 30) * 0.4);
                if (t > 4.0) x = 0;
            }
            else if (input == "pulses") { const double ph = std::fmod (t, 0.5); x = ph < 0.08 ? (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * t) * (1.0f - (float) ph / 0.08f) : 0.0f; }
            x *= level;
            b.setSample (0, i, x); b.setSample (1, i, x);
        }
        p.processBlock (b, m);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, b, c, 0, n);
    }
    out.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (out), sr, 2, 32, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, N);
    return 0;
}
