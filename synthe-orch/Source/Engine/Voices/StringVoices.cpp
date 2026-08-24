#include "../OrchVoice.h"
#include "../OrchConstants.h"
#include "../SinTable.h"
#include "VoiceFamilyProfiles.h"

#include <algorithm>
#include <cmath>

namespace mos
{
namespace k = mos::constants;
namespace
{
float bodyDampingCoeff(const InstrCharacteristics& chars) noexcept
{
    return juce::jlimit(0.25f, 0.80f, 0.72f - chars.bodyDamping * 0.42f);
}
}

void BowedStringVoiceBase::renderOscillators(float& signalL, float& signalR,
                                             const SampleContext& ctx)
{
    for (int o = 0; o < numOscs; ++o)
    {
        auto& osc = oscs[static_cast<std::size_t>(o)];
        const float dt = osc.phaseInc * ctx.pitchMult * (1.0f + ctx.chorusMod);

        float sample = 0.0f;
        switch (chars.oscMode)
        {
        case OscMode::Saw:
            sample = 2.0f * osc.phase - 1.0f;
            sample -= polyBlep(osc.phase, dt);
            break;
        case OscMode::Sine:
            sample = mos::fastSin(osc.phase);
            break;
        case OscMode::Square:
        {
            sample = (osc.phase < 0.5f) ? 1.0f : -1.0f;
            sample += polyBlep(osc.phase, dt);
            float shifted = osc.phase + 0.5f;
            if (shifted >= 1.0f) shifted -= 1.0f;
            sample -= polyBlep(shifted, dt);
            break;
        }
        default:
            break;
        }

        signalL += sample * osc.panL;
        signalR += sample * osc.panR;

        osc.phase += dt;
        if (osc.phase >= 1.0f) osc.phase -= 1.0f;
    }

    const float norm = 1.0f / std::max(1.0f, static_cast<float>(numOscs));
    signalL *= norm;
    signalR *= norm;
}

void BowedStringVoiceBase::renderTransients(float& signalL, float& signalR,
                                            const SampleContext& ctx)
{
    if (bowNoiseLevel > 0.0001f)
    {
        const float noise = (rng.nextFloat() * 2.0f - 1.0f) * bowNoiseLevel;
        const float hpNoise = noise - bowNoiseState;
        bowNoiseState += (noise - bowNoiseState) * 0.15f;
        signalL += hpNoise;
        signalR += hpNoise;
        bowNoiseLevel *= bowNoiseDecay;
    }

    if (chars.bowNoiseAmount > 0.0001f)
    {
        const float scrape = (rng.nextFloat() * 2.0f - 1.0f)
                   * chars.bowNoiseAmount * ctx.envOut * k::kBowScrapeScale
                   * dynamicNoiseScale;
        signalL += scrape;
        signalR += scrape;
    }

    if (bodyFeedback > 0.001f)
    {
        const float mono = (signalL + signalR) * 0.5f;
        const float delayed = readComb(bodyBuf, kBodyBufSize, bodyWritePos, bodyDelaySamples);
        const float damped = bodyDampState + (delayed - bodyDampState) * bodyDampingCoeff(chars);
        bodyDampState = damped;
        if (!(bodyDampState > 1e-15f || bodyDampState < -1e-15f)) bodyDampState = 0.0f;
        bodyBuf[bodyWritePos] = mono + damped * bodyFeedback;
        bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;

        const float dcIn = damped * 0.5f;
        const float dcOut = dcIn - dcX1 + 0.9995f * dcY1;
        dcX1 = dcIn;
        dcY1 = dcOut;
        signalL += dcOut;
        signalR += dcOut;
    }

    if (body2Gain > 0.00005f)
    {
        const float in = (signalL + signalR) * 0.5f;
        const float out2 = body2CoeffA * body2State1 + body2CoeffB * body2State2
                         + body2Gain * in;
        body2State2 = body2State1 * 0.9998f;
        body2State1 = out2 * 0.9998f;
        signalL += out2;
        signalR += out2;
    }
    if (body3Gain > 0.00005f)
    {
        const float in = (signalL + signalR) * 0.5f;
        const float out3 = body3CoeffA * body3State1 + body3CoeffB * body3State2
                         + body3Gain * in;
        body3State2 = body3State1 * 0.9998f;
        body3State1 = out3 * 0.9998f;
        signalL += out3;
        signalR += out3;
    }
}

void BowedStringVoiceBase::renderColor(float& signalL, float& signalR,
                                       const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());
    const float bloom = std::max(0.0f, 1.0f - ctx.envOut) * profile.colorDamping;
    const float gain = (1.0f - bloom) * profile.outputGain;
    signalL *= gain;
    signalR *= gain;
}

void PluckedStringVoiceBase::renderOscillators(float& signalL, float& signalR,
                                               const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());
    brightCutoffCurrent += (brightCutoffTarget - brightCutoffCurrent)
                         * (1.0f - brightDecayCoeff);

    float sum = 0.0f;
    const float partialDenom = std::max(1.0f, static_cast<float>(numActivePartials - 1));
    for (int n = 0; n < numActivePartials; ++n)
    {
        auto& p = partials[static_cast<std::size_t>(n)];
        const float fn = p.phaseInc * static_cast<float>(sr);

        float rolloff = 1.0f;
        if (fn > brightCutoffCurrent)
            rolloff = brightCutoffCurrent / std::max(1.0f, fn);

        const float harmonicPosition = static_cast<float>(n) / partialDenom;
        const float damping = 1.0f - (1.0f - profile.highPartialDamping) * harmonicPosition;
        sum += mos::fastSin(p.phase) * p.amplitude * rolloff * damping;

        p.phase += p.phaseInc * ctx.pitchMult;
        if (p.phase >= 1.0f) p.phase -= 1.0f;
        p.amplitude *= p.decayCoeff;
    }

    signalL += sum * profile.outputGain;
    signalR += sum * profile.outputGain;

    if (bodyFeedback > 0.001f)
    {
        const float mono = (signalL + signalR) * 0.5f;
        const float delayed = readComb(bodyBuf, kBodyBufSize, bodyWritePos, bodyDelaySamples);
        const float damped = bodyDampState + (delayed - bodyDampState) * bodyDampingCoeff(chars);
        bodyDampState = damped;
        if (!(bodyDampState > 1e-15f || bodyDampState < -1e-15f)) bodyDampState = 0.0f;
        bodyBuf[bodyWritePos] = mono + damped * bodyFeedback;
        bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;

        const float dcIn = damped * 0.5f;
        const float dcOut = dcIn - dcX1 + 0.9995f * dcY1;
        dcX1 = dcIn;
        dcY1 = dcOut;
        signalL += dcOut;
        signalR += dcOut;
    }

    if (body2Gain > 0.00005f)
    {
        const float in = (signalL + signalR) * 0.5f;
        const float out2 = body2CoeffA * body2State1 + body2CoeffB * body2State2
                         + body2Gain * in;
        body2State2 = body2State1 * 0.9998f;
        body2State1 = out2 * 0.9998f;
        signalL += out2;
        signalR += out2;
    }
    if (body3Gain > 0.00005f)
    {
        const float in = (signalL + signalR) * 0.5f;
        const float out3 = body3CoeffA * body3State1 + body3CoeffB * body3State2
                         + body3Gain * in;
        body3State2 = body3State1 * 0.9998f;
        body3State1 = out3 * 0.9998f;
        signalL += out3;
        signalR += out3;
    }
}

void PluckedStringVoiceBase::renderTransients(float& signalL, float& signalR,
                                              const SampleContext& ctx)
{
    juce::ignoreUnused(ctx);

    if (pluckLevel > 0.001f)
    {
        const float noise = (rng.nextFloat() * 2.0f - 1.0f) * pluckLevel;
        signalL += noise;
        signalR += noise;
        pluckLevel *= pluckDecayCoeff;
    }
}

#define MOS_DEFINE_VOICE(className, instrumentIndex) \
const InstrCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mos::getCharacteristics(instrumentIndex); \
} \
void className::customizeNoteOn(const InstrSettings& voiceSettings, int noteNumber, float noteVelocity)

MOS_DEFINE_VOICE(ViolonVoice, 0)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.16f);
    scaleChorus(1.15f, 1.10f);
    scaleBowNoise(0.90f, 0.85f);
    setPanOffset(-0.04f);
    tiltActivePartials(0.08f);

    const float noteNorm = voice::normalizedInstrumentRegister(getInstrumentIndex(), noteNumber);
    scaleFilterCutoff(juce::jmap(noteNorm, 0.84f, 0.95f));
    scaleBodyResonance(juce::jmap(noteNorm, 1.26f, 0.86f), juce::jmap(noteNorm, 1.18f, 0.82f));
    scaleBowNoise(juce::jmap(noteNorm, 1.10f, 0.72f), juce::jmap(noteNorm, 1.05f, 0.82f));
    scaleMaxAge(juce::jmap(noteNorm, 1.08f, 0.95f));
    tiltActivePartials(juce::jmap(noteNorm, -0.05f, 0.05f));
}

MOS_DEFINE_VOICE(AltoVoice, 1)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.24f);
    scaleFilterCutoff(0.88f);
    scaleBodyResonance(1.10f, 1.10f);
    setPanOffset(-0.01f);
    tiltActivePartials(-0.08f);

    const float noteNorm = voice::normalizedInstrumentRegister(getInstrumentIndex(), noteNumber);
    scaleFilterCutoff(juce::jmap(noteNorm, 0.82f, 0.92f));
    scaleBodyResonance(juce::jmap(noteNorm, 1.22f, 0.92f), juce::jmap(noteNorm, 1.16f, 0.88f));
    scaleBowNoise(juce::jmap(noteNorm, 1.08f, 0.76f), juce::jmap(noteNorm, 1.04f, 0.84f));
    scaleMaxAge(juce::jmap(noteNorm, 1.10f, 0.96f));
    tiltActivePartials(juce::jmap(noteNorm, -0.08f, 0.03f));
}

MOS_DEFINE_VOICE(VioloncelleVoice, 2)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.28f);
    scaleBodyResonance(1.22f, 1.18f);
    scaleFilterCutoff(0.86f);
    scaleMaxAge(1.10f);
    setPanOffset(0.04f);

    const float noteNorm = voice::normalizedInstrumentRegister(getInstrumentIndex(), noteNumber);
    scaleFilterCutoff(juce::jmap(noteNorm, 0.78f, 0.90f));
    scaleBodyResonance(juce::jmap(noteNorm, 1.28f, 0.94f), juce::jmap(noteNorm, 1.20f, 0.90f));
    scaleBowNoise(juce::jmap(noteNorm, 1.12f, 0.78f), juce::jmap(noteNorm, 1.08f, 0.86f));
    scaleMaxAge(juce::jmap(noteNorm, 1.14f, 0.98f));
    tiltActivePartials(juce::jmap(noteNorm, -0.12f, 0.00f));
}

MOS_DEFINE_VOICE(ContrebasseVoice, 3)
{
    juce::ignoreUnused(voiceSettings, noteVelocity);
    setVibratoDelaySeconds(0.42f);
    scaleChorus(0.35f, 0.90f);
    scaleFilterCutoff(0.72f);
    scaleBodyResonance(1.18f, 1.20f);
    scaleMaxAge(1.18f);
    setPanOffset(0.08f);
    if (noteNumber < 48)
        tiltActivePartials(-0.20f);

    const float noteNorm = voice::normalizedInstrumentRegister(getInstrumentIndex(), noteNumber);
    scaleFilterCutoff(juce::jmap(noteNorm, 0.70f, 0.82f));
    scaleBodyResonance(juce::jmap(noteNorm, 1.30f, 0.98f), juce::jmap(noteNorm, 1.24f, 0.94f));
    scaleBowNoise(juce::jmap(noteNorm, 1.06f, 0.82f), juce::jmap(noteNorm, 1.10f, 0.90f));
    scaleMaxAge(juce::jmap(noteNorm, 1.16f, 1.00f));
}

MOS_DEFINE_VOICE(HarpeVoice, 4)
{
    juce::ignoreUnused(voiceSettings, noteVelocity);
    scalePluck(1.24f, 0.82f);
    scaleBodyResonance(1.12f, 1.25f);
    scaleChorus(0.0f);
    setPanOffset(juce::jlimit(-0.12f, 0.12f, (static_cast<float>(noteNumber) - 60.0f) / 120.0f));
    tiltActivePartials(0.18f);
}

#undef MOS_DEFINE_VOICE

} // namespace mos
