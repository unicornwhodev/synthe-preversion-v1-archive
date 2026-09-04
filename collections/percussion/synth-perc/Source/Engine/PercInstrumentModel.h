#pragma once

#include "PercDefs.h"

#include <array>

namespace mpc
{

enum class PercRenderEngineMode
{
    LegacyFamily = 0,
    V2,
    V2ModelOnly
};

enum class PercEngineReadiness
{
    TargetOnly = 0,
    DedicatedVoice
};

enum class PercInstrumentAlgorithm
{
    TimbalesMembraneBessel = 0,
    MarimbaWoodBarResonator,
    DjembeHandDrumSkin,
    RainstickGranularCascade,
    SingingBowlRubBeating,
    WindChimesTubeCluster,
    TubularBellHumStrike,
    TriangleSteelShimmer,
    GlockenspielHardMallet
};

struct PercInstrumentModel
{
    const char* modelId = "";
    const char* targetEngineId = "";
    SynthMode synthMode = SynthMode::Modal;
    PercInstrumentAlgorithm algorithm = PercInstrumentAlgorithm::TimbalesMembraneBessel;
    PercEngineReadiness readiness = PercEngineReadiness::TargetOnly;
    const char* exciterModel = "";
    const char* resonatorModel = "";
    const char* auditionFocus = "";
};

const std::array<PercInstrumentModel, kNumInstruments>& getPercInstrumentModels() noexcept;
const PercInstrumentModel& getPercInstrumentModel(int instrumentIndex) noexcept;
PercInstrumentAlgorithm getPercInstrumentAlgorithm(int instrumentIndex) noexcept;

const char* getPercInstrumentAlgorithmName(PercInstrumentAlgorithm algorithm) noexcept;
const char* getPercEngineReadinessName(PercEngineReadiness readiness) noexcept;
const char* getPercRenderEngineModeName(PercRenderEngineMode mode) noexcept;

PercRenderEngineMode getPercRenderEngineMode() noexcept;
void setPercRenderEngineMode(PercRenderEngineMode mode) noexcept;
bool isPercDedicatedVoiceActive() noexcept;

} // namespace mpc
