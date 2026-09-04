#include "VoiceFamilyProfiles.h"

#include <algorithm>
#include <array>

namespace mos::voice
{
namespace
{
constexpr std::array<FamilyDynamicsProfile, kNumFamilies> kFamilyDynamics = {{
    // cutoff, partial, noise base/vel/vel^2, formant base/vel^2
    { 0.22f, 0.24f, 0.82f, 0.00f, 0.38f, 1.00f, 0.00f }, // strings
    { 0.30f, 0.42f, 0.88f, 0.30f, 0.00f, 0.96f, 0.08f }, // woodwinds
    { 0.36f, 0.62f, 0.76f, 0.00f, 0.52f, 0.94f, 0.12f }, // brass
    { 0.12f, 0.18f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f }  // percussion
}};

constexpr std::array<InstrumentSynthesisProfile, kNumInstruments> kInstrumentProfiles = {{
    { SynthesisPath::BowedString,     0.92f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.04f, 1.00f, 0.97f },
    { SynthesisPath::BowedString,     0.88f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.06f, 1.00f, 0.96f },
    { SynthesisPath::BowedString,     0.84f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.07f, 1.00f, 0.96f },
    { SynthesisPath::BowedString,     0.78f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.08f, 1.00f, 0.98f },
    { SynthesisPath::PluckedString,   0.94f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.02f, 1.00f, 0.95f },
    { SynthesisPath::FluteAir,        0.92f, 0.28f, 0.78f, 0.00f, 0.00f, 0.82f, 0.04f, 1.00f, 0.94f },
    { SynthesisPath::DoubleReed,      0.62f, 0.22f, 0.64f, 0.30f, 0.06f, 0.62f, 0.16f, 1.00f, 0.90f },
    { SynthesisPath::SingleReed,      0.72f, 0.23f, 0.64f, 0.44f, 0.08f, 0.74f, 0.10f, 1.00f, 0.93f },
    { SynthesisPath::DoubleReed,      0.68f, 0.20f, 0.68f, 0.36f, 0.06f, 0.80f, 0.13f, 1.00f, 0.94f },
    { SynthesisPath::FluteAir,        0.70f, 0.23f, 0.58f, 0.00f, 0.00f, 0.66f, 0.08f, 1.00f, 0.84f },
    { SynthesisPath::DoubleReed,      0.64f, 0.21f, 0.62f, 0.28f, 0.06f, 0.74f, 0.15f, 1.00f, 0.91f },
    { SynthesisPath::SingleReed,      0.66f, 0.20f, 0.58f, 0.34f, 0.06f, 0.82f, 0.14f, 1.00f, 0.95f },
    { SynthesisPath::BrassBell,       0.80f, 0.30f, 0.68f, 0.00f, 0.00f, 0.86f, 0.06f, 1.00f, 0.88f },
    { SynthesisPath::BrassBell,       0.86f, 0.30f, 0.74f, 0.00f, 0.00f, 0.92f, 0.04f, 1.00f, 0.86f },
    { SynthesisPath::BrassBell,       0.74f, 0.30f, 0.70f, 0.00f, 0.00f, 0.86f, 0.07f, 1.00f, 0.87f },
    { SynthesisPath::BrassBell,       0.64f, 0.30f, 0.64f, 0.00f, 0.00f, 0.80f, 0.10f, 1.00f, 0.84f },
    { SynthesisPath::ModalPercussion, 0.66f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f, 0.58f, 0.90f },
    { SynthesisPath::ModalPercussion, 0.72f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f, 0.48f, 0.86f },
    { SynthesisPath::ModalPercussion, 0.66f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f, 0.40f, 0.90f },
    { SynthesisPath::ModalPercussion, 0.74f, 0.35f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f, 0.50f, 0.84f }
}};

int familyIndex(const Family family) noexcept
{
    return std::clamp(static_cast<int>(family), 0, kNumFamilies - 1);
}

int instrumentIndex(const int instrIndex) noexcept
{
    return std::clamp(instrIndex, 0, kNumInstruments - 1);
}
} // namespace

const FamilyDynamicsProfile& familyDynamicsProfile(const Family family) noexcept
{
    return kFamilyDynamics[static_cast<std::size_t>(familyIndex(family))];
}

const InstrumentSynthesisProfile& instrumentProfile(const int instrIndex) noexcept
{
    return kInstrumentProfiles[static_cast<std::size_t>(instrumentIndex(instrIndex))];
}

bool supportsSustainedLegato(const Family family, const InstrCharacteristics& chars) noexcept
{
    return chars.oscMode == OscMode::Saw
        || chars.breathNoiseAmount > 0.01f
        || (family == Family::Cuivres && chars.oscMode == OscMode::Additive);
}

float dynamicCutoffDepth(const Family family) noexcept
{
    return familyDynamicsProfile(family).cutoffDepth;
}

float dynamicPartialDepth(const Family family) noexcept
{
    return familyDynamicsProfile(family).partialDepth;
}

float dynamicNoiseScale(const Family family, const float velocity) noexcept
{
    const auto& profile = familyDynamicsProfile(family);
    const float velocityClamped = std::clamp(velocity, 0.0f, 1.0f);
    return profile.noiseBase
        + velocityClamped * profile.noiseVelocity
        + velocityClamped * velocityClamped * profile.noiseVelocitySquared;
}

float formantDynamicScale(const Family family, const float velocityCurve) noexcept
{
    const auto& profile = familyDynamicsProfile(family);
    return profile.formantBase + std::clamp(velocityCurve, 0.0f, 1.0f) * profile.formantVelocitySquared;
}

float normalizedInstrumentRegister(const int instrumentIndexToUse, const int note) noexcept
{
    const auto range = getInstrMidiNoteRange(instrumentIndexToUse);
    const int span = std::max(1, range.high - range.low);
    const float normalized = static_cast<float>(note - range.low) / static_cast<float>(span);
    return std::clamp(normalized, 0.0f, 1.0f);
}

} // namespace mos::voice
