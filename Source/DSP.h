#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
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

    std::vector<float> buffer;
    int writeIdx = 0;
};

class AllpassFilter
{
public:
    void resize (int maxSamples) { delay.resize (maxSamples); }
    void reset() { delay.reset(); }

    inline float process (float input, float delaySamples, float coeff) noexcept
    {
        const float delayed = delay.readFrac (delaySamples);
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

// Two-tap rotating delay pitch shifter with triangular crossfade (taps always sum to unity gain,
// so it can sit inside a feedback loop without adding energy).
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        window = static_cast<float> (sampleRate * 0.060);
        delay.resize (static_cast<int> (window) + 16);
        reset();
    }

    void reset()
    {
        delay.reset();
        phase = 0.0f;
    }

    inline float process (float x, float ratio) noexcept
    {
        delay.write (x);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float g1 = 1.0f - std::abs (2.0f * phase - 1.0f);
        const float g2 = 1.0f - std::abs (2.0f * p2 - 1.0f);
        return delay.readFrac (2.0f + phase * window) * g1
             + delay.readFrac (2.0f + p2 * window) * g2;
    }

private:
    DelayLine delay;
    float window = 2880.0f;
    float phase = 0.0f;
};

// Simple per-sample linear smoother that is cheap enough to run on many values
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
