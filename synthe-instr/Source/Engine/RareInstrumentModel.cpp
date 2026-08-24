#include "RareInstrumentModel.h"

#include <algorithm>
#include <atomic>

namespace mis
{
namespace
{
constexpr std::array<RareInstrumentModel, kNumInstruments> kRareInstrumentModels {{
    {
        "nyckelharpa_keyed_bow_sympathetic",
        "keyed_bow_sympathetic_strings",
        "rosined bow with keyed tangent attack",
        "dual wooden body plus sympathetic string cloud",
        "bow scrape, keyed onset, sustained sympathetic bloom",
        RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic,
        SynthesisMode::Bowed,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "gayageum_zither_pluck_bend",
        "silk_zither_pluck_with_left_hand_bend",
        "finger pluck with bridge-position color",
        "wide zither board with bendable aftertone",
        "clear pluck, short silk decay, pitch-pressure ornament",
        RareInstrumentAlgorithm::GayageumSilkZitherBend,
        SynthesisMode::Plucked,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "chapman_stick_touchboard_tap",
        "touchboard_tap_dual_range_strings",
        "two-hand tap impulse with velocity hardness",
        "electric touchboard string pair and pickup body",
        "fast tapped onset, separated bass/treble register, clean sustain",
        RareInstrumentAlgorithm::ChapmanStickTouchboardTap,
        SynthesisMode::Plucked,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "yayli_tanbur_longneck_bow_glide",
        "longneck_bowed_lute_makam_glide",
        "slow bow pressure with nasal contact point",
        "long-neck lute body with modal pitch drift",
        "continuous bowed line, nasal resonance, restrained glide",
        RareInstrumentAlgorithm::YayliTanburLongneckBowGlide,
        SynthesisMode::Bowed,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "crwth_bowed_lyre_drone_bridge",
        "bowed_lyre_asymmetric_bridge",
        "bowed string plus thumb-drone gesture",
        "box lyre body with bridge-to-back coupling",
        "double-resonator body, drone impression, archaic bowed roughness",
        RareInstrumentAlgorithm::CrwthBowedLyreDroneBridge,
        SynthesisMode::Bowed,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "carnyx_bronze_lip_reed_horn",
        "bronze_lip_reed_war_horn",
        "lip-reed brass buzz with pressure breaks",
        "long bronze tube and animal-head bell projection",
        "brassy unstable call, strong odd/even harmonic spread, battle-horn bite",
        RareInstrumentAlgorithm::CarnyxBronzeLipReedHorn,
        SynthesisMode::Blown,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "aulos_double_reed_dual_bore",
        "double_reed_two_pipe_cylindrical_bore",
        "paired reed pressure with pipe beating",
        "two narrow bores with odd-harmonic emphasis",
        "nasal reed tone, dual-pipe beating, antique pressure strain",
        RareInstrumentAlgorithm::AulosDoubleReedDualBore,
        SynthesisMode::Blown,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "fujara_overtone_flute_octave",
        "overtone_flute_side_air_column",
        "soft edge-tone breath with overblow selection",
        "long vertical flute column favoring upper partials",
        "deep breath noise, second-octave lift, pastoral slow attack",
        RareInstrumentAlgorithm::FujaraOvertoneFluteOctave,
        SynthesisMode::Blown,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "gemshorn_horn_vessel_flute",
        "horn_vessel_recorder_sine_bore",
        "gentle fipple air stream",
        "closed horn vessel with soft low-order modes",
        "near-sine medieval flute tone, low breath, rounded attack",
        RareInstrumentAlgorithm::GemshornHornVesselFlute,
        SynthesisMode::Blown,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "dizi_bamboo_membrane_flute",
        "bamboo_flute_dimo_membrane_buzz",
        "edge-tone breath with membrane rattle",
        "bamboo tube plus dimo membrane side resonator",
        "bright flute core, membrane buzz, quick ornamental response",
        RareInstrumentAlgorithm::DiziBambooMembraneBuzz,
        SynthesisMode::Blown,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "angklung_bamboo_shaken_tube",
        "shaken_bamboo_tube_pair",
        "frame shake with alternating tube impacts",
        "paired bamboo tubes with rattling pitch center",
        "shaken attack clusters, bamboo knock, pitched shimmer",
        RareInstrumentAlgorithm::AngklungShakenBambooTubePair,
        SynthesisMode::Struck,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "udu_clay_helmholtz_hand",
        "clay_pot_helmholtz_hand_percussion",
        "hand slap and hole-pop excitation",
        "Helmholtz cavity plus clay wall modes",
        "organic low air thump, hand noise, short ceramic resonance",
        RareInstrumentAlgorithm::UduClayHelmholtzHand,
        SynthesisMode::Struck,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "pyeongyeong_stone_chime_modal",
        "stone_lithophone_inharmonic_modal",
        "hard mallet strike with very short contact",
        "L-shaped stone plate with long inharmonic modes",
        "pure stone attack, sparse upper modes, solemn long decay",
        RareInstrumentAlgorithm::PyeongyeongStoneChimeModal,
        SynthesisMode::Struck,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "cristal_baschet_glass_rod_friction",
        "glass_rod_friction_metal_resonator",
        "wet-finger glass friction with bowed sustain",
        "glass rods into metallic cone reflectors",
        "singing glass fundamental, continuous excitation, metallic halo",
        RareInstrumentAlgorithm::CristalBaschetGlassRodFriction,
        SynthesisMode::Struck,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "mbira_metal_lamella_rattle",
        "lamellophone_pluck_rattle_body",
        "thumb pluck with tine stiffness",
        "metal lamellae, wooden board, optional rattle layer",
        "metallic tine attack, cyclic buzz, compact wooden body",
        RareInstrumentAlgorithm::MbiraMetalLamellaRattle,
        SynthesisMode::Plucked,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "handpan_steel_shell_tuned_modal",
        "tuned_steel_shell_modal_fields",
        "finger strike with palm damping",
        "coupled steel shell tone fields",
        "round tuned attack, singing upper partials, controlled steel tail",
        RareInstrumentAlgorithm::HandpanSteelShellModal,
        SynthesisMode::Struck,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "theremine_antenna_field_glide",
        "continuous_field_sine_glide",
        "hand-distance pitch and volume gesture",
        "pure electronic oscillator with human pitch instability",
        "legato glide, delayed vibrato, vocal pure tone without key attack",
        RareInstrumentAlgorithm::TheremineAntennaFieldGlide,
        SynthesisMode::Electronic,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "ondes_martenot_ribbon_keyboard_diffuser",
        "ribbon_keyboard_oscillator_diffusers",
        "keyboard/ribbon gesture with touche intensity",
        "electronic oscillator through diffuseur-style color stages",
        "keyboard attack into ribbon glide, restrained vibrato, diffuser color",
        RareInstrumentAlgorithm::OndesMartenotRibbonDiffuser,
        SynthesisMode::Electronic,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "pyrophone_flame_rijke_tube",
        "flame_driven_rijke_tube",
        "thermal turbulence and flame pressure",
        "heated tube resonance with unstable air column",
        "warm ignition noise, unstable tube tone, flame spectral motion",
        RareInstrumentAlgorithm::PyrophoneFlameRijkeTube,
        SynthesisMode::Electronic,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "hydraulophone_water_jet_reed",
        "water_jet_hydraulic_reed_resonator",
        "water-jet occlusion turbulence",
        "liquid flow resonator and soft pipe body",
        "liquid attack, wet turbulence, gentle pitch smear",
        RareInstrumentAlgorithm::HydraulophoneWaterJetReed,
        SynthesisMode::Electronic,
        RareEngineReadiness::DedicatedVoice
    },
    {
        "yaybahar_spring_membrane_bow",
        "bowed_string_spring_membrane_coupler",
        "bowed string feeding spring delay network",
        "helical springs into drum membranes",
        "spring echoes, membrane bloom, acoustic synth-like drone",
        RareInstrumentAlgorithm::YaybaharSpringMembraneBow,
        SynthesisMode::Bowed,
        RareEngineReadiness::DedicatedVoice
    },
}};

std::atomic<int> gRareRenderEngineMode { static_cast<int>(RareRenderEngineMode::V2) };
} // namespace

const std::array<RareInstrumentModel, kNumInstruments>& getRareInstrumentModels()
{
    return kRareInstrumentModels;
}

const RareInstrumentModel& getRareInstrumentModel(const int instrumentIndex)
{
    return kRareInstrumentModels[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

RareInstrumentAlgorithm getRareInstrumentAlgorithm(const int instrumentIndex)
{
    return getRareInstrumentModel(instrumentIndex).algorithm;
}

const char* getRareInstrumentAlgorithmName(const RareInstrumentAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
        case RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic: return "nyckelharpa_keyed_bow_sympathetic";
        case RareInstrumentAlgorithm::GayageumSilkZitherBend: return "gayageum_silk_zither_bend";
        case RareInstrumentAlgorithm::ChapmanStickTouchboardTap: return "chapman_stick_touchboard_tap";
        case RareInstrumentAlgorithm::YayliTanburLongneckBowGlide: return "yayli_tanbur_longneck_bow_glide";
        case RareInstrumentAlgorithm::CrwthBowedLyreDroneBridge: return "crwth_bowed_lyre_drone_bridge";
        case RareInstrumentAlgorithm::CarnyxBronzeLipReedHorn: return "carnyx_bronze_lip_reed_horn";
        case RareInstrumentAlgorithm::AulosDoubleReedDualBore: return "aulos_double_reed_dual_bore";
        case RareInstrumentAlgorithm::FujaraOvertoneFluteOctave: return "fujara_overtone_flute_octave";
        case RareInstrumentAlgorithm::GemshornHornVesselFlute: return "gemshorn_horn_vessel_flute";
        case RareInstrumentAlgorithm::DiziBambooMembraneBuzz: return "dizi_bamboo_membrane_buzz";
        case RareInstrumentAlgorithm::AngklungShakenBambooTubePair: return "angklung_shaken_bamboo_tube_pair";
        case RareInstrumentAlgorithm::UduClayHelmholtzHand: return "udu_clay_helmholtz_hand";
        case RareInstrumentAlgorithm::PyeongyeongStoneChimeModal: return "pyeongyeong_stone_chime_modal";
        case RareInstrumentAlgorithm::CristalBaschetGlassRodFriction: return "cristal_baschet_glass_rod_friction";
        case RareInstrumentAlgorithm::MbiraMetalLamellaRattle: return "mbira_metal_lamella_rattle";
        case RareInstrumentAlgorithm::HandpanSteelShellModal: return "handpan_steel_shell_modal";
        case RareInstrumentAlgorithm::TheremineAntennaFieldGlide: return "theremine_antenna_field_glide";
        case RareInstrumentAlgorithm::OndesMartenotRibbonDiffuser: return "ondes_martenot_ribbon_diffuser";
        case RareInstrumentAlgorithm::PyrophoneFlameRijkeTube: return "pyrophone_flame_rijke_tube";
        case RareInstrumentAlgorithm::HydraulophoneWaterJetReed: return "hydraulophone_water_jet_reed";
        case RareInstrumentAlgorithm::YaybaharSpringMembraneBow: return "yaybahar_spring_membrane_bow";
    }

    return "unknown";
}

const char* getRareEngineReadinessName(const RareEngineReadiness readiness) noexcept
{
    switch (readiness)
    {
        case RareEngineReadiness::TargetOnly: return "target_only";
        case RareEngineReadiness::SharedFamilyProfile: return "shared_family_profile";
        case RareEngineReadiness::DedicatedVoice: return "dedicated_voice";
    }

    return "unknown";
}

RareRenderEngineMode getRareRenderEngineMode() noexcept
{
    const auto value = gRareRenderEngineMode.load(std::memory_order_relaxed);
    switch (static_cast<RareRenderEngineMode>(value))
    {
        case RareRenderEngineMode::LegacyFamily:
        case RareRenderEngineMode::V2:
        case RareRenderEngineMode::V2ModelOnly:
            return static_cast<RareRenderEngineMode>(value);
    }

    return RareRenderEngineMode::V2;
}

void setRareRenderEngineMode(const RareRenderEngineMode mode) noexcept
{
    gRareRenderEngineMode.store(static_cast<int>(mode), std::memory_order_relaxed);
}

const char* getRareRenderEngineModeName(const RareRenderEngineMode mode) noexcept
{
    switch (mode)
    {
        case RareRenderEngineMode::LegacyFamily: return "legacy_family";
        case RareRenderEngineMode::V2: return "v2";
        case RareRenderEngineMode::V2ModelOnly: return "v2_model_only";
    }

    return "unknown";
}

} // namespace mis
