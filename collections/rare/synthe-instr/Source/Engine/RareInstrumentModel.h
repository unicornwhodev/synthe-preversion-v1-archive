#pragma once

#include "InstrumentDefs.h"

#include <array>

namespace mis
{

enum class RareEngineReadiness
{
    TargetOnly = 0,
    SharedFamilyProfile,
    DedicatedVoice
};

enum class RareRenderEngineMode
{
    LegacyFamily = 0,
    V2,
    V2ModelOnly
};

enum class RareInstrumentAlgorithm
{
    NyckelharpaKeyedBowSympathetic = 0,
    GayageumSilkZitherBend,
    ChapmanStickTouchboardTap,
    YayliTanburLongneckBowGlide,
    CrwthBowedLyreDroneBridge,
    CarnyxBronzeLipReedHorn,
    AulosDoubleReedDualBore,
    FujaraOvertoneFluteOctave,
    GemshornHornVesselFlute,
    DiziBambooMembraneBuzz,
    AngklungShakenBambooTubePair,
    UduClayHelmholtzHand,
    PyeongyeongStoneChimeModal,
    CristalBaschetGlassRodFriction,
    MbiraMetalLamellaRattle,
    HandpanSteelShellModal,
    TheremineAntennaFieldGlide,
    OndesMartenotRibbonDiffuser,
    PyrophoneFlameRijkeTube,
    HydraulophoneWaterJetReed,
    YaybaharSpringMembraneBow
};

struct RareInstrumentModel
{
    const char* modelId = "";
    const char* targetEngineId = "";
    const char* exciterModel = "";
    const char* resonatorModel = "";
    const char* auditionFocus = "";
    RareInstrumentAlgorithm algorithm = RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic;
    SynthesisMode synthesisMode = SynthesisMode::Bowed;
    RareEngineReadiness readiness = RareEngineReadiness::TargetOnly;
};

const std::array<RareInstrumentModel, kNumInstruments>& getRareInstrumentModels();
const RareInstrumentModel& getRareInstrumentModel(int instrumentIndex);
RareInstrumentAlgorithm getRareInstrumentAlgorithm(int instrumentIndex);
const char* getRareInstrumentAlgorithmName(RareInstrumentAlgorithm algorithm) noexcept;
const char* getRareEngineReadinessName(RareEngineReadiness readiness) noexcept;
RareRenderEngineMode getRareRenderEngineMode() noexcept;
void setRareRenderEngineMode(RareRenderEngineMode mode) noexcept;
const char* getRareRenderEngineModeName(RareRenderEngineMode mode) noexcept;

} // namespace mis
