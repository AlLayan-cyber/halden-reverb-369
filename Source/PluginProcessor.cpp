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
} // namespace

// knobs that follow the algorithm (Ambience has its own set); Mix, gains, FX and routing stay put
static const char* const kRecallIds[] = { "preDelayMs", "size", "decay", "highCutHz", "shape", "spread", "diffusion",
                                          "bassMult", "crossoverHz", "spin", "wander", "width", "ducking" };
static const std::vector<std::pair<juce::String, float>>& ambienceDefaults()
{
    static const std::vector<std::pair<juce::String, float>> v {
        { "preDelayMs", 4.0f }, { "size", 0.55f }, { "decay", 0.9f }, { "highCutHz", 9000.0f }, { "shape", 0.25f },
        { "spread", 0.55f }, { "diffusion", 0.70f }, { "bassMult", 1.0f }, { "crossoverHz", 400.0f },
        { "spin", 0.30f }, { "wander", 0.25f }, { "width", 1.10f }, { "ducking", 0.0f } };
    return v;
}

const juce::StringArray& HaldenReverbAudioProcessor::algorithmNames()
{
    static const juce::StringArray names { "Random Hall", "Concert Hall", "Hall", "Chamber", "Room", "Vintage Plate", "Ambience", "HaalAi" };
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
    lastAlgo = juce::roundToInt (apvts.getRawParameterValue ("algorithm")->load());
    apvts.addParameterListener ("algorithm", this);
}

HaldenReverbAudioProcessor::~HaldenReverbAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("algorithm", this);
}

void HaldenReverbAudioProcessor::parameterChanged (const juce::String&, float newValue)
{
    pendingAlgo.store (juce::roundToInt (newValue));
    triggerAsyncUpdate();   // may come from the audio thread: do the work on the message thread
}

void HaldenReverbAudioProcessor::setKnobs (const std::vector<std::pair<juce::String, float>>& values)
{
    for (const auto& [id, v] : values)
        if (auto* q = apvts.getParameter (id))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost (q->convertTo0to1 (v));
            q->endChangeGesture();
        }
}

void HaldenReverbAudioProcessor::handleAsyncUpdate()
{
    const int now = pendingAlgo.load();
    if (now == lastAlgo) return;

    if (now == kAmbienceAlgo)
    {
        juce::StringArray saved;
        for (auto* id : kRecallIds)
            saved.add (juce::String (id) + "=" + juce::String (apvts.getRawParameterValue (id)->load(), 5));
        apvts.state.setProperty ("mainKnobs", saved.joinIntoString (";"), nullptr);
        setKnobs (ambienceDefaults());
    }
    else if (lastAlgo == kAmbienceAlgo)
    {
        std::vector<std::pair<juce::String, float>> restore;
        for (auto& kv : juce::StringArray::fromTokens (apvts.state.getProperty ("mainKnobs").toString(), ";", ""))
            if (kv.contains ("="))
                restore.push_back ({ kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
        if (restore.empty())   // never dialled anything: the plain defaults
            for (auto* id : kRecallIds)
                if (auto* q = apvts.getParameter (id)) restore.push_back ({ id, q->convertFrom0to1 (q->getDefaultValue()) });
        setKnobs (restore);
    }
    lastAlgo = now;
}

void HaldenReverbAudioProcessor::resetAllToDefaults()
{
    const int algo = juce::roundToInt (apvts.getRawParameterValue ("algorithm")->load());
    for (auto* q : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q))
        {
            if (rp->getParameterID() == "algorithm" || rp->getParameterID() == "routing") continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    apvts.state.removeProperty ("mainKnobs", nullptr);
    if (algo == kAmbienceAlgo) setKnobs (ambienceDefaults());
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
    p.push_back (std::make_unique<APF> (pid ("shimMix"), "Shimmer Amount", NRange (0.0f, 1.0f, 0.01f), 0.5f));
    p.push_back (std::make_unique<APB> (pid ("shimGlide"), "Shimmer Glide", false));
    p.push_back (std::make_unique<APB> (pid ("freezeOn"), "Freeze", false));
    p.push_back (std::make_unique<APF> (pid ("freezeFade"), "Freeze Fade", NRange (0.02f, 4.0f, 0.01f, 0.5f), 0.4f));
    p.push_back (std::make_unique<APB> (pid ("riserOn"), "Riser", false));
    p.push_back (std::make_unique<APF> (pid ("riserAmt"), "Riser Amount", NRange (0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back (std::make_unique<APB> (pid ("monoOn"), "Mono Bass", false));
    p.push_back (std::make_unique<APF> (pid ("monoHz"), "Mono Bass Freq", NRange (60.0f, 400.0f, 1.0f, 0.6f), 150.0f));

    // IN SPACE
    p.push_back (std::make_unique<APB> (pid ("spaceOn"), "In Space", false));
    p.push_back (std::make_unique<APF> (pid ("orbit"), "Orbit", NRange (0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<APF> (pid ("spaceSize"), "Space Size", NRange (0.5f, 1.5f, 0.01f), 1.10f));
    p.push_back (std::make_unique<APF> (pid ("spaceDecay"), "Space Decay", NRange (1.0f, 15.0f, 0.01f, 0.6f), 6.5f));
    p.push_back (std::make_unique<APB> (pid ("stardust"), "Stardust +12", false));
    p.push_back (std::make_unique<APC> (pid ("launchBars"), "Launch Length", juce::StringArray { "1 bar", "2 bars", "4 bars", "8 bars" }, 1));
    p.push_back (std::make_unique<APC> (pid ("routing"), "Routing", juce::StringArray { "Insert", "Send" }, 0));

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
    haal.prepare (sampleRate);
    haalHp1.setHighPass (120.0f, static_cast<float> (sampleRate));
    haalHp2.setHighPass (120.0f, static_cast<float> (sampleRate));
    haalLight.setPeak (280.0f, static_cast<float> (sampleRate), 0.7f, -2.0f);   // HaalAi voicing: never heavy in the low-mids
    shiftL.prepare (sampleRate, 0.080);
    shiftR.prepare (sampleRate, 0.080);
    glideL.prepare (sampleRate, 0.080);
    glideR.prepare (sampleRate, 0.080);
    onset.prepare (sampleRate);

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
    shiftL.reset(); shiftR.reset(); glideL.reset(); glideR.reset();
    onset.prepare (currentSampleRate);
    samplesSinceOnset = 1.0e9;
    glideSemis = riserSmooth = glideW = actFast = actPeak = shimEnv = lastWetL = lastWetR = 0.0f;
    tankL = tankR = xoverLpL = xoverLpR = dampLpL = dampLpR = inHpL = inHpR = dryEnv = duckEnv = duckSq = duckHeldDb = duckGrDb = 0.0f;
    duckHold = 0;
    pumpGain = 1.0f;
    orbitS = 0.0f; launchActive = false; spaceWasOn = false;
    spaceDryL.reset(); spaceDryR.reset();
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
    float decay = raw (apvts, "decay") * (1.0f + 2.0f * riser);
    if (raw (apvts, "spaceOn") >= 0.5f && raw (apvts, "orbit") > 0.0f) decay = std::max (decay, raw (apvts, "spaceDecay"));
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
    static constexpr std::array<AlgoProfile, 8> profiles {{
        { 1.00f, 0.32f, 0.85f, 0.35f, 1.15f, 1.00f, 0.55f }, // Random Hall
        { 1.18f, 0.28f, 0.90f, 0.55f, 0.95f, 0.95f, 0.00f }, // Concert Hall
        { 0.92f, 0.36f, 0.84f, 0.25f, 0.85f, 1.00f, 0.00f }, // Hall
        { 0.72f, 0.44f, 0.82f, 0.15f, 0.75f, 1.05f, 0.00f }, // Chamber
        { 0.54f, 0.58f, 0.72f, 0.08f, 0.60f, 0.92f, 0.00f }, // Room
        { 0.68f, 0.18f, 0.94f, 0.02f, 0.80f, 0.82f, 0.00f }, // Vintage Plate
        { 0.42f, 0.85f, 0.28f, 0.05f, 0.40f, 1.10f, 0.00f }, // Ambience
        { 1.06f, 0.10f, 0.96f, 0.70f, 0.32f, 0.62f, 0.20f }  // HaalAi: warm, soft-edged cloud, little detune
    }};

    const int algoIdx = juce::jlimit (0, 7, static_cast<int> (raw (apvts, "algorithm")));
    const bool haalOn = algoIdx == kHaalAlgo;
    const auto& prof = profiles[static_cast<size_t> (algoIdx)];
    const float sr = static_cast<float> (currentSampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // ---- Parameters
    const float riserRaw = raw (apvts, "riserOn") >= 0.5f ? raw (apvts, "riserAmt") : 0.0f;
    riserSmooth += (1.0f - std::exp (-static_cast<float> (numSamples) / (0.12f * static_cast<float> (currentSampleRate)))) * (riserRaw - riserSmooth);
    if (std::abs (riserRaw - riserSmooth) < 1.0e-4f) riserSmooth = riserRaw;
    const float riser = riserSmooth;

    // ---- IN SPACE: ORBIT macro (own size / decay take over the tank as ORBIT leaves 0)
    // switched off = as if IN SPACE was never touched (its knob settings are kept for next time)
    const bool spaceOn = raw (apvts, "spaceOn") >= 0.5f;
    const float orbitParam = spaceOn ? raw (apvts, "orbit") : 0.0f;
    if (! spaceOn) launchActive = false;
    const bool sendMode = raw (apvts, "routing") >= 0.5f;
    const bool stardust = raw (apvts, "stardust") >= 0.5f;
    static constexpr double kLaunchBars[4] = { 1.0, 2.0, 4.0, 8.0 };
    if (spaceGoRequest.exchange (false) && spaceOn)
    {
        if (launchActive) launchActive = false;   // second press cancels
        else
        {
            launchActive = true;
            launchPos = 0.0;
            launchLenBeats = 4.0 * kLaunchBars[juce::jlimit (0, 3, static_cast<int> (raw (apvts, "launchBars")))];
            launchFrom = orbitParam;
        }
    }
    const float spaceE = smoothstep (0.0f, 0.08f, orbitS);   // how much the IN SPACE laws apply
    const float sizeTarget = (raw (apvts, "size") + (raw (apvts, "spaceSize") - raw (apvts, "size")) * spaceE) * prof.tankScale;
    const float decaySec = (raw (apvts, "decay") + (raw (apvts, "spaceDecay") - raw (apvts, "decay")) * spaceE) * (1.0f + 2.0f * riser);
    float bassMult = raw (apvts, "bassMult") * (haalOn ? 0.8f : 1.0f);   // HaalAi keeps lows light
    bassMult += (0.72f - bassMult) * spaceE;                                // IN SPACE: lows fade first, the tail floats
    const float crossoverHz = raw (apvts, "crossoverHz");
    float highCutHz = std::min (sr * 0.45f, raw (apvts, "highCutHz") * prof.highBias);
    highCutHz += (std::min (sr * 0.45f, std::max (highCutHz, 17000.0f)) - highCutHz) * spaceE;   // IN SPACE tail stays open on top
    const float diffusion = std::min (1.0f, raw (apvts, "diffusion") + (haalOn ? 0.1f : 0.0f));
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

    // Shimmer: the wet signal is pitch-shifted and fed back into the reverb input, so each pass
    // through the whole reverb adds a new layer an interval away (classic bloom, not a fast cascade).
    // Glide: after a phrase ends, the tail bends smoothly to the interval; it never moves while singing.
    // Riser adds an octave-up shimmer of its own.
    const bool glideOn = shimOn && shimGlide;
    const bool cascadeOn = shimOn && ! shimGlide;
    const float glideAmt = glideOn ? raw (apvts, "shimMix") : 0.0f;
    float shimAmount = cascadeOn ? raw (apvts, "shimMix") * 0.9f : 0.0f;
    const float semis = cascadeOn ? shimInterval : 12.0f;
    shimAmount = std::min (0.92f, shimAmount + 0.45f * riser + (stardust ? 0.55f * smoothstep (0.45f, 1.0f, orbitS) : 0.0f));
    const float glideSeconds = juce::jlimit (0.5f, 6.0f, raw (apvts, "decay") * 0.8f);
    const float glideSmooth = 1.0f - std::exp (-1.0f / (0.030f * sr));
    const float shimRatioL = std::pow (2.0f, (semis + 0.04f) / 12.0f);   // +-4 cents: wider, less metallic
    const float shimRatioR = std::pow (2.0f, (semis - 0.04f) / 12.0f);
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
        xPre.reset (s, 0.05, std::max (1.0f, raw (apvts, "preDelayMs") * 0.001f * sr));
        xSize.reset (s, 0.2, sizeTarget);
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
        smRiserGain.reset (s, 0.05, juce::Decibels::decibelsToGain (6.0f * riser));
        firstBlock = false;
    }

    float preMs = haalOn ? std::max (raw (apvts, "preDelayMs"), haalMinPreDelayMs()) : raw (apvts, "preDelayMs");
    preMs += ((haalOn ? haalMinPreDelayMs() : 8.0f) - preMs) * spaceE;   // IN SPACE: short pre-delay
    xPre.setTarget (std::max (1.0f, preMs * 0.001f * sr));
    xSize.setTarget (sizeTarget);

    // ---- HaalAi: learning control, static curve smoothing
    if (haalOn)
    {
        if (pendingCurveReady.exchange (false, std::memory_order_acquire))
            haal.setCurve (pendingCurve, pendingNum, pendingShelf);
        if (haalRequestLearn.exchange (false) || (! haal.hasCurve() && ! haal.isLearning()))
            haal.startLearning();
        if (! haalWasActive)
        {
            haal.resetDynamics();
            for (int b = 0; b < kDyn; ++b) { haalDynLogf[(size_t) b] = haal.dyn[(size_t) b].logf; haalDynGain[(size_t) b] = 0.0f; }
        }

        const float a = 1.0f - std::exp (-static_cast<float> (numSamples) / (0.25f * sr));
        for (int b = 0; b < kStatic; ++b)
        {
            const bool used = haal.hasCurve() && b < haal.numStatic;
            const float target = used ? haal.staticBands[(size_t) b].gain : 0.0f;
            auto& cur = haalStaticCur[(size_t) b];
            cur += a * (target - cur);
            const auto& bd = haal.staticBands[(size_t) b];
            haalStaticFilt[(size_t) b].setPeak (used ? bd.freq : 1000.0f, sr, used ? bd.q : 1.0f, cur);
        }
        haalShelfCur += a * ((haal.hasCurve() ? haal.shelfGain : 0.0f) - haalShelfCur);
        haalShelfF.setHighShelf (8000.0f, sr, haalShelfCur);
    }
    else if (pendingCurveReady.exchange (false, std::memory_order_acquire))
    {
        haal.setCurve (pendingCurve, pendingNum, pendingShelf);
    }
    haalWasActive = haalOn;
    const float orbitRise = 1.0f - std::exp (-1.0f / (0.030f * sr));
    const float orbitFall = 1.0f - std::exp (-1.0f / (0.008f * sr));   // the drop is instant
    const float haalFreqA = 1.0f - std::exp (-32.0f / (0.020f * sr));
    const float haalAtkA  = 1.0f - std::exp (-32.0f / (0.006f * sr));
    const float haalRelA  = 1.0f - std::exp (-32.0f / (0.090f * sr));
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
    smRiserGain.setTarget (juce::Decibels::decibelsToGain (6.0f * riser));

    if (eqEnabled)
    {
        const float lo = raw (apvts, "eqLowCutHz"), hi = raw (apvts, "eqHighCutHz");
        eqHpfL.setHighPass (lo, sr); eqHpfR.setHighPass (lo, sr);
        eqLpfL.setLowPass (hi, sr);  eqLpfR.setLowPass (hi, sr);
    }
    const bool riserFilter = riser > 0.001f;
    if (riserFilter)
    {
        const float hp = 20.0f * std::pow (30.0f, riser);   // 20 Hz -> 600 Hz
        riserHpL1.setHighPass (hp, sr); riserHpL2.setHighPass (hp, sr);
        riserHpR1.setHighPass (hp, sr); riserHpR2.setHighPass (hp, sr);
    }
    if (monoBassOn)
    {
        monoSideHp1.setHighPass (monoHz, sr);
        monoSideHp2.setHighPass (monoHz, sr);
    }
    {
        const float shimHp = semis < 0.0f ? 90.0f : 220.0f;
        const float shimLp = semis < 0.0f ? 5000.0f : 8000.0f;
        shimHpL.setHighPass (shimHp, sr); shimHpR.setHighPass (shimHp, sr);
        shimLpL.setLowPass (shimLp, sr); shimLpR.setLowPass (shimLp, sr);
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
    // ducker: RMS detector, hold across the gaps between words, smooth dB attack / slow release
    const float duckDepthDb = 24.0f * ducking;
    const float duckDet = 1.0f - std::exp (-1.0f / (0.010f * sr));
    const float duckAtk = 1.0f - std::exp (-1.0f / (0.015f * sr));
    const float duckRel = 1.0f - std::exp (-1.0f / (0.40f * sr));
    const int duckHoldSamples = static_cast<int> (0.18f * sr);
    const float pumpAlpha = 1.0f - std::exp (-1.0f / (0.003f * sr));
    const float actAtk = 1.0f - std::exp (-1.0f / (0.002f * sr));
    const float actRel = 1.0f - std::exp (-1.0f / (0.040f * sr));
    const float actPeakRel = std::exp (-1.0f / (0.30f * sr));
    const float shimEnvRel = std::exp (-1.0f / (0.25f * sr));
    const float glideFadeIn = 1.0f / (0.15f * sr);
    const float glideFadeOut = 1.0f / (0.12f * sr);
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

        // ORBIT: knob, or the LAUNCH ramp (tempo-synced) up to 100 %, then an instant drop
        float orbitTarget = orbitParam;
        if (launchActive)
        {
            launchPos += beatsPerSample;
            const double pr = launchPos / launchLenBeats;
            if (pr >= 1.0) launchActive = false;
            else orbitTarget = launchFrom + (1.0f - launchFrom) * static_cast<float> (pr);
        }
        orbitS += (orbitTarget < orbitS ? orbitFall : orbitRise) * (orbitTarget - orbitS);
        if (orbitS < 1.0e-4f && orbitTarget <= 0.0f) orbitS = 0.0f;
        if (--spaceCounter <= 0)
        {
            spaceCounter = 32;
            if (orbitS > 0.0f)
                for (auto* c : { &spaceDryL, &spaceDryR })
                    c->setOrbit (orbitS, sr);
        }
        float dryL = outL[i] * inGain;
        float dryR = (outR != nullptr ? outR[i] : outL[i]) * inGain;
        pkInL = std::max (pkInL, std::abs (dryL));
        pkInR = std::max (pkInR, std::abs (dryR));
        // IN SPACE low cut sits at the input: the voice and what feeds the reverb leave together
        if (orbitS > 0.0f)
        {
            dryL = spaceDryL.process (dryL);
            dryR = spaceDryR.process (dryR);
            spaceWasOn = true;
        }
        else if (spaceWasOn)
        {
            spaceDryL.reset(); spaceDryR.reset();
            spaceWasOn = false;
        }

        // Size changes crossfade between two delay lengths instead of stretching the delays,
        // so the notes in the tail keep their pitch while the knob moves.
        xSize.tick (0.02f);
        const float sA = xSize.a, sB = xSize.b, sx = xSize.x;
        // equal-power weights: the two readouts of a dense tail are uncorrelated
        const float wA = std::cos (sx * juce::MathConstants<float>::halfPi), wB = std::sin (sx * juce::MathConstants<float>::halfPi);
        const float sizeParam = xSize.current();
        const float clampA = juce::jlimit (0.45f, 1.40f, sA), clampB = juce::jlimit (0.45f, 1.40f, sB);
        const float freeze = smFreeze.next();
        const float gM = smGMid.next() * (1.0f - freeze) + 0.99995f * freeze;
        const float gL = smGLow.next() * (1.0f - freeze) + 0.99995f * freeze;

        // ---- Ducking envelope (dry level)
        const float dryLvl = std::max (std::abs (dryL), std::abs (dryR));
        dryEnv += (dryLvl > dryEnv ? envAtk : envRel) * (dryLvl - dryEnv);
        float duckGain = 1.0f;
        {
            duckSq += duckDet * (0.5f * (dryL * dryL + dryR * dryR) - duckSq);
            const float lvlDb = 10.0f * std::log10 (duckSq + 1.0e-12f);
            const float target = duckDepthDb * smoothstep (-48.0f, -34.0f, lvlDb);   // any sung level ducks fully
            if (target >= duckHeldDb - 0.5f) { duckHeldDb = std::max (target, duckHeldDb); duckHold = duckHoldSamples; }
            else if (duckHold > 0) --duckHold;                                          // keep it through short gaps
            else duckHeldDb = target;
            duckGrDb += (duckHeldDb > duckGrDb ? duckAtk : duckRel) * (duckHeldDb - duckGrDb);
            if (duckGrDb < 1.0e-4f) duckGrDb = 0.0f;
            if (duckGrDb > 0.0f) duckGain = std::exp (-0.11512925f * duckGrDb);
        }
        minDuckGain = std::min (minDuckGain, duckGain);

        // ---- Phrase activity (level independent): input counts as "sounding" while it is sustained,
        //      i.e. within 6 dB of its short-term peak (a held note yes, a decaying drum hit no).
        //      Glide waits for silence after the phrase.
        actFast += (dryLvl > actFast ? actAtk : actRel) * (dryLvl - actFast);
        actPeak = std::max (actFast, actPeak * actPeakRel);
        const bool hit = onset.process (dryLvl);
        const bool sounding = actFast > 0.0005f && actFast > actPeak * 0.5f;
        if (sounding || hit)
        {
            samplesSinceOnset = 0.0;
        }
        else
        {
            samplesSinceOnset += 1.0;
        }

        // ---- Input conditioning, pre-delay and input diffusion
        inHpL += hpAlpha * (dryL - inHpL);
        inHpR += hpAlpha * (dryR - inHpR);
        if (haalOn)
        {
            // HaalAi send: low cut, learned voice curve, air shelf; analysed before the pre-delay
            float m = 0.5f * ((dryL - inHpL) + (dryR - inHpR));
            m = haalLight.process (haalHp2.process (haalHp1.process (m)));
            for (auto& f : haalStaticFilt) m = f.process (m);
            m = haalShelfF.process (m);
            haal.push (m, sounding);
            preDelayL.write (m);
            preDelayR.write (m);
        }
        else
        {
            preDelayL.write (dryL - inHpL);
            preDelayR.write (dryR - inHpR);
        }
        xPre.tick (1.0f);
        const bool pX = xPre.x > 0.0f;
        const float pA = preDelayL.readFrac (xPre.a), qA = preDelayR.readFrac (xPre.a);
        const float preL = pX ? pA + (preDelayL.readFrac (xPre.b) - pA) * xPre.x : pA;
        const float preR = pX ? qA + (preDelayR.readFrac (xPre.b) - qA) * xPre.x : qA;
        float monoIn = 0.5f * (preL + preR);

        if (haalOn)
        {
            // three dynamic bells (D1-D3) gliding after the detected resonances; the pre-delay
            // (at least 32 ms here) lets them act before the analysed sound reaches the reverb
            if (--haalCounter <= 0)
            {
                haalCounter = 32;
                for (int b = 0; b < kDyn; ++b)
                {
                    const auto& d = haal.dyn[(size_t) b];
                    auto& lf = haalDynLogf[(size_t) b];
                    auto& g = haalDynGain[(size_t) b];
                    lf += haalFreqA * (d.targetLogf - lf);
                    g += (d.targetGain < g ? haalAtkA : haalRelA) * (d.targetGain - g);
                    haalDynFilt[(size_t) b].setPeak (std::exp2 (lf), sr, 4.5f, g);
                }
            }
            for (auto& f : haalDynFilt) monoIn = f.process (monoIn);
        }

        float shimFeed = 0.0f;
        const float shim = smShim.next() * (1.0f - freeze);
        if (shim > 0.0005f)
        {
            const float sL = shimLpL.process (shimHpL.process (shiftL.process (lastWetL, shimRatioL)));
            const float sR = shimLpR.process (shimHpR.process (shiftR.process (lastWetR, shimRatioR)));
            shimFeed = shim * 0.5f * (sL + sR);
            // safety limiter on the feedback path: the loop can never run away
            const float a = std::abs (shimFeed);
            shimEnv = a > shimEnv ? a : shimEnv * shimEnvRel;
            if (shimEnv > 0.35f) shimFeed *= 0.35f / shimEnv;
        }

        float diff = inAp1.process (monoIn + shimFeed, 142.0f * srScale, inDiff1);
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
        const float modDepth = smModDepth.next() * (1.0f - freeze);
        const bool hq = freeze > 0.001f || orbitS > 0.0f;   // Hermite reads keep the top open (Freeze, IN SPACE)

        // ---- Tank (cross-coupled, with optional pitch-shifted feedback for shimmer)
        const float feedIn = diff * (1.0f - freeze);
        const float crossR = tankR, crossL = tankL;

        // inside the feedback loop: linear crossfade (gain never above 1)
        const auto rd = [&] (const haldendsp::DelayLine& line, float base)
        {
            const float a = line.read (base * sA, hq);
            return sx > 0.0f ? a + (line.read (base * sB, hq) - a) * sx : a;
        };
        // output taps: equal-power crossfade (no level dip in the middle)
        const auto tap = [&] (const haldendsp::DelayLine& line, float base)
        {
            const float a = line.read (base * sA, hq);
            return sx > 0.0f ? a * wA + line.read (base * sB, hq) * wB : a;
        };
        const float ss = srScale;

        const float mApL = modApL.processX (feedIn + crossR, 672.0f * ss * clampA + lfoL * modDepth, 672.0f * ss * clampB + lfoL * modDepth, sx, tankDiff1, hq);
        delayL1.write (mApL);
        const float dL1Out = rd (delayL1, 4453.0f * ss);
        xoverLpL += xoverAlpha * (dL1Out - xoverLpL);
        dampLpL += highAlpha * ((dL1Out - xoverLpL) - dampLpL);
        const float dampedL = (xoverLpL * gL + dampLpL * gM) * (1.0f - freeze) + dL1Out * gM * freeze;
        delayL2.write (apL2.processX (dampedL, 1800.0f * ss * sA, 1800.0f * ss * sB, sx, -tankDiff2, hq));
        const float nextTankL = rd (delayL2, 3720.0f * ss);

        const float mApR = modApR.processX (feedIn + crossL, 908.0f * ss * clampA + lfoR * modDepth, 908.0f * ss * clampB + lfoR * modDepth, sx, tankDiff1, hq);
        delayR1.write (mApR);
        const float dR1Out = rd (delayR1, 4217.0f * ss);
        xoverLpR += xoverAlpha * (dR1Out - xoverLpR);
        dampLpR += highAlpha * ((dR1Out - xoverLpR) - dampLpR);
        const float dampedR = (xoverLpR * gL + dampLpR * gM) * (1.0f - freeze) + dR1Out * gM * freeze;
        delayR2.write (apR2.processX (dampedR, 2656.0f * ss * sA, 2656.0f * ss * sB, sx, -tankDiff2, hq));
        const float nextTankR = rd (delayR2, 3163.0f * ss);

        tankL = haldendsp::flushDenormal (nextTankL);
        tankR = haldendsp::flushDenormal (nextTankR);

        float wetL =
            0.6f * tap (delayR1, 266.0f * ss) +
            0.6f * tap (delayR1, 2974.0f * ss) -
            0.5f * tap (apR2.delay, 1235.0f * ss) +
            0.5f * tap (delayR2, 1996.0f * ss) -
            0.5f * tap (delayL1, 1990.0f * ss) -
            0.4f * tap (apL2.delay, 187.0f * ss) -
            0.4f * tap (delayL2, 1066.0f * ss);

        float wetR =
            0.6f * tap (delayL1, 353.0f * ss) +
            0.6f * tap (delayL1, 3627.0f * ss) -
            0.5f * tap (apL2.delay, 1228.0f * ss) +
            0.5f * tap (delayL2, 2673.0f * ss) -
            0.5f * tap (delayR1, 2111.0f * ss) -
            0.4f * tap (apR2.delay, 335.0f * ss) -
            0.4f * tap (delayR2, 121.0f * ss);

        const float erW = smErWeight.next() * (1.0f - freeze);
        const float lateW = smLateWeight.next();
        wetL = erL * erW + wetL * 0.42f * lateW;
        wetR = erR * erW + wetR * 0.42f * lateW;
        lastWetL = haldendsp::flushDenormal (wetL);
        lastWetR = haldendsp::flushDenormal (wetR);

        // ---- FX: Shimmer Glide (tail bends to the interval after each phrase)
        if (glideOn || glideW > 0.0f)
        {
            const float silentSec = static_cast<float> (samplesSinceOnset / sr) - 0.12f;
            const bool gliding = glideOn && freeze < 0.5f && silentSec > 0.0f;
            if (gliding)
            {
                const float progress = smoothstep (0.0f, 1.0f, silentSec / glideSeconds);
                glideSemis += glideSmooth * (shimInterval * progress - glideSemis);
                glideW = std::min (1.0f, glideW + glideFadeIn);
            }
            else
            {
                // new phrase (or Glide switched off): keep the bent tail where it is and fade it out,
                // then reset quietly. The pitch never jumps.
                glideW = std::max (0.0f, glideW - glideFadeOut);
                if (glideW <= 0.0f) glideSemis = 0.0f;
            }
            const float ratio = std::pow (2.0f, glideSemis / 12.0f);
            const float gl = glideL.process (wetL, ratio);
            const float gr = glideR.process (wetR, ratio);
            const float w = glideW * (glideOn ? glideAmt : 1.0f);
            wetL = wetL * (1.0f - w) + gl * w;
            wetR = wetR * (1.0f - w) + gr * w;
        }

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

        // ---- FX: Pump (tempo-synced volume shape on the tail)
        float pumpTarget = 1.0f;
        if (pumpOn)
        {
            double ph = ppq / pumpBeats;
            ph -= std::floor (ph);
            const float rise = smoothstep (0.06f, 0.72f, static_cast<float> (ph));
            pumpTarget = 1.0f - pumpDepth * (1.0f - rise * rise);
        }
        pumpGain += pumpAlpha * (pumpTarget - pumpGain);
        ppq += beatsPerSample;

        const float tailGain = pumpGain * duckGain;
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

        // ---- Mix (equal power: 100% = fully wet; SEND = always fully wet) and output gain
        const float mix = sendMode ? 1.0f : smMix.next();
        float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
        if (orbitS > 0.0f)
        {
            const float e = smoothstep (0.0f, 0.08f, orbitS);
            if (sendMode)
                wetG *= 1.0f + (juce::Decibels::decibelsToGain (6.0f * orbitS) - 1.0f) * e;   // the space grows on the bus
            else
            {
                // insert: dry stays at full level (already cut from below), the space around it grows
                const float spaceWet = juce::Decibels::decibelsToGain (kSpaceWetDb0 + kSpaceWetDbPerUnit * orbitS);
                dryG += (1.0f - dryG) * e;
                wetG += (spaceWet - wetG) * e;
            }
        }
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

    spaceOrbitNow.store (orbitS, std::memory_order_relaxed);
    spaceLaunchProgress.store (launchActive ? static_cast<float> (launchPos / launchLenBeats) : 0.0f, std::memory_order_relaxed);

    haalLearning.store (haal.isLearning(), std::memory_order_relaxed);
    haalProgress.store (haal.learnProgress(), std::memory_order_relaxed);
    if (haal.hasCurve())
    {
        haalNumStatic.store (haal.numStatic, std::memory_order_relaxed);
        for (int b = 0; b < kStatic; ++b)
        {
            haalStaticF[(size_t) b].store (haal.staticBands[(size_t) b].freq, std::memory_order_relaxed);
            haalStaticG[(size_t) b].store (haal.staticBands[(size_t) b].gain, std::memory_order_relaxed);
            haalStaticQ[(size_t) b].store (haal.staticBands[(size_t) b].q, std::memory_order_relaxed);
        }
        haalShelf.store (haal.shelfGain, std::memory_order_relaxed);
    }
    for (int b = 0; b < kDyn; ++b)
    {
        haalDynF[(size_t) b].store (std::exp2 (haalDynLogf[(size_t) b]), std::memory_order_relaxed);
        haalDynG[(size_t) b].store (haalOn ? haalDynGain[(size_t) b] : 0.0f, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* HaldenReverbAudioProcessor::createEditor()
{
    return new HaldenReverbAudioProcessorEditor (*this);
}

void HaldenReverbAudioProcessor::publishHaalCurve()
{
    const int n = haalNumStatic.load();
    if (haalProgress.load() < 1.0f || haalLearning.load()) return;
    juce::StringArray parts;
    parts.add (juce::String (haalShelf.load(), 3));
    for (int b = 0; b < n; ++b)
        parts.add (juce::String (haalStaticF[(size_t) b].load(), 1) + ":" + juce::String (haalStaticG[(size_t) b].load(), 3) + ":" + juce::String (haalStaticQ[(size_t) b].load(), 3));
    apvts.state.setProperty ("haalaiCurve", parts.joinIntoString (";"), nullptr);
}

void HaldenReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    publishHaalCurve();
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HaldenReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        // a loaded project is taken as it is: no Ambience recall for this change
        lastAlgo = juce::roundToInt (apvts.getRawParameterValue ("algorithm")->load());
        pendingAlgo.store (lastAlgo);
        const auto curve = apvts.state.getProperty ("haalaiCurve").toString();
        if (curve.isNotEmpty())
        {
            auto parts = juce::StringArray::fromTokens (curve, ";", "");
            pendingShelf = parts[0].getFloatValue();
            pendingNum = 0;
            for (int i = 1; i < parts.size() && pendingNum < kStatic; ++i)
            {
                auto v = juce::StringArray::fromTokens (parts[i], ":", "");
                if (v.size() == 3)
                    pendingCurve[(size_t) pendingNum++] = { v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue() };
            }
            pendingCurveReady.store (true, std::memory_order_release);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HaldenReverbAudioProcessor();
}
