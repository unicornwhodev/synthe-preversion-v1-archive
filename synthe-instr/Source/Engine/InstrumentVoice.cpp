#include "InstrumentVoice.h"
#include "RareInstrumentModel.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace mis
{

namespace
{
std::uint32_t nextVoiceInstanceSerial() noexcept
{
    static std::atomic<std::uint32_t> counter { 0 };
    return counter.fetch_add(1u, std::memory_order_relaxed) + 1u;
}

std::uint32_t hashSeedComponent(std::uint32_t seed, std::uint32_t value) noexcept
{
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

std::uint32_t quantizeSeedFloat(const float value, const float scale) noexcept
{
    return static_cast<std::uint32_t>(std::lround(value * scale));
}

std::uint64_t makeDeterministicVoiceSeed(const InstrumentSettings& settings,
                                        const InstrumentCharacteristics& chars,
                                        const int note,
                                        const float velocity,
                                        const std::uint32_t voiceInstanceSerial,
                                        const std::uint32_t noteTriggerCount) noexcept
{
    juce::ignoreUnused(voiceInstanceSerial);
    std::uint32_t seed = 0x6d2b79f5u;
    seed = hashSeedComponent(seed, static_cast<std::uint32_t>(note & 0xff));
    seed = hashSeedComponent(seed, static_cast<std::uint32_t>(chars.synthesisMode));
    seed = hashSeedComponent(seed, static_cast<std::uint32_t>(chars.partialCount));
    seed = hashSeedComponent(seed, noteTriggerCount);
    seed = hashSeedComponent(seed, quantizeSeedFloat(velocity, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.tuneSemitones, 100.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.exciter, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.body, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.sympathetic, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.strikePosition, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.breathPressure, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.noiseAmount, 1000.0f));
    seed = hashSeedComponent(seed, quantizeSeedFloat(settings.drive, 100.0f));
    return static_cast<std::uint64_t>(seed == 0u ? 0x13579bdfu : seed);
}

std::uint32_t nextRandomU32(std::uint64_t& state) noexcept
{
    std::uint64_t x = state != 0ull ? state : 0x9e3779b97f4a7c15ull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x != 0ull ? x : 0x9e3779b97f4a7c15ull;
    return static_cast<std::uint32_t>((state * 0x2545f4914f6cdd1dull) >> 32);
}

float coeffToTarget(const float seconds, const double sampleRate, const float target) noexcept
{
    const float safeSeconds = std::max(0.0001f, seconds);
    const float safeSampleRate = static_cast<float>(std::max(1.0, sampleRate));
    const float safeTarget = juce::jlimit(1.0e-6f, 0.999f, target);
    return std::exp(std::log(safeTarget) / (safeSeconds * safeSampleRate));
}

float makeSvfCoefficient(const float cutoffHz, const float sampleRate, const float filterQinv)
{
    const float safeCutoff = juce::jlimit(20.0f, sampleRate * 0.45f, cutoffHz);
    const float rawF = 2.0f * std::sin(juce::MathConstants<float>::pi * safeCutoff / sampleRate);
    const float maxF = -filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f);
    return juce::jmin(rawF, maxF * 0.95f);
}

float tanhAntiderivative(const float x) noexcept
{
    const float ax = std::abs(x);
    if (ax > 20.0f)
        return ax - std::log(2.0f);
    return std::log(std::cosh(x));
}

bool isYaybaharLike(const InstrumentCharacteristics& c) noexcept
{
    return c.synthesisMode == SynthesisMode::Bowed && c.inharmonicityB > 0.002f && c.bodyDelayRatio < 0.75f;
}

bool usesRareV2Profile() noexcept
{
    return getRareRenderEngineMode() != RareRenderEngineMode::LegacyFamily;
}

float safeTailDamping(const InstrumentCharacteristics& c) noexcept
{
    if (!usesRareV2Profile())
        return 1.0f;
    return juce::jlimit(0.5f, 2.5f, c.engineTailDamping);
}

float safeDensityLimit(const InstrumentCharacteristics& c) noexcept
{
    if (!usesRareV2Profile())
        return 1.0f;
    return juce::jlimit(0.25f, 1.5f, c.engineDensityLimit);
}

float safePitchFocus(const InstrumentCharacteristics& c) noexcept
{
    if (!usesRareV2Profile())
        return 1.0f;
    return juce::jlimit(0.5f, 2.0f, c.enginePitchFocus);
}

float safeAttackAccent(const InstrumentCharacteristics& c) noexcept
{
    if (!usesRareV2Profile())
        return 1.0f;
    return juce::jlimit(0.5f, 1.8f, c.engineAttackAccent);
}

float safeSpectralMotion(const InstrumentCharacteristics& c) noexcept
{
    if (!usesRareV2Profile())
        return 0.0f;
    return juce::jlimit(0.0f, 1.0f, c.engineSpectralMotion);
}

float rareDedicatedBaseGain(const RareInstrumentAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic: return 0.42f;
        case RareInstrumentAlgorithm::GayageumSilkZitherBend: return 0.34f;
        case RareInstrumentAlgorithm::ChapmanStickTouchboardTap: return 0.32f;
        case RareInstrumentAlgorithm::YayliTanburLongneckBowGlide: return 0.28f;
        case RareInstrumentAlgorithm::CrwthBowedLyreDroneBridge: return 0.29f;
        case RareInstrumentAlgorithm::CarnyxBronzeLipReedHorn: return 0.34f;
        case RareInstrumentAlgorithm::AulosDoubleReedDualBore: return 0.27f;
        case RareInstrumentAlgorithm::FujaraOvertoneFluteOctave: return 0.27f;
        case RareInstrumentAlgorithm::GemshornHornVesselFlute: return 0.30f;
        case RareInstrumentAlgorithm::DiziBambooMembraneBuzz: return 0.25f;
        case RareInstrumentAlgorithm::AngklungShakenBambooTubePair: return 0.006f;
        case RareInstrumentAlgorithm::UduClayHelmholtzHand: return 0.003f;
        case RareInstrumentAlgorithm::PyeongyeongStoneChimeModal: return 0.010f;
        case RareInstrumentAlgorithm::CristalBaschetGlassRodFriction: return 0.008f;
        case RareInstrumentAlgorithm::MbiraMetalLamellaRattle: return 0.36f;
        case RareInstrumentAlgorithm::HandpanSteelShellModal: return 0.006f;
        case RareInstrumentAlgorithm::TheremineAntennaFieldGlide: return 0.30f;
        case RareInstrumentAlgorithm::OndesMartenotRibbonDiffuser: return 1.05f;
        case RareInstrumentAlgorithm::PyrophoneFlameRijkeTube: return 0.34f;
        case RareInstrumentAlgorithm::HydraulophoneWaterJetReed: return 0.34f;
        case RareInstrumentAlgorithm::YaybaharSpringMembraneBow: return 0.34f;
    }

    return 0.20f;
}
}

// =========================================================================
// InstrumentVoiceBase — shared helpers
// =========================================================================

InstrumentVoiceBase::InstrumentVoiceBase()
    : voiceInstanceSerial(nextVoiceInstanceSerial())
{
}

float InstrumentVoiceBase::polyBlep(const float t, const float dt)
{
    if (t < dt)
    {
        const float u = t / dt;
        return u + u - u * u - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        const float u = (t - 1.0f) / dt;
        return u * u + u + u + 1.0f;
    }
    return 0.0f;
}

void InstrumentVoiceBase::seedRandom(const std::uint64_t seed) noexcept
{
    rngState = seed != 0ull ? seed : 0x9e3779b97f4a7c15ull;
    (void) nextRandomFloat();
}

float InstrumentVoiceBase::nextRandomFloat() noexcept
{
    return static_cast<float>(nextRandomU32(rngState) >> 8) * (1.0f / 16777216.0f);
}

float InstrumentVoiceBase::nextRandomBipolar() noexcept
{
    return nextRandomFloat() * 2.0f - 1.0f;
}
float InstrumentVoiceBase::getWaveform(const float phase01, const float morph, const float phaseInc)
{
    const float sine = fastSin(phase01);
    const float tri  = 4.0f * std::abs(phase01 - 0.5f) - 1.0f;
    float saw  = 2.0f * phase01 - 1.0f;
    saw -= polyBlep(phase01, phaseInc);

    if (morph <= 0.5f)
    {
        const float t = morph * 2.0f;
        return sine * (1.0f - t) + tri * t;
    }
    const float t = (morph - 0.5f) * 2.0f;
    return tri * (1.0f - t) + saw * t;
}

float InstrumentVoiceBase::readComb(const float* buf, const int bufSize,
                                    const int writePos, const float delaySamples)
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int   idx0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(idx0);

    auto wrap = [bufSize](int i) -> int {
        return ((i % bufSize) + bufSize) % bufSize;
    };

    const float sm1 = buf[wrap(idx0 - 1)];
    const float s0  = buf[wrap(idx0)];
    const float s1  = buf[wrap(idx0 + 1)];
    const float s2  = buf[wrap(idx0 + 2)];

    // Hermite (cubic) interpolation
    const float c0 = s0;
    const float c1 = 0.5f * (s1 - sm1);
    const float c2 = sm1 - 2.5f * s0 + 2.0f * s1 - 0.5f * s2;
    const float c3 = 0.5f * (s2 - sm1) + 1.5f * (s0 - s1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void InstrumentVoiceBase::forceQuickRelease() noexcept
{
    if (envState == EnvState::Off)
        return;

    envState = EnvState::Release;
    releaseCoeff = coeffToTarget(0.005f, sr, 0.001f);
}

void InstrumentVoiceBase::beginNote(const InstrumentSettings& s,
                                    const int note, const float velocity,
                                    const double sampleRate)
{
    settings   = s;
    chars      = getCharacteristics();
    instrumentIndex = juce::jlimit(0, kNumInstruments - 1, getInstrumentIndex());
    const auto& rareModel = getRareInstrumentModel(instrumentIndex);
    rareAlgorithm = rareModel.algorithm;
    rareDedicatedActive = rareModel.readiness == RareEngineReadiness::DedicatedVoice;
    sr         = std::max(1.0, sampleRate);
    vel        = juce::jlimit(0.0f, 1.0f, velocity);
    midiNote   = note;
    ageSamples = 0;
    seedRandom(makeDeterministicVoiceSeed(settings, chars, note, vel,
                                           voiceInstanceSerial, noteTriggerCount++));

    // Synthesis-mode-dependent velocity curve for realistic dynamics
    switch (chars.synthesisMode)
    {
    case SynthesisMode::Bowed:
        vel = 0.3f + vel * 0.7f;                   // bowed: compressed, can't drop below ~30%
        break;
    case SynthesisMode::Plucked:
        vel = std::pow(vel, 0.7f);                  // plucked: gentle expansion of soft dynamics
        break;
    case SynthesisMode::Blown:
        vel = 0.2f + vel * 0.8f;                    // blown: compressed air column
        break;
    case SynthesisMode::Struck:
        vel = std::pow(vel, 1.5f);                   // struck: expanded dynamic range
        break;
    case SynthesisMode::Electronic:
        break;                                       // linear
    }

    // Pitch
    oscFreqHz = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f + settings.tuneSemitones) / 12.0f);
    oscPhase  = 0.0f;

    // Brightness envelope couples the explicit brightness control with the cutoff parameter.
    const float baseCutoff = juce::jlimit(120.0f, static_cast<float>(sr) * 0.45f, settings.cutoffHz);
    const float brightnessScale = 0.35f + juce::jlimit(0.0f, 1.0f, settings.brightness) * 1.45f;
    brightCutoffTarget  = juce::jlimit(120.0f,
                                       static_cast<float>(sr) * 0.45f,
                                       baseCutoff * brightnessScale * (0.65f + vel * 0.70f));
    brightCutoffCurrent = juce::jlimit(120.0f,
                                       static_cast<float>(sr) * 0.45f,
                                       brightCutoffTarget * (1.02f + settings.exciter * 0.28f + vel * 0.20f));
    {
        const float bDecayMs = (30.0f + vel * 70.0f) * (1.15f - juce::jlimit(0.0f, 1.0f, settings.brightness) * 0.35f);
        brightDecayCoeff = std::exp(-1.0f / (bDecayMs * 0.001f * static_cast<float>(sr)));
    }

    // ADSR coefficients
    const auto fsr = static_cast<float>(sr);
    attackRate = (settings.attackSeconds > 0.0001f)
                     ? 1.0f / (settings.attackSeconds * fsr)
                     : 1.0f;
    {
        float effectiveDecay = settings.decaySeconds;
        if (!chars.sustained)
        {
            const float regFactor = 1.0f - (static_cast<float>(note) - 60.0f) / 88.0f * 0.5f;
            effectiveDecay *= std::max(0.4f, regFactor);
        }
        decayCoeff = std::exp(-1.0f / (std::max(0.01f, effectiveDecay) * fsr));
    }
    releaseCoeff = coeffToTarget(std::max(0.01f, settings.releaseSeconds), sr, std::exp(-1.0f));

    envLevel = 0.0f;
    envState = (settings.attackSeconds > 0.0001f) ? EnvState::Attack : EnvState::Decay;
    if (envState == EnvState::Decay)
        envLevel = 1.0f;

    // SVF filter (with Jury stability guard)
    filterQinv = 1.0f / juce::jmax(0.5f, settings.filterQ);
    filterTargetF = makeSvfCoefficient(settings.cutoffHz, fsr, filterQinv);
    filterF = filterTargetF;
    filterCoeffSmoothing = 1.0f - std::exp(-1.0f / (kFilterSmoothingTimeMs * 0.001f * fsr));
    svfLow  = 0.0f;
    svfBand = 0.0f;
    dcX1    = 0.0f;
    dcY1    = 0.0f;
    outputDcX1 = 0.0f;
    outputDcY1 = 0.0f;
    const float normalisedDc = juce::MathConstants<float>::twoPi * kDcBlockFrequencyHz / fsr;
    dcBlockerCoeff = juce::jlimit(0.0f, 0.99999f, std::exp(-normalisedDc));

    // Pan
    const auto p = juce::jlimit(-1.0f, 1.0f, settings.pan);
    panL = std::sqrt(0.5f * (1.0f - p));
    panR = std::sqrt(0.5f * (1.0f + p));

    // Noise
    noiseLpState = 0.0f;

    rarePhaseA = std::fmod(0.071f + static_cast<float>(instrumentIndex) * 0.037f, 1.0f);
    rarePhaseB = std::fmod(0.173f + static_cast<float>(instrumentIndex) * 0.053f, 1.0f);
    rarePhaseC = std::fmod(0.311f + static_cast<float>(instrumentIndex) * 0.029f, 1.0f);
    rareEnvA = 1.0f;
    rareEnvB = 0.0f;
    rareStateA = 0.0f;
    rareStateB = 0.0f;
    rareStateC = 0.0f;
    rareHold = 0.0f;
    rareSignatureGain = rareDedicatedBaseGain(rareAlgorithm);

    // Per-voice drive ADAA state.
    drivePreviousInput = 0.0f;
    driveHasPreviousInput = false;

    // Vibrato — active for sustained bowed and blown instruments only
    vibratoPhase = 0.0f;
    vibratoAge   = 0;
    if (chars.synthesisMode == SynthesisMode::Bowed || chars.synthesisMode == SynthesisMode::Blown)
    {
        vibratoRateHz       = 4.5f + vel * 1.5f;           // 4.5–6 Hz depending on velocity
        vibratoDepth        = 0.006f / safePitchFocus(chars); // restrained for pitch-focused voices
        vibratoOnsetSamples = static_cast<int>(sr * 0.20f); // fade in after 200 ms
    }
    else
    {
        vibratoDepth = 0.0f;
    }

    // Max duration guard
    maxAgeSamples = static_cast<int>(sr * std::max(0.1f, settings.decaySeconds * 8.0f + settings.releaseSeconds * 3.0f));
    if (maxAgeSamples > static_cast<int>(sr * 600.0))
        maxAgeSamples = static_cast<int>(sr * 600.0);
}

float InstrumentVoiceBase::advanceEnvelope() noexcept
{
    switch (envState)
    {
    case EnvState::Attack:
        envLevel += attackRate;
        if (envLevel >= 1.0f)
        {
            envLevel = 1.0f;
            envState = EnvState::Decay;
        }
        break;
    case EnvState::Decay:
        envLevel = settings.sustainLevel + (envLevel - settings.sustainLevel) * decayCoeff;
        if (envLevel <= settings.sustainLevel + 0.001f)
        {
            envLevel = settings.sustainLevel;
            envState = chars.sustained ? EnvState::Sustain : EnvState::Release;
        }
        break;
    case EnvState::Sustain:
        break;
    case EnvState::Release:
        envLevel *= releaseCoeff;
        if (envLevel < 0.0001f)
        {
            envLevel = 0.0f;
            envState = EnvState::Off;
        }
        break;
    case EnvState::Off:
        break;
    }
    return envLevel;
}

float InstrumentVoiceBase::advanceBrightness() noexcept
{
    brightCutoffCurrent = brightCutoffTarget
                        + (brightCutoffCurrent - brightCutoffTarget) * brightDecayCoeff;
    return std::max(50.0f, brightCutoffCurrent);
}

float InstrumentVoiceBase::applyFilter(float signal) noexcept
{
    const float hp = signal - svfLow - filterQinv * svfBand;
    svfBand += filterF * hp;
    svfLow  += filterF * svfBand;

    // Denormal flush
    if (!(svfBand > kDenormalFloor || svfBand < -kDenormalFloor)) svfBand = 0.0f;
    if (!(svfLow  > kDenormalFloor || svfLow  < -kDenormalFloor)) svfLow  = 0.0f;
    if (!std::isfinite(svfBand)) svfBand = 0.0f;
    if (!std::isfinite(svfLow))  svfLow  = 0.0f;

    return svfLow;
}

void InstrumentVoiceBase::updateFilterCoefficient(const float cutoffHz) noexcept
{
    const auto fsr = static_cast<float>(sr);
    const float safeCutoff = juce::jlimit(20.0f, fsr * 0.45f, std::isfinite(cutoffHz) ? cutoffHz : 1000.0f);
    filterTargetF = makeSvfCoefficient(safeCutoff, fsr, filterQinv);
    if (!std::isfinite(filterTargetF))
        filterTargetF = 0.0f;

    filterF += filterCoeffSmoothing * (filterTargetF - filterF);
    if (!std::isfinite(filterF))
    {
        filterF = filterTargetF;
        svfLow = 0.0f;
        svfBand = 0.0f;
    }
}

float InstrumentVoiceBase::applyDcBlocker(float signal) noexcept
{
    const float x = signal;
    signal = x - dcX1 + dcBlockerCoeff * dcY1;
    dcX1 = x;
    dcY1 = signal;
    if (!(dcY1 > kDenormalFloor || dcY1 < -kDenormalFloor)) dcY1 = 0.0f;
    return signal;
}
float InstrumentVoiceBase::applyOutputDcBlocker(float signal) noexcept
{
    const float x = signal;
    signal = x - outputDcX1 + dcBlockerCoeff * outputDcY1;
    outputDcX1 = x;
    outputDcY1 = signal;
    if (!(outputDcY1 > kDenormalFloor || outputDcY1 < -kDenormalFloor)) outputDcY1 = 0.0f;
    return signal;
}

float InstrumentVoiceBase::applyVoiceDrive(float signal) noexcept
{
    if (settings.drive <= 1.01f)
    {
        driveHasPreviousInput = false;
        return signal;
    }

    const float d = juce::jlimit(1.0f, 24.0f, settings.drive);
    const float x = signal * d;
    float shaped = 0.0f;

    if (!driveHasPreviousInput)
    {
        shaped = std::tanh(x);
        driveHasPreviousInput = true;
    }
    else
    {
        const float dx = x - drivePreviousInput;
        shaped = std::abs(dx) > 1.0e-5f
            ? (tanhAntiderivative(x) - tanhAntiderivative(drivePreviousInput)) / dx
            : std::tanh(0.5f * (x + drivePreviousInput));
        if (!std::isfinite(shaped))
            shaped = std::tanh(x);
    }

    drivePreviousInput = x;
    const float norm = 1.0f / std::max(0.001f, std::tanh(d));
    return shaped * norm;
}

bool InstrumentVoiceBase::usesRareDedicatedAlgorithm() const noexcept
{
    return rareDedicatedActive && getRareRenderEngineMode() != RareRenderEngineMode::LegacyFamily;
}

float InstrumentVoiceBase::applyRareDedicatedSignature(float signal,
                                                       const float env,
                                                       const float phaseInc,
                                                       const float noiseRaw) noexcept
{
    if (!usesRareDedicatedAlgorithm())
        return signal;

    const float fsr = static_cast<float>(std::max(1.0, sr));
    const float inc = juce::jlimit(0.0f, 0.45f, std::abs(phaseInc));
    const float lfoRate = (0.17f + static_cast<float>(instrumentIndex) * 0.013f) / fsr;
    float ratioA = 1.0f;
    float ratioB = 1.5f;
    float ratioC = 0.5f;
    float noiseCoeff = 0.08f;
    float bodyCoeff = 0.025f;
    float dryScale = 0.94f;

    switch (rareAlgorithm)
    {
        case RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic: ratioA = 2.0f; ratioB = 7.0f; ratioC = 0.5f; noiseCoeff = 0.12f; bodyCoeff = 0.030f; dryScale = 0.91f; break;
        case RareInstrumentAlgorithm::GayageumSilkZitherBend: ratioA = 1.006f; ratioB = 2.98f; ratioC = 0.25f; noiseCoeff = 0.09f; bodyCoeff = 0.020f; dryScale = 0.88f; break;
        case RareInstrumentAlgorithm::ChapmanStickTouchboardTap: ratioA = 1.0f; ratioB = 4.0f; ratioC = 0.125f; noiseCoeff = 0.15f; bodyCoeff = 0.014f; dryScale = 0.88f; break;
        case RareInstrumentAlgorithm::YayliTanburLongneckBowGlide: ratioA = 0.995f; ratioB = 2.97f; ratioC = 0.33f; noiseCoeff = 0.10f; bodyCoeff = 0.035f; dryScale = 0.90f; break;
        case RareInstrumentAlgorithm::CrwthBowedLyreDroneBridge: ratioA = 0.5f; ratioB = 2.03f; ratioC = 0.25f; noiseCoeff = 0.12f; bodyCoeff = 0.040f; dryScale = 0.89f; break;
        case RareInstrumentAlgorithm::CarnyxBronzeLipReedHorn: ratioA = 2.0f; ratioB = 5.0f; ratioC = 0.18f; noiseCoeff = 0.18f; bodyCoeff = 0.020f; dryScale = 0.86f; break;
        case RareInstrumentAlgorithm::AulosDoubleReedDualBore: ratioA = 1.006f; ratioB = 3.0f; ratioC = 0.22f; noiseCoeff = 0.13f; bodyCoeff = 0.022f; dryScale = 0.88f; break;
        case RareInstrumentAlgorithm::FujaraOvertoneFluteOctave: ratioA = 2.0f; ratioB = 3.0f; ratioC = 0.16f; noiseCoeff = 0.10f; bodyCoeff = 0.020f; dryScale = 0.89f; break;
        case RareInstrumentAlgorithm::GemshornHornVesselFlute: ratioA = 1.0f; ratioB = 1.5f; ratioC = 0.12f; noiseCoeff = 0.09f; bodyCoeff = 0.045f; dryScale = 0.94f; break;
        case RareInstrumentAlgorithm::DiziBambooMembraneBuzz: ratioA = 2.0f; ratioB = 9.0f; ratioC = 0.28f; noiseCoeff = 0.16f; bodyCoeff = 0.018f; dryScale = 0.88f; break;
        case RareInstrumentAlgorithm::AngklungShakenBambooTubePair: ratioA = 1.0f; ratioB = 2.70f; ratioC = 0.09f; noiseCoeff = 0.20f; bodyCoeff = 0.018f; dryScale = 0.985f; break;
        case RareInstrumentAlgorithm::UduClayHelmholtzHand: ratioA = 0.5f; ratioB = 1.5f; ratioC = 0.10f; noiseCoeff = 0.10f; bodyCoeff = 0.055f; dryScale = 0.990f; break;
        case RareInstrumentAlgorithm::PyeongyeongStoneChimeModal: ratioA = 2.756f; ratioB = 5.404f; ratioC = 0.07f; noiseCoeff = 0.12f; bodyCoeff = 0.018f; dryScale = 0.985f; break;
        case RareInstrumentAlgorithm::CristalBaschetGlassRodFriction: ratioA = 1.5f; ratioB = 4.0f; ratioC = 0.11f; noiseCoeff = 0.09f; bodyCoeff = 0.040f; dryScale = 0.985f; break;
        case RareInstrumentAlgorithm::MbiraMetalLamellaRattle: ratioA = 2.14f; ratioB = 5.2f; ratioC = 0.18f; noiseCoeff = 0.18f; bodyCoeff = 0.020f; dryScale = 0.86f; break;
        case RareInstrumentAlgorithm::HandpanSteelShellModal: ratioA = 2.0f; ratioB = 3.0f; ratioC = 0.08f; noiseCoeff = 0.08f; bodyCoeff = 0.035f; dryScale = 0.990f; break;
        case RareInstrumentAlgorithm::TheremineAntennaFieldGlide: ratioA = 1.0f; ratioB = 1.01f; ratioC = 0.08f; noiseCoeff = 0.03f; bodyCoeff = 0.018f; dryScale = 0.96f; break;
        case RareInstrumentAlgorithm::OndesMartenotRibbonDiffuser: ratioA = 1.0f; ratioB = 1.10f; ratioC = 0.10f; noiseCoeff = 0.06f; bodyCoeff = 0.018f; dryScale = 0.96f; break;
        case RareInstrumentAlgorithm::PyrophoneFlameRijkeTube: ratioA = 2.0f; ratioB = 5.0f; ratioC = 0.31f; noiseCoeff = 0.22f; bodyCoeff = 0.020f; dryScale = 0.85f; break;
        case RareInstrumentAlgorithm::HydraulophoneWaterJetReed: ratioA = 1.5f; ratioB = 3.5f; ratioC = 0.24f; noiseCoeff = 0.24f; bodyCoeff = 0.030f; dryScale = 0.85f; break;
        case RareInstrumentAlgorithm::YaybaharSpringMembraneBow: ratioA = 1.5f; ratioB = 2.8f; ratioC = 0.06f; noiseCoeff = 0.16f; bodyCoeff = 0.055f; dryScale = 0.86f; break;
    }

    rarePhaseA += juce::jlimit(0.0f, 0.45f, inc * ratioA);
    rarePhaseB += juce::jlimit(0.0f, 0.45f, inc * ratioB);
    rarePhaseC += juce::jlimit(0.0f, 0.45f, inc * ratioC + lfoRate);
    rarePhaseA -= std::floor(rarePhaseA);
    rarePhaseB -= std::floor(rarePhaseB);
    rarePhaseC -= std::floor(rarePhaseC);

    rareStateA += noiseCoeff * (noiseRaw - rareStateA);
    rareStateB += bodyCoeff * (signal - rareStateB);
    rareStateC = rareStateC * 0.994f + rareStateB * 0.006f;
    rareEnvA *= 0.99945f;
    rareEnvB += 0.0012f * (env - rareEnvB);

    const float toneA = fastSin(rarePhaseA);
    const float toneB = fastSin(rarePhaseB);
    const float lfo = fastSin(rarePhaseC);
    const float attack = rareEnvA;
    const float flicker = rareStateA * (0.55f + 0.45f * std::abs(lfo));
    const float lfoPos = juce::jlimit(0.0f, 1.0f, 0.5f + 0.5f * lfo);
    const float pressure = juce::jlimit(0.0f, 1.0f, settings.breathPressure);
    const float bowPressure = juce::jlimit(0.0f, 1.0f, settings.bowPressure);
    const float strikePosition = juce::jlimit(0.0f, 1.0f, settings.strikePosition);
    float signature = 0.0f;

    switch (rareAlgorithm)
    {
        case RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic:
        {
            const float keyChatter = attack * std::tanh(noiseRaw * 1.9f + toneB * 0.55f);
            const float sympatheticHalo = fastSin(rarePhaseB + 0.071f) * 0.24f + toneA * 0.16f;
            signature = keyChatter * 0.42f + sympatheticHalo + flicker * (0.20f + bowPressure * 0.18f);
            break;
        }
        case RareInstrumentAlgorithm::GayageumSilkZitherBend:
        {
            const float bendGesture = std::tanh((toneA - rareStateC) * 2.1f + lfo * 0.22f);
            signature = 0.25f * toneA + 0.22f * toneB * (0.62f + 0.38f * lfoPos)
                      + attack * 0.24f * noiseRaw - 0.24f * rareStateB
                      + 0.18f * bendGesture;
            break;
        }
        case RareInstrumentAlgorithm::ChapmanStickTouchboardTap:
        {
            const float tapClick = attack * std::tanh(noiseRaw * 2.6f + toneB * 0.85f);
            const float pickupBite = std::tanh(signal * 3.0f + rareStateB * 1.2f);
            signature = 0.55f * tapClick + 0.19f * toneA + 0.23f * pickupBite + 0.10f * rareStateB;
            break;
        }
        case RareInstrumentAlgorithm::YayliTanburLongneckBowGlide:
        {
            rareHold = rareHold * 0.9992f + (lfo * (0.18f + bowPressure * 0.20f)) * 0.0008f;
            signature = 0.24f * toneA
                      + 0.24f * std::tanh(signal * 1.9f + rareHold * 5.0f)
                      + 0.20f * flicker - 0.14f * rareStateC;
            break;
        }
        case RareInstrumentAlgorithm::CrwthBowedLyreDroneBridge:
            rareHold = rareHold * 0.997f + (toneA + rareStateB * 0.6f) * 0.003f;
            signature = 0.28f * toneA + 0.20f * toneB + 0.34f * rareHold
                      + 0.23f * rareStateC + attack * 0.16f * noiseRaw;
            break;
        case RareInstrumentAlgorithm::CarnyxBronzeLipReedHorn:
        {
            const float pressureBreak = std::tanh(signal * 2.6f + toneB * (0.65f + pressure * 0.55f)
                                                  + flicker * 3.0f);
            signature = 0.47f * pressureBreak + 0.18f * toneA + 0.14f * noiseRaw * pressure;
            break;
        }
        case RareInstrumentAlgorithm::AulosDoubleReedDualBore:
            signature = 0.30f * (toneA - toneB) + 0.32f * std::tanh(flicker * 2.5f + signal * 0.8f);
            break;
        case RareInstrumentAlgorithm::FujaraOvertoneFluteOctave:
            signature = 0.34f * toneA * (0.65f + 0.35f * settings.breathPressure) + 0.20f * toneB + 0.18f * flicker;
            break;
        case RareInstrumentAlgorithm::GemshornHornVesselFlute:
            signature = 0.52f * toneA + 0.36f * rareStateB - 0.16f * toneB + 0.08f * flicker;
            break;
        case RareInstrumentAlgorithm::DiziBambooMembraneBuzz:
            signature = 0.30f * toneA + 0.34f * std::tanh(toneB * 0.85f + flicker * 3.0f) + attack * 0.12f * noiseRaw;
            break;
        case RareInstrumentAlgorithm::AngklungShakenBambooTubePair:
        {
            const float shake = std::pow(lfoPos, 5.0f);
            const float frameKnock = std::tanh((noiseRaw + toneB * 0.35f) * (1.2f + strikePosition));
            rareHold += 0.24f * (shake * frameKnock - rareHold);
            signature = 0.24f * toneA + 0.34f * toneB + 0.78f * rareHold;
            break;
        }
        case RareInstrumentAlgorithm::UduClayHelmholtzHand:
        {
            const float holePop = attack * std::tanh((noiseRaw + rareStateB * 2.0f) * (1.1f + strikePosition));
            signature = 0.46f * toneA + 0.36f * rareStateB - 0.16f * toneB + 0.32f * holePop;
            break;
        }
        case RareInstrumentAlgorithm::PyeongyeongStoneChimeModal:
            signature = 0.42f * toneA + 0.28f * toneB + 0.16f * rareStateC;
            break;
        case RareInstrumentAlgorithm::CristalBaschetGlassRodFriction:
            signature = 0.36f * toneA + 0.28f * toneB + 0.22f * std::tanh(rareStateB * 2.4f + flicker);
            break;
        case RareInstrumentAlgorithm::MbiraMetalLamellaRattle:
        {
            const float rattle = std::tanh(flicker * 4.2f + attack * noiseRaw * 0.55f);
            signature = attack * (0.30f * noiseRaw + 0.18f * toneB)
                      + 0.30f * toneA + 0.46f * rattle + 0.10f * rareStateC;
            break;
        }
        case RareInstrumentAlgorithm::HandpanSteelShellModal:
            rareHold = rareHold * 0.996f + (toneA * 0.6f + toneB * 0.4f) * 0.004f;
            signature = 0.26f * toneA + 0.34f * toneB + 0.30f * rareHold + 0.18f * rareStateC;
            break;
        case RareInstrumentAlgorithm::TheremineAntennaFieldGlide:
            signature = 0.45f * signal * lfo + 0.14f * toneA + 0.08f * rareStateB;
            break;
        case RareInstrumentAlgorithm::OndesMartenotRibbonDiffuser:
        {
            const float lateGate = ageSamples > static_cast<int>(sr * 0.52) ? 1.0f : 0.005f;
            signature = lateGate * (0.70f * rareStateB + 0.18f * signal * lfo + 0.55f * flicker);
            break;
        }
        case RareInstrumentAlgorithm::PyrophoneFlameRijkeTube:
        {
            const float flame = std::tanh(toneA * 0.60f + toneB * 0.46f
                                          + flicker * 4.2f + noiseRaw * (0.25f + 0.75f * lfoPos));
            signature = 0.44f * flame + 0.24f * noiseRaw * (0.3f + 0.7f * lfoPos);
            break;
        }
        case RareInstrumentAlgorithm::HydraulophoneWaterJetReed:
        {
            const float waterJet = std::tanh(flicker * 2.8f + rareStateA * 2.0f
                                             + rareStateB * 1.35f + noiseRaw * pressure * 0.28f);
            signature = 0.24f * toneA + 0.22f * toneB + 0.55f * waterJet + 0.10f * fastSin(rarePhaseC + 0.113f);
            break;
        }
        case RareInstrumentAlgorithm::YaybaharSpringMembraneBow:
            rareHold = rareHold * 0.985f + (toneA + rareStateB * 1.4f) * 0.015f;
            signature = 0.28f * toneA + 0.26f * toneB + 0.48f * rareHold + 0.30f * flicker;
            break;
    }

    float modelScale = getRareRenderEngineMode() == RareRenderEngineMode::V2ModelOnly ? 2.0f : 1.0f;
    if (getRareRenderEngineMode() == RareRenderEngineMode::V2ModelOnly
        && rareAlgorithm == RareInstrumentAlgorithm::PyeongyeongStoneChimeModal)
        modelScale = 6.0f;

    float effectiveDryScale = dryScale;
    if (getRareRenderEngineMode() == RareRenderEngineMode::V2ModelOnly
        && rareAlgorithm == RareInstrumentAlgorithm::PyeongyeongStoneChimeModal)
        effectiveDryScale = 0.960f;

    const float shaped = signal * effectiveDryScale + signature * rareSignatureGain * modelScale * (0.72f + 0.28f * rareEnvB);
    return std::isfinite(shaped) ? juce::jlimit(-6.0f, 6.0f, shaped) : 0.0f;
}

void InstrumentVoiceBase::writeOutput(float* left, float* right,
                                      const int idx, const float signal) const noexcept
{
    left[idx] += signal * panL;
    if (right != nullptr)
        right[idx] += signal * panR;
}

// =========================================================================
//  BowedStringVoiceBase — continuous friction + multi-partial
// =========================================================================

void BowedStringVoiceBase::noteOn(const InstrumentSettings& s,
                                  const int note, const float velocity,
                                  const double sampleRate)
{
    beginNote(s, note, velocity, sampleRate);
    const auto fsr = static_cast<float>(sr);

    // Body comb
    const float bodyFreq = oscFreqHz * chars.bodyDelayRatio;
    bodyDelaySamples = juce::jlimit(2.0f, static_cast<float>(kBodyBufSize - 2),
                                    fsr / std::max(20.0f, bodyFreq));
    const float tailDamping = safeTailDamping(chars);
    const float densityLimit = safeDensityLimit(chars);
    bodyFeedback  = settings.body * 0.85f * densityLimit / tailDamping;
    if (isYaybaharLike(chars))
        bodyFeedback *= 0.72f;
    bodyFeedback  = std::min(bodyFeedback, 0.95f); // stability clamp for high-Q resonators (e.g. Udu Q=5.0)
    bodyDampState = 0.0f;
    bodyWritePos  = 0;
    std::memset(bodyBuf, 0, sizeof(bodyBuf));

    // Secondary body comb — only for instruments with dual resonator (numBodyModes > 1, e.g. Crwth)
    if (chars.numBodyModes > 1)
    {
        const float body2Freq = oscFreqHz * chars.bodyDelayRatio * 0.85f; // slightly different ratio for second chamber
        body2DelaySamples = juce::jlimit(2.0f, static_cast<float>(kBody2BufSize - 2),
                                         fsr / std::max(20.0f, body2Freq));
        body2Feedback  = settings.body * 0.70f * densityLimit / tailDamping;
        body2Feedback  = std::min(body2Feedback, 0.95f);
        body2DampState = 0.0f;
        body2WritePos  = 0;
        std::memset(body2Buf, 0, sizeof(body2Buf));
    }

    // Sympathetic combs (up to 4 from sympatheticMatrix)
    numSympCombs = 0;
    for (int c = 0; c < kMaxSympathetic; ++c)
    {
        if (std::abs(chars.sympatheticMatrix[c]) < 0.01f)
            continue;
        const float sympFreq = oscFreqHz * std::pow(2.0f, chars.sympatheticMatrix[c] / 12.0f);
        auto& sc = sympCombs[numSympCombs];
        sc.delaySamples = juce::jlimit(2.0f, static_cast<float>(kSympBufSize - 2),
                                       fsr / std::max(20.0f, sympFreq));
        sc.feedback  = settings.sympathetic * (isYaybaharLike(chars) ? 0.36f : 0.55f)
                     * densityLimit / tailDamping;
        sc.writePos  = 0;
        std::memset(sc.buf, 0, sizeof(sc.buf));
        ++numSympCombs;
    }

    // Friction
    frictionLpState = 0.0f;
}

void BowedStringVoiceBase::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
        envState = EnvState::Release;
}

void BowedStringVoiceBase::render(juce::AudioBuffer<float>& buffer,
                                  const int startSample, const int numSamples)
{
    if (envState == EnvState::Off) return;
    if (buffer.getNumChannels() <= 0) return;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off) break;

        const float env = advanceEnvelope();
        if (envState == EnvState::Off) break;

        const float safeCutoff = advanceBrightness();

        // ---- Vibrato (onset-delayed LFO) ----
        ++vibratoAge;
        const float vibratoMod = (vibratoDepth > 0.0f && vibratoAge > vibratoOnsetSamples)
            ? (1.0f + vibratoDepth * fastSin(vibratoPhase)) : 1.0f;
        vibratoPhase += vibratoRateHz / static_cast<float>(sr);
        if (vibratoPhase >= 1.0f) vibratoPhase -= 1.0f;
        const float phaseInc = oscFreqHz * pitchBendFactor * vibratoMod / static_cast<float>(sr);

        // ---- Friction excitation (LP-filtered noise * bowSpeed * bowPressure) ----
        const float noiseRaw = nextRandomBipolar();
        frictionLpState += chars.exciterBrightness * 0.4f * (noiseRaw - frictionLpState);
        const float friction = frictionLpState * settings.bowSpeed
                             * (0.5f + settings.bowPressure * 0.5f) * env;

        // ---- Multi-partial oscillator with fastSin ----
        float osc = getWaveform(oscPhase, chars.waveformMorph, phaseInc);

        const bool denseSpringResonator = isYaybaharLike(chars);
        const int nPartials = std::min(chars.partialCount, kMaxPartials);
        const int densityPartialLimit = safeDensityLimit(chars) < 0.75f ? 5 : 6;
        const int partialLimit = denseSpringResonator ? std::min(nPartials, densityPartialLimit)
                                                      : std::min(nPartials, 8);
        for (int p = 0; p < partialLimit; ++p)
        {
            if (chars.partialRatios[p] < 0.001f) break;
            const float n = chars.partialRatios[p];
            const float stretch = std::sqrt(1.0f + chars.inharmonicityB * n * n);
            const float partialHz = oscFreqHz * n * stretch * pitchBendFactor;
            const float r = partialHz / safeCutoff;
            float rolloff = 1.0f / (1.0f + r * r);
            if (denseSpringResonator)
                rolloff *= 1.0f / (1.0f + 0.12f * n * n);
            float partialPhase = oscPhase * n * stretch;
            partialPhase -= std::floor(partialPhase);
            osc += fastSin(partialPhase) * chars.partialAmps[p] * rolloff;
        }

        // ---- Mix oscillator + friction ----
        float signal = osc + friction * settings.exciter * 2.0f;

        // ---- Noise layer ----
        noiseLpState += 0.18f * (noiseRaw - noiseLpState);
        signal += noiseLpState * settings.noiseAmount;

        // ---- Body comb (primary) — feedback attenuated by env so it dies after noteOff ----
        if (settings.body > 0.001f)
        {
            const float delayed = readComb(bodyBuf, kBodyBufSize, bodyWritePos, bodyDelaySamples);
            bodyDampState += chars.bodyDamping * 0.4f * (delayed - bodyDampState);
            if (!(bodyDampState > 1e-15f || bodyDampState < -1e-15f)) bodyDampState = 0.0f;
            bodyBuf[bodyWritePos] = signal * 0.4f + bodyDampState * bodyFeedback * env;
            bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;
            signal += delayed * settings.body * (denseSpringResonator ? 0.68f : 1.0f);
        }

        // ---- Body comb (secondary — Crwth dual-resonator) — also env-attenuated ----
        if (settings.body > 0.001f && chars.numBodyModes > 1)
        {
            const float delayed2 = readComb(body2Buf, kBody2BufSize, body2WritePos, body2DelaySamples);
            body2DampState += chars.bodyDamping * 0.4f * (delayed2 - body2DampState);
            if (!(body2DampState > 1e-15f || body2DampState < -1e-15f)) body2DampState = 0.0f;
            body2Buf[body2WritePos] = signal * 0.3f + body2DampState * body2Feedback * env;
            body2WritePos = (body2WritePos + 1) % kBody2BufSize;
            signal += delayed2 * settings.body * 0.5f; // secondary body at lower weight
        }

        // ---- Sympathetic combs ----
        if (settings.sympathetic > 0.001f)
        {
            for (int c = 0; c < numSympCombs; ++c)
            {
                auto& sc = sympCombs[c];
                const float delayed = readComb(sc.buf, kSympBufSize, sc.writePos, sc.delaySamples);
                const float releaseGate = envState == EnvState::Release ? env : 1.0f;
                sc.buf[sc.writePos] = signal * 0.06f + delayed * sc.feedback * releaseGate;
                sc.writePos = (sc.writePos + 1) % kSympBufSize;
                signal += delayed * settings.sympathetic * (denseSpringResonator ? 0.16f : 0.3f);
            }
        }

        signal = applyRareDedicatedSignature(signal, env, phaseInc, noiseRaw);

        // ---- DC blocker (first-order high-pass, ~5 Hz) ----
        signal = applyDcBlocker(signal);

        // ---- Drive ----
        signal = applyVoiceDrive(signal);

        // ---- SVF filter — cutoff tracks brightness envelope every 64 samples ----
        updateFilterCoefficient(brightCutoffCurrent);
        signal = applyOutputDcBlocker(applyFilter(signal));

        // ---- Output ----
        signal *= env * vel * settings.level;
        writeOutput(left, right, startSample + i, signal);

        // ---- Phase advance ----
        oscPhase += phaseInc;
        while (oscPhase >= 1.0f) oscPhase -= 1.0f;

        if (++ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

// =========================================================================
//  PluckedStringVoiceBase — Karplus-Strong + body comb
// =========================================================================

void PluckedStringVoiceBase::noteOn(const InstrumentSettings& s,
                                    const int note, const float velocity,
                                    const double sampleRate)
{
    beginNote(s, note, velocity, sampleRate);
    const auto fsr = static_cast<float>(sr);

    // KS delay line
    ksDelaySamples = juce::jlimit(2.0f, static_cast<float>(kKsBufSize - 2),
                                  fsr / std::max(20.0f, oscFreqHz));
    ksDampState = 0.0f;
    ksWritePos  = 0;
    std::memset(ksBuf, 0, sizeof(ksBuf));

    // Fill KS buffer with noise burst (exciter)
    const int burstLen = std::min(static_cast<int>(ksDelaySamples), kKsBufSize);
    for (int i = 0; i < burstLen; ++i)
    {
        float n = nextRandomBipolar();
        // Brightness shapes exciter spectrum
        n *= (1.0f - chars.exciterBrightness * 0.3f * static_cast<float>(i) / static_cast<float>(burstLen));
        ksBuf[i] = n * vel;
    }
    ksWritePos = burstLen % kKsBufSize;

    // Pluck-position comb notch: nulls harmonics at multiples of 1/position
    {
        const float pluckPos = juce::jlimit(0.05f, 0.95f, settings.strikePosition);
        const int notchPeriod = std::max(2, static_cast<int>(ksDelaySamples * pluckPos));
        for (int i = notchPeriod; i < burstLen; ++i)
            ksBuf[i] -= ksBuf[i - notchPeriod] * 0.5f;
    }

    // Body comb
    const float bodyFreq = oscFreqHz * chars.bodyDelayRatio;
    bodyDelaySamples = juce::jlimit(2.0f, static_cast<float>(kBodyBufSize - 2),
                                    fsr / std::max(20.0f, bodyFreq));
    const float tailDamping = safeTailDamping(chars);
    bodyFeedback  = settings.body * 0.85f / tailDamping;
    bodyFeedback  = std::min(bodyFeedback, 0.95f); // stability clamp for high-Q resonators
    bodyDampState = 0.0f;
    bodyWritePos  = 0;
    std::memset(bodyBuf, 0, sizeof(bodyBuf));

    // Sympathetic comb (single, from first sympatheticMatrix entry)
    const float sympSemis = (std::abs(chars.sympatheticMatrix[0]) > 0.01f)
                                ? chars.sympatheticMatrix[0] : chars.sympatheticSemis;
    const float sympFreq = oscFreqHz * std::pow(2.0f, sympSemis / 12.0f);
    sympDelaySamples = juce::jlimit(2.0f, static_cast<float>(kSympBufSize - 2),
                                    fsr / std::max(20.0f, sympFreq));
    sympFeedback = settings.sympathetic * 0.60f / tailDamping;
    sympWritePos = 0;
    std::memset(sympBuf, 0, sizeof(sympBuf));

    // Exciter decay
    exciterEnvLevel = safeAttackAccent(chars);
    exciterDecay = std::exp(-tailDamping / (std::max(1.0f, chars.exciterDecayMs) * 0.001f * fsr));
}

void PluckedStringVoiceBase::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
        envState = EnvState::Release;
}

void PluckedStringVoiceBase::render(juce::AudioBuffer<float>& buffer,
                                    const int startSample, const int numSamples)
{
    if (envState == EnvState::Off) return;
    if (buffer.getNumChannels() <= 0) return;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const float phaseInc = oscFreqHz * pitchBendFactor / static_cast<float>(sr);

    // Damping coefficient from body characteristics
    const float tailDamping = safeTailDamping(chars);
    const float dampCoeff = juce::jlimit(0.05f, 0.95f, 0.3f + chars.bodyDamping * 0.5f
                                                        + (tailDamping - 1.0f) * 0.12f);

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off) break;

        const float env = advanceEnvelope();
        if (envState == EnvState::Off) break;

        advanceBrightness();

        // ---- KS: read delay line → LP filter → write ----
        const float delayed = readComb(ksBuf, kKsBufSize, ksWritePos, ksDelaySamples);
        ksDampState += dampCoeff * (delayed - ksDampState);
        const float feedback = juce::jlimit(0.970f, 0.998f,
                                            0.996f - (1.0f - settings.body) * 0.01f
                                            - (tailDamping - 1.0f) * 0.008f);
        ksBuf[ksWritePos] = ksDampState * feedback;
        ksWritePos = (ksWritePos + 1) % kKsBufSize;

        float signal = delayed;

        // ---- Additional exciter (decaying noise for first ms) ----
        if (exciterEnvLevel > 0.001f)
        {
            const float noise = nextRandomBipolar();
            signal += noise * exciterEnvLevel * settings.exciter * 0.3f;
            exciterEnvLevel *= exciterDecay;
        }

        // ---- Body comb ----
        if (settings.body > 0.001f)
        {
            const float bodyDel = readComb(bodyBuf, kBodyBufSize, bodyWritePos, bodyDelaySamples);
            bodyDampState += chars.bodyDamping * 0.4f * (bodyDel - bodyDampState);
            if (!(bodyDampState > 1e-15f || bodyDampState < -1e-15f)) bodyDampState = 0.0f;
            bodyBuf[bodyWritePos] = signal * 0.3f + bodyDampState * bodyFeedback;
            bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;
            signal += bodyDel * settings.body;
        }

        // ---- Sympathetic comb ----
        if (settings.sympathetic > 0.001f)
        {
            const float sympDel = readComb(sympBuf, kSympBufSize, sympWritePos, sympDelaySamples);
            sympBuf[sympWritePos] = signal * 0.08f + sympDel * sympFeedback;
            sympWritePos = (sympWritePos + 1) % kSympBufSize;
            signal += sympDel * settings.sympathetic * 0.5f;
        }

        const float signatureNoise = usesRareDedicatedAlgorithm() ? nextRandomBipolar() : 0.0f;
        signal = applyRareDedicatedSignature(signal, env, phaseInc, signatureNoise);

        // ---- DC blocker (first-order high-pass, ~5 Hz) ----
        signal = applyDcBlocker(signal);

        // ---- Drive ----
        signal = applyVoiceDrive(signal);

        // ---- SVF filter — cutoff tracks brightness envelope every 64 samples ----
        updateFilterCoefficient(brightCutoffCurrent);
        signal = applyOutputDcBlocker(applyFilter(signal));

        // ---- Output ----
        signal *= env * vel * settings.level;
        writeOutput(left, right, startSample + i, signal);

        if (++ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

// =========================================================================
//  WindVoiceBase — breath/reed + bore resonator
// =========================================================================

void WindVoiceBase::noteOn(const InstrumentSettings& s,
                           const int note, const float velocity,
                           const double sampleRate)
{
    beginNote(s, note, velocity, sampleRate);
    const auto fsr = static_cast<float>(sr);

    // Bore resonator
    const float boreFreq = oscFreqHz * chars.bodyDelayRatio;
    boreDelaySamples = juce::jlimit(2.0f, static_cast<float>(kBoreBufSize - 2),
                                    fsr / std::max(20.0f, boreFreq));
    const float tailDamping = safeTailDamping(chars);
    boreFeedback  = (0.6f + settings.body * 0.35f) / tailDamping;
    boreDampState = 0.0f;
    boreWritePos  = 0;
    std::memset(boreBuf, 0, sizeof(boreBuf));

    // Tonehole / breath state
    toneholeLpState = 0.0f;
    breathLpState   = 0.0f;

    // Aulos-style twin pipe: slight detune and low level, no public parameter.
    secondaryPipePhase = 0.0f;
    secondaryPipeLevel = chars.oddHarmonicsOnly ? (0.08f + 0.08f * settings.breathPressure) : 0.0f;
    secondaryPipeRatio = chars.oddHarmonicsOnly ? 1.006f : 1.0f;

    reedBuzzPhase = 0.0f;
    reedBuzzLevel = juce::jlimit(0.0f, 0.28f, safeSpectralMotion(chars)
                                             * (0.50f + settings.brightness * 0.75f));
    overblowBlend = juce::jlimit(0.0f, 0.35f,
                                 (chars.waveformMorph < 0.15f && chars.partialRatios[0] >= 1.9f)
                                     ? safeSpectralMotion(chars) * 2.4f
                                     : 0.0f);
}

void WindVoiceBase::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
        envState = EnvState::Release;
}

void WindVoiceBase::render(juce::AudioBuffer<float>& buffer,
                           const int startSample, const int numSamples)
{
    if (envState == EnvState::Off) return;
    if (buffer.getNumChannels() <= 0) return;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off) break;

        const float env = advanceEnvelope();
        if (envState == EnvState::Off) break;

        const float safeCutoff = advanceBrightness();

        // ---- Vibrato (onset-delayed LFO) ----
        ++vibratoAge;
        const float vibratoMod = (vibratoDepth > 0.0f && vibratoAge > vibratoOnsetSamples)
            ? (1.0f + vibratoDepth * fastSin(vibratoPhase)) : 1.0f;
        vibratoPhase += vibratoRateHz / static_cast<float>(sr);
        if (vibratoPhase >= 1.0f) vibratoPhase -= 1.0f;
        const float phaseInc = oscFreqHz * pitchBendFactor * vibratoMod / static_cast<float>(sr);

        // ---- Breath excitation (LP noise * breathPressure) ----
        const float noiseRaw = nextRandomBipolar();
        breathLpState += chars.exciterBrightness * 0.5f * (noiseRaw - breathLpState);
        const float breath = breathLpState * settings.breathPressure * env;

        // ---- Oscillator (tube resonance character) ----
        float osc = getWaveform(oscPhase, chars.waveformMorph, phaseInc);

        // Partials with brightness rolloff (overtone series)
        // Aulos keeps explicitly odd ratios (3/5/7) instead of filtering by array index.
        const int nPartials = std::min(chars.partialCount, 8);
        for (int p = 0; p < nPartials; ++p)
        {
            if (chars.partialRatios[p] < 0.001f) break;
            const float n = chars.partialRatios[p];
            const int harmonicIndex = static_cast<int>(std::round(n));
            if (chars.oddHarmonicsOnly && harmonicIndex > 0 && (harmonicIndex % 2 == 0)) continue;
            const float stretch = std::sqrt(1.0f + chars.inharmonicityB * n * n);
            const float partialHz = oscFreqHz * n * stretch * pitchBendFactor;
            const float fsr = static_cast<float>(sr);
            const float nyquistGuard = partialHz <= fsr * 0.45f
                ? 1.0f
                : juce::jlimit(0.0f, 1.0f, (fsr * 0.50f - partialHz) / (fsr * 0.05f));
            if (nyquistGuard <= 0.0001f)
                continue;
            const float r = partialHz / safeCutoff;
            const float rolloff = (1.0f / (1.0f + r * r)) * nyquistGuard;
            float pPhase = oscPhase * n * stretch;
            pPhase -= std::floor(pPhase);
            osc += fastSin(pPhase) * chars.partialAmps[p] * rolloff;
        }

        if (secondaryPipeLevel > 0.0f)
        {
            osc += getWaveform(secondaryPipePhase, chars.waveformMorph, phaseInc * secondaryPipeRatio)
                 * secondaryPipeLevel;
            secondaryPipePhase += phaseInc * secondaryPipeRatio;
            while (secondaryPipePhase >= 1.0f) secondaryPipePhase -= 1.0f;
        }

        if (overblowBlend > 0.0f)
        {
            float overtonePhase = oscPhase * 2.0f;
            overtonePhase -= std::floor(overtonePhase);
            osc = osc * (1.0f - overblowBlend * 0.42f)
                + fastSin(overtonePhase) * overblowBlend * (0.75f + settings.breathPressure * 0.25f);
        }

        // ---- Mix oscillator + breath noise ----
        float signal = osc * (0.7f + settings.breathPressure * 0.3f)
                     + breath * settings.exciter * 2.0f;

        if (reedBuzzLevel > 0.0f)
        {
            reedBuzzPhase += phaseInc * (chars.oddHarmonicsOnly ? 7.0f : 9.0f);
            reedBuzzPhase -= std::floor(reedBuzzPhase);
            const float buzz = std::tanh((breath * 9.0f + fastSin(reedBuzzPhase) * 0.55f)
                                         * reedBuzzLevel * 4.0f);
            signal += buzz * reedBuzzLevel;
        }

        // ---- Bore resonator (comb filter) ----
        // Cylindrical bore (bodyDelayRatio > 1.5, e.g. Aulos): sign inversion reinforces odd harmonics
        // Conical bore (bodyDelayRatio <= 1.5): positive feedback supports all harmonics
        {
            const float boreDel = readComb(boreBuf, kBoreBufSize, boreWritePos, boreDelaySamples);
            boreDampState += chars.bodyDamping * 0.35f * (boreDel - boreDampState);
            if (!(boreDampState > 1e-15f || boreDampState < -1e-15f)) boreDampState = 0.0f;
            if (chars.bodyDelayRatio > 1.5f)
            {
                // Cylindrical: open-end reflection with phase inversion (odd harmonics)
                const float safeFb = std::min(boreFeedback, 0.88f);
                boreBuf[boreWritePos] = signal * 0.35f - boreDampState * safeFb;
                boreWritePos = (boreWritePos + 1) % kBoreBufSize;
                signal = signal * 0.4f - boreDel * 0.6f;
            }
            else
            {
                // Conical: all harmonics reinforced
                boreBuf[boreWritePos] = signal * 0.35f + boreDampState * boreFeedback;
                boreWritePos = (boreWritePos + 1) % kBoreBufSize;
                signal = signal * 0.4f + boreDel * 0.6f;
            }
        }

        // ---- DC blocker (first-order high-pass, ~5 Hz) ----
        signal = applyDcBlocker(signal);

        // ---- Tonehole LP filter ----
        toneholeLpState += 0.25f * (signal - toneholeLpState);
        signal = signal * 0.6f + toneholeLpState * 0.4f;

        // ---- Noise layer ----
        noiseLpState += 0.18f * (noiseRaw - noiseLpState);
        signal += noiseLpState * settings.noiseAmount;

        signal = applyRareDedicatedSignature(signal, env, phaseInc, noiseRaw);

        // ---- Drive ----
        signal = applyVoiceDrive(signal);

        // ---- SVF filter — cutoff tracks brightness envelope every 64 samples ----
        updateFilterCoefficient(brightCutoffCurrent);
        signal = applyOutputDcBlocker(applyFilter(signal));

        // ---- Output ----
        signal *= env * vel * settings.level;
        writeOutput(left, right, startSample + i, signal);

        // ---- Phase advance ----
        oscPhase += phaseInc;
        while (oscPhase >= 1.0f) oscPhase -= 1.0f;

        if (++ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

// =========================================================================
//  StruckResonatorVoiceBase — impulse + multi-mode modal synthesis
// =========================================================================

void StruckResonatorVoiceBase::noteOn(const InstrumentSettings& s,
                                      const int note, const float velocity,
                                      const double sampleRate)
{
    beginNote(s, note, velocity, sampleRate);
    const auto fsr = static_cast<float>(sr);
    const float normalizedBody = juce::jlimit(0.0f, 1.0f, settings.body);
    const float normalizedSympathetic = juce::jlimit(0.0f, 1.0f, settings.sympathetic);
    const float tailDamping = safeTailDamping(chars);
    const float densityLimit = safeDensityLimit(chars);

    // Exciter decay
    exciterEnvLevel = juce::jmax(0.20f, settings.exciter) * (0.45f + velocity * 0.55f)
                    * safeAttackAccent(chars);
    exciterDecay = std::exp(-tailDamping / (std::max(1.0f, chars.exciterDecayMs) * 0.001f * fsr));
    modeSpread = 0.88f + normalizedBody * 0.44f + (settings.strikePosition - 0.5f) * 0.22f;
    cavityBlend = (0.18f + normalizedBody * 0.72f) * densityLimit;
    sympatheticBlend = normalizedSympathetic * densityLimit;
    cavityState = 0.0f;
    sympatheticState = 0.0f;
    const float targetTrim = 26.0f + normalizedBody * 42.0f + normalizedSympathetic * 14.0f
                           + settings.exciter * 10.0f;
    struckOutputTrim = std::isfinite(targetTrim) ? juce::jlimit(1.0f, 80.0f, targetTrim) : 1.0f;

    continuousExcitePhase = 0.0f;
    continuousExciteLevel = chars.hasContinuousExcitation
        ? (0.030f + settings.exciter * 0.070f) * (0.40f + velocity * 0.60f)
        : 0.0f;
    continuousExciteDecay = std::exp(-1.0f / (0.85f * fsr));

    // Modal resonators — biquad bandpass per mode
    const bool injectFundamental = chars.partialRatios[0] < 0.95f || chars.partialRatios[0] > 1.05f;
    numModes = std::min(chars.numBodyModes + (injectFundamental ? 1 : 0), kMaxBodyModes);
    if (numModes < 1) numModes = 1;

    // Count how many explicit partial ratios are defined
    int numDefinedPartials = 0;
    for (int p = 0; p < 4; ++p)
        if (chars.partialRatios[p] > 0.001f) ++numDefinedPartials;

    const bool pyeongyeongModelOnly = usesRareDedicatedAlgorithm()
        && getRareRenderEngineMode() == RareRenderEngineMode::V2ModelOnly
        && rareAlgorithm == RareInstrumentAlgorithm::PyeongyeongStoneChimeModal;

    for (int m = 0; m < numModes; ++m)
    {
        auto& mode = modes[m];
        // Struck instruments need a reliable first mode. Some definitions only list upper modes.
        const int partialIndex = injectFundamental ? (m - 1) : m;

        float modeRatio = 1.0f;
        bool hasExplicitRatio = false;
        if (injectFundamental && m == 0)
        {
            modeRatio = 1.0f;
            hasExplicitRatio = true;
        }
        else if (partialIndex >= 0 && partialIndex < 8 && chars.partialRatios[partialIndex] > 0.001f)
        {
            modeRatio = chars.partialRatios[partialIndex];
            hasExplicitRatio = true;
        }
        else if (partialIndex >= numDefinedPartials && numDefinedPartials >= 2)
        {
            // Extrapolate inharmonic growth from defined partials instead of harmonic fallback
            const float lastR  = chars.partialRatios[numDefinedPartials - 1];
            const float firstR = chars.partialRatios[0];
            const float growth = (lastR - firstR) / static_cast<float>(numDefinedPartials - 1);
            modeRatio = lastR + growth * static_cast<float>(partialIndex - numDefinedPartials + 1);
        }
        else
        {
            modeRatio = static_cast<float>(m + 1); // fallback: harmonic series
        }

        if (pyeongyeongModelOnly)
            modeRatio *= m == 0 ? 0.998f : (1.0f + 0.010f * static_cast<float>(m));

        // Only apply inharmonicity stretch for modes without explicit ratios;
        // explicit ratios already encode the correct inharmonic frequencies.
        const float stretch = hasExplicitRatio
            ? 1.0f
            : std::sqrt(1.0f + chars.inharmonicityB * modeRatio * modeRatio);
        const float spreadExponent = numModes > 1
            ? (static_cast<float>(m) / static_cast<float>(numModes - 1) - 0.5f)
            : 0.0f;
        const float spreadRatio = std::pow(modeSpread, spreadExponent);
        const float modeFreq = juce::jlimit(20.0f, fsr * 0.45f, oscFreqHz * modeRatio * stretch * spreadRatio);
        const float w0 = juce::MathConstants<float>::twoPi * modeFreq / fsr;

        // Q varies by mode and instrument
            const float modeQ = (chars.bodyResonanceQ
                            * (0.80f + normalizedBody * 1.80f)
                            * (1.0f - static_cast<float>(m) * 0.06f))
                            / tailDamping;
        const float alpha = std::sin(w0) / (2.0f * std::max(0.5f, modeQ));

        // Bandpass: H(z) = alpha / (1 + alpha - 2*cos(w0)*z^-1 + (1-alpha)*z^-2)
        const float cosW = std::cos(w0);
        const float a0inv = 1.0f / (1.0f + alpha);
        mode.b0 = alpha * a0inv;
        mode.a1 = -2.0f * cosW * a0inv;
        mode.a2 = (1.0f - alpha) * a0inv;
        mode.y1 = 0.0f;
        mode.y2 = 0.0f;

        // Gain: adaptive fundamental, then partial amplitude, then fallback
        if (injectFundamental && m == 0)
            mode.gain = (chars.partialAmps[0] > 0.001f) ? chars.partialAmps[0] * 0.8f : 0.35f;
        else if (partialIndex >= 0 && partialIndex < 8 && chars.partialAmps[partialIndex] > 0.001f)
            mode.gain = chars.partialAmps[partialIndex];
        else
            mode.gain = 0.3f / static_cast<float>(m + 1);

        if (pyeongyeongModelOnly)
            mode.gain *= m == 0 ? 0.72f : (1.18f + 0.055f * static_cast<float>(m));

        mode.gain *= (0.90f + normalizedBody * 1.90f) * (1.00f + normalizedSympathetic * 0.35f);
    }
}

void StruckResonatorVoiceBase::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
        envState = EnvState::Release;
}

void StruckResonatorVoiceBase::render(juce::AudioBuffer<float>& buffer,
                                      const int startSample, const int numSamples)
{
    if (envState == EnvState::Off) return;
    if (buffer.getNumChannels() <= 0) return;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off) break;

        const float env = advanceEnvelope();
        if (envState == EnvState::Off) break;

        advanceBrightness();

        // ---- Exciter impulse (short decaying noise burst) ----
        float exciter = 0.0f;
        float signatureNoise = 0.0f;
        if (exciterEnvLevel > 0.0001f)
        {
            const float noise = nextRandomBipolar();
            signatureNoise = noise;
            exciter = noise * exciterEnvLevel;
            exciterEnvLevel *= exciterDecay;

            // Strike position shapes exciter spectrum
            noiseLpState += (0.2f + settings.strikePosition * 0.4f) * (exciter - noiseLpState);
            exciter = noiseLpState * juce::jmax(0.12f, settings.exciter);
        }

        float modalInput = exciter;
        if (continuousExciteLevel > 1.0e-5f)
        {
            const float glassRub = fastSin(continuousExcitePhase) * continuousExciteLevel;
            continuousExcitePhase += (oscFreqHz * 0.503f) / static_cast<float>(sr);
            while (continuousExcitePhase >= 1.0f) continuousExcitePhase -= 1.0f;
            continuousExciteLevel *= continuousExciteDecay;
            modalInput += glassRub * (0.35f + settings.body * 0.45f);
        }

        // ---- Modal synthesis: sum resonating biquad bandpasses ----
        float signal = 0.0f;
        for (int m = 0; m < numModes; ++m)
        {
            auto& mode = modes[m];
            // Direct Form II biquad
            const float w = modalInput - mode.a1 * mode.y1 - mode.a2 * mode.y2;
            const float y = mode.b0 * (w - mode.y2);
            mode.y2 = mode.y1;
            mode.y1 = w;
            signal += y * mode.gain;
        }

        // Keep a small dry attack so struck instruments remain clearly audible.
        signal += exciter * (0.30f + settings.exciter * 0.22f);

        // Cavity/body weight: reinforces the main resonant cluster and lengthens the bloom.
        const float cavityCoeff = 0.05f + cavityBlend * 0.22f;
        cavityState += cavityCoeff * (signal - cavityState);
        const float cavitySignal = signal * (0.62f + cavityBlend * 0.52f) + cavityState * cavityBlend;

        // Sympathetic coupling: a restrained recirculation that makes coupling audible without runaway.
        const float sympatheticFeedback = juce::jlimit(0.950f, 0.997f,
                                                       0.990f + settings.body * 0.007f
                                                       - (safeTailDamping(chars) - 1.0f) * 0.012f);
        sympatheticState = juce::jlimit(-6.0f, 6.0f,
                                        sympatheticState * sympatheticFeedback
                                        + cavitySignal * (0.030f + sympatheticBlend * 0.075f));
        signal = cavitySignal + sympatheticState * sympatheticBlend * 0.58f;

        if (usesRareDedicatedAlgorithm() && signatureNoise == 0.0f)
            signatureNoise = nextRandomBipolar();
        signal = applyRareDedicatedSignature(signal, env,
                                             oscFreqHz * pitchBendFactor / static_cast<float>(sr),
                                             signatureNoise);

        // ---- SVF filter (body radiation) — cutoff tracks brightness envelope every 64 samples ----
        updateFilterCoefficient(brightCutoffCurrent);
        signal = applyOutputDcBlocker(applyFilter(signal));

        // ---- Drive ----
        signal = applyVoiceDrive(signal);

        if (getRareRenderEngineMode() == RareRenderEngineMode::V2ModelOnly
            && rareAlgorithm == RareInstrumentAlgorithm::PyeongyeongStoneChimeModal)
        {
            const float stoneEdge = 0.020f * fastSin(rarePhaseA)
                                  + 0.014f * fastSin(rarePhaseB + 0.217f);
            signal += stoneEdge * (0.40f + 0.60f * env);
        }

        // ---- Output ----
        signal *= env * vel * settings.level * struckOutputTrim;
        if (!std::isfinite(signal))
            signal = 0.0f;
        signal = juce::jlimit(-4.0f, 4.0f, signal);
        writeOutput(left, right, startSample + i, signal);

        if (++ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

// =========================================================================
//  ElectronicVoiceBase — morphable osc + FM + waveshaping
// =========================================================================

void ElectronicVoiceBase::noteOn(const InstrumentSettings& s,
                                 const int note, const float velocity,
                                 const double sampleRate)
{
    beginNote(s, note, velocity, sampleRate);

    // FM setup from partials
    fmPhase = 0.0f;
    fmRatio = (chars.partialRatios[0] > 0.001f) ? chars.partialRatios[0] : 2.0f;
    fmDepth = chars.partialAmps[0] * settings.exciter * 2.0f;

    // Continuous-controller profiles without new public parameters.
    vibratoPhase = 0.0f;
    vibratoAge = 0;
    gestureGainCurrent = 0.82f + velocity * 0.18f;
    gestureGainTarget = 0.70f + velocity * 0.36f;
    gestureGainCoeff = 1.0f - std::exp(-1.0f / (0.008f * static_cast<float>(sr)));
    electronicPitchTarget = 1.0f;
    electronicPitchCurrent = 1.0f;
    electronicPitchCoeff = 1.0f - std::exp(-1.0f / (0.030f * static_cast<float>(sr)));
    spectralMotionPhase = 0.0f;
    spectralMotionDepth = juce::jlimit(0.0f, 0.20f, safeSpectralMotion(chars));

    if (chars.partialCount <= 1 && chars.waveformMorph < 0.05f) // Theremine
    {
        vibratoRateHz = 5.2f;
        vibratoDepth = 0.006f / safePitchFocus(chars);
        vibratoOnsetSamples = static_cast<int>(sr * 0.18);
        electronicPitchCurrent = 0.994f;
    }
    else if (chars.partialCount >= 8 && chars.waveformMorph > 0.35f) // Ondes Martenot
    {
        vibratoRateHz = 5.8f;
        vibratoDepth = 0.0045f / safePitchFocus(chars);
        vibratoOnsetSamples = static_cast<int>(sr * 0.11);
        electronicPitchCurrent = 0.991f;
        electronicPitchCoeff = 1.0f - std::exp(-1.0f / (0.045f * static_cast<float>(sr)));
    }
    else if (chars.inharmonicityB > 0.0005f) // Hydraulophone
    {
        vibratoRateHz = 3.9f;
        vibratoDepth = 0.0015f / safePitchFocus(chars);
        vibratoOnsetSamples = static_cast<int>(sr * 0.22);
        if (usesRareV2Profile())
            spectralMotionDepth = juce::jmax(spectralMotionDepth, 0.085f);
    }
    else // Pyrophone and other conceptual tube voices
    {
        vibratoRateHz = 4.3f;
        vibratoDepth = 0.0025f / safePitchFocus(chars);
        vibratoOnsetSamples = static_cast<int>(sr * 0.20);
        if (usesRareV2Profile())
            spectralMotionDepth = juce::jmax(spectralMotionDepth, 0.065f);
    }
}

void ElectronicVoiceBase::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
        envState = EnvState::Release;
}

void ElectronicVoiceBase::render(juce::AudioBuffer<float>& buffer,
                                 const int startSample, const int numSamples)
{
    if (envState == EnvState::Off) return;
    if (buffer.getNumChannels() <= 0) return;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off) break;

        const float env = advanceEnvelope();
        if (envState == EnvState::Off) break;

        const float safeCutoff = advanceBrightness();

        // ---- Vibrato (onset-delayed LFO for continuous instruments) ----
        ++vibratoAge;
        const float vibratoMod = (vibratoDepth > 0.0f && vibratoAge > vibratoOnsetSamples)
            ? (1.0f + vibratoDepth * fastSin(vibratoPhase)) : 1.0f;
        vibratoPhase += vibratoRateHz / static_cast<float>(sr);
        if (vibratoPhase >= 1.0f) vibratoPhase -= 1.0f;
        electronicPitchCurrent += electronicPitchCoeff * (electronicPitchTarget - electronicPitchCurrent);
        gestureGainCurrent += gestureGainCoeff * (gestureGainTarget - gestureGainCurrent);

        const float fsr = static_cast<float>(sr);
        const float rawPhaseInc = oscFreqHz * pitchBendFactor * vibratoMod * electronicPitchCurrent / fsr;
        const float phaseInc = std::isfinite(rawPhaseInc) ? juce::jlimit(0.0f, 0.45f, rawPhaseInc) : 0.0f;
        const float fmPhaseInc = std::isfinite(phaseInc * fmRatio) ? juce::jlimit(0.0f, 0.45f, phaseInc * fmRatio) : 0.0f;
        const float carrierHz = phaseInc * fsr;
        const float modHz = fmPhaseInc * fsr;
        const float sidebandBudgetHz = fsr * 0.45f - carrierHz;
        const float fmBudget = modHz > 1.0f ? juce::jlimit(0.0f, 1.0f, sidebandBudgetHz / (modHz * 3.0f)) : 1.0f;
        const float effectiveFmDepth = fmDepth * fmBudget;

        // ---- FM modulator ----
        const float modSig = fastSin(fmPhase) * effectiveFmDepth;

        // ---- Carrier oscillator with FM ----
        float carrierPhase = oscPhase + modSig;
        carrierPhase -= std::floor(carrierPhase);
        if (carrierPhase < 0.0f) carrierPhase += 1.0f;

        float signal = getWaveform(carrierPhase, chars.waveformMorph, phaseInc);

        // ---- Additional partials (additive over FM carrier) ----
        for (int p = 1; p < std::min(chars.partialCount, 8); ++p)
        {
            if (chars.partialRatios[p] < 0.001f) break;
            const float n = chars.partialRatios[p];
            const float stretch = std::sqrt(1.0f + chars.inharmonicityB * n * n);
            const float partialHz = carrierHz * n * stretch;
            const float nyquistGuard = partialHz <= fsr * 0.45f
                ? 1.0f
                : juce::jlimit(0.0f, 1.0f, (fsr * 0.50f - partialHz) / (fsr * 0.05f));
            if (nyquistGuard <= 0.0001f)
                continue;
            const float r = partialHz / safeCutoff;
            const float rolloff = (1.0f / (1.0f + r * r)) * nyquistGuard;
            float pPhase = oscPhase * n * stretch;
            pPhase -= std::floor(pPhase);
            signal += fastSin(pPhase) * chars.partialAmps[p] * rolloff;
        }

        // ---- Noise and instrument-specific spectral gesture ----
        const float noiseRaw = nextRandomBipolar();
        noiseLpState += 0.18f * (noiseRaw - noiseLpState);
        const float spectralMotion = spectralMotionDepth > 0.0f ? spectralMotionDepth * fastSin(spectralMotionPhase) : 0.0f;
        spectralMotionPhase += 0.37f / static_cast<float>(sr);
        if (spectralMotionPhase >= 1.0f) spectralMotionPhase -= 1.0f;
        signal += noiseLpState * settings.noiseAmount * (1.0f + spectralMotion);
        if (spectralMotionDepth > 0.0f)
        {
            const float spectralDrive = 1.0f + spectralMotion * (0.30f + settings.exciter * 0.24f);
            signal *= spectralDrive;
            signal += std::tanh(noiseLpState * spectralMotionDepth * 7.0f) * spectralMotionDepth * 0.28f;
        }

        signal = applyRareDedicatedSignature(signal, env, phaseInc, noiseRaw);

        // ---- Drive ----
        signal = applyVoiceDrive(signal);

        // ---- SVF filter — cutoff tracks brightness envelope every 64 samples ----
        updateFilterCoefficient(brightCutoffCurrent);
        signal = applyOutputDcBlocker(applyFilter(signal));

        // ---- Output ----
        signal *= env * vel * settings.level * gestureGainCurrent;
        writeOutput(left, right, startSample + i, signal);

        // ---- Phase advance ----
        oscPhase += phaseInc;
        while (oscPhase >= 1.0f) oscPhase -= 1.0f;
        if (!std::isfinite(oscPhase)) oscPhase = 0.0f;

        fmPhase += fmPhaseInc;
        while (fmPhase >= 1.0f) fmPhase -= 1.0f;
        if (!std::isfinite(fmPhase)) fmPhase = 0.0f;

        if (++ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

// =========================================================================
// Leaf class getCharacteristics() implementations
// =========================================================================

#define MIS_DEFINE_INSTR_VOICE(className, index) \
const InstrumentCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mis::getCharacteristics(index); \
}

MIS_DEFINE_INSTR_VOICE(NyckelharpaVoice, 0)
MIS_DEFINE_INSTR_VOICE(GayageumVoice, 1)
MIS_DEFINE_INSTR_VOICE(ChapmanStickVoice, 2)
MIS_DEFINE_INSTR_VOICE(YayliTanburVoice, 3)
MIS_DEFINE_INSTR_VOICE(CrwthVoice, 4)
MIS_DEFINE_INSTR_VOICE(CarnyxVoice, 5)
MIS_DEFINE_INSTR_VOICE(AulosVoice, 6)
MIS_DEFINE_INSTR_VOICE(FujaraVoice, 7)
MIS_DEFINE_INSTR_VOICE(GemshornVoice, 8)
MIS_DEFINE_INSTR_VOICE(DiziVoice, 9)
MIS_DEFINE_INSTR_VOICE(AngklungVoice, 10)
MIS_DEFINE_INSTR_VOICE(UduVoice, 11)
MIS_DEFINE_INSTR_VOICE(PyeongyeongVoice, 12)
MIS_DEFINE_INSTR_VOICE(CristalBaschetVoice, 13)
MIS_DEFINE_INSTR_VOICE(MbiraVoice, 14)
MIS_DEFINE_INSTR_VOICE(HandpanVoice, 15)
MIS_DEFINE_INSTR_VOICE(ThereminVoice, 16)
MIS_DEFINE_INSTR_VOICE(OndesMartenotVoice, 17)
MIS_DEFINE_INSTR_VOICE(PyrophoneVoice, 18)
MIS_DEFINE_INSTR_VOICE(HydraulophoneVoice, 19)
MIS_DEFINE_INSTR_VOICE(YaybaharVoice, 20)

#undef MIS_DEFINE_INSTR_VOICE

// =========================================================================
// Factory — dispatch by SynthesisMode
// =========================================================================
std::unique_ptr<InstrumentVoice> createVoiceForInstrument(const int instrumentIndex)
{
    switch (juce::jlimit(0, kNumInstruments - 1, instrumentIndex))
    {
        case 0:  return std::make_unique<NyckelharpaVoice>();
        case 1:  return std::make_unique<GayageumVoice>();
        case 2:  return std::make_unique<ChapmanStickVoice>();
        case 3:  return std::make_unique<YayliTanburVoice>();
        case 4:  return std::make_unique<CrwthVoice>();
        case 5:  return std::make_unique<CarnyxVoice>();
        case 6:  return std::make_unique<AulosVoice>();
        case 7:  return std::make_unique<FujaraVoice>();
        case 8:  return std::make_unique<GemshornVoice>();
        case 9:  return std::make_unique<DiziVoice>();
        case 10: return std::make_unique<AngklungVoice>();
        case 11: return std::make_unique<UduVoice>();
        case 12: return std::make_unique<PyeongyeongVoice>();
        case 13: return std::make_unique<CristalBaschetVoice>();
        case 14: return std::make_unique<MbiraVoice>();
        case 15: return std::make_unique<HandpanVoice>();
        case 16: return std::make_unique<ThereminVoice>();
        case 17: return std::make_unique<OndesMartenotVoice>();
        case 18: return std::make_unique<PyrophoneVoice>();
        case 19: return std::make_unique<HydraulophoneVoice>();
        case 20: return std::make_unique<YaybaharVoice>();
        default: break;
    }

    return std::make_unique<NyckelharpaVoice>();
}

} // namespace mis
