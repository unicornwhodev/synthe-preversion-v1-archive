#include <JuceHeader.h>

#include "PercVoice.h"
#include "SinTable.h"
#include <cmath>
#include <algorithm>

namespace mpc
{

void PercVoice::updateEnvelopeCoefficients(float sampleRate) noexcept
{
    const float fsr = std::max(1.0f, sampleRate);
    const float attackSeconds = std::max(baseAttackSeconds * std::clamp(currentModulation.attackScale, 0.0625f, 16.0f), 0.0001f);
    const float decaySeconds = std::max(baseDecaySeconds * std::clamp(currentModulation.decayScale, 0.0625f, 16.0f), 0.01f);

    envAttackInc = attackSeconds > 0.0001f ? 1.0f / (attackSeconds * fsr) : 1.0f;
    envDecayMul = std::exp(-1.0f / (decaySeconds * fsr));
    envRelMul = std::exp(-1.0f / (std::max(baseReleaseSeconds, 0.005f) * fsr));
}

void PercVoice::updatePanFromModulation() noexcept
{
    const float pan = juce::jlimit(-1.0f, 1.0f, basePan + currentModulation.panAdd);
    const float panPhase = (pan * 0.5f + 0.5f) * 0.25f;
    panL = mpc::fastSin(panPhase + 0.25f);
    panR = mpc::fastSin(panPhase);
}

void PercVoice::updateFilterFromModulation(float sampleRate) noexcept
{
    const float fsr = std::max(1.0f, sampleRate);
    filterQinv = juce::jlimit(0.08f, 1.8f, baseFilterQinv - currentModulation.resonanceAdd * 0.35f);
    filterMaxF = (-filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f)) * 0.95f;
    const float cutoffMul = std::isfinite(currentModulation.cutoffMul)
        ? std::clamp(currentModulation.cutoffMul, 0.0625f, 16.0f)
        : 1.0f;
    filterBaseF = juce::jmin(baseFilterBaseF * cutoffMul, filterMaxF);
    if (!std::isfinite(filterBaseF))
        filterBaseF = juce::jlimit(0.0f, filterMaxF, baseFilterBaseF);
    filterCurrentF = juce::jlimit(0.0f, filterMaxF, std::isfinite(filterCurrentF) ? filterCurrentF : filterBaseF);
    const float resonanceAdd = std::isfinite(currentModulation.resonanceAdd) ? currentModulation.resonanceAdd : 0.0f;
    bodyFeedback = juce::jlimit(0.0f, 0.9995f,
                                baseBodyFeedback * std::clamp(1.0f + resonanceAdd * 0.35f,
                                                              0.25f, 2.0f));
    (void) fsr;
}

void PercVoice::setVoiceModulation(const VoiceModulation& modulation, double sampleRate) noexcept
{
    const float ducking = std::min(currentModulation.duckingMul, modulation.duckingMul);
    currentModulation = modulation;
    currentModulation.duckingMul = juce::jlimit(0.0f, 1.0f, ducking);
    nextNoteModulation = currentModulation;
    const float safePitchSemi = std::isfinite(currentModulation.pitchSemi)
        ? juce::jlimit(-48.0f, 48.0f, currentModulation.pitchSemi)
        : 0.0f;
    modPitchFactor = std::exp2(safePitchSemi / 12.0f);
    levelGain = juce::jlimit(0.0f, 4.0f, baseLevelGain * std::clamp(currentModulation.levelMul, 0.0f, 4.0f));
    updatePanFromModulation();
    if (!quickReleaseForced)
        updateEnvelopeCoefficients(static_cast<float>(sampleRate));
    updateFilterFromModulation(static_cast<float>(sampleRate));
}

void PercVoice::reset() noexcept
{
    active = false;
    envStage = Off;
    envLevel = 0.0f;
    envAttackInc = 0.0f;
    envDecayMul = 1.0f;
    envSustain = 0.0f;
    envRelMul = 1.0f;
    brightCutoffTarget = 5000.0f;
    brightCutoffCurrent = 5000.0f;
    brightDecayCoeff = 1.0f;
    clickLevel = 0.0f;
    clickDecay = 1.0f;
    clickFilt = 0.0f;
    clickHpState = 0.0f;
    noiseLevel = 0.0f;
    noiseDecayCoef = 1.0f;
    noiseCurrent = 0.0f;
    noiseBright = 0.5f;
    noisePrev = 0.0f;
    bodyBuf.fill(0.0f);
    bodyWritePos = 0;
    bodyDelay = 0.0f;
    bodyFeedback = 0.0f;
    bodyDampBand = 0.0f;
    bodyDampLow = 0.0f;
    bodyDampF = 0.0f;
    bodyDampQinv = 0.707f;
    svfBand = 0.0f;
    svfLow = 0.0f;
    filterF = 0.0f;
    filterQinv = 0.707f;
    filterBaseF = 0.0f;
    filterCurrentF = 0.0f;
    filterDecayCoeff = 1.0f;
    filterMaxF = 0.0f;
    panL = 0.707f;
    panR = 0.707f;
    stereoWidth = 0.0f;
    colorShift = 0.0f;
    velocity = 0.0f;
    levelGain = 1.0f;
    pitchFollowing = 1.0f;
    randomization = 0.0f;
    roundRobinCount = 0;
    dcX1 = 0.0f;
    dcY1 = 0.0f;
    numActiveModes = 0;
    modes = {};
    age = 0.0f;
    maxAge = 30.0f;
    pitchBendFactor = 1.0f;
    modPitchFactor = 1.0f;
    baseBodyFeedback = 0.0f;
    baseFilterBaseF = 0.0f;
    baseFilterQinv = 0.707f;
    baseAttackSeconds = 0.005f;
    baseDecaySeconds = 1.5f;
    baseReleaseSeconds = 0.2f;
    quickReleaseForced = false;
    basePan = 0.0f;
    baseLevelGain = 1.0f;
    currentModulation = {};
    nextNoteModulation = {};
    instrumentIndex = 0;
    percAlgorithm = PercInstrumentAlgorithm::TimbalesMembraneBessel;
    percDedicatedActive = false;
    percModelOnly = false;
    dedicatedPhaseA = 0.0f;
    dedicatedPhaseB = 0.0f;
    dedicatedPhaseC = 0.0f;
    dedicatedPhaseD = 0.0f;
    dedicatedStateA = 0.0f;
    dedicatedStateB = 0.0f;
    dedicatedStateC = 0.0f;
    dedicatedPulse = 0.0f;
    dedicatedEnv = 0.0f;
    dedicatedDecay = 1.0f;
    dedicatedGain = 0.0f;
    dedicatedPan = 0.0f;
    dedicatedRateA = 0.0f;
    dedicatedRateB = 0.0f;
    dedicatedRateC = 0.0f;
    dedicatedRateD = 0.0f;
}

// =========================================================================
// RNG
// =========================================================================
float PercVoice::nextRandom()
{
    rngState = rngState * 1664525u + 1013904223u;
    return static_cast<float>(rngState >> 8) / 16777216.0f;  // [0, 1)
}

// =========================================================================
// Comb reader — Hermite cubic interpolation for production-quality body resonance
// =========================================================================
float PercVoice::readComb(float delaySamples) const
{
    if (!std::isfinite(delaySamples))
        return 0.0f;

    delaySamples = juce::jlimit(1.0f, static_cast<float>(kBodyBufSize - 3), delaySamples);

    float readPos = std::fmod(static_cast<float>(bodyWritePos) - delaySamples,
                              static_cast<float>(kBodyBufSize));
    if (!std::isfinite(readPos))
        return 0.0f;

    if (readPos < 0.0f)
        readPos += static_cast<float>(kBodyBufSize);

    int idx1 = static_cast<int>(readPos);
    float frac = readPos - static_cast<float>(idx1);
    if (!std::isfinite(frac))
        return 0.0f;

    auto wrap = [](int i) -> std::size_t {
        return static_cast<std::size_t>(i & (kBodyBufSize - 1));
    };

    const float y0 = bodyBuf[wrap(idx1 - 1)];
    const float y1 = bodyBuf[wrap(idx1)];
    const float y2 = bodyBuf[wrap(idx1 + 1)];
    const float y3 = bodyBuf[wrap(idx1 + 2)];
    if (!std::isfinite(y0) || !std::isfinite(y1) || !std::isfinite(y2) || !std::isfinite(y3))
        return 0.0f;

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    const float interpolated = ((c3 * frac + c2) * frac + c1) * frac + c0;

    return std::isfinite(interpolated) ? interpolated : 0.0f;
}

bool PercVoice::usesPercDedicatedAlgorithm() const noexcept
{
    return percDedicatedActive && getPercRenderEngineMode() != PercRenderEngineMode::LegacyFamily;
}

void PercVoice::applyDedicatedNoteProfile(const float fundHz,
                                          const float sr,
                                          const float vel,
                                          const InstrSettings& settings) noexcept
{
    if (!usesPercDedicatedAlgorithm())
        return;

    const float safeSr = std::max(1.0f, sr);
    const float safeFund = std::clamp(std::isfinite(fundHz) ? fundHz : 220.0f, 20.0f, safeSr * 0.45f);
    const float safeVelocity = std::clamp(std::isfinite(vel) ? vel : 0.0f, 0.0f, 1.0f);
    const float velocityCurve = std::sqrt(safeVelocity);
    const float modelScale = percModelOnly ? 1.28f : 1.0f;

    auto rateForHz = [safeSr](float hz) noexcept -> float {
        const float safeHz = std::clamp(std::isfinite(hz) ? hz : 20.0f, 0.1f, safeSr * 0.45f);
        return safeHz / safeSr;
    };

    auto setDecaySeconds = [this, safeSr](const float seconds) noexcept {
        const float safeSeconds = std::max(0.01f, std::isfinite(seconds) ? seconds : 0.25f);
        dedicatedDecay = std::exp(-1.0f / (safeSeconds * safeSr));
    };

    auto scaleMode = [this](const int modeIndex, const float ampMul, const float decayPower) noexcept {
        if (modeIndex < 0 || modeIndex >= numActiveModes)
            return;
        auto& mode = modes[static_cast<std::size_t>(modeIndex)];
        mode.amplitude *= ampMul;
        mode.currentAmp *= ampMul;
        mode.decayCoef = std::clamp(std::pow(std::clamp(mode.decayCoef, 0.0f, 0.999999f),
                                             std::max(0.05f, decayPower)),
                                    0.0f, 0.999999f);
    };

    auto retuneMode = [this](const int modeIndex, const float cents) noexcept {
        if (modeIndex < 0 || modeIndex >= numActiveModes)
            return;
        auto& mode = modes[static_cast<std::size_t>(modeIndex)];
        const float ratio = std::exp2(cents / 1200.0f);
        mode.basePhaseInc = std::clamp(mode.basePhaseInc * ratio, 0.0f, 0.45f);
        mode.phaseInc = mode.basePhaseInc;
    };

    dedicatedPhaseA = nextRandom();
    dedicatedPhaseB = nextRandom();
    dedicatedPhaseC = nextRandom();
    dedicatedPhaseD = nextRandom();
    dedicatedStateA = 0.0f;
    dedicatedStateB = 0.0f;
    dedicatedStateC = 0.0f;
    dedicatedPulse = 0.0f;
    dedicatedEnv = std::clamp(0.24f + velocityCurve * 0.92f, 0.0f, 1.35f);
    dedicatedPan = (nextRandom() - 0.5f) * 0.58f;
    dedicatedGain = 0.035f * modelScale;
    setDecaySeconds(std::max(0.05f, settings.decaySeconds));

    switch (percAlgorithm)
    {
        case PercInstrumentAlgorithm::TimbalesMembraneBessel:
            dedicatedRateA = rateForHz(safeFund * 0.985f);
            dedicatedRateB = rateForHz(safeFund * 1.593f * 1.012f);
            dedicatedRateC = rateForHz(5.2f + safeVelocity * 2.3f);
            dedicatedRateD = rateForHz(31.0f + safeVelocity * 19.0f);
            dedicatedGain = 0.070f * modelScale;
            setDecaySeconds(std::max(0.45f, settings.decaySeconds * 0.78f));
            scaleMode(0, 1.06f, 0.94f);
            scaleMode(1, 1.04f, 0.99f);
            scaleMode(2, 0.88f, 1.12f);
            retuneMode(0, -3.5f);
            retuneMode(1, 5.0f);
            retuneMode(2, -7.0f);
            clickLevel *= 1.03f;
            noiseLevel *= 1.04f;
            noiseCurrent = noiseLevel;
            baseBodyFeedback = juce::jlimit(0.0f, 0.9995f, baseBodyFeedback * 1.06f);
            bodyFeedback = juce::jlimit(0.0f, 0.9995f, bodyFeedback * 1.06f);
            filterCurrentF = std::min(filterMaxF, filterCurrentF * 0.92f);
            break;

        case PercInstrumentAlgorithm::MarimbaWoodBarResonator:
            dedicatedRateA = rateForHz(safeFund);
            dedicatedRateB = rateForHz(safeFund * 3.93f);
            dedicatedRateC = rateForHz(std::max(55.0f, safeFund * 0.50f));
            dedicatedRateD = rateForHz(82.0f + safeVelocity * 68.0f);
            dedicatedGain = 0.072f * modelScale;
            setDecaySeconds(std::max(0.55f, settings.decaySeconds * 0.92f));
            scaleMode(0, 1.18f, 0.86f);
            scaleMode(1, 0.92f, 1.08f);
            scaleMode(2, 0.72f, 1.22f);
            retuneMode(1, -6.0f);
            retuneMode(2, 9.0f);
            clickLevel *= 0.88f;
            clickFiltCoeff = std::clamp(clickFiltCoeff * 0.72f, 0.08f, 0.6f);
            noiseLevel *= 0.76f;
            noiseCurrent = noiseLevel;
            baseBodyFeedback = juce::jlimit(0.0f, 0.9995f, baseBodyFeedback * 1.22f);
            bodyFeedback = juce::jlimit(0.0f, 0.9995f, bodyFeedback * 1.22f);
            break;

        case PercInstrumentAlgorithm::DjembeHandDrumSkin:
            dedicatedRateA = rateForHz(std::max(48.0f, safeFund * 0.62f));
            dedicatedRateB = rateForHz(safeFund * (1.42f + safeVelocity * 0.28f));
            dedicatedRateC = rateForHz(610.0f + safeVelocity * 940.0f);
            dedicatedRateD = rateForHz(18.0f + safeVelocity * 25.0f);
            dedicatedGain = 0.126f * modelScale;
            setDecaySeconds(std::max(0.12f, settings.decaySeconds * 0.48f));
            scaleMode(0, 1.30f, 1.28f);
            scaleMode(1, 0.82f + safeVelocity * 0.40f, 1.08f);
            scaleMode(2, 0.68f + safeVelocity * 0.56f, 1.34f);
            retuneMode(0, -8.0f);
            retuneMode(1, 12.0f);
            clickLevel *= 0.82f + safeVelocity * 0.74f;
            noiseLevel *= 1.04f + safeVelocity * 0.42f;
            noiseCurrent = noiseLevel;
            baseBodyFeedback = juce::jlimit(0.0f, 0.9995f, baseBodyFeedback * 1.12f);
            bodyFeedback = juce::jlimit(0.0f, 0.9995f, bodyFeedback * 1.12f);
            break;

        case PercInstrumentAlgorithm::RainstickGranularCascade:
            dedicatedRateA = rateForHz(90.0f + nextRandom() * 80.0f);
            dedicatedRateB = rateForHz(210.0f + nextRandom() * 190.0f);
            dedicatedRateC = rateForHz(8.0f + safeVelocity * 9.0f);
            dedicatedRateD = rateForHz(19.0f + safeVelocity * 24.0f);
            dedicatedGain = 0.086f * modelScale;
            setDecaySeconds(std::max(1.35f, settings.decaySeconds * 1.05f));
            scaleMode(0, 0.64f, 0.74f);
            scaleMode(1, 0.45f, 0.82f);
            clickLevel *= 0.35f;
            noiseLevel *= 1.45f;
            noiseCurrent = noiseLevel;
            noiseDecayCoef = std::clamp(std::pow(noiseDecayCoef, 0.52f), 0.0f, 0.999999f);
            baseBodyFeedback = juce::jlimit(0.0f, 0.9995f, baseBodyFeedback * 1.28f);
            bodyFeedback = juce::jlimit(0.0f, 0.9995f, bodyFeedback * 1.28f);
            break;

        case PercInstrumentAlgorithm::SingingBowlRubBeating:
            dedicatedRateA = rateForHz(safeFund * 1.000f);
            dedicatedRateB = rateForHz(safeFund * 1.012f);
            dedicatedRateC = rateForHz(safeFund * 2.730f);
            dedicatedRateD = rateForHz(0.34f + settings.color * 0.72f);
            dedicatedGain = 0.134f * modelScale;
            setDecaySeconds(std::max(4.0f, settings.decaySeconds * 1.65f));
            scaleMode(0, 1.14f, 0.72f);
            scaleMode(1, 1.08f, 0.78f);
            scaleMode(2, 1.02f, 0.86f);
            retuneMode(0, -4.0f);
            retuneMode(1, 11.0f);
            retuneMode(2, -13.0f);
            retuneMode(3, 17.0f);
            clickLevel *= 0.55f;
            noiseLevel *= 0.70f;
            noiseCurrent = noiseLevel;
            filterCurrentF = std::min(filterMaxF, filterCurrentF * 1.08f);
            break;

        case PercInstrumentAlgorithm::WindChimesTubeCluster:
            dedicatedRateA = rateForHz(safeFund * (1.48f + nextRandom() * 0.38f));
            dedicatedRateB = rateForHz(safeFund * (2.34f + nextRandom() * 0.70f));
            dedicatedRateC = rateForHz(safeFund * (4.20f + nextRandom() * 1.40f));
            dedicatedRateD = rateForHz(2.6f + safeVelocity * 5.8f);
            dedicatedGain = 0.132f * modelScale;
            setDecaySeconds(std::max(1.6f, settings.decaySeconds * 1.20f));
            scaleMode(0, 0.92f, 0.88f);
            scaleMode(1, 1.06f, 0.86f);
            scaleMode(2, 1.18f, 0.92f);
            retuneMode(0, -11.0f);
            retuneMode(1, 17.0f);
            retuneMode(2, -23.0f);
            retuneMode(3, 29.0f);
            clickLevel *= 1.20f;
            noiseLevel *= 0.62f;
            noiseCurrent = noiseLevel;
            break;

        case PercInstrumentAlgorithm::TubularBellHumStrike:
            dedicatedRateA = rateForHz(safeFund * 0.500f);
            dedicatedRateB = rateForHz(safeFund * 1.000f);
            dedicatedRateC = rateForHz(safeFund * 2.730f);
            dedicatedRateD = rateForHz(0.28f + safeVelocity * 0.34f);
            dedicatedGain = 0.160f * modelScale;
            setDecaySeconds(std::max(3.2f, settings.decaySeconds * 1.18f));
            scaleMode(0, 1.00f, 0.82f);
            scaleMode(1, 0.98f, 0.90f);
            scaleMode(4, 1.02f, 0.98f);
            retuneMode(0, -18.0f);
            retuneMode(1, 9.0f);
            retuneMode(2, -14.0f);
            retuneMode(4, 21.0f);
            retuneMode(6, -27.0f);
            clickLevel *= 1.08f;
            noiseLevel *= 0.82f;
            noiseCurrent = noiseLevel;
            break;

        case PercInstrumentAlgorithm::TriangleSteelShimmer:
            dedicatedRateA = rateForHz(safeFund * 2.756f);
            dedicatedRateB = rateForHz(safeFund * 5.404f);
            dedicatedRateC = rateForHz(safeFund * 13.350f);
            dedicatedRateD = rateForHz(1.6f + safeVelocity * 1.2f);
            dedicatedGain = 0.084f * modelScale;
            setDecaySeconds(std::max(1.9f, settings.decaySeconds * 1.04f));
            scaleMode(0, 0.80f, 0.86f);
            scaleMode(1, 1.18f, 0.80f);
            scaleMode(2, 1.30f, 0.88f);
            retuneMode(1, 8.0f);
            retuneMode(2, -15.0f);
            retuneMode(3, 19.0f);
            clickLevel *= 1.18f;
            noiseLevel *= 1.08f;
            noiseCurrent = noiseLevel;
            filterCurrentF = std::min(filterMaxF, filterCurrentF * 1.12f);
            break;

        case PercInstrumentAlgorithm::GlockenspielHardMallet:
            dedicatedRateA = rateForHz(safeFund);
            dedicatedRateB = rateForHz(safeFund * 2.756f);
            dedicatedRateC = rateForHz(safeFund * 5.404f);
            dedicatedRateD = rateForHz(1200.0f + safeVelocity * 1600.0f);
            dedicatedGain = 0.126f * modelScale;
            setDecaySeconds(std::max(1.4f, settings.decaySeconds * 0.92f));
            scaleMode(0, 1.04f, 0.90f);
            scaleMode(1, 1.20f, 0.86f);
            scaleMode(2, 1.16f, 0.94f);
            retuneMode(0, -5.0f);
            retuneMode(1, 10.0f);
            retuneMode(2, -16.0f);
            retuneMode(3, 23.0f);
            clickLevel *= 1.30f;
            clickFiltCoeff = std::clamp(clickFiltCoeff * 1.18f, 0.15f, 0.75f);
            noiseLevel *= 0.76f;
            noiseCurrent = noiseLevel;
            filterCurrentF = std::min(filterMaxF, filterCurrentF * 1.10f);
            break;
    }

    if (percModelOnly)
    {
        // Model-only rendering stays dry but exposes the dedicated resonator more clearly.
        dedicatedGain *= 1.18f;
        noiseLevel *= 0.96f;
        noiseCurrent = noiseLevel;
    }

    float dedicatedHeadroom = percModelOnly ? 0.90f : 0.92f;
    if (percAlgorithm == PercInstrumentAlgorithm::TimbalesMembraneBessel)
        dedicatedHeadroom = percModelOnly ? 0.64f : 0.58f;
    else if (percAlgorithm == PercInstrumentAlgorithm::DjembeHandDrumSkin)
        dedicatedHeadroom = percModelOnly ? 0.78f : 0.72f;
    else if (percAlgorithm == PercInstrumentAlgorithm::RainstickGranularCascade)
        dedicatedHeadroom = percModelOnly ? 1.02f : 1.22f;
    else if (percAlgorithm == PercInstrumentAlgorithm::SingingBowlRubBeating)
        dedicatedHeadroom = percModelOnly ? 0.72f : 0.66f;
    else if (percAlgorithm == PercInstrumentAlgorithm::WindChimesTubeCluster)
        dedicatedHeadroom = percModelOnly ? 0.76f : 0.68f;
    else if (percAlgorithm == PercInstrumentAlgorithm::TubularBellHumStrike)
        dedicatedHeadroom = percModelOnly ? 0.90f : 0.92f;
    else if (percAlgorithm == PercInstrumentAlgorithm::TriangleSteelShimmer
             || percAlgorithm == PercInstrumentAlgorithm::GlockenspielHardMallet)
        dedicatedHeadroom = percModelOnly ? 0.82f : 0.76f;
    baseLevelGain *= dedicatedHeadroom;
    levelGain = juce::jlimit(0.0f, 4.0f, levelGain * dedicatedHeadroom);
}

void PercVoice::applyDedicatedRenderSignature(float& signalL,
                                              float& signalR,
                                              const float noiseSig,
                                              const float sampleRate) noexcept
{
    if (!usesPercDedicatedAlgorithm() || dedicatedGain <= 0.0f || dedicatedEnv <= 0.00001f)
        return;

    const float safeSr = std::max(1.0f, sampleRate);
    auto advance = [](float& phase, const float rate) noexcept -> float {
        phase += std::clamp(rate, -0.45f, 0.45f);
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f) phase += 1.0f;
        return mpc::fastSin(phase);
    };

    auto wrapPhase = [](float& phase, const float rate) noexcept -> bool {
        phase += std::max(0.0f, rate);
        if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
            return true;
        }
        return false;
    };

    const float rawNoise = nextRandom() * 2.0f - 1.0f;
    const float env = std::clamp(dedicatedEnv, 0.0f, 1.8f);
    float sig = 0.0f;
    float side = 0.0f;

    switch (percAlgorithm)
    {
        case PercInstrumentAlgorithm::TimbalesMembraneBessel:
        {
            const float wobble = 0.94f + 0.06f * advance(dedicatedPhaseC, dedicatedRateC);
            const float membrane = advance(dedicatedPhaseA, dedicatedRateA)
                + 0.42f * advance(dedicatedPhaseB, dedicatedRateB);
            dedicatedStateA = dedicatedStateA * 0.992f + noiseSig * 0.008f;
            sig = membrane * wobble + dedicatedStateA * 0.45f;
            side = advance(dedicatedPhaseD, dedicatedRateD) * 0.10f;
            break;
        }

        case PercInstrumentAlgorithm::MarimbaWoodBarResonator:
        {
            const float bar = advance(dedicatedPhaseA, dedicatedRateA)
                + 0.20f * advance(dedicatedPhaseB, dedicatedRateB);
            dedicatedStateA = dedicatedStateA * 0.994f + advance(dedicatedPhaseC, dedicatedRateC) * 0.006f;
            dedicatedStateB = dedicatedStateB * 0.985f + rawNoise * 0.015f;
            sig = bar * 0.78f + dedicatedStateA * 0.82f + dedicatedStateB * 0.14f;
            side = advance(dedicatedPhaseD, dedicatedRateD) * 0.035f;
            break;
        }

        case PercInstrumentAlgorithm::DjembeHandDrumSkin:
        {
            const float bass = advance(dedicatedPhaseA, dedicatedRateA);
            const float tone = advance(dedicatedPhaseB, dedicatedRateB);
            dedicatedStateA = dedicatedStateA * 0.68f + rawNoise * 0.32f;
            dedicatedStateB = dedicatedStateB * 0.92f + (rawNoise - dedicatedStateA) * 0.08f;
            const float slap = dedicatedStateB * (0.62f + velocity * 0.72f)
                + advance(dedicatedPhaseC, dedicatedRateC) * 0.08f;
            sig = bass * 0.82f + tone * 0.30f + slap * 0.58f;
            side = advance(dedicatedPhaseD, dedicatedRateD) * slap * 0.10f;
            break;
        }

        case PercInstrumentAlgorithm::RainstickGranularCascade:
        {
            if (wrapPhase(dedicatedPhaseD, dedicatedRateD))
                dedicatedPulse = std::max(dedicatedPulse, 0.24f + nextRandom() * 0.76f);
            dedicatedPulse *= 0.988f;
            dedicatedStateA = dedicatedStateA * 0.80f + rawNoise * dedicatedPulse * 0.20f;
            dedicatedStateB = dedicatedStateB * 0.96f + dedicatedStateA * 0.04f;
            const float tube = 0.16f * advance(dedicatedPhaseA, dedicatedRateA)
                + 0.10f * advance(dedicatedPhaseB, dedicatedRateB);
            const float drift = 0.5f + 0.5f * advance(dedicatedPhaseC, dedicatedRateC);
            sig = dedicatedStateB * (0.90f + drift * 0.30f) + noiseSig * 0.38f + tube;
            side = dedicatedStateA * 0.45f;
            break;
        }

        case PercInstrumentAlgorithm::SingingBowlRubBeating:
        {
            const float beat = advance(dedicatedPhaseA, dedicatedRateA)
                - 0.92f * advance(dedicatedPhaseB, dedicatedRateB);
            const float rim = advance(dedicatedPhaseC, dedicatedRateC)
                * (0.86f + 0.14f * advance(dedicatedPhaseD, dedicatedRateD));
            dedicatedStateA = dedicatedStateA * 0.9988f + rawNoise * 0.0012f;
            dedicatedStateB = dedicatedStateB * 0.9995f + beat * 0.0005f;
            sig = beat * 0.94f + rim * 0.56f + dedicatedStateA * 0.28f + dedicatedStateB * 0.22f;
            side = (beat - rim) * 0.16f;
            break;
        }

        case PercInstrumentAlgorithm::WindChimesTubeCluster:
        {
            if (wrapPhase(dedicatedPhaseD, dedicatedRateD))
                dedicatedPulse = std::max(dedicatedPulse, 0.35f + nextRandom() * 0.65f);
            dedicatedPulse *= 0.995f;
            const float tubes = advance(dedicatedPhaseA, dedicatedRateA) * 0.54f
                + advance(dedicatedPhaseB, dedicatedRateB) * 0.38f
                + advance(dedicatedPhaseC, dedicatedRateC) * 0.28f;
            dedicatedStateA = dedicatedStateA * 0.998f + rawNoise * 0.002f;
            sig = tubes * (0.38f + dedicatedPulse * 0.94f) + dedicatedStateA * 0.08f;
            side = tubes * dedicatedPulse * 0.22f;
            break;
        }

        case PercInstrumentAlgorithm::TubularBellHumStrike:
        {
            const float hum = advance(dedicatedPhaseA, dedicatedRateA);
            const float octave = advance(dedicatedPhaseB, dedicatedRateB);
            const float bell = advance(dedicatedPhaseC, dedicatedRateC);
            const float slow = 0.92f + 0.08f * advance(dedicatedPhaseD, dedicatedRateD);
            dedicatedStateA = dedicatedStateA * 0.996f + rawNoise * 0.004f;
            sig = (hum * 0.34f - octave * 0.28f + bell * 0.74f + dedicatedStateA * 0.22f) * slow;
            side = (bell - hum) * 0.18f;
            break;
        }

        case PercInstrumentAlgorithm::TriangleSteelShimmer:
        {
            const float shimmer = advance(dedicatedPhaseA, dedicatedRateA) * 0.44f
                + advance(dedicatedPhaseB, dedicatedRateB) * 0.38f
                + advance(dedicatedPhaseC, dedicatedRateC) * 0.28f;
            const float flicker = 0.72f + 0.28f * advance(dedicatedPhaseD, dedicatedRateD);
            dedicatedStateA = dedicatedStateA * 0.86f + rawNoise * 0.14f;
            sig = shimmer * flicker + dedicatedStateA * 0.12f;
            side = shimmer * 0.16f;
            break;
        }

        case PercInstrumentAlgorithm::GlockenspielHardMallet:
        {
            const float bar = advance(dedicatedPhaseA, dedicatedRateA) * 0.58f
                + advance(dedicatedPhaseB, dedicatedRateB) * 0.40f
                + advance(dedicatedPhaseC, dedicatedRateC) * 0.22f;
            dedicatedStateA = dedicatedStateA * 0.72f + rawNoise * 0.28f;
            const float mallet = advance(dedicatedPhaseD, dedicatedRateD) * dedicatedStateA * 0.16f;
            sig = bar + mallet;
            side = bar * 0.07f;
            break;
        }
    }

    if (!std::isfinite(sig) || !std::isfinite(side))
        return;

    const float panPhase = (std::clamp(dedicatedPan, -1.0f, 1.0f) * 0.5f + 0.5f) * 0.25f;
    const float sigL = mpc::fastSin(panPhase + 0.25f);
    const float sigR = mpc::fastSin(panPhase);
    const float scaled = dedicatedGain * env;
    signalL += scaled * (sig * sigL + side * 0.35f);
    signalR += scaled * (sig * sigR - side * 0.35f);

    dedicatedEnv *= std::clamp(dedicatedDecay, 0.0f, 0.9999995f);
    if (dedicatedEnv < 1.0e-6f || !std::isfinite(dedicatedEnv))
        dedicatedEnv = 0.0f;

    (void) safeSr;
}

// =========================================================================
// noteOn
// =========================================================================
void PercVoice::noteOn(const InstrSettings& s, int midiNote, float vel, double sampleRate)
{
    const auto& ch = getCharacteristics();
    const auto preparedModulation = nextNoteModulation;
    const auto previousRoundRobin = roundRobinCount;
    reset();
    instrumentIndex = juce::jlimit(0, kNumInstruments - 1, getInstrumentIndex());
    const auto& model = getPercInstrumentModel(instrumentIndex);
    percAlgorithm = model.algorithm;
    const auto renderMode = getPercRenderEngineMode();
    percDedicatedActive = model.readiness == PercEngineReadiness::DedicatedVoice
        && renderMode != PercRenderEngineMode::LegacyFamily;
    percModelOnly = renderMode == PercRenderEngineMode::V2ModelOnly;
    currentModulation = preparedModulation;
    nextNoteModulation = preparedModulation;

    const float noteSafePitchSemi = std::isfinite(currentModulation.pitchSemi)
        ? juce::jlimit(-48.0f, 48.0f, currentModulation.pitchSemi)
        : 0.0f;
    modPitchFactor = std::exp2(noteSafePitchSemi / 12.0f);
    active   = true;
    age      = 0.0f;
    // One-shot: short maxAge to release voice quickly. Normal: scale with decay+release
    if (s.oneShot)
        maxAge = std::min(10.0f, std::max(0.5f, s.oneShotDecayMs * 0.001f * 5.0f));
    else
        maxAge = std::min(20.0f, std::max(5.0f, s.decaySeconds * 4.0f + s.releaseSeconds * 2.0f));
    velocity = vel;

    roundRobinCount = (previousRoundRobin + 1) % 3;
    rngState = static_cast<uint32_t>(midiNote * 73
        + static_cast<int>(vel * 1000.0f)
        + roundRobinCount * 7919);

    float sr = static_cast<float>(sampleRate);
    storedSampleRate = std::max(1.0f, sr);

    // Fundamental frequency from MIDI note
    float tuned = static_cast<float>(midiNote) + s.tuneSemitones;
    float fundHz = 440.0f * std::pow(2.0f, (tuned - 69.0f + currentModulation.pitchSemi) / 12.0f);

    // Level
    baseLevelGain = s.level * vel;
    levelGain = juce::jlimit(0.0f, 4.0f, baseLevelGain * std::clamp(currentModulation.levelMul, 0.0f, 4.0f));

    // Pitch following
    pitchFollowing = ch.pitchFollowing;
    randomization  = ch.randomization;
    colorShift     = s.color;
    baseAttackSeconds = s.attackSeconds;
    const float attackVelScale = 1.0f - vel * 0.5f;
    const float attackBrightScale = 1.0f - s.brightness * 0.3f;
    baseAttackSeconds *= attackVelScale * attackBrightScale;
    // One-shot mode: override decay with short fixed value for rhythmic/groove use
    if (s.oneShot)
        baseDecaySeconds = std::max(s.oneShotDecayMs * 0.001f, 0.010f); // min 10ms
    else
        baseDecaySeconds = s.decaySeconds;
    baseReleaseSeconds = s.releaseSeconds;
    basePan = s.pan;

    // A. Velocity brightness envelope — exponential curve (sqrt) for musical response
    // pp: restrained brightness, ff: natural compression instead of harsh brightness
    // curve: pow(vel, 0.5) compresses the top, raises the floor
    const float brightCurve    = std::sqrt(std::clamp(vel, 0.0f, 1.0f));
    const float brightMult    = 0.3f + brightCurve * 1.7f;
    brightCutoffTarget        = fundHz * ch.brightBaseMultiplier * brightMult;
    brightCutoffCurrent       = brightCutoffTarget * (1.0f + vel * 1.5f);
    const float bDecayMs      = 3.0f + vel * 3.0f; // 3-6ms exponential time constant for snappy attack mordant
    brightDecayCoeff          = std::exp(-1.0f / (bDecayMs * 0.001f * sr));

    // B. Register-dependent decay: higher notes decay faster (linear, upward only — graves neutres)
    const float noteNorm        = (static_cast<float>(midiNote) - 60.0f) / 60.0f;
    const float regDecayScale   = 1.0f - std::max(0.0f, noteNorm) * 0.25f;
    const float regDecayClamped = std::clamp(regDecayScale, 0.50f, 1.0f);

    // ---- Setup modes ----
    numActiveModes = std::min(ch.numModes, kMaxModes);

    for (int m = 0; m < numActiveModes; ++m)
    {
        auto& mode = modes[static_cast<std::size_t>(m)];

        // C. Mode frequency ratio: physical fixed ratios if available, else power law
        float modeRatio;
        if (ch.useFixedRatios && m < 12 && ch.fixedRatios[m] > 0.001f)
            modeRatio = ch.fixedRatios[m];
        else
            modeRatio = std::pow(static_cast<float>(m + 1), ch.modeSpread / 2.0f);

        // Mix pitched and noise-based
        float modeHz = fundHz * modeRatio * pitchFollowing
                     + (1.0f - pitchFollowing) * (200.0f + nextRandom() * 800.0f);

        // Keep Color mostly timbral on pitched instruments; allow wider drift only on textural sources.
        const bool texturalInstrument = ch.pitchFollowing < 0.25f || ch.randomization > 0.5f;
        const float colorPitchDepth = texturalInstrument ? 0.15f : 0.02f;
        modeHz *= (1.0f + (s.color - 0.5f) * colorPitchDepth);

        // Randomize slightly for wind chimes etc.
        if (randomization > 0.01f)
            modeHz *= (1.0f + (nextRandom() - 0.5f) * randomization * 0.3f);

        // Clamp frequency
        modeHz = std::clamp(modeHz, 20.0f, sr * 0.45f);

        mode.phase    = nextRandom();  // random start phase
        mode.basePhaseInc = modeHz / sr;
        mode.phaseInc = mode.basePhaseInc;

        // Round-robin: phase rotation per variation for natural organic feel
        // Wrap offset [0, 2/3) maps to 3 evenly-spaced timbres
        float rrOffset = static_cast<float>(roundRobinCount) / 3.0f;
        float rrPhaseAdd = (nextRandom() - 0.5f) * 0.25f * rrOffset;
        mode.phase = std::fmod(mode.phase + rrPhaseAdd + 1.0f, 1.0f);

        // A. Amplitude uses metallic field for spectral tilt (no static brightBoost — rolloff in render)
        // metallic=1.0 → equal modes (bright metal),  metallic=0.0 → fast rolloff (dark wood)
        float ampRoll = 1.0f / (1.0f + static_cast<float>(m) * (1.0f - ch.metallic) * 0.5f);
        mode.amplitude  = ampRoll;
        mode.currentAmp = ampRoll;

        // B. Per-mode decay: higher modes + higher notes decay faster
        // Inertance normalization: divide by ch.decayNorm (modeDecayBase * defaultDecay)
        // and multiply by referenceDecayNorm so the Decay knob gives comparable
        // durations across instruments with very different natural decay times.
        constexpr float referenceDecayNorm = 5.0f; // Timbales natural decay = 5.0
        float modeDecaySec = ch.modeDecayBase * s.decaySeconds * referenceDecayNorm / ch.decayNorm
            / (1.0f + static_cast<float>(m) * ch.modeDecaySpread * s.damping * 0.3f);
        modeDecaySec *= ch.ringTime;
        modeDecaySec *= regDecayClamped;   // B. register scale
        modeDecaySec  = std::max(modeDecaySec, 0.01f);

        mode.decayCoef = std::exp(-1.0f / (modeDecaySec * sr));

        // Per-mode stereo panning
        float modeSpread = s.stereoWidth * (nextRandom() - 0.5f) * 2.0f;
        float panPhase = (modeSpread * 0.5f + 0.5f) * 0.25f;
        mode.panL = mpc::fastSin(panPhase + 0.25f);
        mode.panR = mpc::fastSin(panPhase);
    }

    // D. Attack click transient (mallet/stick impact, velocity-dependent with round-robin variance)
    // sqrt curve for click level: natural compression, avoids harsh pp→ff jump
    const float clickCurve = std::sqrt(std::clamp(vel, 0.0f, 1.0f));
    clickLevel  = ch.clickAmount * clickCurve * 0.8f;
    // Round-robin variance: ±15% on level, ±10% on decay for organic feel
    const float rrLevelVar = 1.0f + (nextRandom() - 0.5f) * 0.15f;
    const float rrDecVar  = 1.0f + (nextRandom() - 0.5f) * 0.10f;
    clickLevel  *= rrLevelVar;
    const float clickDecayMs = ch.clickDecayMs * rrDecVar;
    clickDecay  = std::exp(-1.0f / (clickDecayMs * 0.001f * sr));
    clickFilt     = 0.0f;
    clickHpState  = 0.0f;
    // Adaptive click filter: dark wood (brightBaseMul ~3) → slow LP (dark click),
    // bright metal (brightBaseMul ~8) → fast LP (bright transient crack)
    // Also modulated slightly by metallic: more metallic → slightly faster
    clickFiltCoeff = std::clamp(0.15f + ch.brightBaseMultiplier * 0.05f + ch.metallic * 0.1f, 0.15f, 0.6f);

    // ---- Noise excitation ----
    float noiseAmt = ch.noiseAmount * s.noise;
    noiseLevel   = noiseAmt * vel;
    noiseCurrent = noiseLevel;
    noiseBright  = ch.noiseBrightness * s.brightness;
    noisePrev    = 0.0f;

    float noiseDecSec = std::max(ch.noiseDecay, 0.001f);
    noiseDecayCoef = std::exp(-1.0f / (noiseDecSec * sr));

    // ---- Body resonator ----
    bodyBuf.fill(0.0f);
    bodyWritePos = 0;
    bodyDampBand = 0.0f;
    bodyDampLow  = 0.0f;

    if (ch.bodyResonance > 0.001f && s.body > 0.01f)
    {
        float bodyHz = fundHz * ch.bodyDelay;
        bodyHz = std::max(bodyHz, 30.0f);
        bodyDelay    = sr / bodyHz;
        bodyDelay    = std::min(bodyDelay, static_cast<float>(kBodyBufSize - 2));
        baseBodyFeedback = ch.bodyResonance * s.body * 0.98f;
        bodyFeedback = baseBodyFeedback;

        // 2-pole SVF damping: cutoff based on body damping parameter
        float dampHz = std::clamp(bodyHz * (1.5f + (1.0f - ch.bodyDamping) * 6.0f), 100.0f, sr * 0.4f);
        bodyDampF    = 2.0f * mpc::fastSin(dampHz / sr * 0.5f);
        bodyDampQinv = 0.5f + ch.bodyDamping * 1.0f;  // Q range: 0.5–1.5
    }
    else
    {
        bodyDelay    = 0.0f;
        bodyFeedback = 0.0f;
    }

    // ---- SVF filter ----
    svfBand = 0.0f;
    svfLow  = 0.0f;

    float cutNorm = std::clamp(s.cutoffHz / sr, 20.0f / sr, 0.45f);
    filterF = 2.0f * mpc::fastSin(cutNorm * 0.5f);
    filterQinv = 1.0f / std::max(0.5f, 0.5f + (1.0f - s.brightness) * 1.0f);
    filterMaxF = (-filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f)) * 0.95f;
    filterF = std::min(filterF, filterMaxF);
    filterBaseF = filterF;
    baseFilterBaseF = filterBaseF;
    baseFilterQinv = filterQinv;
    filterCurrentF = filterF * (1.0f + vel * 3.0f);  // bright on attack
    filterDecayCoeff = std::exp(-1.0f / (0.015f * sr)); // 15ms decay — snappy filter sweep

    // ---- ADSR envelope ----
    envStage = Attack;
    envLevel = 0.0f;
    envSustain  = s.sustainLevel;
    updateEnvelopeCoefficients(sr);

    // ---- Stereo panning ----
    stereoWidth = s.stereoWidth;
    updatePanFromModulation();
    updateFilterFromModulation(sr);
    applyDedicatedNoteProfile(fundHz, sr, vel, s);

    // ---- DC blocker ----
    dcX1 = 0.0f;
    dcY1 = 0.0f;
}

// =========================================================================
// noteOff
// =========================================================================
void PercVoice::noteOff()
{
    if (envStage != Off)
    {
        quickReleaseForced = false;
        envStage = Release;
    }
}

void PercVoice::forceQuickRelease() noexcept
{
    if (envStage == Off)
        return;
    envStage = Release;
    quickReleaseForced = true;
    const auto releaseSamples = std::max(1, static_cast<int>(std::round(std::max(1.0f, storedSampleRate) * 0.005f)));
    envRelMul = std::exp(std::log(0.001f) / static_cast<float>(releaseSamples));
}

// =========================================================================
// render
// =========================================================================
void PercVoice::render(float& outL, float& outR, double sampleRate)
{
    if (!active)
    {
        outL = outR = 0.0f;
        return;
    }

    float sr = static_cast<float>(sampleRate);
    storedSampleRate = std::max(1.0f, sr);

    // ---- ADSR envelope ----
    switch (envStage)
    {
        case Attack:
            envLevel += envAttackInc;
            if (envLevel >= 1.0f) { envLevel = 1.0f; envStage = Decay; }
            break;
        case Decay:
            envLevel = envSustain + (envLevel - envSustain) * envDecayMul;
            if (envLevel <= envSustain + 0.0001f) envStage = Sustain;
            break;
        case Sustain:
            break;
        case Release:
            envLevel *= envRelMul;
            if (envLevel < 0.0001f) { active = false; outL = outR = 0.0f; return; }
            break;
        case Off:
            active = false; outL = outR = 0.0f; return;
    }

    // ---- Generate noise excitation ----
    float noiseSig = 0.0f;
    if (noiseCurrent > 0.0001f)
    {
        float raw = nextRandom() * 2.0f - 1.0f;
        // Simple LP to shape noise brightness
        float lp = noisePrev + noiseBright * (raw - noisePrev);
        noisePrev = lp;
        noiseSig = lp * noiseCurrent;
        noiseCurrent *= noiseDecayCoef;
    }

    // ---- Modal oscillators ----
    float signalL = 0.0f;
    float signalR = 0.0f;
    const float rawPbf = pitchBendFactor * modPitchFactor;
    const float pbf = std::isfinite(rawPbf) ? juce::jlimit(0.0625f, 16.0f, rawPbf) : 1.0f;

    // A. Brightness envelope decay — once per sample before mode loop
    brightCutoffCurrent = brightCutoffTarget
                        + (brightCutoffCurrent - brightCutoffTarget) * brightDecayCoeff;
    const float safeCutoff = std::max(50.0f, brightCutoffCurrent);
    const float fsrInv = 1.0f / static_cast<float>(sampleRate);

    for (int m = 0; m < numActiveModes; ++m)
    {
        auto& mode = modes[static_cast<std::size_t>(m)];
        if (mode.currentAmp < 0.00001f) continue;

        if (!std::isfinite(mode.phase) || !std::isfinite(mode.basePhaseInc) || !std::isfinite(mode.currentAmp))
        {
            mode.phase = 0.0f;
            mode.basePhaseInc = 0.0f;
            mode.phaseInc = 0.0f;
            mode.currentAmp = 0.0f;
            continue;
        }

        const float effectivePhaseInc = mode.basePhaseInc * pbf;
        if (!std::isfinite(effectivePhaseInc) || effectivePhaseInc <= 0.0f)
        {
            mode.currentAmp = 0.0f;
            continue;
        }
        const float aliasGuardGain = effectivePhaseInc <= 0.45f
            ? 1.0f
            : std::clamp((0.50f - effectivePhaseInc) / 0.05f, 0.0f, 1.0f);
        mode.phaseInc = std::min(effectivePhaseInc, 0.45f);
        if (aliasGuardGain <= 0.0001f)
        {
            mode.currentAmp *= mode.decayCoef;
            continue;
        }

        float osc = mpc::fastSin(mode.phase);
        mode.phase += mode.phaseInc;
        while (mode.phase >= 1.0f) mode.phase -= 1.0f;
        while (mode.phase < 0.0f) mode.phase += 1.0f;

        // A. Per-mode frequency rolloff using animated brightness cutoff
        const float partialHz = mode.phaseInc / fsrInv;   // phaseInc * sr
        const float ratio     = partialHz / safeCutoff;
        const float rolloff   = 1.0f / (1.0f + ratio * ratio);

        float sig = osc * mode.currentAmp * rolloff * aliasGuardGain;
        mode.currentAmp *= mode.decayCoef;

        signalL += sig * mode.panL;
        signalR += sig * mode.panR;
    }

    // D. Attack click transient (brief HP noise burst — mallet/stick impact)
    if (clickLevel > 0.0001f)
    {
        const float raw   = nextRandom() * 2.0f - 1.0f;
        clickFilt        += clickFiltCoeff * (raw - clickFilt);
        clickHpState      = raw - clickFilt + 0.995f * clickHpState;  // true DC-blocking HP
        const float click = clickHpState;
        signalL += click * clickLevel * 0.45f;
        signalR += click * clickLevel * 0.45f;
        clickLevel *= clickDecay;
    }

    // Add noise excitation to signal — stereo positioned using stereoWidth + random
    float noisePan = stereoWidth * (nextRandom() - 0.5f) * 2.0f;
    float noisePanL = mpc::fastSin((0.5f + noisePan * 0.5f) * 0.25f + 0.25f);
    float noisePanR = mpc::fastSin((0.5f - noisePan * 0.5f) * 0.25f);
    signalL += noiseSig * noisePanL;
    signalR += noiseSig * noisePanR;

    // ---- Body resonator (2-pole SVF damping) ----
    if (bodyFeedback > 0.001f && bodyDelay > 1.0f)
    {
        float delayed = readComb(bodyDelay);

        // SVF lowpass for frequency-dependent damping
        float hp = delayed - bodyDampLow - bodyDampQinv * bodyDampBand;
        bodyDampBand += bodyDampF * hp;
        bodyDampLow  += bodyDampF * bodyDampBand;
        float bodyOut = bodyDampLow * bodyFeedback;

        // Body stereo spread — use signal's existing L/R difference to derive a spread direction
        const float sigDiff = signalL - signalR;
        const float bodySpreadL = bodyOut * (1.0f + sigDiff * 0.25f);
        const float bodySpreadR = bodyOut * (1.0f - sigDiff * 0.25f);

        float mixed = (signalL + signalR) * 0.5f + bodyOut;
        if (!std::isfinite(mixed) || std::abs(mixed) > 32.0f)
        {
            mixed = 0.0f;
            bodyDampBand = 0.0f;
            bodyDampLow = 0.0f;
        }
        bodyBuf[static_cast<std::size_t>(bodyWritePos)] = mixed;
        bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;

        signalL += bodySpreadL * 0.5f;
        signalR += bodySpreadR * 0.5f;

        // Denormal flush for body damping SVF
        if (std::abs(bodyDampBand) < 1e-25f) bodyDampBand = 0.0f;
        if (std::abs(bodyDampLow)  < 1e-25f) bodyDampLow  = 0.0f;
    }

    applyDedicatedRenderSignature(signalL, signalR, noiseSig, sr);

    // ---- DC blocker (R = 0.99975) ----
    {
        constexpr float R = 0.99975f;
        float mono = (signalL + signalR) * 0.5f;
        float dcOut = mono - dcX1 + R * dcY1;
        if (!std::isfinite(dcOut) || !std::isfinite(mono))
        {
            dcX1 = 0.0f;
            dcY1 = 0.0f;
            dcOut = 0.0f;
            mono = 0.0f;
        }
        dcX1 = mono;
        dcY1 = dcOut;
        float diff = (signalL - signalR) * 0.5f;
        signalL = dcOut + diff;
        signalR = dcOut - diff;
    }

    // ---- SVF lowpass (stereo-preserving, animated cutoff) ----
    {
        filterCurrentF = filterBaseF + (filterCurrentF - filterBaseF) * filterDecayCoeff;
        const float useF = std::min(filterCurrentF, filterMaxF);

        const float monoIn = (signalL + signalR) * 0.5f;
        float hp = monoIn - svfLow - filterQinv * svfBand;
        if (!std::isfinite(hp) || !std::isfinite(useF) || !std::isfinite(svfBand) || !std::isfinite(svfLow))
        {
            svfBand = 0.0f;
            svfLow = 0.0f;
            hp = 0.0f;
        }
        svfBand += useF * hp;
        svfLow  += useF * svfBand;
        if (!std::isfinite(svfBand) || !std::isfinite(svfLow))
        {
            svfBand = 0.0f;
            svfLow = 0.0f;
        }
        const float diff = (signalL - signalR) * 0.5f;
        signalL = svfLow + diff;
        signalR = svfLow - diff;

        // Denormal flush for SVF filter
        if (std::abs(svfBand) < 1e-25f) svfBand = 0.0f;
        if (std::abs(svfLow)  < 1e-25f) svfLow  = 0.0f;
    }

    float filtL = signalL;
    float filtR = signalR;

    // ---- Apply envelope and gain ----
    // Ducking: if a new note just stole this slot, duckingMul is < 1. Recover toward 1 each frame (≈50ms time constant)
    if (currentModulation.duckingMul < 1.0f)
    {
        currentModulation.duckingMul = currentModulation.duckingMul * 0.999f + 0.001f; // toward 1.0
        if (currentModulation.duckingMul >= 0.9995f)
            currentModulation.duckingMul = 1.0f;
    }
    float duckingMul = currentModulation.duckingMul;
    float gain = envLevel * levelGain * duckingMul;
    outL = filtL * gain * panL;
    outR = filtR * gain * panR;
    if (usesPercDedicatedAlgorithm())
    {
        constexpr float dedicatedCeiling = 0.98f;
        if (std::abs(outL) > dedicatedCeiling)
            outL = dedicatedCeiling * std::tanh(outL / dedicatedCeiling);
        if (std::abs(outR) > dedicatedCeiling)
            outR = dedicatedCeiling * std::tanh(outR / dedicatedCeiling);
    }
    if (!std::isfinite(outL) || !std::isfinite(outR) || std::abs(outL) > 8.0f || std::abs(outR) > 8.0f)
    {
        svfBand = svfLow = bodyDampBand = bodyDampLow = dcX1 = dcY1 = 0.0f;
        outL = std::isfinite(outL) ? 0.95f * std::tanh(outL / 0.95f) : 0.0f;
        outR = std::isfinite(outR) ? 0.95f * std::tanh(outR / 0.95f) : 0.0f;
    }
    else if (pbf > 4.0f)
    {
        outL = 0.95f * std::tanh(outL / 0.95f);
        outR = 0.95f * std::tanh(outR / 0.95f);
    }

    // ---- Age guard ----
    age += 1.0f / static_cast<float>(sampleRate);
    if (age > maxAge) active = false;
}

void PercVoice::renderBlock(float* outL, float* outR, int numSamples, double sampleRate)
{
    if (outL == nullptr || outR == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
        render(outL[i], outR[i], sampleRate);
}

#define MPC_DEFINE_PERC_VOICE(className, instrumentIndex) \
const InstrCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mpc::getCharacteristics(instrumentIndex); \
}

MPC_DEFINE_PERC_VOICE(TimbalesVoice, 0)
MPC_DEFINE_PERC_VOICE(MarimbaVoice, 1)
MPC_DEFINE_PERC_VOICE(DjembeVoice, 2)
MPC_DEFINE_PERC_VOICE(RainstickVoice, 3)
MPC_DEFINE_PERC_VOICE(SingingBowlVoice, 4)
MPC_DEFINE_PERC_VOICE(WindChimesVoice, 5)
MPC_DEFINE_PERC_VOICE(TubularBellVoice, 6)
MPC_DEFINE_PERC_VOICE(TriangleVoice, 7)
MPC_DEFINE_PERC_VOICE(GlockenspielVoice, 8)

#undef MPC_DEFINE_PERC_VOICE

std::unique_ptr<PercVoice> createVoiceForInstrument(const int instrIndex)
{
    switch (std::clamp(instrIndex, 0, kNumInstruments - 1))
    {
        case 0: return std::make_unique<TimbalesVoice>();
        case 1: return std::make_unique<MarimbaVoice>();
        case 2: return std::make_unique<DjembeVoice>();
        case 3: return std::make_unique<RainstickVoice>();
        case 4: return std::make_unique<SingingBowlVoice>();
        case 5: return std::make_unique<WindChimesVoice>();
        case 6: return std::make_unique<TubularBellVoice>();
        case 7: return std::make_unique<TriangleVoice>();
        case 8: return std::make_unique<GlockenspielVoice>();
        default: break;
    }

    return {};
}

} // namespace mpc
