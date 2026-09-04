#include "PercInstrumentModel.h"

#include <algorithm>
#include <atomic>

namespace mpc
{
namespace
{

std::atomic<int> gPercRenderEngineMode { static_cast<int>(PercRenderEngineMode::V2) };

constexpr std::array<PercInstrumentModel, kNumInstruments> kPercInstrumentModels = {{
    { "perc.timbales.v2", "timbales", SynthMode::Modal,
      PercInstrumentAlgorithm::TimbalesMembraneBessel,
      PercEngineReadiness::DedicatedVoice,
      "felt-stick membrane strike", "bessel membrane partials plus kettle body",
      "pitched drum fundamental, broad head slap, rolling copper body" },

    { "perc.marimba.v2", "marimba", SynthMode::Modal,
      PercInstrumentAlgorithm::MarimbaWoodBarResonator,
      PercEngineReadiness::DedicatedVoice,
      "soft yarn mallet", "undercut wood bar and tuned tube",
      "warm bar attack, tuned undercut overtones, tube bloom" },

    { "perc.djembe.v2", "djembe", SynthMode::Hybrid,
      PercInstrumentAlgorithm::DjembeHandDrumSkin,
      PercEngineReadiness::DedicatedVoice,
      "hand bass tone slap", "goat-skin membrane and shell cavity",
      "velocity-dependent bass, tone, slap and shell air" },

    { "perc.rainstick.v2", "rainstick", SynthMode::Noise,
      PercInstrumentAlgorithm::RainstickGranularCascade,
      PercEngineReadiness::DedicatedVoice,
      "seed cascade", "thorn tube scattering chamber",
      "granular falling pulses and damped tube rustle" },

    { "perc.singing_bowl.v2", "singing_bowl", SynthMode::Modal,
      PercInstrumentAlgorithm::SingingBowlRubBeating,
      PercEngineReadiness::DedicatedVoice,
      "rim rub and soft striker", "beating circular shell modes",
      "slow beating partials, rim shimmer, long metallic halo" },

    { "perc.wind_chimes.v2", "wind_chimes", SynthMode::Modal,
      PercInstrumentAlgorithm::WindChimesTubeCluster,
      PercEngineReadiness::DedicatedVoice,
      "wind-driven tube contact", "suspended aluminium tube cluster",
      "staggered tube hits, random bright chimes, drifting stereo" },

    { "perc.tubular_bell.v2", "tubular_bell", SynthMode::Modal,
      PercInstrumentAlgorithm::TubularBellHumStrike,
      PercEngineReadiness::DedicatedVoice,
      "rawhide hammer", "hollow tube with hum partial",
      "0.5x hum, octave bloom, long bright bell decay" },

    { "perc.triangle.v2", "triangle", SynthMode::Modal,
      PercInstrumentAlgorithm::TriangleSteelShimmer,
      PercEngineReadiness::DedicatedVoice,
      "steel beater edge", "open triangular steel bar",
      "edge tick, high shimmer, slightly unstable inharmonic tail" },

    { "perc.glockenspiel.v2", "glockenspiel", SynthMode::Modal,
      PercInstrumentAlgorithm::GlockenspielHardMallet,
      PercEngineReadiness::DedicatedVoice,
      "hard plastic mallet", "compact steel bar",
      "glassy attack, tuned steel partials, bright short bloom" }
}};

int clampInstrumentIndex(const int instrumentIndex) noexcept
{
    return std::clamp(instrumentIndex, 0, kNumInstruments - 1);
}

} // namespace

const std::array<PercInstrumentModel, kNumInstruments>& getPercInstrumentModels() noexcept
{
    return kPercInstrumentModels;
}

const PercInstrumentModel& getPercInstrumentModel(const int instrumentIndex) noexcept
{
    return kPercInstrumentModels[static_cast<std::size_t>(clampInstrumentIndex(instrumentIndex))];
}

PercInstrumentAlgorithm getPercInstrumentAlgorithm(const int instrumentIndex) noexcept
{
    return getPercInstrumentModel(instrumentIndex).algorithm;
}

const char* getPercInstrumentAlgorithmName(const PercInstrumentAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case PercInstrumentAlgorithm::TimbalesMembraneBessel: return "timbales_membrane_bessel";
        case PercInstrumentAlgorithm::MarimbaWoodBarResonator: return "marimba_wood_bar_resonator";
        case PercInstrumentAlgorithm::DjembeHandDrumSkin: return "djembe_hand_drum_skin";
        case PercInstrumentAlgorithm::RainstickGranularCascade: return "rainstick_granular_cascade";
        case PercInstrumentAlgorithm::SingingBowlRubBeating: return "singing_bowl_rub_beating";
        case PercInstrumentAlgorithm::WindChimesTubeCluster: return "wind_chimes_tube_cluster";
        case PercInstrumentAlgorithm::TubularBellHumStrike: return "tubular_bell_hum_strike";
        case PercInstrumentAlgorithm::TriangleSteelShimmer: return "triangle_steel_shimmer";
        case PercInstrumentAlgorithm::GlockenspielHardMallet: return "glockenspiel_hard_mallet";
    }

    return "unknown";
}

const char* getPercEngineReadinessName(const PercEngineReadiness readiness) noexcept
{
    switch (readiness)
    {
        case PercEngineReadiness::TargetOnly: return "target_only";
        case PercEngineReadiness::DedicatedVoice: return "dedicated_voice";
    }

    return "unknown";
}

const char* getPercRenderEngineModeName(const PercRenderEngineMode mode) noexcept
{
    switch (mode)
    {
        case PercRenderEngineMode::LegacyFamily: return "legacy_family";
        case PercRenderEngineMode::V2: return "v2";
        case PercRenderEngineMode::V2ModelOnly: return "v2_model_only";
    }

    return "unknown";
}

PercRenderEngineMode getPercRenderEngineMode() noexcept
{
    const auto value = gPercRenderEngineMode.load(std::memory_order_relaxed);
    if (value < static_cast<int>(PercRenderEngineMode::LegacyFamily)
        || value > static_cast<int>(PercRenderEngineMode::V2ModelOnly))
        return PercRenderEngineMode::V2;

    return static_cast<PercRenderEngineMode>(value);
}

void setPercRenderEngineMode(const PercRenderEngineMode mode) noexcept
{
    gPercRenderEngineMode.store(static_cast<int>(mode), std::memory_order_relaxed);
}

bool isPercDedicatedVoiceActive() noexcept
{
    return getPercRenderEngineMode() != PercRenderEngineMode::LegacyFamily;
}

} // namespace mpc
