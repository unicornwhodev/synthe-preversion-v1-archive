#pragma once

#include "../OrchDefs.h"

namespace mos::voice
{

enum class SynthesisPath
{
    BowedString = 0,
    PluckedString,
    FluteAir,
    DoubleReed,
    SingleReed,
    BrassBell,
    ModalPercussion
};

struct FamilyDynamicsProfile
{
    float cutoffDepth;
    float partialDepth;
    float noiseBase;
    float noiseVelocity;
    float noiseVelocitySquared;
    float formantBase;
    float formantVelocitySquared;
};

struct InstrumentSynthesisProfile
{
    SynthesisPath path;
    float highPartialDamping;
    float breathFilterCoeff;
    float sustainedNoiseScale;
    float reedBuzzScale;
    float releaseBuzzScale;
    float formantSendScale;
    float colorDamping;
    float modalEdgeScale;
    float outputGain;
};

const FamilyDynamicsProfile& familyDynamicsProfile(Family family) noexcept;
const InstrumentSynthesisProfile& instrumentProfile(int instrIndex) noexcept;

bool  supportsSustainedLegato(Family family, const InstrCharacteristics& chars) noexcept;
float dynamicCutoffDepth(Family family) noexcept;
float dynamicPartialDepth(Family family) noexcept;
float dynamicNoiseScale(Family family, float velocity) noexcept;
float formantDynamicScale(Family family, float velocityCurve) noexcept;
float normalizedInstrumentRegister(int instrumentIndex, int note) noexcept;

} // namespace mos::voice
