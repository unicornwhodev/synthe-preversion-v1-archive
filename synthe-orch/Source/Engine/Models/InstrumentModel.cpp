#include "InstrumentModel.h"
#include "../SinTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mos::v2
{
namespace
{
float clampAudio(const float value) noexcept
{
    return juce::jlimit(-4.0f, 4.0f, std::isfinite(value) ? value : 0.0f);
}

float wrapPhase(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase < 0.0f ? phase + 1.0f : phase;
}

std::uint32_t seedFor(const InstrumentModelNoteContext& context, const std::uint32_t salt) noexcept
{
    auto seed = static_cast<std::uint32_t>((context.instrumentIndex + 1) * 2654435761u)
        ^ static_cast<std::uint32_t>((context.midiNote + 512) * 2246822519u)
        ^ static_cast<std::uint32_t>(juce::jlimit(0, 1000, static_cast<int>(context.velocity * 1000.0f)))
        ^ salt;
    return seed == 0 ? 0x7f4a7c15u : seed;
}

float articulationPressureScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 1.08f;
        case InstrumentArticulation::Marcato:  return 1.18f;
        case InstrumentArticulation::Soft:     return 0.78f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float articulationNoiseScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 1.22f;
        case InstrumentArticulation::Marcato:  return 1.36f;
        case InstrumentArticulation::Soft:     return 0.62f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float articulationToneScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 1.08f;
        case InstrumentArticulation::Marcato:  return 1.18f;
        case InstrumentArticulation::Soft:     return 0.78f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float articulationBodyScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 0.82f;
        case InstrumentArticulation::Marcato:  return 0.95f;
        case InstrumentArticulation::Soft:     return 1.10f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float articulationBloomScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 0.70f;
        case InstrumentArticulation::Marcato:  return 1.35f;
        case InstrumentArticulation::Soft:     return 0.65f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float averageFormantGain(const InstrCharacteristics& characteristics) noexcept
{
    if (!characteristics.hasFormants)
        return 0.0f;

    return juce::jlimit(0.0f, 1.2f,
        (characteristics.formantGains[0]
            + characteristics.formantGains[1]
            + characteristics.formantGains[2]) / 3.0f);
}

float formantBrightnessCue(const InstrCharacteristics& characteristics) noexcept
{
    if (!characteristics.hasFormants)
        return 1.0f;

    const float gainSum = juce::jmax(0.001f,
        characteristics.formantGains[0]
            + characteristics.formantGains[1]
            + characteristics.formantGains[2]);
    const float weightedFrequency =
        (characteristics.formantFreqs[0] * characteristics.formantGains[0]
            + characteristics.formantFreqs[1] * characteristics.formantGains[1]
            + characteristics.formantFreqs[2] * characteristics.formantGains[2]) / gainSum;

    return juce::jlimit(0.74f, 1.42f,
        0.88f
            + averageFormantGain(characteristics) * 0.22f
            + juce::jlimit(0.0f, 1.0f, weightedFrequency / 4200.0f) * 0.22f
            + (characteristics.formantRegisterScale - 0.90f) * 0.22f);
}

float formantBodyCue(const InstrCharacteristics& characteristics) noexcept
{
    if (!characteristics.hasFormants)
        return 1.0f;

    return juce::jlimit(0.78f, 1.50f,
        0.88f
            + characteristics.formantGains[0] * 0.28f
            + characteristics.formantGains[1] * 0.18f
            + averageFormantGain(characteristics) * 0.16f
            + (1.0f - characteristics.formantRegisterScale) * 0.12f);
}

float articulationDecayTimeScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 0.48f;
        case InstrumentArticulation::Marcato:  return 0.72f;
        case InstrumentArticulation::Soft:     return 1.16f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float articulationLevelScale(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return 0.95f;
        case InstrumentArticulation::Marcato:  return 1.05f;
        case InstrumentArticulation::Soft:     return 0.72f;
        case InstrumentArticulation::Sustain:
        default:                              return 1.0f;
    }
}

float legatoAmountFor(const bool active, const float amount) noexcept
{
    return active ? juce::jlimit(0.0f, 1.0f, amount) : 0.0f;
}

float legatoOnsetScaleFor(const bool active,
                          const float amount,
                          const float onsetScale,
                          const float minimumScale) noexcept
{
    const float legato = legatoAmountFor(active, amount);
    if (legato <= 0.0f)
        return 1.0f;

    const float target = juce::jlimit(minimumScale, 1.0f, onsetScale);
    return 1.0f + (target - 1.0f) * legato;
}

float legatoTargetScaleFor(const bool active,
                           const float amount,
                           const float targetScale) noexcept
{
    const float legato = legatoAmountFor(active, amount);
    return 1.0f + (targetScale - 1.0f) * legato;
}

class NoiseState
{
public:
    void reset(const std::uint32_t seed) noexcept
    {
        state = seed == 0 ? 0x51ed270bu : seed;
    }

    float nextSigned() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (static_cast<float>(state & 0x00ffffffu) / 8388607.5f) - 1.0f;
    }

private:
    std::uint32_t state = 0x51ed270bu;
};

class LegacyInstrumentModel final : public InstrumentModel
{
public:
    const char* name() const noexcept override { return "legacy-family"; }
};

enum class BowedStringAlgorithm
{
    ViolinBright = 0,
    ViolaWarm,
    CelloResonant,
    ContrabassDeep
};

struct BowedStringProfile
{
    const char* modelName;
    BowedStringAlgorithm algorithm;
    std::uint32_t noiseSalt;
    float phaseSeedScale;
    float phaseVelocityScale;
    float secondPhaseOffset;
    float secondRatioBase;
    float secondRatioRegister;
    float secondAmpBase;
    float secondAmpRegister;
    float pressureBase;
    float pressureVelocity;
    float memoryBase;
    float memoryVelocity;
    float noiseBase;
    float noiseVelocity;
    float noiseLowRegister;
    float noiseHighRegister;
    float bodyLowRegister;
    float bodyHighRegister;
    float stringLowRegister;
    float stringHighRegister;
    float bodyFastBase;
    float bodyFastRegister;
    float bodySlowBase;
    float bodySlowLowRegister;
    float bodySubtract;
    float stereoL;
    float stereoR;
    float chorusSensitivity;
    float legacyCoreGain;
};

constexpr BowedStringProfile kViolinProfile {
    "v2-violin-bowed",
    BowedStringAlgorithm::ViolinBright,
    0x5b0d15eau,
    0.173f,
    0.37f,
    0.127f,
    2.005f,
    0.012f,
    0.20f,
    0.08f,
    1.45f,
    2.20f,
    0.020f,
    0.018f,
    0.010f,
    0.025f,
    0.85f,
    0.62f,
    0.105f,
    0.060f,
    0.040f,
    0.055f,
    0.055f,
    0.020f,
    0.012f,
    0.010f,
    0.48f,
    0.82f,
    0.74f,
    0.10f,
    0.44f
};

constexpr BowedStringProfile kViolaProfile {
    "v2-viola-bowed",
    BowedStringAlgorithm::ViolaWarm,
    0x29a76c3du,
    0.151f,
    0.31f,
    0.191f,
    2.002f,
    0.009f,
    0.16f,
    0.06f,
    1.35f,
    1.85f,
    0.018f,
    0.016f,
    0.009f,
    0.020f,
    0.82f,
    0.58f,
    0.125f,
    0.078f,
    0.038f,
    0.050f,
    0.050f,
    0.017f,
    0.014f,
    0.012f,
    0.53f,
    0.79f,
    0.77f,
    0.075f,
    0.42f
};

constexpr BowedStringProfile kCelloProfile {
    "v2-cello-bowed",
    BowedStringAlgorithm::CelloResonant,
    0x8d42e5b1u,
    0.137f,
    0.27f,
    0.243f,
    2.001f,
    0.006f,
    0.13f,
    0.05f,
    1.22f,
    1.58f,
    0.016f,
    0.013f,
    0.007f,
    0.016f,
    0.76f,
    0.52f,
    0.165f,
    0.106f,
    0.034f,
    0.043f,
    0.044f,
    0.014f,
    0.017f,
    0.015f,
    0.60f,
    0.76f,
    0.82f,
    0.052f,
    0.40f
};

constexpr BowedStringProfile kContrabassProfile {
    "v2-contrabass-bowed",
    BowedStringAlgorithm::ContrabassDeep,
    0x3c91f26bu,
    0.109f,
    0.23f,
    0.307f,
    2.000f,
    0.004f,
    0.10f,
    0.035f,
    1.08f,
    1.28f,
    0.014f,
    0.010f,
    0.005f,
    0.012f,
    0.70f,
    0.46f,
    0.210f,
    0.135f,
    0.030f,
    0.036f,
    0.038f,
    0.011f,
    0.020f,
    0.018f,
    0.68f,
    0.73f,
    0.86f,
    0.035f,
    0.36f
};

const BowedStringProfile& bowedStringProfileFor(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 1:  return kViolaProfile;
        case 2:  return kCelloProfile;
        case 3:  return kContrabassProfile;
        case 0:
        default: return kViolinProfile;
    }
}

bool isBowedStringV2Index(const int instrumentIndex) noexcept
{
    return instrumentIndex >= 0 && instrumentIndex <= 3;
}

float bowedPressureScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 1.10f;
        case BowedStringAlgorithm::ViolaWarm:      return 0.98f;
        case BowedStringAlgorithm::CelloResonant:  return 0.88f;
        case BowedStringAlgorithm::ContrabassDeep: return 0.78f;
        default:                                   return 1.0f;
    }
}

float bowedNoiseScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 1.50f;
        case BowedStringAlgorithm::ViolaWarm:      return 0.94f;
        case BowedStringAlgorithm::CelloResonant:  return 0.70f;
        case BowedStringAlgorithm::ContrabassDeep: return 0.42f;
        default:                                   return 1.0f;
    }
}

float bowedBodyScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 0.82f;
        case BowedStringAlgorithm::ViolaWarm:      return 1.06f;
        case BowedStringAlgorithm::CelloResonant:  return 1.28f;
        case BowedStringAlgorithm::ContrabassDeep: return 1.50f;
        default:                                   return 1.0f;
    }
}

float bowedStringScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 1.12f;
        case BowedStringAlgorithm::ViolaWarm:      return 1.02f;
        case BowedStringAlgorithm::CelloResonant:  return 0.94f;
        case BowedStringAlgorithm::ContrabassDeep: return 0.86f;
        default:                                   return 1.0f;
    }
}

float bowedMemoryScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 0.86f;
        case BowedStringAlgorithm::ViolaWarm:      return 1.00f;
        case BowedStringAlgorithm::CelloResonant:  return 1.15f;
        case BowedStringAlgorithm::ContrabassDeep: return 1.30f;
        default:                                   return 1.0f;
    }
}

float bowedSecondRatioScale(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 1.006f;
        case BowedStringAlgorithm::ViolaWarm:      return 1.002f;
        case BowedStringAlgorithm::CelloResonant:  return 0.998f;
        case BowedStringAlgorithm::ContrabassDeep: return 0.994f;
        default:                                   return 1.0f;
    }
}

float bowedScrapeCoeff(const BowedStringAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BowedStringAlgorithm::ViolinBright:   return 0.070f;
        case BowedStringAlgorithm::ViolaWarm:      return 0.095f;
        case BowedStringAlgorithm::CelloResonant:  return 0.125f;
        case BowedStringAlgorithm::ContrabassDeep: return 0.170f;
        default:                                   return 0.110f;
    }
}

class BowedStringModel final : public InstrumentModel
{
public:
    explicit BowedStringModel(const int instrumentIndexToUse) noexcept
        : profile(bowedStringProfileFor(instrumentIndexToUse))
    {
    }

    const char* name() const noexcept override { return profile.modelName; }
    bool isV2() const noexcept override { return true; }
    float legacyCoreGain() const noexcept override { return profile.legacyCoreGain; }

    void noteOn(const InstrumentModelNoteContext& context) override
    {
        sampleRate = static_cast<float>(std::max(22050.0, context.sampleRate));
        baseFrequencyHz = context.baseFrequencyHz;
        velocity = juce::jlimit(0.0f, 1.0f, context.velocity);
        noteExpression = juce::jlimit(0.0f, 1.5f, context.expression);
        noteTone = juce::jlimit(0.0f, 1.0f, context.tone);
        noteMotion = juce::jlimit(0.0f, 1.0f, context.motion);
        noteArticulation = juce::jlimit(0.0f, 1.0f, context.articulation);
        noteArticulationMode = context.articulationMode;
        noteLegatoAmount = legatoAmountFor(context.legatoTransition, context.legatoAmount);
        noteLegatoOnsetScale = legatoOnsetScaleFor(context.legatoTransition,
                                                   context.legatoAmount,
                                                   context.legatoOnsetScale,
                                                   0.30f);
        const auto range = getInstrMidiNoteRange(context.instrumentIndex);
        const auto span = std::max(1, range.high - range.low);
        registerNorm = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(context.midiNote - range.low) / static_cast<float>(span));
        phase = wrapPhase(profile.phaseSeedScale * static_cast<float>(context.midiNote)
            + velocity * profile.phaseVelocityScale);
        secondPhase = wrapPhase(phase + profile.secondPhaseOffset);
        bodyFast = 0.0f;
        bodySlow = 0.0f;
        scrapeState = 0.0f;
        bowMemory = noteLegatoAmount * 0.10f * velocity;
        noise.reset(seedFor(context, profile.noiseSalt));

        const auto& characteristics = context.characteristics;
        const float bowNoiseCue = juce::jlimit(0.72f, 1.62f,
            0.78f + characteristics.bowNoiseAmount * 4.20f);
        bodySignatureCue = juce::jlimit(0.74f, 1.72f,
            0.82f
                + characteristics.bodyDelayRatio * 0.66f
                + characteristics.bodyMaxFeedback * 0.72f
                + characteristics.builtInWarmth * 0.30f
                - characteristics.bodyDamping * 0.22f);
        bridgeSignatureCue = juce::jlimit(0.70f, 1.58f,
            1.10f
                + (0.30f - characteristics.builtInWarmth) * 0.72f
                + (characteristics.bowNoiseAmount - 0.10f) * 2.20f
                + characteristics.attackShape * 0.18f
                - characteristics.bodyDelayRatio * 0.30f);
        oddEvenSignatureCue = juce::jlimit(0.84f, 1.24f,
            0.96f
                + static_cast<float>(juce::jlimit(0, 3, characteristics.numOscillators)) * 0.045f
                + juce::jlimit(0.0f, 0.006f, characteristics.detuneAmount) * 18.0f
                - characteristics.builtInWarmth * 0.10f);

        bowPressure = (profile.pressureBase + velocity * profile.pressureVelocity)
            * (0.88f + noteArticulation * 0.24f)
            * (0.92f + noteExpression * 0.08f)
            * bowedPressureScale(profile.algorithm)
            * articulationPressureScale(noteArticulationMode)
            * legatoTargetScaleFor(context.legatoTransition, context.legatoAmount, 0.88f)
            * juce::jlimit(0.82f, 1.34f, bridgeSignatureCue * 0.96f);
        bowNoise = (profile.noiseBase + velocity * profile.noiseVelocity)
            * juce::jmap(registerNorm, profile.noiseLowRegister, profile.noiseHighRegister)
            * (0.82f + noteArticulation * 0.36f)
            * (0.92f + noteTone * 0.18f)
            * bowedNoiseScale(profile.algorithm)
            * articulationNoiseScale(noteArticulationMode)
            * noteLegatoOnsetScale
            * bowNoiseCue
            * bridgeSignatureCue;
        bodyGain = juce::jmap(registerNorm, profile.bodyLowRegister, profile.bodyHighRegister)
            * (1.08f - noteTone * 0.14f)
            * bowedBodyScale(profile.algorithm)
            * articulationBodyScale(noteArticulationMode)
            * bodySignatureCue;
        stringGain = juce::jmap(registerNorm, profile.stringLowRegister, profile.stringHighRegister)
            * (0.86f + noteTone * 0.30f)
            * bowedStringScale(profile.algorithm)
            * articulationToneScale(noteArticulationMode)
            * bridgeSignatureCue
            * oddEvenSignatureCue;
    }

    void renderPreFilter(const InstrumentModelFrame& frame,
                         float& signalL,
                         float& signalR) noexcept override
    {
        const auto sr = std::max(22050.0f, frame.sampleRate > 0.0f ? frame.sampleRate : sampleRate);
        const float freq = juce::jlimit(20.0f, sr * 0.46f, baseFrequencyHz * frame.pitchMult);
        const float inc = freq / sr;
        const float secondInc = inc
            * (profile.secondRatioBase + registerNorm * profile.secondRatioRegister)
            * bowedSecondRatioScale(profile.algorithm);
        const float frameTone = juce::jlimit(0.0f, 1.0f, frame.tone);
        const float frameMotion = juce::jlimit(0.0f, 1.0f, frame.motion);
        const float frameExpression = juce::jlimit(0.0f, 1.5f, frame.expression);
        const float frameArticulation = juce::jlimit(0.0f, 1.0f, frame.articulation);
        const auto articulationMode = frame.articulationMode;
        const float frameLegatoOnsetScale = legatoOnsetScaleFor(frame.legatoTransition,
                                                                frame.legatoAmount,
                                                                frame.legatoOnsetScale,
                                                                0.30f);
        const float expressionDrive = 0.86f + frameExpression * 0.14f;

        const float fundamental = mos::fastSin(phase);
        float second = mos::fastSin(secondPhase)
            * (profile.secondAmpBase + registerNorm * profile.secondAmpRegister);
        const float upperScratch = mos::fastSin(wrapPhase(phase * 3.0f + 0.031f));
        float excitation = fundamental + second + bowMemory * (0.18f + frameMotion * 0.08f);
        float pressureDrive = bowPressure * (0.96f + frameArticulation * 0.08f);

        if (profile.algorithm == BowedStringAlgorithm::ViolinBright)
        {
            excitation += upperScratch * (0.070f + frameTone * 0.040f) * bridgeSignatureCue;
            pressureDrive *= 1.06f;
        }
        else if (profile.algorithm == BowedStringAlgorithm::ViolaWarm)
        {
            second *= 0.92f;
            excitation = fundamental * 0.96f
                + second * oddEvenSignatureCue
                + upperScratch * 0.018f * bridgeSignatureCue
                + bowMemory * (0.24f + frameMotion * 0.07f);
            pressureDrive *= 0.98f;
        }
        else if (profile.algorithm == BowedStringAlgorithm::CelloResonant)
        {
            second *= 0.84f;
            excitation = fundamental * 0.92f
                + second * oddEvenSignatureCue
                + upperScratch * 0.014f * bridgeSignatureCue
                + bowMemory * (0.30f + frameMotion * 0.08f) * bodySignatureCue;
            pressureDrive *= 0.94f;
        }
        else if (profile.algorithm == BowedStringAlgorithm::ContrabassDeep)
        {
            second *= 0.74f;
            excitation = fundamental * 0.86f
                + second * oddEvenSignatureCue
                + bowMemory * (0.36f + frameMotion * 0.06f) * bodySignatureCue;
            pressureDrive *= 0.88f;
        }

        const float friction = std::tanh(excitation * pressureDrive);
        bowMemory += (friction - bowMemory)
            * (profile.memoryBase + velocity * profile.memoryVelocity)
            * bowedMemoryScale(profile.algorithm);

        const float rawNoise = noise.nextSigned();
        scrapeState += (rawNoise - scrapeState) * bowedScrapeCoeff(profile.algorithm);
        const float scrape = (rawNoise - scrapeState)
            * bowNoise
            * (0.35f + frame.envOut * 0.65f)
            * (0.78f + frameArticulation * 0.34f)
            * articulationNoiseScale(articulationMode)
            * frameLegatoOnsetScale;

        bodyFast += (friction - bodyFast) * (profile.bodyFastBase + registerNorm * profile.bodyFastRegister);
        bodySlow += (bodyFast - bodySlow)
            * (profile.bodySlowBase + (1.0f - registerNorm) * profile.bodySlowLowRegister);
        float body = bodyFast - bodySlow * profile.bodySubtract;
        if (profile.algorithm == BowedStringAlgorithm::CelloResonant)
            body = bodyFast * 0.92f - bodySlow * profile.bodySubtract * 0.72f;
        else if (profile.algorithm == BowedStringAlgorithm::ContrabassDeep)
            body = bodyFast * 0.72f + bodySlow * 0.30f;

        const float layer = clampAudio((friction * stringGain * (0.92f + frameTone * 0.18f)
            + body * bodyGain
            + scrape) * expressionDrive * articulationLevelScale(articulationMode));
        signalL += layer * profile.stereoL;
        signalR += layer * profile.stereoR;

        phase = wrapPhase(phase + inc * (1.0f + frame.chorusMod * profile.chorusSensitivity));
        secondPhase = wrapPhase(secondPhase + secondInc);
    }

private:
    const BowedStringProfile& profile;
    NoiseState noise;
    float sampleRate = 44100.0f;
    float baseFrequencyHz = 440.0f;
    float velocity = 0.0f;
    float noteExpression = 1.0f;
    float noteTone = 0.5f;
    float noteMotion = 0.0f;
    float noteArticulation = 0.5f;
    InstrumentArticulation noteArticulationMode = InstrumentArticulation::Sustain;
    float noteLegatoAmount = 0.0f;
    float noteLegatoOnsetScale = 1.0f;
    float registerNorm = 0.5f;
    float phase = 0.0f;
    float secondPhase = 0.0f;
    float bodyFast = 0.0f;
    float bodySlow = 0.0f;
    float scrapeState = 0.0f;
    float bowMemory = 0.0f;
    float bowPressure = 1.0f;
    float bowNoise = 0.0f;
    float bodyGain = 0.0f;
    float stringGain = 0.0f;
    float bodySignatureCue = 1.0f;
    float bridgeSignatureCue = 1.0f;
    float oddEvenSignatureCue = 1.0f;
};

enum class BrassAlgorithm
{
    HornConical = 0,
    TrumpetBright,
    TromboneSlide,
    TubaLowBrass
};

struct BrassProfile
{
    const char* modelName;
    BrassAlgorithm algorithm;
    std::uint32_t noiseSalt;
    float phaseSeedScale;
    float phaseVelocityScale;
    float bellPhaseOffset;
    float bellRatioBase;
    float bellRatioRegister;
    float bellAmpBase;
    float bellAmpRegister;
    float lipDriveBase;
    float lipDriveVelocity;
    float lipMemoryBase;
    float lipMemoryVelocity;
    float lipGainBase;
    float lipGainVelocity;
    float bellGainBase;
    float bellGainVelocity;
    float airGainBase;
    float airGainVelocity;
    float airFilterCoeff;
    float bloomSecondsBase;
    float bloomSecondsVelocity;
    float bloomBase;
    float bloomVelocity;
    float bellFastBase;
    float bellFastRegister;
    float bellSlowCoeff;
    float bellSubtract;
    float stereoL;
    float stereoR;
    float legacyCoreGain;
};

constexpr BrassProfile kHornProfile {
    "v2-french-horn-brass",
    BrassAlgorithm::HornConical,
    0x4f7a61b3u,
    0.103f,
    0.33f,
    0.271f,
    2.72f,
    0.055f,
    0.18f,
    0.08f,
    1.78f,
    2.45f,
    0.024f,
    0.016f,
    0.034f,
    0.030f,
    0.090f,
    0.048f,
    0.004f,
    0.010f,
    0.110f,
    0.065f,
    0.070f,
    0.48f,
    0.38f,
    0.070f,
    0.025f,
    0.016f,
    0.50f,
    0.80f,
    0.78f,
    0.38f
};

constexpr BrassProfile kTrumpetProfile {
    "v2-trumpet-brass",
    BrassAlgorithm::TrumpetBright,
    0x71f3c9adu,
    0.119f,
    0.41f,
    0.310f,
    2.98f,
    0.080f,
    0.23f,
    0.12f,
    2.10f,
    3.40f,
    0.030f,
    0.020f,
    0.038f,
    0.040f,
    0.070f,
    0.060f,
    0.006f,
    0.014f,
    0.160f,
    0.045f,
    0.055f,
    0.65f,
    0.55f,
    0.085f,
    0.035f,
    0.018f,
    0.42f,
    0.76f,
    0.82f,
    0.36f
};

constexpr BrassProfile kTromboneProfile {
    "v2-trombone-brass",
    BrassAlgorithm::TromboneSlide,
    0x93b42d17u,
    0.091f,
    0.29f,
    0.354f,
    2.52f,
    0.050f,
    0.16f,
    0.07f,
    1.62f,
    2.15f,
    0.026f,
    0.014f,
    0.036f,
    0.032f,
    0.105f,
    0.052f,
    0.004f,
    0.011f,
    0.125f,
    0.075f,
    0.085f,
    0.44f,
    0.34f,
    0.065f,
    0.022f,
    0.017f,
    0.56f,
    0.75f,
    0.85f,
    0.40f
};

constexpr BrassProfile kTubaProfile {
    "v2-tuba-brass",
    BrassAlgorithm::TubaLowBrass,
    0x162edc89u,
    0.073f,
    0.22f,
    0.417f,
    2.18f,
    0.032f,
    0.12f,
    0.045f,
    1.35f,
    1.70f,
    0.020f,
    0.010f,
    0.040f,
    0.026f,
    0.135f,
    0.045f,
    0.003f,
    0.008f,
    0.090f,
    0.105f,
    0.105f,
    0.34f,
    0.26f,
    0.052f,
    0.014f,
    0.020f,
    0.64f,
    0.72f,
    0.88f,
    0.34f
};

const BrassProfile& brassProfileFor(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 12: return kHornProfile;
        case 14: return kTromboneProfile;
        case 15: return kTubaProfile;
        case 13:
        default: return kTrumpetProfile;
    }
}

bool isBrassV2Index(const int instrumentIndex) noexcept
{
    return instrumentIndex >= 12 && instrumentIndex <= 15;
}

float brassLipDriveScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 0.90f;
        case BrassAlgorithm::TrumpetBright: return 1.16f;
        case BrassAlgorithm::TromboneSlide: return 0.98f;
        case BrassAlgorithm::TubaLowBrass:  return 0.78f;
        default:                            return 1.0f;
    }
}

float brassLipGainScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 0.92f;
        case BrassAlgorithm::TrumpetBright: return 1.22f;
        case BrassAlgorithm::TromboneSlide: return 1.02f;
        case BrassAlgorithm::TubaLowBrass:  return 0.84f;
        default:                            return 1.0f;
    }
}

float brassBellGainScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 1.24f;
        case BrassAlgorithm::TrumpetBright: return 1.05f;
        case BrassAlgorithm::TromboneSlide: return 1.16f;
        case BrassAlgorithm::TubaLowBrass:  return 1.34f;
        default:                            return 1.0f;
    }
}

float brassAirScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 0.78f;
        case BrassAlgorithm::TrumpetBright: return 1.12f;
        case BrassAlgorithm::TromboneSlide: return 0.86f;
        case BrassAlgorithm::TubaLowBrass:  return 0.64f;
        default:                            return 1.0f;
    }
}

float brassMemoryScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 1.16f;
        case BrassAlgorithm::TrumpetBright: return 0.82f;
        case BrassAlgorithm::TromboneSlide: return 1.04f;
        case BrassAlgorithm::TubaLowBrass:  return 1.28f;
        default:                            return 1.0f;
    }
}

float brassBellRatioScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 0.92f;
        case BrassAlgorithm::TrumpetBright: return 1.08f;
        case BrassAlgorithm::TromboneSlide: return 0.98f;
        case BrassAlgorithm::TubaLowBrass:  return 0.84f;
        default:                            return 1.0f;
    }
}

float brassSustainBodyScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 1.34f;
        case BrassAlgorithm::TrumpetBright: return 1.18f;
        case BrassAlgorithm::TromboneSlide: return 1.05f;
        case BrassAlgorithm::TubaLowBrass:  return 1.42f;
        default:                            return 1.0f;
    }
}

float brassSignatureEdgeScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::TrumpetBright: return 1.20f;
        case BrassAlgorithm::HornConical:   return 0.96f;
        case BrassAlgorithm::TromboneSlide: return 0.92f;
        case BrassAlgorithm::TubaLowBrass:  return 0.62f;
        default:                            return 1.0f;
    }
}

float brassSignatureBodyScale(const BrassAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case BrassAlgorithm::HornConical:   return 1.04f;
        case BrassAlgorithm::TrumpetBright: return 0.94f;
        case BrassAlgorithm::TromboneSlide: return 1.00f;
        case BrassAlgorithm::TubaLowBrass:  return 0.88f;
        default:                            return 1.0f;
    }
}

class BrassModel final : public InstrumentModel
{
public:
    explicit BrassModel(const int instrumentIndexToUse) noexcept
        : profile(brassProfileFor(instrumentIndexToUse))
    {
    }

    const char* name() const noexcept override { return profile.modelName; }
    bool isV2() const noexcept override { return true; }
    float legacyCoreGain() const noexcept override { return profile.legacyCoreGain; }

    void noteOn(const InstrumentModelNoteContext& context) override
    {
        sampleRate = static_cast<float>(std::max(22050.0, context.sampleRate));
        baseFrequencyHz = context.baseFrequencyHz;
        velocity = juce::jlimit(0.0f, 1.0f, context.velocity);
        noteExpression = juce::jlimit(0.0f, 1.5f, context.expression);
        noteTone = juce::jlimit(0.0f, 1.0f, context.tone);
        noteMotion = juce::jlimit(0.0f, 1.0f, context.motion);
        noteArticulation = juce::jlimit(0.0f, 1.0f, context.articulation);
        noteArticulationMode = context.articulationMode;
        noteLegatoAmount = legatoAmountFor(context.legatoTransition, context.legatoAmount);
        noteLegatoOnsetScale = legatoOnsetScaleFor(context.legatoTransition,
                                                   context.legatoAmount,
                                                   context.legatoOnsetScale,
                                                   0.32f);
        const auto range = getInstrMidiNoteRange(context.instrumentIndex);
        const auto span = std::max(1, range.high - range.low);
        registerNorm = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(context.midiNote - range.low) / static_cast<float>(span));

        lipPhase = wrapPhase(profile.phaseSeedScale * static_cast<float>(context.midiNote)
            + velocity * profile.phaseVelocityScale);
        bellPhase = wrapPhase(lipPhase + profile.bellPhaseOffset);
        lipMemory = noteLegatoAmount * 0.08f * velocity;
        bellFast = 0.0f;
        bellSlow = 0.0f;
        bellHold = 0.0f;
        airState = 0.0f;
        bloom = legatoTargetScaleFor(context.legatoTransition, context.legatoAmount, 0.28f);
        noise.reset(seedFor(context, profile.noiseSalt));

        const auto& characteristics = context.characteristics;
        const float averageFormant = averageFormantGain(characteristics);
        lipSignatureCue = juce::jlimit(0.82f, 1.46f,
            0.88f
                + characteristics.oddHarmonicBias * 0.52f
                + characteristics.attackShape * 0.24f
                + static_cast<float>(juce::jlimit(0, 12, characteristics.numPartials)) * 0.012f);
        bellSignatureCue = juce::jlimit(0.76f, 1.66f,
            0.84f
                + formantBrightnessCue(characteristics) * 0.22f
                + averageFormant * 0.18f
                + (characteristics.brightnessCutoffScale - 2.8f) * 0.12f
                + characteristics.attackShape * 0.10f);
        boreSignatureCue = juce::jlimit(0.78f, 1.74f,
            0.82f
                + characteristics.builtInWarmth * 0.54f
                + characteristics.sustainPlatform * 0.42f
                + formantBodyCue(characteristics) * 0.20f
                + (characteristics.decay2Time - 2.6f) * 0.035f);
        airEdgeSignatureCue = juce::jlimit(0.70f, 1.52f,
            0.86f
                + (1.0f - characteristics.builtInWarmth) * 0.22f
                + characteristics.attackShape * 0.28f
                + (characteristics.brightnessCutoffScale - 2.8f) * 0.10f);

        lipDrive = (profile.lipDriveBase + velocity * profile.lipDriveVelocity)
            * (0.88f + noteArticulation * 0.28f)
            * (0.94f + noteExpression * 0.07f)
            * brassLipDriveScale(profile.algorithm)
            * articulationPressureScale(noteArticulationMode)
            * legatoTargetScaleFor(context.legatoTransition, context.legatoAmount, 0.84f)
            * lipSignatureCue
            * brassSignatureEdgeScale(profile.algorithm);
        lipGain = (profile.lipGainBase + velocity * profile.lipGainVelocity)
            * (0.88f + noteTone * 0.25f)
            * brassLipGainScale(profile.algorithm)
            * articulationToneScale(noteArticulationMode)
            * lipSignatureCue
            * brassSignatureEdgeScale(profile.algorithm);
        bellGain = (profile.bellGainBase + velocity * profile.bellGainVelocity)
            * (0.86f + noteTone * 0.35f)
            * brassBellGainScale(profile.algorithm)
            * articulationBodyScale(noteArticulationMode)
            * bellSignatureCue
            * boreSignatureCue
            * brassSignatureBodyScale(profile.algorithm);
        airGain = (profile.airGainBase + velocity * profile.airGainVelocity)
            * (0.78f + noteArticulation * 0.36f)
            * brassAirScale(profile.algorithm)
            * articulationNoiseScale(noteArticulationMode)
            * noteLegatoOnsetScale
            * airEdgeSignatureCue
            * brassSignatureEdgeScale(profile.algorithm);
        bloomCoeff = std::exp(-1.0f
            / (std::max(0.020f, profile.bloomSecondsBase + velocity * profile.bloomSecondsVelocity)
               * sampleRate));
    }

    void renderPreFilter(const InstrumentModelFrame& frame,
                         float& signalL,
                         float& signalR) noexcept override
    {
        const auto sr = std::max(22050.0f, frame.sampleRate > 0.0f ? frame.sampleRate : sampleRate);
        const float freq = juce::jlimit(20.0f, sr * 0.46f, baseFrequencyHz * frame.pitchMult);
        const float inc = freq / sr;
        const float bellInc = inc
            * (profile.bellRatioBase + registerNorm * profile.bellRatioRegister)
            * brassBellRatioScale(profile.algorithm);
        const float frameTone = juce::jlimit(0.0f, 1.0f, frame.tone);
        const float frameMotion = juce::jlimit(0.0f, 1.0f, frame.motion);
        const float frameExpression = juce::jlimit(0.0f, 1.5f, frame.expression);
        const float frameArticulation = juce::jlimit(0.0f, 1.0f, frame.articulation);
        const auto articulationMode = frame.articulationMode;
        const float frameLegatoOnsetScale = legatoOnsetScaleFor(frame.legatoTransition,
                                                                frame.legatoAmount,
                                                                frame.legatoOnsetScale,
                                                                0.32f);

        const float lip = mos::fastSin(lipPhase);
        const float bellSine = mos::fastSin(bellPhase);
        const float bellPartial = bellSine
            * (profile.bellAmpBase + registerNorm * profile.bellAmpRegister)
            * bellSignatureCue
            * brassSignatureEdgeScale(profile.algorithm);
        float excitationInput = lip + bellPartial + lipMemory * (0.18f + frameMotion * 0.08f);
        float excitationDrive = lipDrive * (0.94f + frameArticulation * 0.11f);
        float bellBodyScale = 1.0f;
        float bellSubtractScale = 1.0f;
        if (profile.algorithm == BrassAlgorithm::HornConical)
        {
            excitationInput = lip * 0.76f
                + bellPartial * (0.64f + frameTone * 0.08f)
                + lipMemory * (0.30f + frameMotion * 0.08f);
            excitationDrive *= 0.88f;
            bellBodyScale = 1.22f;
            bellSubtractScale = 0.78f;
        }
        else if (profile.algorithm == BrassAlgorithm::TrumpetBright)
        {
            const float edgeFold = std::tanh((lip + bellSine * 0.42f) * 2.2f) * 0.16f;
            excitationInput = lip * 0.92f
                + bellPartial * (1.28f + frameTone * 0.12f)
                + edgeFold
                + lipMemory * (0.12f + frameMotion * 0.06f);
            excitationDrive *= 1.14f;
            bellBodyScale = 0.92f;
            bellSubtractScale = 1.12f;
        }
        else if (profile.algorithm == BrassAlgorithm::TromboneSlide)
        {
            excitationInput = lip * 0.84f
                + bellPartial * (0.90f + frameTone * 0.06f)
                + lipMemory * (0.22f + frameMotion * 0.10f);
            excitationDrive *= 0.98f;
            bellBodyScale = 1.12f;
            bellSubtractScale = 0.92f;
        }
        else
        {
            excitationInput = lip * 0.68f
                + bellPartial * (0.48f + frameTone * 0.04f)
                + lipMemory * (0.36f + frameMotion * 0.10f);
            excitationDrive *= 0.78f;
            bellBodyScale = 1.36f;
            bellSubtractScale = 0.64f;
        }

        const float buzz = std::tanh(excitationInput * excitationDrive);
        lipMemory += (buzz - lipMemory)
            * (profile.lipMemoryBase + velocity * profile.lipMemoryVelocity)
            * brassMemoryScale(profile.algorithm);

        bellFast += (buzz - bellFast) * (profile.bellFastBase + registerNorm * profile.bellFastRegister);
        bellSlow += (bellFast - bellSlow) * profile.bellSlowCoeff;
        const float bell = (bellFast - bellSlow * profile.bellSubtract * bellSubtractScale) * bellBodyScale;
        bellHold += (bell - bellHold)
            * (0.010f + profile.bellSlowCoeff * 0.45f + registerNorm * 0.006f);
        const float bellSustain = bellHold
            * (0.052f + frameTone * 0.022f)
            * (0.74f + frameExpression * 0.18f)
            * brassSustainBodyScale(profile.algorithm)
            * boreSignatureCue
            * articulationBodyScale(articulationMode);

        const float rawNoise = noise.nextSigned();
        airState += (rawNoise - airState) * profile.airFilterCoeff;
        const float air = airState
            * airGain
            * (0.45f + frame.envOut * 0.55f)
            * (0.76f + frameArticulation * 0.36f)
            * articulationNoiseScale(articulationMode)
            * frameLegatoOnsetScale;
        const float attackBloom = 1.0f
            + bloom
            * (profile.bloomBase + velocity * profile.bloomVelocity)
            * (0.82f + frameTone * 0.30f)
            * (profile.algorithm == BrassAlgorithm::TrumpetBright ? 1.22f
               : profile.algorithm == BrassAlgorithm::TubaLowBrass ? 0.72f
               : profile.algorithm == BrassAlgorithm::HornConical ? 0.92f
               : 1.0f)
            * articulationBloomScale(articulationMode);
        bloom *= bloomCoeff;

        const float expressionDrive = 0.88f + frameExpression * 0.12f;
        const float layer = clampAudio((buzz * lipGain
            + bell * bellGain * attackBloom
            + bellSustain
            + air) * expressionDrive * articulationLevelScale(articulationMode));
        signalL += layer * profile.stereoL;
        signalR += layer * profile.stereoR;

        lipPhase = wrapPhase(lipPhase + inc);
        bellPhase = wrapPhase(bellPhase + bellInc);
    }

private:
    const BrassProfile& profile;
    NoiseState noise;
    float sampleRate = 44100.0f;
    float baseFrequencyHz = 440.0f;
    float velocity = 0.0f;
    float noteExpression = 1.0f;
    float noteTone = 0.5f;
    float noteMotion = 0.0f;
    float noteArticulation = 0.5f;
    InstrumentArticulation noteArticulationMode = InstrumentArticulation::Sustain;
    float noteLegatoAmount = 0.0f;
    float noteLegatoOnsetScale = 1.0f;
    float registerNorm = 0.5f;
    float lipPhase = 0.0f;
    float bellPhase = 0.0f;
    float lipMemory = 0.0f;
    float bellFast = 0.0f;
    float bellSlow = 0.0f;
    float bellHold = 0.0f;
    float airState = 0.0f;
    float bloom = 0.0f;
    float bloomCoeff = 1.0f;
    float lipDrive = 1.0f;
    float lipGain = 0.0f;
    float bellGain = 0.0f;
    float airGain = 0.0f;
    float lipSignatureCue = 1.0f;
    float bellSignatureCue = 1.0f;
    float boreSignatureCue = 1.0f;
    float airEdgeSignatureCue = 1.0f;
};

enum class WoodwindAlgorithm
{
    FluteAir = 0,
    OboeDoubleReed,
    ClarinetSingleReed,
    BassoonDoubleReed,
    PiccoloAir,
    EnglishHornDoubleReed,
    BassClarinetSingleReed
};

struct WoodwindProfile
{
    const char* modelName;
    WoodwindAlgorithm algorithm;
    std::uint32_t noiseSalt;
    float phaseSeedScale;
    float phaseVelocityScale;
    float harmonicPhaseOffset;
    float harmonicRatioBase;
    float harmonicRatioRegister;
    float harmonicAmpBase;
    float harmonicAmpRegister;
    float pressureBase;
    float pressureVelocity;
    float memoryBase;
    float memoryVelocity;
    float feedback;
    float toneGainBase;
    float toneGainVelocity;
    float airGainBase;
    float airGainVelocity;
    float airFilterCoeff;
    float airEnvBase;
    float formantCoeffBase;
    float formantCoeffRegister;
    float formantGainBase;
    float formantGainRegister;
    float formantSubtract;
    float stereoL;
    float stereoR;
    float legacyCoreGain;
};

constexpr WoodwindProfile kFluteProfile {
    "v2-flute-air",
    WoodwindAlgorithm::FluteAir,
    0x2ab34c61u,
    0.181f,
    0.28f,
    0.211f,
    2.01f,
    0.018f,
    0.075f,
    0.035f,
    1.05f,
    0.95f,
    0.018f,
    0.010f,
    0.08f,
    0.020f,
    0.018f,
    0.010f,
    0.020f,
    0.115f,
    0.64f,
    0.036f,
    0.018f,
    0.028f,
    0.020f,
    0.22f,
    0.78f,
    0.76f,
    0.40f
};

constexpr WoodwindProfile kOboeProfile {
    "v2-oboe-double-reed",
    WoodwindAlgorithm::OboeDoubleReed,
    0x7c6a2f91u,
    0.149f,
    0.34f,
    0.157f,
    2.55f,
    0.045f,
    0.160f,
    0.070f,
    1.78f,
    2.35f,
    0.030f,
    0.018f,
    0.22f,
    0.030f,
    0.026f,
    0.004f,
    0.010f,
    0.095f,
    0.48f,
    0.052f,
    0.022f,
    0.055f,
    0.028f,
    0.40f,
    0.76f,
    0.80f,
    0.44f
};

constexpr WoodwindProfile kClarinetProfile {
    "v2-clarinet-single-reed",
    WoodwindAlgorithm::ClarinetSingleReed,
    0x4bb62c0du,
    0.127f,
    0.27f,
    0.239f,
    3.01f,
    0.028f,
    0.135f,
    0.045f,
    1.42f,
    1.78f,
    0.026f,
    0.015f,
    0.18f,
    0.028f,
    0.022f,
    0.003f,
    0.008f,
    0.082f,
    0.50f,
    0.046f,
    0.017f,
    0.044f,
    0.023f,
    0.36f,
    0.80f,
    0.78f,
    0.38f
};

constexpr WoodwindProfile kBassoonProfile {
    "v2-bassoon-double-reed",
    WoodwindAlgorithm::BassoonDoubleReed,
    0x9743b51fu,
    0.103f,
    0.23f,
    0.318f,
    2.34f,
    0.026f,
    0.120f,
    0.050f,
    1.35f,
    1.70f,
    0.024f,
    0.012f,
    0.20f,
    0.034f,
    0.020f,
    0.003f,
    0.007f,
    0.070f,
    0.45f,
    0.042f,
    0.014f,
    0.060f,
    0.020f,
    0.52f,
    0.76f,
    0.84f,
    0.40f
};

constexpr WoodwindProfile kPiccoloProfile {
    "v2-piccolo-air",
    WoodwindAlgorithm::PiccoloAir,
    0x1e83d4a7u,
    0.193f,
    0.37f,
    0.143f,
    2.03f,
    0.024f,
    0.095f,
    0.055f,
    1.28f,
    1.28f,
    0.020f,
    0.012f,
    0.07f,
    0.045f,
    0.040f,
    0.035f,
    0.055f,
    0.135f,
    0.68f,
    0.040f,
    0.020f,
    0.075f,
    0.045f,
    0.18f,
    0.74f,
    0.78f,
    0.18f
};

constexpr WoodwindProfile kEnglishHornProfile {
    "v2-english-horn-double-reed",
    WoodwindAlgorithm::EnglishHornDoubleReed,
    0x63e1f85bu,
    0.117f,
    0.25f,
    0.281f,
    2.42f,
    0.034f,
    0.135f,
    0.055f,
    1.48f,
    1.85f,
    0.027f,
    0.014f,
    0.20f,
    0.032f,
    0.022f,
    0.003f,
    0.008f,
    0.078f,
    0.46f,
    0.046f,
    0.018f,
    0.058f,
    0.022f,
    0.46f,
    0.77f,
    0.82f,
    0.42f
};

constexpr WoodwindProfile kBassClarinetProfile {
    "v2-bass-clarinet-single-reed",
    WoodwindAlgorithm::BassClarinetSingleReed,
    0x0d95aef3u,
    0.097f,
    0.20f,
    0.351f,
    3.00f,
    0.018f,
    0.110f,
    0.035f,
    1.20f,
    1.42f,
    0.023f,
    0.011f,
    0.17f,
    0.034f,
    0.018f,
    0.002f,
    0.006f,
    0.066f,
    0.42f,
    0.038f,
    0.012f,
    0.055f,
    0.018f,
    0.54f,
    0.79f,
    0.84f,
    0.40f
};

const WoodwindProfile& woodwindProfileFor(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 6:  return kOboeProfile;
        case 7:  return kClarinetProfile;
        case 8:  return kBassoonProfile;
        case 9:  return kPiccoloProfile;
        case 10: return kEnglishHornProfile;
        case 11: return kBassClarinetProfile;
        case 5:
        default: return kFluteProfile;
    }
}

bool isWoodwindV2Index(const int instrumentIndex) noexcept
{
    return instrumentIndex >= 5 && instrumentIndex <= 11;
}

bool woodwindIsAirColumn(const WoodwindAlgorithm algorithm) noexcept
{
    return algorithm == WoodwindAlgorithm::FluteAir
        || algorithm == WoodwindAlgorithm::PiccoloAir;
}

bool woodwindIsSingleReed(const WoodwindAlgorithm algorithm) noexcept
{
    return algorithm == WoodwindAlgorithm::ClarinetSingleReed
        || algorithm == WoodwindAlgorithm::BassClarinetSingleReed;
}

float woodwindPressureScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 0.64f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.72f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 1.05f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 0.96f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.24f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.12f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 1.02f;
        default:                                       return 1.0f;
    }
}

float woodwindToneScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 0.72f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.88f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 1.12f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 0.98f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.12f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.02f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 0.92f;
        default:                                       return 1.0f;
    }
}

float woodwindAirScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 1.42f;
        case WoodwindAlgorithm::PiccoloAir:            return 1.72f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 0.62f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 0.52f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 0.74f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 0.68f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 0.58f;
        default:                                       return 1.0f;
    }
}

float woodwindMemoryScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 0.58f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.50f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 1.04f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 1.18f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.14f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.24f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 1.34f;
        default:                                       return 1.0f;
    }
}

float woodwindHarmonicRatioScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::PiccoloAir:            return 1.018f;
        case WoodwindAlgorithm::FluteAir:              return 1.006f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.012f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 0.996f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 0.982f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 1.000f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 0.986f;
        default:                                       return 1.0f;
    }
}

float woodwindFormantGainScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 0.54f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.70f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 0.94f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 1.10f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.24f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.12f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 1.34f;
        default:                                       return 1.0f;
    }
}

float woodwindFormantSubtractScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 0.32f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.26f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 0.82f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 0.98f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.30f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.16f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 1.42f;
        default:                                       return 1.0f;
    }
}

float woodwindSustainBodyScale(const WoodwindAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case WoodwindAlgorithm::FluteAir:              return 1.25f;
        case WoodwindAlgorithm::PiccoloAir:            return 0.78f;
        case WoodwindAlgorithm::ClarinetSingleReed:    return 1.42f;
        case WoodwindAlgorithm::BassClarinetSingleReed:return 1.28f;
        case WoodwindAlgorithm::OboeDoubleReed:        return 1.18f;
        case WoodwindAlgorithm::EnglishHornDoubleReed: return 1.24f;
        case WoodwindAlgorithm::BassoonDoubleReed:     return 1.36f;
        default:                                       return 1.0f;
    }
}

class WoodwindModel final : public InstrumentModel
{
public:
    explicit WoodwindModel(const int instrumentIndexToUse) noexcept
        : profile(woodwindProfileFor(instrumentIndexToUse))
    {
    }

    const char* name() const noexcept override { return profile.modelName; }
    bool isV2() const noexcept override { return true; }
    float legacyCoreGain() const noexcept override { return profile.legacyCoreGain; }

    void noteOn(const InstrumentModelNoteContext& context) override
    {
        sampleRate = static_cast<float>(std::max(22050.0, context.sampleRate));
        baseFrequencyHz = context.baseFrequencyHz;
        velocity = juce::jlimit(0.0f, 1.0f, context.velocity);
        noteExpression = juce::jlimit(0.0f, 1.5f, context.expression);
        noteTone = juce::jlimit(0.0f, 1.0f, context.tone);
        noteMotion = juce::jlimit(0.0f, 1.0f, context.motion);
        noteArticulation = juce::jlimit(0.0f, 1.0f, context.articulation);
        noteArticulationMode = context.articulationMode;
        noteLegatoAmount = legatoAmountFor(context.legatoTransition, context.legatoAmount);
        noteLegatoOnsetScale = legatoOnsetScaleFor(context.legatoTransition,
                                                   context.legatoAmount,
                                                   context.legatoOnsetScale,
                                                   0.34f);
        const auto range = getInstrMidiNoteRange(context.instrumentIndex);
        const auto span = std::max(1, range.high - range.low);
        registerNorm = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(context.midiNote - range.low) / static_cast<float>(span));

        phase = wrapPhase(profile.phaseSeedScale * static_cast<float>(context.midiNote)
            + velocity * profile.phaseVelocityScale);
        harmonicPhase = wrapPhase(phase + profile.harmonicPhaseOffset);
        piccoloEdgePhase = wrapPhase(phase + 0.317f + registerNorm * 0.113f);
        reedMemory = noteLegatoAmount * 0.07f * velocity;
        formantState = 0.0f;
        piccoloEdgeState = 0.0f;
        boreState = 0.0f;
        airState = 0.0f;
        noise.reset(seedFor(context, profile.noiseSalt));

        const auto& characteristics = context.characteristics;
        oddEvenSignatureCue = juce::jlimit(0.72f, 1.58f,
            0.78f
                + characteristics.oddHarmonicBias * 0.78f
                + static_cast<float>(juce::jlimit(0, 12, characteristics.numPartials)) * 0.018f
                + characteristics.attackShape * 0.10f);
        formantSignatureCue = juce::jlimit(0.72f, 1.64f,
            formantBodyCue(characteristics) * formantBrightnessCue(characteristics));
        boreSignatureCue = juce::jlimit(0.76f, 1.62f,
            0.80f
                + characteristics.builtInWarmth * 0.58f
                + characteristics.sustainPlatform * 0.36f
                + formantBodyCue(characteristics) * 0.18f
                - characteristics.breathNoiseAmount * 0.16f);
        edgeToneSignatureCue = juce::jlimit(0.74f, 1.78f,
            0.84f
                + characteristics.breathNoiseAmount * 2.50f
                + (characteristics.brightnessCutoffScale - 3.0f) * 0.14f
                + (1.0f - characteristics.oddHarmonicBias) * 0.10f
                + characteristics.attackShape * 0.12f);

        pressure = (profile.pressureBase + velocity * profile.pressureVelocity)
            * (0.88f + noteArticulation * 0.22f)
            * (0.94f + noteExpression * 0.07f)
            * woodwindPressureScale(profile.algorithm)
            * articulationPressureScale(noteArticulationMode)
            * legatoTargetScaleFor(context.legatoTransition, context.legatoAmount, 0.90f)
            * juce::jlimit(0.84f, 1.36f, 0.92f + characteristics.attackShape * 0.24f
                + characteristics.oddHarmonicBias * 0.16f);
        toneGain = (profile.toneGainBase + velocity * profile.toneGainVelocity)
            * (0.86f + noteTone * 0.28f)
            * woodwindToneScale(profile.algorithm)
            * articulationToneScale(noteArticulationMode)
            * oddEvenSignatureCue;
        airGain = (profile.airGainBase + velocity * profile.airGainVelocity)
            * (0.76f + noteArticulation * 0.40f)
            * (0.94f + noteTone * 0.14f)
            * woodwindAirScale(profile.algorithm)
            * articulationNoiseScale(noteArticulationMode)
            * noteLegatoOnsetScale
            * edgeToneSignatureCue;
    }

    void renderPreFilter(const InstrumentModelFrame& frame,
                         float& signalL,
                         float& signalR) noexcept override
    {
        const auto sr = std::max(22050.0f, frame.sampleRate > 0.0f ? frame.sampleRate : sampleRate);
        const float freq = juce::jlimit(20.0f, sr * 0.46f, baseFrequencyHz * frame.pitchMult);
        const float inc = freq / sr;
        const float harmonicInc = inc
            * (profile.harmonicRatioBase + registerNorm * profile.harmonicRatioRegister)
            * woodwindHarmonicRatioScale(profile.algorithm);
        const float frameTone = juce::jlimit(0.0f, 1.0f, frame.tone);
        const float frameMotion = juce::jlimit(0.0f, 1.0f, frame.motion);
        const float frameExpression = juce::jlimit(0.0f, 1.5f, frame.expression);
        const float frameArticulation = juce::jlimit(0.0f, 1.0f, frame.articulation);
        const auto articulationMode = frame.articulationMode;
        const float frameLegatoOnsetScale = legatoOnsetScaleFor(frame.legatoTransition,
                                                                frame.legatoAmount,
                                                                frame.legatoOnsetScale,
                                                                0.34f);

        const float rawNoise = noise.nextSigned();
        airState += (rawNoise - airState) * profile.airFilterCoeff;
        const float fundamental = mos::fastSin(phase);
        const float harmonicRaw = mos::fastSin(harmonicPhase);
        const bool isPiccolo = profile.algorithm == WoodwindAlgorithm::PiccoloAir;
        const float piccoloEdgeOsc = isPiccolo ? mos::fastSin(piccoloEdgePhase) : 0.0f;
        const float harmonic = harmonicRaw
            * (profile.harmonicAmpBase + registerNorm * profile.harmonicAmpRegister)
            * oddEvenSignatureCue;
        float piccoloEdgeLayer = 0.0f;

        float excitationInput = fundamental + harmonic
            + reedMemory * profile.feedback * (0.92f + frameMotion * 0.16f);
        float excitationDrive = pressure * (0.95f + frameArticulation * 0.10f);
        if (woodwindIsAirColumn(profile.algorithm))
        {
            const float breathTurbulence = airState * (0.10f + frameMotion * 0.06f);
            const float piccoloEdge = isPiccolo
                ? std::tanh((piccoloEdgeOsc + harmonicRaw * 0.42f + airState * 0.18f)
                    * (1.26f + registerNorm * 0.62f))
                : 0.0f;
            if (isPiccolo)
            {
                piccoloEdgeState += (piccoloEdge - piccoloEdgeState) * (0.16f + registerNorm * 0.12f);
                piccoloEdgeLayer = (piccoloEdge - piccoloEdgeState * 0.34f)
                    * (0.070f + frameTone * 0.050f + registerNorm * 0.052f)
                    * (0.84f + frameExpression * 0.16f)
                    * edgeToneSignatureCue
                    * articulationToneScale(articulationMode)
                    * frameLegatoOnsetScale;
            }
            excitationInput = fundamental * (0.42f + frameTone * 0.18f)
                + harmonic * (0.42f + registerNorm * 0.16f)
                + piccoloEdge * (0.24f + frameTone * 0.10f)
                + breathTurbulence
                + reedMemory * profile.feedback * 0.22f;
            excitationDrive *= profile.algorithm == WoodwindAlgorithm::PiccoloAir ? 0.78f : 0.70f;
        }
        else if (woodwindIsSingleReed(profile.algorithm))
        {
            const float lowReed = profile.algorithm == WoodwindAlgorithm::BassClarinetSingleReed
                ? reedMemory * 0.18f - harmonic * 0.16f
                : 0.0f;
            excitationInput = fundamental * 0.56f
                + harmonic * (1.20f + frameTone * 0.14f)
                + lowReed
                + reedMemory * profile.feedback * (1.12f + frameMotion * 0.18f);
            excitationDrive *= profile.algorithm == WoodwindAlgorithm::BassClarinetSingleReed ? 0.98f : 1.06f;
        }
        else
        {
            const float reedFold = std::tanh((fundamental - harmonicRaw) * 1.45f) * 0.18f;
            const float bassoonFold = profile.algorithm == WoodwindAlgorithm::BassoonDoubleReed
                ? std::tanh((fundamental + reedMemory) * 1.10f) * 0.14f
                : 0.0f;
            const float nasalScale = profile.algorithm == WoodwindAlgorithm::OboeDoubleReed ? 1.16f
                                   : profile.algorithm == WoodwindAlgorithm::EnglishHornDoubleReed ? 0.96f
                                   : 0.84f;
            excitationInput = fundamental * (profile.algorithm == WoodwindAlgorithm::BassoonDoubleReed ? 0.58f : 0.50f)
                + harmonic * (1.05f + frameTone * 0.10f) * nasalScale
                + reedFold
                + bassoonFold
                + reedMemory * profile.feedback * (1.22f + frameMotion * 0.16f);
            excitationDrive *= profile.algorithm == WoodwindAlgorithm::OboeDoubleReed ? 1.18f
                            : profile.algorithm == WoodwindAlgorithm::EnglishHornDoubleReed ? 1.08f
                            : 1.00f;
        }

        const float excited = std::tanh(excitationInput * excitationDrive);
        reedMemory += (excited - reedMemory)
            * (profile.memoryBase + velocity * profile.memoryVelocity)
            * woodwindMemoryScale(profile.algorithm);

        const float air = airState
            * airGain
            * (profile.airEnvBase + frame.envOut * (1.0f - profile.airEnvBase))
            * (0.78f + frameArticulation * 0.34f)
            * articulationNoiseScale(articulationMode)
            * frameLegatoOnsetScale;

        formantState += (excited - formantState)
            * (profile.formantCoeffBase + registerNorm * profile.formantCoeffRegister);
        const float formantSubtractScale = woodwindFormantSubtractScale(profile.algorithm);
        const float formantGainScale = woodwindFormantGainScale(profile.algorithm);
        const float formant = formantState - reedMemory * profile.formantSubtract * formantSubtractScale;
        boreState += (formant - boreState)
            * (0.010f + profile.formantCoeffBase * 0.18f + registerNorm * 0.006f);
        const float boreLayer = boreState
            * (0.044f + frameTone * 0.018f + frameMotion * 0.010f)
            * (0.76f + frameExpression * 0.16f)
            * woodwindSustainBodyScale(profile.algorithm)
            * boreSignatureCue
            * articulationBodyScale(articulationMode);

        const float expressionDrive = 0.88f + frameExpression * 0.12f;
        const float layer = clampAudio((excited * toneGain * (0.92f + frameTone * 0.16f)
            + formant * (profile.formantGainBase + registerNorm * profile.formantGainRegister)
                * formantGainScale
                * formantSignatureCue
                * (1.05f - frameTone * 0.10f)
                * articulationBodyScale(articulationMode)
            + piccoloEdgeLayer
            + boreLayer
            + air) * expressionDrive * articulationLevelScale(articulationMode));
        signalL += layer * profile.stereoL;
        signalR += layer * profile.stereoR;

        phase = wrapPhase(phase + inc);
        harmonicPhase = wrapPhase(harmonicPhase + harmonicInc);
        if (isPiccolo)
        {
            const float edgeRatio = juce::jlimit(1.85f, 4.10f, 2.74f + registerNorm * 0.76f + frameTone * 0.18f);
            piccoloEdgePhase = wrapPhase(piccoloEdgePhase + inc * edgeRatio);
        }
    }

private:
    const WoodwindProfile& profile;
    NoiseState noise;
    float sampleRate = 44100.0f;
    float baseFrequencyHz = 440.0f;
    float velocity = 0.0f;
    float noteExpression = 1.0f;
    float noteTone = 0.5f;
    float noteMotion = 0.0f;
    float noteArticulation = 0.5f;
    InstrumentArticulation noteArticulationMode = InstrumentArticulation::Sustain;
    float noteLegatoAmount = 0.0f;
    float noteLegatoOnsetScale = 1.0f;
    float registerNorm = 0.5f;
    float phase = 0.0f;
    float harmonicPhase = 0.0f;
    float piccoloEdgePhase = 0.0f;
    float reedMemory = 0.0f;
    float formantState = 0.0f;
    float piccoloEdgeState = 0.0f;
    float boreState = 0.0f;
    float airState = 0.0f;
    float pressure = 1.0f;
    float toneGain = 0.0f;
    float airGain = 0.0f;
    float oddEvenSignatureCue = 1.0f;
    float formantSignatureCue = 1.0f;
    float boreSignatureCue = 1.0f;
    float edgeToneSignatureCue = 1.0f;
};

enum class PluckedStringAlgorithm
{
    HarpResonant = 0
};

struct PluckedStringProfile
{
    const char* modelName;
    PluckedStringAlgorithm algorithm;
    std::uint32_t noiseSalt;
    float phaseSeedScale;
    float phaseVelocityScale;
    std::array<float, 4> ratios;
    std::array<float, 4> gains;
    std::array<float, 4> decaySeconds;
    float pluckNoiseBase;
    float pluckNoiseVelocity;
    float pluckDecaySeconds;
    float bodyFastCoeff;
    float bodySlowCoeff;
    float bodyGain;
    float toneGain;
    float stereoL;
    float stereoR;
    float legacyCoreGain;
};

constexpr PluckedStringProfile kHarpProfile {
    "v2-harp-plucked",
    PluckedStringAlgorithm::HarpResonant,
    0x6a2de139u,
    0.163f,
    0.29f,
    { 1.000f, 2.010f, 3.070f, 4.160f },
    { 0.62f, 0.28f, 0.16f, 0.10f },
    { 2.40f, 1.35f, 0.82f, 0.52f },
    0.020f,
    0.050f,
    0.038f,
    0.050f,
    0.012f,
    0.115f,
    0.075f,
    0.78f,
    0.84f,
    0.34f
};

float pluckedNoiseScale(const PluckedStringAlgorithm algorithm, const float registerNorm) noexcept
{
    switch (algorithm)
    {
        case PluckedStringAlgorithm::HarpResonant:
            return juce::jmap(registerNorm, 0.88f, 1.54f);
        default:
            return 1.0f;
    }
}

float pluckedToneScale(const PluckedStringAlgorithm algorithm, const float registerNorm) noexcept
{
    switch (algorithm)
    {
        case PluckedStringAlgorithm::HarpResonant:
            return juce::jmap(registerNorm, 0.96f, 1.16f);
        default:
            return 1.0f;
    }
}

float pluckedBodyScale(const PluckedStringAlgorithm algorithm, const float registerNorm) noexcept
{
    switch (algorithm)
    {
        case PluckedStringAlgorithm::HarpResonant:
            return juce::jmap(registerNorm, 1.42f, 0.76f);
        default:
            return 1.0f;
    }
}

float pluckedDecayScale(const PluckedStringAlgorithm algorithm, const float registerNorm) noexcept
{
    switch (algorithm)
    {
        case PluckedStringAlgorithm::HarpResonant:
            return juce::jmap(registerNorm, 1.26f, 0.76f);
        default:
            return 1.0f;
    }
}

float pluckedStiffnessRatio(const PluckedStringAlgorithm algorithm,
                            const std::size_t modeIndex,
                            const float registerNorm) noexcept
{
    switch (algorithm)
    {
        case PluckedStringAlgorithm::HarpResonant:
            return modeIndex == 0 ? 1.0f
                                  : 1.0f + (0.006f + registerNorm * 0.018f)
                                      * static_cast<float>(modeIndex);
        default:
            return 1.0f;
    }
}

class PluckedStringModel final : public InstrumentModel
{
public:
    explicit PluckedStringModel(const PluckedStringProfile& profileToUse) noexcept
        : profile(profileToUse)
    {
    }

    const char* name() const noexcept override { return profile.modelName; }
    bool isV2() const noexcept override { return true; }
    float legacyCoreGain() const noexcept override { return profile.legacyCoreGain; }

    void noteOn(const InstrumentModelNoteContext& context) override
    {
        sampleRate = static_cast<float>(std::max(22050.0, context.sampleRate));
        baseFrequencyHz = context.baseFrequencyHz;
        velocity = juce::jlimit(0.0f, 1.0f, context.velocity);
        noteExpression = juce::jlimit(0.0f, 1.5f, context.expression);
        noteTone = juce::jlimit(0.0f, 1.0f, context.tone);
        noteMotion = juce::jlimit(0.0f, 1.0f, context.motion);
        noteArticulation = juce::jlimit(0.0f, 1.0f, context.articulation);
        noteArticulationMode = context.articulationMode;
        const auto range = getInstrMidiNoteRange(context.instrumentIndex);
        const auto span = std::max(1, range.high - range.low);
        registerNorm = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(context.midiNote - range.low) / static_cast<float>(span));
        noise.reset(seedFor(context, profile.noiseSalt));
        bodyFast = 0.0f;
        bodySlow = 0.0f;
        pluckNoise = (profile.pluckNoiseBase + velocity * profile.pluckNoiseVelocity)
            * (0.72f + noteArticulation * 0.55f)
            * (0.92f + noteTone * 0.16f)
            * pluckedNoiseScale(profile.algorithm, registerNorm)
            * articulationNoiseScale(noteArticulationMode);
        pluckDecay = std::exp(-1.0f / (std::max(0.004f, profile.pluckDecaySeconds) * sampleRate));

        for (std::size_t i = 0; i < phases.size(); ++i)
        {
            phases[i] = wrapPhase(profile.phaseSeedScale * static_cast<float>(context.midiNote)
                + profile.phaseVelocityScale * velocity
                + static_cast<float>(i) * 0.173f);
            amplitudes[i] = profile.gains[i] * (0.65f + velocity * 0.55f)
                * juce::jmap(registerNorm, 1.08f, 0.76f)
                * (0.86f + noteTone * 0.27f)
                * (0.88f + noteArticulation * 0.24f)
                * (0.92f + noteExpression * 0.08f)
                * pluckedToneScale(profile.algorithm, registerNorm)
                * articulationToneScale(noteArticulationMode)
                * articulationLevelScale(noteArticulationMode);
            decayCoeffs[i] = std::exp(-1.0f
                / (std::max(0.050f, profile.decaySeconds[i]
                        * juce::jmap(registerNorm, 1.12f, 0.72f)
                        * pluckedDecayScale(profile.algorithm, registerNorm)
                        * articulationDecayTimeScale(noteArticulationMode))
                   * sampleRate));
        }
    }

    void renderPreFilter(const InstrumentModelFrame& frame,
                         float& signalL,
                         float& signalR) noexcept override
    {
        const auto sr = std::max(22050.0f, frame.sampleRate > 0.0f ? frame.sampleRate : sampleRate);
        const float freq = juce::jlimit(20.0f, sr * 0.46f, baseFrequencyHz * frame.pitchMult);
        const float frameTone = juce::jlimit(0.0f, 1.0f, frame.tone);
        const float frameExpression = juce::jlimit(0.0f, 1.5f, frame.expression);
        const float frameArticulation = juce::jlimit(0.0f, 1.0f, frame.articulation);
        const auto articulationMode = frame.articulationMode;

        float sum = 0.0f;
        for (std::size_t i = 0; i < phases.size(); ++i)
        {
            sum += mos::fastSin(phases[i]) * amplitudes[i];
            phases[i] = wrapPhase(phases[i]
                + (freq * profile.ratios[i] * pluckedStiffnessRatio(profile.algorithm, i, registerNorm)) / sr);
            amplitudes[i] *= decayCoeffs[i];
        }

        const float rawNoise = noise.nextSigned()
            * pluckNoise
            * (0.82f + frameArticulation * 0.30f)
            * articulationNoiseScale(articulationMode);
        pluckNoise *= pluckDecay;
        bodyFast += (sum - bodyFast) * profile.bodyFastCoeff;
        bodySlow += (bodyFast - bodySlow) * profile.bodySlowCoeff;
        float body = bodyFast - bodySlow * 0.42f;
        if (profile.algorithm == PluckedStringAlgorithm::HarpResonant)
            body = bodyFast * (0.92f + (1.0f - registerNorm) * 0.22f) - bodySlow * 0.34f;

        const float expressionDrive = 0.88f + frameExpression * 0.12f;
        const float layer = clampAudio((sum * profile.toneGain * (0.88f + frameTone * 0.24f)
            + body * profile.bodyGain * pluckedBodyScale(profile.algorithm, registerNorm)
                * (1.08f - frameTone * 0.10f) * articulationBodyScale(articulationMode)
            + rawNoise) * expressionDrive * articulationLevelScale(articulationMode));
        signalL += layer * profile.stereoL;
        signalR += layer * profile.stereoR;
    }

private:
    const PluckedStringProfile& profile;
    NoiseState noise;
    std::array<float, 4> phases {};
    std::array<float, 4> amplitudes {};
    std::array<float, 4> decayCoeffs {};
    float sampleRate = 44100.0f;
    float baseFrequencyHz = 440.0f;
    float velocity = 0.0f;
    float noteExpression = 1.0f;
    float noteTone = 0.5f;
    float noteMotion = 0.0f;
    float noteArticulation = 0.5f;
    InstrumentArticulation noteArticulationMode = InstrumentArticulation::Sustain;
    float registerNorm = 0.5f;
    float pluckNoise = 0.0f;
    float pluckDecay = 1.0f;
    float bodyFast = 0.0f;
    float bodySlow = 0.0f;
};

enum class ModalPercussionAlgorithm
{
    TimpaniKettle = 0,
    CelestaTine,
    SnareWire,
    XylophoneBar
};

struct ModalPercussionProfile
{
    const char* modelName;
    ModalPercussionAlgorithm algorithm;
    std::uint32_t noiseSalt;
    std::array<float, 4> ratios;
    std::array<float, 4> gains;
    std::array<float, 4> decaySeconds;
    float toneGain;
    float noiseGainBase;
    float noiseGainVelocity;
    float noiseDecaySeconds;
    float bodyCoeff;
    float bodyGain;
    float stereoL;
    float stereoR;
    float legacyCoreGain;
};

constexpr ModalPercussionProfile kTimpaniProfile {
    "v2-timpani-modal",
    ModalPercussionAlgorithm::TimpaniKettle,
    0x83bd12f7u,
    { 1.000f, 1.505f, 2.020f, 2.730f },
    { 0.72f, 0.30f, 0.16f, 0.09f },
    { 1.60f, 1.15f, 0.78f, 0.48f },
    0.070f,
    0.012f,
    0.026f,
    0.055f,
    0.040f,
    0.090f,
    0.76f,
    0.84f,
    0.44f
};

constexpr ModalPercussionProfile kCelestaProfile {
    "v2-celesta-modal",
    ModalPercussionAlgorithm::CelestaTine,
    0x32a5d79cu,
    { 1.000f, 2.770f, 5.120f, 8.240f },
    { 0.50f, 0.34f, 0.18f, 0.11f },
    { 1.62f, 1.34f, 0.96f, 0.70f },
    0.070f,
    0.010f,
    0.020f,
    0.030f,
    0.055f,
    0.052f,
    0.82f,
    0.78f,
    0.30f
};

constexpr ModalPercussionProfile kSnareProfile {
    "v2-snare-modal",
    ModalPercussionAlgorithm::SnareWire,
    0x5e7c1a24u,
    { 1.000f, 1.640f, 2.850f, 4.900f },
    { 0.40f, 0.28f, 0.23f, 0.18f },
    { 0.44f, 0.32f, 0.23f, 0.15f },
    0.052f,
    0.075f,
    0.130f,
    0.065f,
    0.085f,
    0.045f,
    0.80f,
    0.80f,
    0.30f
};

constexpr ModalPercussionProfile kXylophoneProfile {
    "v2-xylophone-modal",
    ModalPercussionAlgorithm::XylophoneBar,
    0x9b412ccdu,
    { 1.000f, 3.940f, 9.170f, 16.20f },
    { 0.70f, 0.44f, 0.28f, 0.16f },
    { 0.92f, 0.70f, 0.48f, 0.32f },
    0.105f,
    0.018f,
    0.036f,
    0.020f,
    0.060f,
    0.070f,
    0.78f,
    0.82f,
    0.18f
};

const ModalPercussionProfile& modalPercussionProfileFor(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 17: return kCelestaProfile;
        case 18: return kSnareProfile;
        case 19: return kXylophoneProfile;
        case 16:
        default: return kTimpaniProfile;
    }
}

bool isModalPercussionV2Index(const int instrumentIndex) noexcept
{
    return instrumentIndex >= 16 && instrumentIndex <= 19;
}

float modalToneScale(const ModalPercussionAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case ModalPercussionAlgorithm::TimpaniKettle: return 0.92f;
        case ModalPercussionAlgorithm::CelestaTine:   return 1.06f;
        case ModalPercussionAlgorithm::SnareWire:     return 0.68f;
        case ModalPercussionAlgorithm::XylophoneBar:  return 1.18f;
        default:                                      return 1.0f;
    }
}

float modalNoiseScale(const ModalPercussionAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case ModalPercussionAlgorithm::TimpaniKettle: return 0.58f;
        case ModalPercussionAlgorithm::CelestaTine:   return 0.42f;
        case ModalPercussionAlgorithm::SnareWire:     return 2.35f;
        case ModalPercussionAlgorithm::XylophoneBar:  return 0.74f;
        default:                                      return 1.0f;
    }
}

float modalBodyScale(const ModalPercussionAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case ModalPercussionAlgorithm::TimpaniKettle: return 1.46f;
        case ModalPercussionAlgorithm::CelestaTine:   return 0.72f;
        case ModalPercussionAlgorithm::SnareWire:     return 0.42f;
        case ModalPercussionAlgorithm::XylophoneBar:  return 0.78f;
        default:                                      return 1.0f;
    }
}

float modalDecayScale(const ModalPercussionAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case ModalPercussionAlgorithm::TimpaniKettle: return 1.22f;
        case ModalPercussionAlgorithm::CelestaTine:   return 1.08f;
        case ModalPercussionAlgorithm::SnareWire:     return 0.58f;
        case ModalPercussionAlgorithm::XylophoneBar:  return 0.88f;
        default:                                      return 1.0f;
    }
}

float modalStiffnessRatio(const ModalPercussionAlgorithm algorithm, const std::size_t modeIndex) noexcept
{
    switch (algorithm)
    {
        case ModalPercussionAlgorithm::CelestaTine:
            return modeIndex == 0 ? 1.0f : 1.0f + 0.018f * static_cast<float>(modeIndex);
        case ModalPercussionAlgorithm::SnareWire:
            return modeIndex == 0 ? 1.0f : 1.0f + 0.060f * static_cast<float>(modeIndex);
        case ModalPercussionAlgorithm::XylophoneBar:
            return modeIndex == 0 ? 1.0f : 1.0f + 0.030f * static_cast<float>(modeIndex);
        case ModalPercussionAlgorithm::TimpaniKettle:
        default:
            return 1.0f;
    }
}

class ModalPercussionModel final : public InstrumentModel
{
public:
    explicit ModalPercussionModel(const int instrumentIndexToUse) noexcept
        : profile(modalPercussionProfileFor(instrumentIndexToUse))
    {
    }

    const char* name() const noexcept override { return profile.modelName; }
    bool isV2() const noexcept override { return true; }
    float legacyCoreGain() const noexcept override { return profile.legacyCoreGain; }

    void noteOn(const InstrumentModelNoteContext& context) override
    {
        sampleRate = static_cast<float>(std::max(22050.0, context.sampleRate));
        baseFrequencyHz = context.baseFrequencyHz;
        velocity = juce::jlimit(0.0f, 1.0f, context.velocity);
        noteExpression = juce::jlimit(0.0f, 1.5f, context.expression);
        noteTone = juce::jlimit(0.0f, 1.0f, context.tone);
        noteMotion = juce::jlimit(0.0f, 1.0f, context.motion);
        noteArticulation = juce::jlimit(0.0f, 1.0f, context.articulation);
        noteArticulationMode = context.articulationMode;
        noise.reset(seedFor(context, profile.noiseSalt));
        body = 0.0f;
        noiseLevel = (profile.noiseGainBase + velocity * profile.noiseGainVelocity)
            * (0.74f + noteArticulation * 0.50f)
            * (0.92f + noteTone * 0.16f)
            * modalNoiseScale(profile.algorithm)
            * articulationNoiseScale(noteArticulationMode);
        noiseDecay = std::exp(-1.0f / (std::max(0.004f, profile.noiseDecaySeconds
            * (profile.algorithm == ModalPercussionAlgorithm::SnareWire ? 1.55f : 1.0f)) * sampleRate));

        for (std::size_t i = 0; i < phases.size(); ++i)
        {
            phases[i] = wrapPhase(static_cast<float>(context.midiNote) * 0.071f
                + velocity * 0.19f
                + static_cast<float>(i) * 0.217f);
            amplitudes[i] = profile.gains[i]
                * (0.65f + velocity * 0.70f)
                * (0.86f + noteTone * 0.28f)
                * (0.88f + noteArticulation * 0.24f)
                * (0.92f + noteExpression * 0.08f)
                * modalToneScale(profile.algorithm)
                * articulationToneScale(noteArticulationMode)
                * articulationLevelScale(noteArticulationMode);
            decayCoeffs[i] = std::exp(-1.0f
                / (std::max(0.030f, profile.decaySeconds[i]
                        * modalDecayScale(profile.algorithm)
                        * articulationDecayTimeScale(noteArticulationMode))
                   * sampleRate));
        }
    }

    void renderPreFilter(const InstrumentModelFrame& frame,
                         float& signalL,
                         float& signalR) noexcept override
    {
        const auto sr = std::max(22050.0f, frame.sampleRate > 0.0f ? frame.sampleRate : sampleRate);
        const float freq = juce::jlimit(20.0f, sr * 0.46f, baseFrequencyHz * frame.pitchMult);
        const float frameTone = juce::jlimit(0.0f, 1.0f, frame.tone);
        const float frameExpression = juce::jlimit(0.0f, 1.5f, frame.expression);
        const float frameArticulation = juce::jlimit(0.0f, 1.0f, frame.articulation);
        const auto articulationMode = frame.articulationMode;

        float sum = 0.0f;
        for (std::size_t i = 0; i < phases.size(); ++i)
        {
            sum += mos::fastSin(phases[i]) * amplitudes[i];
            phases[i] = wrapPhase(phases[i] + (freq * profile.ratios[i] * modalStiffnessRatio(profile.algorithm, i)) / sr);
            amplitudes[i] *= decayCoeffs[i];
        }

        const float rawNoise = noise.nextSigned();
        const float snareWire = profile.algorithm == ModalPercussionAlgorithm::SnareWire
            ? (std::tanh((rawNoise + body * 2.8f) * 2.2f) * 0.42f + rawNoise * 0.58f)
            : rawNoise;
        const float transient = snareWire
            * noiseLevel
            * (0.80f + frameArticulation * 0.32f)
            * articulationNoiseScale(articulationMode);
        noiseLevel *= noiseDecay;
        const float bodyTarget = profile.algorithm == ModalPercussionAlgorithm::TimpaniKettle
            ? (sum + body * 0.18f)
            : profile.algorithm == ModalPercussionAlgorithm::SnareWire
                ? (sum * 0.32f + transient * 0.18f)
                : sum;
        body += (bodyTarget - body) * profile.bodyCoeff;
        const float expressionDrive = 0.88f + frameExpression * 0.12f;
        const float layer = clampAudio((sum * profile.toneGain * (0.88f + frameTone * 0.24f)
            + body * profile.bodyGain * modalBodyScale(profile.algorithm)
                * (1.08f - frameTone * 0.10f) * articulationBodyScale(articulationMode)
            + transient) * expressionDrive * articulationLevelScale(articulationMode));
        signalL += layer * profile.stereoL;
        signalR += layer * profile.stereoR;
    }

private:
    const ModalPercussionProfile& profile;
    NoiseState noise;
    std::array<float, 4> phases {};
    std::array<float, 4> amplitudes {};
    std::array<float, 4> decayCoeffs {};
    float sampleRate = 44100.0f;
    float baseFrequencyHz = 440.0f;
    float velocity = 0.0f;
    float noteExpression = 1.0f;
    float noteTone = 0.5f;
    float noteMotion = 0.0f;
    float noteArticulation = 0.5f;
    InstrumentArticulation noteArticulationMode = InstrumentArticulation::Sustain;
    float noiseLevel = 0.0f;
    float noiseDecay = 1.0f;
    float body = 0.0f;
};
} // namespace

void InstrumentModel::noteOn(const InstrumentModelNoteContext& context)
{
    juce::ignoreUnused(context);
}

void InstrumentModel::renderPreFilter(const InstrumentModelFrame& frame,
                                      float& signalL,
                                      float& signalR) noexcept
{
    juce::ignoreUnused(frame, signalL, signalR);
}

void InstrumentModel::renderPostColor(const InstrumentModelFrame& frame,
                                      float& signalL,
                                      float& signalR) noexcept
{
    juce::ignoreUnused(frame, signalL, signalR);
}

const char* instrumentArticulationName(const InstrumentArticulation articulation) noexcept
{
    switch (articulation)
    {
        case InstrumentArticulation::Staccato: return "staccato";
        case InstrumentArticulation::Marcato:  return "marcato";
        case InstrumentArticulation::Soft:     return "soft";
        case InstrumentArticulation::Sustain:
        default:                              return "sustain";
    }
}

InstrumentArticulation inferInstrumentArticulation(const InstrSettings& settings,
                                                   const InstrCharacteristics& characteristics,
                                                   const float velocity) noexcept
{
    const float fastAttack = 1.0f - juce::jlimit(0.0f, 1.0f, settings.attackSeconds / 0.42f);
    const bool veryShort = settings.attackSeconds <= 0.015f
        && settings.releaseSeconds <= 0.14f
        && settings.sustainLevel <= 0.42f;
    if (veryShort)
        return InstrumentArticulation::Staccato;

    const bool accented = settings.attackSeconds <= 0.024f
        && settings.character >= 0.58f
        && (settings.brightness >= 0.54f || velocity >= 0.78f)
        && fastAttack >= 0.80f;
    if (accented)
        return InstrumentArticulation::Marcato;

    const bool transientInstrument = characteristics.pluckAmount > 0.01f
        || characteristics.bowNoiseAmount > 0.01f
        || characteristics.breathNoiseAmount > 0.01f;
    const bool softGesture = settings.attackSeconds >= (transientInstrument ? 0.16f : 0.20f)
        || velocity <= 0.48f
        || (settings.character <= 0.34f && settings.brightness <= 0.46f);
    if (softGesture)
        return InstrumentArticulation::Soft;

    return InstrumentArticulation::Sustain;
}

std::unique_ptr<InstrumentModel> createInstrumentModel(const int instrumentIndex)
{
    switch (instrumentIndex)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            return std::make_unique<BowedStringModel>(instrumentIndex);
        case 4:
            return std::make_unique<PluckedStringModel>(kHarpProfile);
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            return std::make_unique<WoodwindModel>(instrumentIndex);
        case 12:
        case 13:
        case 14:
        case 15:
            return std::make_unique<BrassModel>(instrumentIndex);
        case 16:
        case 17:
        case 18:
        case 19:
            return std::make_unique<ModalPercussionModel>(instrumentIndex);
        default: return std::make_unique<LegacyInstrumentModel>();
    }
}

std::unique_ptr<InstrumentModel> createLegacyInstrumentModel()
{
    return std::make_unique<LegacyInstrumentModel>();
}

const char* instrumentModelName(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            return bowedStringProfileFor(instrumentIndex).modelName;
        case 4:
            return kHarpProfile.modelName;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            return woodwindProfileFor(instrumentIndex).modelName;
        case 12:
        case 13:
        case 14:
        case 15:
            return brassProfileFor(instrumentIndex).modelName;
        case 16:
        case 17:
        case 18:
        case 19:
            return modalPercussionProfileFor(instrumentIndex).modelName;
        default: return "legacy-family";
    }
}

const char* instrumentModelAlgorithm(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 0:
            return "violin-bright-bowed";
        case 1:
            return "viola-warm-bowed";
        case 2:
            return "cello-resonant-bowed";
        case 3:
            return "contrabass-deep-bowed";
        case 4:
            return "harp-resonant-plucked";
        case 5:
            return "flute-air-column";
        case 6:
            return "oboe-double-reed";
        case 7:
            return "clarinet-single-reed";
        case 8:
            return "bassoon-double-reed";
        case 9:
            return "piccolo-air-column";
        case 10:
            return "english-horn-double-reed";
        case 11:
            return "bass-clarinet-single-reed";
        case 12:
            return "horn-conical-brass";
        case 13:
            return "trumpet-bright-brass";
        case 14:
            return "trombone-slide-brass";
        case 15:
            return "tuba-low-brass";
        case 16:
            return "timpani-kettle-modal";
        case 17:
            return "celesta-tine-modal";
        case 18:
            return "snare-wire-modal";
        case 19:
            return "xylophone-bar-modal";
        default: return "legacy-family";
    }
}

float instrumentLegacyCoreGain(const int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            return bowedStringProfileFor(instrumentIndex).legacyCoreGain;
        case 4:
            return kHarpProfile.legacyCoreGain;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            return woodwindProfileFor(instrumentIndex).legacyCoreGain;
        case 12:
        case 13:
        case 14:
        case 15:
            return brassProfileFor(instrumentIndex).legacyCoreGain;
        case 16:
        case 17:
        case 18:
        case 19:
            return modalPercussionProfileFor(instrumentIndex).legacyCoreGain;
        default: return 1.0f;
    }
}

bool instrumentUsesV2Model(const int instrumentIndex) noexcept
{
    return isBowedStringV2Index(instrumentIndex)
        || instrumentIndex == 4
        || isWoodwindV2Index(instrumentIndex)
        || isBrassV2Index(instrumentIndex)
        || isModalPercussionV2Index(instrumentIndex);
}

} // namespace mos::v2
