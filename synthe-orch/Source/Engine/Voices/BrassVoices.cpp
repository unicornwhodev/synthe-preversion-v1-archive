#include "../OrchVoice.h"
#include "../OrchConstants.h"
#include "../SinTable.h"
#include "VoiceFamilyProfiles.h"

#include <algorithm>
#include <cmath>

namespace mos
{
namespace k = mos::constants;

void BrassVoiceBase::renderOscillators(float& signalL, float& signalR,
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

    signalL += sum;
    signalR += sum;
}

void BrassVoiceBase::renderTransients(float& signalL, float& signalR,
                                      const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());

    if (brassTransientLevel > 1.0e-6f)
    {
        const float noise = rng.nextFloat() * 2.0f - 1.0f;
        brassTransientHpState += 0.35f * (noise - brassTransientHpState);
        const float brightNoise = noise - brassTransientHpState;
        const float attack = brightNoise * brassTransientLevel * (0.65f + ctx.envOut * 0.35f);
        signalL += attack;
        signalR += attack;
        brassTransientLevel *= brassTransientDecay;
    }

    const float airAmount = chars.breathNoiseAmount > 0.0001f
        ? chars.breathNoiseAmount
        : (k::kBrassSustainAirBase + vel * k::kBrassSustainAirVel)
            * (0.70f + chars.oddHarmonicBias * 0.35f);
    if (airAmount > 0.0001f)
    {
        const float airNoise = (rng.nextFloat() * 2.0f - 1.0f)
                             * airAmount * ctx.envOut * k::kAirColumnNoiseScale
                             * dynamicNoiseScale * profile.sustainedNoiseScale;
        signalL += airNoise;
        signalR += airNoise;
    }

    if (breathReleaseTailLevel > 0.00001f)
    {
        const float tailNoise = (rng.nextFloat() * 2.0f - 1.0f) * breathReleaseTailLevel;
        breathNoiseFilt += (tailNoise - breathNoiseFilt) * 0.12f;
        const float tail = breathNoiseFilt * (0.58f + chars.oddHarmonicBias * 0.24f)
                         * profile.sustainedNoiseScale;
        signalL += tail;
        signalR += tail;
        breathReleaseTailLevel *= breathReleaseTailDecay;
    }

    if (form1Gain > 0.00001f || form2Gain > 0.00001f || form3Gain > 0.00001f)
    {
        const float in = (signalL + signalR) * 0.5f;
        float formantSum = 0.0f;

        if (form1Gain > 0.00001f)
        {
            const float out1 = form1CoeffA * form1State1
                             + form1CoeffB * form1State2 + form1Gain * in;
            form1State2 = form1State1;
            form1State1 = out1;
            formantSum += out1;
        }
        if (form2Gain > 0.00001f)
        {
            const float out2 = form2CoeffA * form2State1
                             + form2CoeffB * form2State2 + form2Gain * in;
            form2State2 = form2State1;
            form2State1 = out2;
            formantSum += out2;
        }
        if (form3Gain > 0.00001f)
        {
            const float out3 = form3CoeffA * form3State1
                             + form3CoeffB * form3State2 + form3Gain * in;
            form3State2 = form3State1;
            form3State1 = out3;
            formantSum += out3;
        }

        signalL += formantSum * profile.formantSendScale;
        signalR += formantSum * profile.formantSendScale;
    }
}

void BrassVoiceBase::renderColor(float& signalL, float& signalR,
                                 const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());
    if (chars.hasFormants)
    {
        const float attackBoost = std::max(0.0f, ctx.envOut - k::kBrassBloomThreshold)
            * k::kBrassBloomScale * brassBloomScale * (1.0f - profile.colorDamping);
        signalL *= (1.0f + attackBoost);
        signalR *= (1.0f + attackBoost);
    }

    signalL *= profile.outputGain;
    signalR *= profile.outputGain;
}

#define MOS_DEFINE_VOICE(className, instrumentIndex) \
const InstrCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mos::getCharacteristics(instrumentIndex); \
} \
void className::customizeNoteOn(const InstrSettings& voiceSettings, int noteNumber, float noteVelocity)

MOS_DEFINE_VOICE(CorFrancaisVoice, 12)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.20f);
    scalePitchTransient(0.75f, 1.35f);
    scaleFormantGains(0.92f);
    scaleFilterCutoff(0.88f);
    scaleChorus(1.06f, 1.05f);
    tiltActivePartials(-0.04f);
    setPanOffset(-0.03f);
}

MOS_DEFINE_VOICE(TrompetteVoice, 13)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.02f);
    scalePitchTransient(1.18f, 0.82f);
    scaleFilterCutoff(1.00f);
    scaleFormantGains(0.98f);
    tiltActivePartials(0.05f);
    setPanOffset(0.03f);
}

MOS_DEFINE_VOICE(TromboneVoice, 14)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.08f);
    scalePitchTransient(0.86f, 1.25f);
    scaleFilterCutoff(0.88f);
    scaleFormantGains(0.96f);
    tiltActivePartials(-0.10f);
    setPanOffset(0.06f);
}

MOS_DEFINE_VOICE(TubaVoice, 15)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.14f);
    scalePitchTransient(0.66f, 1.50f);
    scaleFilterCutoff(0.68f);
    scaleFormantGains(1.00f);
    scaleMaxAge(1.16f);
    tiltActivePartials(-0.24f);
    setPanOffset(0.08f);
}

#undef MOS_DEFINE_VOICE

} // namespace mos
