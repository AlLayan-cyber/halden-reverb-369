#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NRange = juce::NormalisableRange<float>;

juce::ParameterID pid (const char* id) { return { id, 1 }; }

float raw (const juce::AudioProcessorValueTreeState& s, const char* id)
{
    return s.getRawParameterValue (id)->load();
}

float smoothstep (float e0, float e1, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

constexpr float kShimSemis[4] = { -12.0f, -7.0f, 7.0f, 12.0f };
constexpr double kPumpBeats[4] = { 2.0, 1.0, 0.5, 0.25 };   // 1/2, 1/4, 1/8, 1/16
constexpr double kGateBeats[4] = { 0.25, 0.5, 1.0, 2.0 };   // 1/16, 1/8, 1/4, 1/2
} // namespace

const juce::StringArray& HaldenReverbAudioProcessor::algorithmNames()
{
    static const juce::StringArray names { "Random Hall", "Concert Hall", "Hall", "Chamber", "Room", "Vintage Plate", "Ambience" };
    return names;
}

HaldenReverbAudioProcessor::HaldenReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& v : inPeak)  v.store (0.0f);
    for (auto& v : outPeak) v.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout HaldenReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<APC> (pid ("algorithm"), "Algorithm", algorithmNames(), 0));
    p.push_back (std::make_unique<APF> (pid ("preDelayMs"), "Pre-Delay", NRange (0.0f, 250.0f, 0.5f), 28.0f));
    p.push_back (std::make_unique<APF> (pid ("size"), "Size", NRange (0.25f, 1.50f, 0.01f), 0.85f));
    p.push_back (std::make_unique<APF> (pid ("decay"), "Decay", NRange (0.20f, 15.0f, 0.01f, 0.55f), 3.10f));
    p.push_back (std::make_unique<APF> (pid ("bassMult"), "Bass Mult", NRange (0.40f, 2.50f, 0.01f), 1.30f));
    p.push_back (std::make_unique<APF> (pid ("crossoverHz"), "Crossover", NRange (80.0f, 1600.0f, 1.0f, 0.5f), 360.0f));
    p.push_back (std::make_unique<APF> (pid ("highCutHz"), "Damping", NRange (1200.0f, 18000.0f, 10.0f, 0.5f), 6800.0f));
    p.push_back (std::make_unique<APF> (pid ("diffusion"), "Diffusion", NRange (0.0f, 1.0f, 0.01f), 0.82f));
    p.push_back (std::make_unique<APF> (pid ("shape"), "Shape", NRange (0.0f, 1.0f, 0.01f), 0.42f));
    p.push_back (std::make_unique<APF> (pid ("spread"), "Spread", NRange (0.0f, 1.0f, 0.01f), 0.65f));
    p.push_back (std::make_unique<APF> (pid ("spin"), "Spin", NRange (0.0f, 1.0f, 0.01f), 0.52f));
    p.push_back (std::make_unique<APF> (pid ("wander"), "Wander", NRange (0.0f, 1.0f, 0.01f), 0.48f));
    p.push_back (std::make_unique<APF> (pid ("width"), "Width", NRange (0.0f, 1.50f, 0.01f), 1.05f));
    p.push_back (std::make_unique<APF> (pid ("ducking"), "Ducking", NRange (0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back (std::make_unique<APB> (pid ("eqEnabled"), "Post EQ", false));
    p.push_back (std::make_unique<APF> (pid ("eqLowCutHz"), "EQ Low Cut", NRange (20.0f, 2000.0f, 1.0f, 0.45f), 500.0f));
    p.push_back (std::make_unique<APF> (pid ("eqHighCutHz"), "EQ High Cut", NRange (1000.0f, 20000.0f, 10.0f, 0.45f), 6000.0f));
    p.push_back (std::make_unique<APF> (pid ("mix"), "Mix", NRange (0.0f, 1.0f, 0.01f), 0.25f));
    p.push_back (std::make_unique<APF> (pid ("inGain"), "Input", NRange (-24.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<APF> (pid ("outGain"), "Output", NRange (-24.0f, 12.0f, 0.1f), 0.0f));

    // FX rack
    p.push_back (std::make_unique<APB> (pid ("pumpOn"), "Pump", false));
    p.push_back (std::make_unique<APC> (pid ("pumpRate"), "Pump Rate", juce::StringArray { "1/2", "1/4", "1/8", "1/16" }, 1));
    p.push_back (std::make_unique<APF> (pid ("pumpDepth"), "Pump Depth", NRange (0.0f, 1.0f, 0.01f), 0.6f));
    p.push_back (std::make_unique<APB> (pid ("shimOn"), "Shimmer", false));
    p.push_back (std::make_unique<APC> (pid ("shimPitch"), "Shimmer Pitch", juce::StringArray { "-12", "-7", "+7", "+12" }, 3));
    p.push_back (std::make_unique<APF> (pid ("shimMix"), "Shimmer Amount", NRange (0.0f, 1.0f, 0.01f), 0.3f));
    p.push_back (std::make_unique<APB> (pid ("shimGlide"), "Shimmer Glide", false));
    p.push_back (std::make_unique<APB> (pid ("freezeOn"), "Freeze", false));
    p.push_back (std::make_unique<APF> (pid ("freezeFade"), "Freeze Fade", NRange (0.02f, 4.0f, 0.01f, 0.5f), 0.4f));
    p.push_back (std::make_unique<APB> (pid ("riserOn"), "Riser", false));
    p.push_back (std::make_unique<APF> (pid ("riserAmt"), "Riser Amount", NRange (0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back (std::make_unique<APB> (pid ("monoOn"), "Mono Bass", false));
    p.push_back (std::make_unique<APF> (pid ("monoHz"), "Mono Bass Freq", NRange (60.0f, 400.0f, 1.0f, 0.6f), 150.0f));
    p.push_back (std::make_unique<APB> (pid ("gateOn"), "Gate", false));
    p.push_back (std::make_unique<APC> (pid ("gateLen"), "Gate Length", juce::StringArray { "1/16", "1/8", "1/4", "1/2" }, 1));
    p.push_back (std::make_unique<APF> (pid ("gateRelease"), "Gate Release", NRange (5.0f, 400.0f, 1.0f, 0.5f), 60.0f));

    return { p.begin(), p.end() };
}

void HaldenReverbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    scopeSampleRate.store (sampleRate);
    srScale = static_cast<float> (sampleRate / 29761.0);

    const auto sz = [this] (float base) { return static_cast<int> (std::ceil (base * srScale)); };
    preDelayL.resize (static_cast<int> (std::ceil (sampleRate * 0.30)));
    preDelayR.resize (static_cast<int> (std::ceil (sampleRate * 0.30)));
    earlyDelay.resize (static_cast<int> (std::ceil (sampleRate * 0.25)));

    inAp1.resize (sz (1200.0f)); inAp2.resize (sz (1200.0f));
    inAp3.resize (sz (2000.0f)); inAp4.resize (sz (2000.0f));
    modApL.resize (sz (3500.0f)); modApR.resize (sz (3500.0f));
    delayL1.resize (sz (10000.0f)); delayR1.resize (sz (10000.0f));
    apL2.resize (sz (6000.0f)); apR2.resize (sz (6000.0f));
    delayL2.resize (sz (9000.0f)); delayR2.resize (sz (9000.0f));
    shiftL.prepare (sampleRate);
    shiftR.prepare (sampleRate);

    firstBlock = true;
    releaseResources();
}

void HaldenReverbAudioProcessor::releaseResources()
{
    preDelayL.reset(); preDelayR.reset(); earlyDelay.reset();
    inAp1.reset(); inAp2.reset(); inAp3.reset(); inAp4.reset();
    modApL.reset(); modApR.reset();
    delayL1.reset(); delayR1.reset();
    apL2.reset(); apR2.reset();
    delayL2.reset(); delayR2.reset();
    for (auto* f : { &eqHpfL, &eqHpfR, &eqLpfL, &eqLpfR, &riserHpL1, &riserHpR1, &riserHpL2, &riserHpR2,
                     &monoSideHp1, &monoSideHp2, &shimHpL, &shimHpR, &shimLpL, &shimLpR })
        f->reset();
    shiftL.reset(); shiftR.reset();
    tankL = tankR = xoverLpL = xoverLpR = dampLpL = dampLpR = inHpL = inHpR = dryEnv = duckEnv = 0.0f;
    pumpGain = gateGain = 1.0f;
    gateOpenSamples = 0;
    gateArmed = true;
    scopeScratchCount = 0;
    for (auto& v : inPeak)  v.store (0.0f);
    for (auto& v : outPeak) v.store (0.0f);
    duckReductionDb.store (0.0f);
}

bool HaldenReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

double HaldenReverbAudioProcessor::getTailLengthSeconds() const
{
    if (raw (apvts, "freezeOn") >= 0.5f)
        return 120.0;
    const float riser = raw (apvts, "riserOn") >= 0.5f ? raw (apvts, "riserAmt") : 0.0f;
    const float decay = raw (apvts, "decay") * (1.0f + 2.0f * riser);
    const float lowMult = std::max (1.0f, raw (apvts, "bassMult"));
    return static_cast<double> (decay * lowMult * 1.7f + raw (apvts, "preDelayMs") * 0.001f + 0.5f);
}

void HaldenReverbAudioProcessor::pushToScope (const float* data, int num) noexcept
{
    const auto scope = scopeFifo.write (num);
    if (scope.blockSize1 > 0) std::copy (data, data + scope.blockSize1, scopeBuffer.begin() + scope.startIndex1);
    if (scope.blockSize2 > 0) std::copy (data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2, scopeBuffer.begin() + scope.startIndex2);
}

void HaldenReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() == 0) return;

    // Mono input on a stereo output: duplicate the input so both sides carry the dry signal
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    struct AlgoProfile { float tankScale, erWeight, lateWeight, bloomBias, modBias, highBias, randomMod; };
    static constexpr std::array<AlgoProfile, 7> profiles {{
        { 1.00f, 0.32f, 0.85f, 0.35f, 1.15f, 1.00f, 0.55f }, // Random Hall
        { 1.18f, 0.28f, 0.90f, 0.55f, 0.95f, 0.95f, 0.00f }, // Concert Hall
        { 0.92f, 0.36f, 0.84f, 0.25f, 0.85f, 1.00f, 0.00f }, // Hall
        { 0.72f, 0.44f, 0.82f, 0.15f, 0.75f, 1.05f, 0.00f }, // Chamber
        { 0.54f, 0.58f, 0.72f, 0.08f, 0.60f, 0.92f, 0.00f }, // Room
        { 0.68f, 0.18f, 0.94f, 0.02f, 0.80f, 0.82f, 0.00f }, // Vintage Plate
        { 0.42f, 0.85f, 0.28f, 0.05f, 0.40f, 1.10f, 0.00f }  // Ambience
    }};

    const int algoIdx = juce::jlimit (0, 6, static_cast<int> (raw (apvts, "algorithm")));
    const auto& prof = profiles[static_cast<size_t> (algoIdx)];
    const float sr = static_cast<float> (currentSampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // ---- Parameters
    const float riser = raw (apvts, "riserOn") >= 0.5f ? raw (apvts, "riserAmt") : 0.0f;
    const float sizeTarget = raw (apvts, "size") * prof.tankScale * (1.0f + 0.18f * riser);
    const float decaySec = raw (apvts, "decay") * (1.0f + 2.0f * riser);
    const float bassMult = raw (apvts, "bassMult");
    const float crossoverHz = raw (apvts, "crossoverHz");
    const float highCutHz = std::min (sr * 0.45f, raw (apvts, "highCutHz") * prof.highBias);
    const float diffusion = raw (apvts, "diffusion");
    const float shape = raw (apvts, "shape");
    const float spread = raw (apvts, "spread");
    const float spin = raw (apvts, "spin");
    const float wander = raw (apvts, "wander");
    const float ducking = raw (apvts, "ducking");
    const bool eqEnabled = raw (apvts, "eqEnabled") >= 0.5f;

    const bool pumpOn = raw (apvts, "pumpOn") >= 0.5f;
    const double pumpBeats = kPumpBeats[juce::jlimit (0, 3, static_cast<int> (raw (apvts, "pumpRate")))];
    const float pumpDepth = raw (apvts, "pumpDepth");
    const bool shimOn = raw (apvts, "shimOn") >= 0.5f;
    const float shimInterval = kShimSemis[juce::jlimit (0, 3, static_cast<int> (raw (apvts, "shimPitch")))];
    const bool shimGlide = raw (apvts, "shimGlide") >= 0.5f;
    const bool freezeOn = raw (apvts, "freezeOn") >= 0.5f;
    const bool gateOn = raw (apvts, "gateOn") >= 0.5f;
    const double gateBeats = kGateBeats[juce::jlimit (0, 3, static_cast<int> (raw (apvts, "gateLen")))];
    const float gateRelMs = raw (apvts, "gateRelease");

    // Shimmer amount and interval (Riser adds an octave-up shimmer on its own)
    float shimAmount = shimOn ? raw (apvts, "shimMix") * 0.7f : 0.0f;
    float semis = shimOn ? shimInterval : 12.0f;
    shimAmount = std::min (0.85f, shimAmount + 0.30f * riser);
    if (shimOn && shimGlide)
        semis = shimInterval < 0.0f ? -1.0f : 1.0f;   // small step per pass = continuous glide
    const float shimRatio = std::pow (2.0f, semis / 12.0f);
    const bool monoBassOn = raw (apvts, "monoOn") >= 0.5f || (shimOn && shimInterval < 0.0f);
    const float monoHz = raw (apvts, "monoHz");

    // ---- Tempo
    double bpm = 120.0, hostPpq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
            playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    const double beatsPerSample = bpm / 60.0 / currentSampleRate;
    double ppq = playing ? hostPpq : freeRunPpq;
    const int gateLenSamples = static_cast<int> (gateBeats * 60.0 / bpm * currentSampleRate);

    // ---- Derived tank values
    const float loopSec = std::max (0.04f, (4453.0f + 3720.0f + 672.0f + 1800.0f) * srScale * sizeTarget / sr);
    float gMid, gLow;
    if (algoIdx == 6) // Ambience: short, controlled tail including the low end
    {
        gMid = std::min (0.45f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec * 0.35f)));
        gLow = std::min (0.50f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec * 0.35f * bassMult)));
    }
    else
    {
        gMid = std::min (0.988f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec)));
        gLow = std::min (0.993f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec * bassMult)));
    }

    const float bloomShape = std::min (1.0f, shape * 0.75f + prof.bloomBias * 0.45f);
    const float modDepthTarget = (2.5f + spin * 14.0f + wander * 16.0f) * srScale * prof.modBias;

    if (firstBlock)
    {
        const double s = currentSampleRate;
        smPreDelay.reset (s, 0.15, raw (apvts, "preDelayMs") * 0.001f * sr);
        smSize.reset (s, 0.25, sizeTarget);
        smGMid.reset (s, 0.05, gMid);
        smGLow.reset (s, 0.05, gLow);
        smErWeight.reset (s, 0.05, prof.erWeight * (1.15f - bloomShape * 0.45f));
        smLateWeight.reset (s, 0.05, prof.lateWeight);
        smModDepth.reset (s, 0.10, modDepthTarget);
        smMix.reset (s, 0.03, raw (apvts, "mix"));
        smWidth.reset (s, 0.03, raw (apvts, "width"));
        smInGain.reset (s, 0.03, juce::Decibels::decibelsToGain (raw (apvts, "inGain")));
        smOutGain.reset (s, 0.03, juce::Decibels::decibelsToGain (raw (apvts, "outGain")));
        smShim.reset (s, 0.08, shimAmount);
        smFreeze.reset (s, raw (apvts, "freezeFade"), freezeOn ? 1.0f : 0.0f);
        smRiserWidth.reset (s, 0.05, 0.5f * riser);
        smRiserGain.reset (s, 0.05, 1.0f + 0.4f * riser);
        firstBlock = false;
    }

    smPreDelay.setTarget (std::max (1.0f, raw (apvts, "preDelayMs") * 0.001f * sr));
    smSize.setTarget (sizeTarget);
    smGMid.setTarget (gMid);
    smGLow.setTarget (gLow);
    smErWeight.setTarget (prof.erWeight * (1.15f - bloomShape * 0.45f));
    smLateWeight.setTarget (prof.lateWeight);
    smModDepth.setTarget (modDepthTarget);
    smMix.setTarget (raw (apvts, "mix"));
    smWidth.setTarget (raw (apvts, "width"));
    smInGain.setTarget (juce::Decibels::decibelsToGain (raw (apvts, "inGain")));
    smOutGain.setTarget (juce::Decibels::decibelsToGain (raw (apvts, "outGain")));
    smShim.setTarget (shimAmount);
    smFreeze.steps = std::max (1, static_cast<int> (currentSampleRate * raw (apvts, "freezeFade")));
    smFreeze.setTarget (freezeOn ? 1.0f : 0.0f);
    smRiserWidth.setTarget (0.5f * riser);
    smRiserGain.setTarget (1.0f + 0.4f * riser);

    if (eqEnabled)
    {
        const float lo = raw (apvts, "eqLowCutHz"), hi = raw (apvts, "eqHighCutHz");
        eqHpfL.setHighPass (lo, sr); eqHpfR.setHighPass (lo, sr);
        eqLpfL.setLowPass (hi, sr);  eqLpfR.setLowPass (hi, sr);
    }
    const bool riserFilter = riser > 0.001f;
    if (riserFilter)
    {
        const float hp = 20.0f * std::pow (75.0f, riser * riser);
        riserHpL1.setHighPass (hp, sr); riserHpL2.setHighPass (hp, sr);
        riserHpR1.setHighPass (hp, sr); riserHpR2.setHighPass (hp, sr);
    }
    if (monoBassOn)
    {
        monoSideHp1.setHighPass (monoHz, sr);
        monoSideHp2.setHighPass (monoHz, sr);
    }
    {
        const float shimHp = semis < 0.0f ? 110.0f : 280.0f;
        shimHpL.setHighPass (shimHp, sr); shimHpR.setHighPass (shimHp, sr);
        shimLpL.setLowPass (9000.0f, sr); shimLpR.setLowPass (9000.0f, sr);
    }

    const float inDiff1 = 0.25f + diffusion * 0.50f;
    const float inDiff2 = 0.18f + diffusion * 0.45f;
    const float tankDiff1 = 0.35f + diffusion * 0.35f;
    const float tankDiff2 = 0.25f + diffusion * 0.25f;

    const float xoverAlpha = 1.0f - std::exp ((-twoPi * crossoverHz) / sr);
    const float highAlpha = 1.0f - std::exp ((-twoPi * highCutHz) / sr);
    const float hpAlpha = 1.0f - std::exp ((-twoPi * 35.0f) / sr);
    const float envAtk = 1.0f - std::exp (-1.0f / (0.004f * sr));
    const float envRel = 1.0f - std::exp (-1.0f / (0.18f * sr));
    const float pumpAlpha = 1.0f - std::exp (-1.0f / (0.003f * sr));
    const float gateAtk = 1.0f - std::exp (-1.0f / (0.001f * sr));
    const float gateRel = 1.0f - std::exp (-1.0f / (gateRelMs * 0.001f * sr));
    const float randAlpha = 1.0f - std::exp (-1.0f / (0.09f * sr));
    const int randPeriod = std::max (1, static_cast<int> (sr * 0.07f));

    const float spinInc = (twoPi * (0.15f + spin * 1.35f)) / sr;
    const float wanderInc = (twoPi * (0.04f + wander * 0.38f)) / sr;

    static constexpr std::array<float, 8> erTapRatios { 0.07f, 0.13f, 0.21f, 0.31f, 0.43f, 0.57f, 0.74f, 0.92f };

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float minDuckGain = 1.0f;
    float pkInL = 0.0f, pkInR = 0.0f, pkOutL = 0.0f, pkOutR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inGain = smInGain.next();
        const float dryL = outL[i] * inGain;
        const float dryR = (outR != nullptr ? outR[i] : outL[i]) * inGain;
        pkInL = std::max (pkInL, std::abs (dryL));
        pkInR = std::max (pkInR, std::abs (dryR));

        const float sizeParam = smSize.next();
        const float tankClampSize = juce::jlimit (0.45f, 1.40f, sizeParam);
        const float freeze = smFreeze.next();
        const float gM = smGMid.next() * (1.0f - freeze) + 0.99995f * freeze;
        const float gL = smGLow.next() * (1.0f - freeze) + 0.99995f * freeze;
        const float shim = smShim.next();

        // ---- Ducking envelope (dry level), also used as the gate trigger
        const float dryLvl = std::max (std::abs (dryL), std::abs (dryR));
        dryEnv += (dryLvl > dryEnv ? envAtk : envRel) * (dryLvl - dryEnv);
        const float duckGain = ducking > 0.001f
            ? std::max (0.063f, 1.0f / (1.0f + dryEnv * ducking * 22.0f))
            : 1.0f;
        minDuckGain = std::min (minDuckGain, duckGain);

        // ---- Input conditioning, pre-delay and input diffusion
        inHpL += hpAlpha * (dryL - inHpL);
        inHpR += hpAlpha * (dryR - inHpR);
        preDelayL.write (dryL - inHpL);
        preDelayR.write (dryR - inHpR);
        const float preSamples = smPreDelay.next();
        const float preL = preDelayL.readFrac (preSamples);
        const float preR = preDelayR.readFrac (preSamples);
        const float monoIn = 0.5f * (preL + preR);

        float diff = inAp1.process (monoIn, 142.0f * srScale, inDiff1);
        diff = inAp2.process (diff, 107.0f * srScale, inDiff1);
        diff = inAp3.process (diff, 379.0f * srScale, inDiff2);
        diff = inAp4.process (diff, 277.0f * srScale, inDiff2);

        // ---- Early reflections
        earlyDelay.write (0.65f * diff + 0.35f * monoIn);
        const float erSpanMs = (22.0f + spread * 95.0f) * (0.6f + sizeParam * 0.4f);
        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < 8; ++t)
        {
            const float tapMs = 4.0f + erTapRatios[static_cast<size_t> (t)] * erSpanMs;
            const float tapVal = earlyDelay.readFrac ((tapMs * 0.001f) * sr);
            const float pos = static_cast<float> (t) / 7.0f;
            const float envGain = (1.0f - bloomShape) * (1.0f - pos * 0.55f) + bloomShape * (0.25f + pos * 0.75f);
            if ((t & 1) == 0) { erL += tapVal * envGain * 0.22f; erR += tapVal * envGain * 0.14f; }
            else              { erR += tapVal * envGain * 0.22f; erL -= tapVal * envGain * 0.14f; }
        }

        // ---- Modulation (sine LFOs, plus a smoothed random walk for Random Hall)
        spinPhase += spinInc;     if (spinPhase > twoPi) spinPhase -= twoPi;
        wanderPhase += wanderInc; if (wanderPhase > twoPi) wanderPhase -= twoPi;
        if (--randomCounter <= 0)
        {
            randomCounter = randPeriod;
            randomTarget = rng.nextFloat() * 2.0f - 1.0f;
        }
        randomValue += randAlpha * (randomTarget - randomValue);
        const float rm = prof.randomMod;
        const float lfoL = (std::sin (spinPhase) * 0.6f + std::sin (wanderPhase) * 0.4f) * (1.0f - rm) + randomValue * rm;
        const float lfoR = (std::cos (spinPhase) * 0.6f + std::cos (wanderPhase * 0.87f) * 0.4f) * (1.0f - rm) - randomValue * rm;
        const float modDepth = smModDepth.next();

        // ---- Tank (cross-coupled, with optional pitch-shifted feedback for shimmer)
        const float feedIn = diff * (1.0f - freeze);
        float crossR = tankR, crossL = tankL;
        if (shim > 0.0005f)
        {
            const float sR = shimLpR.process (shimHpR.process (shiftR.process (tankR, shimRatio)));
            const float sL = shimLpL.process (shimHpL.process (shiftL.process (tankL, shimRatio)));
            crossR = (1.0f - shim) * tankR + shim * sR;
            crossL = (1.0f - shim) * tankL + shim * sL;
        }

        const float mApL = modApL.process (feedIn + crossR, 672.0f * srScale * tankClampSize + lfoL * modDepth, tankDiff1);
        delayL1.write (mApL);
        const float dL1Out = delayL1.readFrac (4453.0f * srScale * sizeParam);
        xoverLpL += xoverAlpha * (dL1Out - xoverLpL);
        dampLpL += highAlpha * ((dL1Out - xoverLpL) - dampLpL);
        const float dampedL = (xoverLpL * gL + dampLpL * gM) * (1.0f - freeze) + dL1Out * gM * freeze;
        delayL2.write (apL2.process (dampedL, 1800.0f * srScale * sizeParam, -tankDiff2));
        const float nextTankL = delayL2.readFrac (3720.0f * srScale * sizeParam);

        const float mApR = modApR.process (feedIn + crossL, 908.0f * srScale * tankClampSize + lfoR * modDepth, tankDiff1);
        delayR1.write (mApR);
        const float dR1Out = delayR1.readFrac (4217.0f * srScale * sizeParam);
        xoverLpR += xoverAlpha * (dR1Out - xoverLpR);
        dampLpR += highAlpha * ((dR1Out - xoverLpR) - dampLpR);
        const float dampedR = (xoverLpR * gL + dampLpR * gM) * (1.0f - freeze) + dR1Out * gM * freeze;
        delayR2.write (apR2.process (dampedR, 2656.0f * srScale * sizeParam, -tankDiff2));
        const float nextTankR = delayR2.readFrac (3163.0f * srScale * sizeParam);

        tankL = haldendsp::flushDenormal (nextTankL);
        tankR = haldendsp::flushDenormal (nextTankR);

        float wetL =
            0.6f * delayR1.readFrac (266.0f * srScale * sizeParam) +
            0.6f * delayR1.readFrac (2974.0f * srScale * sizeParam) -
            0.5f * apR2.delay.readFrac (1235.0f * srScale * sizeParam) +
            0.5f * delayR2.readFrac (1996.0f * srScale * sizeParam) -
            0.5f * delayL1.readFrac (1990.0f * srScale * sizeParam) -
            0.4f * apL2.delay.readFrac (187.0f * srScale * sizeParam) -
            0.4f * delayL2.readFrac (1066.0f * srScale * sizeParam);

        float wetR =
            0.6f * delayL1.readFrac (353.0f * srScale * sizeParam) +
            0.6f * delayL1.readFrac (3627.0f * srScale * sizeParam) -
            0.5f * apL2.delay.readFrac (1228.0f * srScale * sizeParam) +
            0.5f * delayL2.readFrac (2673.0f * srScale * sizeParam) -
            0.5f * delayR1.readFrac (2111.0f * srScale * sizeParam) -
            0.4f * apR2.delay.readFrac (335.0f * srScale * sizeParam) -
            0.4f * delayR2.readFrac (121.0f * srScale * sizeParam);

        const float erW = smErWeight.next() * (1.0f - freeze);
        const float lateW = smLateWeight.next();
        wetL = erL * erW + wetL * 0.42f * lateW;
        wetR = erR * erW + wetR * 0.42f * lateW;

        // ---- FX: Riser (high-pass sweep, lift and widen)
        const float riserGain = smRiserGain.next();
        if (riserFilter)
        {
            wetL = riserHpL2.process (riserHpL1.process (wetL)) * riserGain;
            wetR = riserHpR2.process (riserHpR1.process (wetR)) * riserGain;
        }

        // ---- FX: Mono Bass (side channel high-passed, so lows are mono)
        float mid = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR);
        if (monoBassOn)
            side = monoSideHp2.process (monoSideHp1.process (side));

        // ---- FX: Gate (input-triggered, tempo-synced hold)
        if (gateOn)
        {
            if (gateArmed && dryEnv > 0.05f) { gateOpenSamples = gateLenSamples; gateArmed = false; }
            if (dryEnv < 0.02f) gateArmed = true;
            const float target = gateOpenSamples > 0 ? 1.0f : 0.0f;
            if (gateOpenSamples > 0) --gateOpenSamples;
            gateGain += (target > gateGain ? gateAtk : gateRel) * (target - gateGain);
        }
        else
        {
            gateGain += gateAtk * (1.0f - gateGain);
        }

        // ---- FX: Pump (tempo-synced volume shape on the tail)
        float pumpTarget = 1.0f;
        if (pumpOn)
        {
            double ph = ppq / pumpBeats;
            ph -= std::floor (ph);
            pumpTarget = 1.0f - pumpDepth * (1.0f - smoothstep (0.0f, 0.6f, static_cast<float> (ph)));
        }
        pumpGain += pumpAlpha * (pumpTarget - pumpGain);
        ppq += beatsPerSample;

        const float tailGain = gateGain * pumpGain * duckGain;
        const float width = smWidth.next() + smRiserWidth.next();
        float finalWetL = (mid + side * width) * tailGain;
        float finalWetR = (mid - side * width) * tailGain;

        if (eqEnabled)
        {
            finalWetL = eqLpfL.process (eqHpfL.process (finalWetL));
            finalWetR = eqLpfR.process (eqHpfR.process (finalWetR));
        }

        scopeScratch[static_cast<size_t> (scopeScratchCount++)] = 0.5f * (finalWetL + finalWetR);
        if (scopeScratchCount == static_cast<int> (scopeScratch.size()))
        {
            pushToScope (scopeScratch.data(), scopeScratchCount);
            scopeScratchCount = 0;
        }

        // ---- Mix (equal power: 100% = fully wet) and output gain
        const float mix = smMix.next();
        const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
        const float outGain = smOutGain.next();
        const float oL = (dryL * dryG + finalWetL * wetG) * outGain;
        const float oR = (dryR * dryG + finalWetR * wetG) * outGain;
        pkOutL = std::max (pkOutL, std::abs (oL));
        pkOutR = std::max (pkOutR, std::abs (oR));

        outL[i] = oL;
        if (outR != nullptr) outR[i] = oR;
    }

    if (! playing)
        freeRunPpq = ppq;

    const auto storeMax = [] (std::atomic<float>& a, float v)
    {
        if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
    };
    storeMax (inPeak[0], pkInL);  storeMax (inPeak[1], pkInR);
    storeMax (outPeak[0], pkOutL); storeMax (outPeak[1], pkOutR);
    duckReductionDb.store (ducking > 0.001f ? -juce::Decibels::gainToDecibels (minDuckGain) : 0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* HaldenReverbAudioProcessor::createEditor()
{
    return new HaldenReverbAudioProcessorEditor (*this);
}

void HaldenReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HaldenReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HaldenReverbAudioProcessor();
}
