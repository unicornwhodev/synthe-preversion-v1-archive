#include "../OrchVoice.h"
#include "../OrchConstants.h"
#include "../SinTable.h"
#include "VoiceFamilyProfiles.h"

#include <algorithm>
#include <cmath>

namespace mos
{
namespace k = mos::constants;

void WoodwindVoiceBase::renderOscillators(float& signalL, float& signalR,
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

void WoodwindVoiceBase::renderTransients(float& signalL, float& signalR,
                                         const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());

    if (breathNoiseLevel > 0.0001f)
    {
        const float noise = (rng.nextFloat() * 2.0f - 1.0f) * breathNoiseLevel;
        breathNoiseFilt += (noise - breathNoiseFilt) * profile.breathFilterCoeff;
        signalL += breathNoiseFilt;
        signalR += breathNoiseFilt;
        breathNoiseLevel *= breathNoiseDecay;
    }

    if (chars.breathNoiseAmount > 0.0001f)
    {
        const float effortFactor = 0.7f + 0.3f * std::min(1.0f,
            ctx.envOut / std::max(0.01f, decay1Target));
        const float airNoise = (rng.nextFloat() * 2.0f - 1.0f)
                     * chars.breathNoiseAmount * ctx.envOut * k::kAirColumnNoiseScale
                     * effortFactor * dynamicNoiseScale * profile.sustainedNoiseScale;
        signalL += airNoise;
        signalR += airNoise;

        if (profile.reedBuzzScale > 0.0f && chars.oddHarmonicBias > 0.3f)
        {
            const float buzz = airNoise * chars.oddHarmonicBias * k::kReedBuzzScale
                             * profile.reedBuzzScale;
            signalL += buzz;
            signalR += buzz;
        }
    }

    if (breathReleaseTailLevel > 0.00001f)
    {
        const float tailNoise = (rng.nextFloat() * 2.0f - 1.0f) * breathReleaseTailLevel;
        breathNoiseFilt += (tailNoise - breathNoiseFilt) * (profile.breathFilterCoeff * 0.55f);
        signalL += breathNoiseFilt;
        signalR += breathNoiseFilt;
        if (profile.releaseBuzzScale > 0.0f && chars.oddHarmonicBias > 0.3f)
        {
            const float tailBuzz = breathNoiseFilt * chars.oddHarmonicBias * profile.releaseBuzzScale;
            signalL += tailBuzz;
            signalR += tailBuzz;
        }
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

void WoodwindVoiceBase::renderColor(float& signalL, float& signalR,
                                    const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());
    const float damping = profile.colorDamping * (0.55f + (1.0f - ctx.envOut) * 0.45f);
    const float gain = (1.0f - damping) * profile.outputGain;
    signalL *= gain;
    signalR *= gain;
}

#define MOS_DEFINE_VOICE(className, instrumentIndex) \
const InstrCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mos::getCharacteristics(instrumentIndex); \
} \
void className::customizeNoteOn(const InstrSettings& voiceSettings, int noteNumber, float noteVelocity)

MOS_DEFINE_VOICE(FluteVoice, 5)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.12f);
    scaleBreathNoise(1.10f, 0.90f);
    scaleFilterCutoff(1.00f);
    scaleFormantGains(0.92f);
    tiltActivePartials(0.02f);
    setPanOffset(-0.06f);
}

MOS_DEFINE_VOICE(HautboisVoice, 6)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.18f);
    scaleFormantGains(0.78f);
    scaleBreathNoise(0.96f, 1.02f);
    scaleFilterCutoff(0.70f);
    scalePitchTransient(0.50f, 1.32f);
    tiltActivePartials(-0.18f);
    setPanOffset(-0.02f);
}

MOS_DEFINE_VOICE(ClarinetteVoice, 7)
{
    juce::ignoreUnused(voiceSettings, noteVelocity);
    setVibratoDelaySeconds(0.10f);
    const bool chalumeau = noteNumber < 60;
    scaleFilterCutoff(chalumeau ? 0.80f : 0.94f);
    scaleFormantGains(chalumeau ? 0.88f : 0.98f);
    tiltActivePartials(chalumeau ? -0.14f : -0.02f);
    setPanOffset(0.02f);
}

MOS_DEFINE_VOICE(BassonVoice, 8)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.14f);
    scaleFilterCutoff(0.76f);
    scaleFormantGains(1.02f);
    scaleMaxAge(1.10f);
    tiltActivePartials(-0.20f);
    setPanOffset(0.05f);
}

MOS_DEFINE_VOICE(PiccoloVoice, 9)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.06f);
    scaleBreathNoise(0.92f, 0.78f);
    scaleFilterCutoff(1.06f);
    scaleFormantGains(0.78f);
    scaleMaxAge(0.84f);
    tiltActivePartials(0.08f);
    setPanOffset(-0.08f);
}

MOS_DEFINE_VOICE(CorAnglaisVoice, 10)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.10f);
    scaleBreathNoise(0.82f, 0.86f);
    scaleFilterCutoff(0.84f);
    scaleFormantGains(1.00f);
    tiltActivePartials(-0.10f);
    setPanOffset(-0.04f);
}

MOS_DEFINE_VOICE(ClarinetteBasseVoice, 11)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    setVibratoDelaySeconds(0.12f);
    scaleBreathNoise(0.72f, 0.90f);
    scaleFilterCutoff(0.70f);
    scaleFormantGains(1.02f);
    scaleMaxAge(1.10f);
    tiltActivePartials(-0.22f);
    setPanOffset(0.04f);
}

#undef MOS_DEFINE_VOICE

} // namespace mos
