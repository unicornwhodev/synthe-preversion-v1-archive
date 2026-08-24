#include "../OrchVoice.h"
#include "../SinTable.h"
#include "VoiceFamilyProfiles.h"

#include <algorithm>
#include <cmath>

namespace mos
{

void PercussionVoiceBase::renderOscillators(float& signalL, float& signalR,
                                            const SampleContext& ctx)
{
    const auto& profile = voice::instrumentProfile(getInstrumentIndex());

    float am = 1.0f;
    if (chorusDepth > 0.0f)
    {
        am = 1.0f - chorusDepth
           + chorusDepth * (0.5f + 0.5f * mos::fastSin(chorusPhase));
        chorusPhase += chorusPhaseInc;
        if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
    }

    float sum = 0.0f;
    const float partialDenom = std::max(1.0f, static_cast<float>(numActivePartials - 1));
    for (int n = 0; n < numActivePartials; ++n)
    {
        auto& p = partials[static_cast<std::size_t>(n)];
        const float modePosition = static_cast<float>(n) / partialDenom;
        const float damping = 1.0f - (1.0f - profile.highPartialDamping) * modePosition;

        sum += mos::fastSin(p.phase) * p.amplitude * damping;

        p.phase += p.phaseInc * ctx.pitchMult;
        if (p.phase >= 1.0f) p.phase -= 1.0f;
        p.amplitude *= p.decayCoeff;
    }

    sum *= am;
    signalL += sum;
    signalR += sum;
}

void PercussionVoiceBase::renderTransients(float& signalL, float& signalR,
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

void PercussionVoiceBase::renderColor(float& signalL, float& signalR,
                                      const SampleContext& ctx)
{
    juce::ignoreUnused(ctx);

    const auto& profile = voice::instrumentProfile(getInstrumentIndex());
    const float mono = (signalL + signalR) * 0.5f;
    const float edge = mono * mono * 0.05f * profile.modalEdgeScale;
    signalL += edge;
    signalR += edge;
    signalL *= profile.outputGain;
    signalR *= profile.outputGain;
}

#define MOS_DEFINE_VOICE(className, instrumentIndex) \
const InstrCharacteristics& className::getCharacteristics() const noexcept \
{ \
    return mos::getCharacteristics(instrumentIndex); \
} \
void className::customizeNoteOn(const InstrSettings& voiceSettings, int noteNumber, float noteVelocity)

MOS_DEFINE_VOICE(TimbalesVoice, 16)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    scalePluck(1.32f, 0.80f);
    scaleChorus(0.0f);
    scalePartial(0, 1.08f, 1.20f);
    scalePartial(1, 1.00f, 1.05f);
    scalePartial(2, 0.92f, 0.86f);
    scalePartial(3, 0.84f, 0.72f);
    scaleMaxAge(1.08f);
}

MOS_DEFINE_VOICE(CelestaVoice, 17)
{
    juce::ignoreUnused(voiceSettings, noteVelocity);
    scalePluck(1.10f, 0.65f);
    scaleChorus(0.0f);
    scalePartial(1, 1.08f, 0.90f);
    scalePartial(2, 1.18f, 0.82f);
    scalePartial(3, 1.25f, 0.72f);
    tiltActivePartials(0.22f);
    scaleFilterCutoff(1.08f);
    setPanOffset(juce::jlimit(-0.10f, 0.10f, (static_cast<float>(noteNumber) - 72.0f) / 140.0f));
}

MOS_DEFINE_VOICE(SnareVoice, 18)
{
    juce::ignoreUnused(voiceSettings, noteNumber, noteVelocity);
    scalePluck(1.42f, 0.58f);
    scaleChorus(0.0f);
    scalePartial(0, 1.20f, 0.62f);
    scalePartial(1, 0.92f, 0.54f);
    scalePartial(2, 0.74f, 0.48f);
    scalePartial(3, 0.68f, 0.42f);
    scaleMaxAge(0.62f);
    tiltActivePartials(0.16f);
}

MOS_DEFINE_VOICE(XylophoneVoice, 19)
{
    juce::ignoreUnused(voiceSettings, noteVelocity);
    scalePluck(1.28f, 0.52f);
    scaleChorus(0.0f);
    scalePartial(1, 1.10f, 0.72f);
    scalePartial(2, 1.18f, 0.58f);
    scalePartial(3, 1.06f, 0.48f);
    scaleMaxAge(0.72f);
    scaleFilterCutoff(1.18f);
    tiltActivePartials(0.26f);
    setPanOffset(juce::jlimit(-0.10f, 0.10f, (static_cast<float>(noteNumber) - 72.0f) / 120.0f));
}

#undef MOS_DEFINE_VOICE

} // namespace mos
