#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace haldendsp
{
inline float flushDenormal (float v) noexcept { return std::abs (v) < 1.0e-18f ? 0.0f : v; }

class DelayLine
{
public:
    void resize (int maxSamples)
    {
        buffer.assign (static_cast<size_t> (std::max (256, maxSamples)), 0.0f);
        writeIdx = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

    inline void write (float sample) noexcept
    {
        buffer[static_cast<size_t> (writeIdx)] = sample;
        if (++writeIdx >= static_cast<int> (buffer.size()))
            writeIdx = 0;
    }

    // delaySamples = 1 returns the most recently written sample
    inline float readFrac (float delaySamples) const noexcept
    {
        const int sz = static_cast<int> (buffer.size());
        const float clamped = juce::jlimit (1.0f, static_cast<float> (sz - 3), delaySamples);
        const int intPart = static_cast<int> (clamped);
        const float frac = clamped - static_cast<float> (intPart);
        int idx0 = writeIdx - intPart;
        if (idx0 < 0) idx0 += sz;
        int idx1 = idx0 - 1;
        if (idx1 < 0) idx1 += sz;
        return buffer[static_cast<size_t> (idx0)] * (1.0f - frac)
             + buffer[static_cast<size_t> (idx1)] * frac;
    }

    // 4-point Hermite interpolation: much less high-frequency loss than linear, used while frozen
    inline float readHermite (float delaySamples) const noexcept
    {
        const int sz = static_cast<int> (buffer.size());
        const float clamped = juce::jlimit (2.0f, static_cast<float> (sz - 4), delaySamples);
        const int ip = static_cast<int> (clamped);
        const float f = clamped - static_cast<float> (ip);
        const auto at = [this, sz] (int d) { int i = writeIdx - d; if (i < 0) i += sz; return buffer[static_cast<size_t> (i)]; };
        const float xm1 = at (ip - 1), x0 = at (ip), x1 = at (ip + 1), x2 = at (ip + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    inline float read (float delaySamples, bool hq) const noexcept { return hq ? readHermite (delaySamples) : readFrac (delaySamples); }

    std::vector<float> buffer;
    int writeIdx = 0;
};

class AllpassFilter
{
public:
    void resize (int maxSamples) { delay.resize (maxSamples); }
    void reset() { delay.reset(); }

    inline float process (float input, float delaySamples, float coeff, bool hq = false) noexcept
    {
        const float delayed = delay.read (delaySamples, hq);
        float v = flushDenormal (input - coeff * delayed);
        v = juce::jlimit (-4.0f, 4.0f, v);
        delay.write (v);
        return delayed + coeff * v;
    }

    // Same as process(), but the internal delay is read at two lengths and crossfaded (x: 0 -> 1),
    // so a length change never stretches the delay (no pitch bend).
    inline float processX (float input, float dA, float dB, float x, float coeff, bool hq = false) noexcept
    {
        const float a = delay.read (dA, hq);
        const float delayed = x > 0.0f ? a + (delay.read (dB, hq) - a) * x : a;
        float v = flushDenormal (input - coeff * delayed);
        v = juce::jlimit (-4.0f, 4.0f, v);
        delay.write (v);
        return delayed + coeff * v;
    }

    DelayLine delay;
};

class BiquadFilter
{
public:
    void reset() noexcept { z1 = z2 = 0.0f; }

    void setHighPass (float freq, float sr, float q = 0.70710678f) noexcept
    {
        const float f = juce::jlimit (10.0f, sr * 0.45f, freq);
        const float w0 = juce::MathConstants<float>::twoPi * f / sr;
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw0) * 0.5f) / a0;
        b1 = -(1.0f + cosw0) / a0;
        b2 = b0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass (float freq, float sr, float q = 0.70710678f) noexcept
    {
        const float f = juce::jlimit (10.0f, sr * 0.45f, freq);
        const float w0 = juce::MathConstants<float>::twoPi * f / sr;
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = ((1.0f - cosw0) * 0.5f) / a0;
        b1 = (1.0f - cosw0) / a0;
        b2 = b0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setPeak (float freq, float sr, float q, float gainDb) noexcept
    {
        const float f = juce::jlimit (20.0f, sr * 0.45f, freq);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * f / sr;
        const float alpha = std::sin (w0) / (2.0f * q);
        const float cosw0 = std::cos (w0);
        const float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cosw0) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = b1;
        a2 = (1.0f - alpha / A) / a0;
    }

    void setHighShelf (float freq, float sr, float gainDb) noexcept
    {
        const float f = juce::jlimit (20.0f, sr * 0.45f, freq);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * f / sr;
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / 2.0f * std::sqrt (2.0f);
        const float sq = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + sq;
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + sq) / a0;
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0) / a0;
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - sq) / a0;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * cosw0 - sq) / a0;
    }

    // magnitude response in dB at freq (for drawing curves)
    float magnitudeDb (float freq, float sr) const noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * freq / sr;
        const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2.0 * w);
        const auto num = (double) b0 + (double) b1 * z1c + (double) b2 * z2c;
        const auto den = 1.0 + (double) a1 * z1c + (double) a2 * z2c;
        return static_cast<float> (20.0 * std::log10 (std::abs (num / den) + 1.0e-12));
    }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = flushDenormal (b1 * x - a1 * y + z2);
        z2 = flushDenormal (b2 * x - a2 * y);
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

// Two-grain WSOLA pitch shifter. Hann grains (weights always sum to 1) read a delay line at a
// rotating offset; whenever a grain restarts, its start point is chosen by cross-correlation
// with the grain that is currently playing, so the two stay phase-aligned. This removes the
// warble and comb filtering of a plain delay-line shifter. Safe inside feedback loops.
class PitchShifter
{
public:
    void prepare (double sampleRate, double windowSeconds = 0.080)
    {
        const double k = sampleRate / 48000.0;
        window = static_cast<float> (sampleRate * windowSeconds);
        search = static_cast<int> (window * 0.5f);
        corrLen = static_cast<int> (768.0 * k);
        searchStep = std::max (2, static_cast<int> (4.0 * k));
        delay.resize (static_cast<int> (window) + search + corrLen + 16);
        reset();
    }

    void reset()
    {
        delay.reset();
        phase = 0.0f;
        offset[0] = offset[1] = 0.0f;
        last[0] = 0.0f; last[1] = 0.5f;
        filled = 0;
    }

    inline float process (float x, float ratio) noexcept
    {
        delay.write (x);
        const int size = static_cast<int> (delay.buffer.size());
        if (filled < size) ++filled;

        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            float p = phase + 0.5f * static_cast<float> (k);
            if (p >= 1.0f) p -= 1.0f;
            if (std::abs (p - last[k]) > 0.5f)
                offset[k] = findOffset (k, p);
            last[k] = p;
            const float s = std::sin (juce::MathConstants<float>::pi * p);
            out += s * s * delay.readFrac (2.0f + offset[k] + p * window);
        }
        return out;
    }

private:
    inline float at (int d) const noexcept    // d = 1 is the newest sample
    {
        const int size = static_cast<int> (delay.buffer.size());
        int i = delay.writeIdx - d;
        while (i < 0) i += size;
        return delay.buffer[static_cast<size_t> (i)];
    }

    float findOffset (int k, float p) const noexcept
    {
        if (filled < static_cast<int> (delay.buffer.size()))
            return 0.0f;
        const int o = 1 - k;
        float po = phase + 0.5f * static_cast<float> (o);
        if (po >= 1.0f) po -= 1.0f;
        const int refD = static_cast<int> (2.0f + offset[o] + po * window);
        const int baseD = static_cast<int> (2.0f + p * window);
        float best = -1.0e30f;
        int bestC = 0;
        for (int c = 0; c < search; c += searchStep)
        {
            float acc = 0.0f;
            for (int i = 0; i < corrLen; i += 2)
                acc += at (baseD + c + i) * at (refD + i);
            if (acc > best) { best = acc; bestC = c; }
        }
        return static_cast<float> (bestC);
    }

    DelayLine delay;
    float window = 3840.0f, phase = 0.0f;
    float offset[2] {}, last[2] {};
    int search = 1920, corrLen = 768, searchStep = 4, filled = 0;
};

// Detects note starts from the ratio of a fast and a slow envelope, so it works at any input level.
struct OnsetDetector
{
    void prepare (double sr)
    {
        const auto c = [sr] (double t) { return static_cast<float> (1.0 - std::exp (-1.0 / (t * sr))); };
        fastA = c (0.002); fastR = c (0.040); slowA = c (0.060); slowR = c (0.350);
        fast = slow = 0.0f; armed = true;
    }

    // returns true on the sample where a new onset is detected
    inline bool process (float level) noexcept
    {
        fast += (level > fast ? fastA : fastR) * (level - fast);
        slow += (level > slow ? slowA : slowR) * (level - slow);
        if (armed && fast > 0.0025f && fast > slow * 1.9f) { armed = false; return true; }
        if (! armed && fast < slow * 1.15f) armed = true;
        return false;
    }

    float fast = 0.0f, slow = 0.0f, fastA = 0.0f, fastR = 0.0f, slowA = 0.0f, slowR = 0.0f;
    bool armed = true;
};

// Simple per-sample linear smoother that is cheap enough to run on many values
// A length (delay time / size) that changes by crossfading between the old and the new value
// instead of sliding. Reads use a (old), b (new) and x (0..1).
struct XfadeLength
{
    void reset (double sampleRate, double seconds, float initial)
    {
        steps = std::max (1, static_cast<int> (sampleRate * seconds));
        a = b = target = initial;
        x = 0.0f;
        inc = 1.0f / static_cast<float> (steps);
    }
    void setTarget (float t) noexcept { target = t; }
    // call once per sample
    inline void tick (float tolerance) noexcept
    {
        if (x > 0.0f)
        {
            x += inc;
            if (x >= 1.0f) { a = b; x = 0.0f; }
        }
        else if (std::abs (target - a) > tolerance)
        {
            b = target;
            x = inc;
        }
    }
    inline float current() const noexcept { return a + (b - a) * x; }

    float a = 1.0f, b = 1.0f, target = 1.0f, x = 0.0f, inc = 0.01f;
    int steps = 100;
};

struct Smooth
{
    void reset (double sampleRate, double seconds, float initial)
    {
        steps = std::max (1, static_cast<int> (sampleRate * seconds));
        current = target = initial;
        countdown = 0;
        increment = 0.0f;
    }

    void setTarget (float t) noexcept
    {
        if (juce::exactlyEqual (t, target)) return;
        target = t;
        countdown = steps;
        increment = (target - current) / static_cast<float> (steps);
    }

    inline float next() noexcept
    {
        if (countdown <= 0) return target;
        current += increment;
        if (--countdown == 0) current = target;
        return current;
    }

    float current = 0.0f, target = 0.0f, increment = 0.0f;
    int steps = 1, countdown = 0;
};
} // namespace haldendsp
