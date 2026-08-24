cmake_minimum_required(VERSION 3.22)

project(UWdeVST_perc VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MPC_ENABLE_VST2 "Enable legacy VST2 format (requires legacy JUCE + VST2 SDK)" OFF)
set(UWDEVST_JUCE_DIR "" CACHE PATH "Optional path to a JUCE checkout to use instead of FetchContent")

if (UWDEVST_JUCE_DIR AND EXISTS "${UWDEVST_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${UWDEVST_JUCE_DIR}" JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(MPC_PLUGIN_FORMATS Standalone VST3)
if (MPC_ENABLE_VST2)
    list(APPEND MPC_PLUGIN_FORMATS VST)
endif()

set(_uwdevst_default_assets_dir "${CMAKE_CURRENT_SOURCE_DIR}/../assets versions png")
if (NOT EXISTS "${_uwdevst_default_assets_dir}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../assets_versions_png")
    set(_uwdevst_default_assets_dir "${CMAKE_CURRENT_SOURCE_DIR}/../assets_versions_png")
endif()

set(UWDEVST_SHARED_ASSETS_DIR "${_uwdevst_default_assets_dir}" CACHE PATH "Path to the shared UI/assets directory")
if (IS_ABSOLUTE "${UWDEVST_SHARED_ASSETS_DIR}")
    set(ASSETS_DIR "${UWDEVST_SHARED_ASSETS_DIR}")
else()
    get_filename_component(ASSETS_DIR "${UWDEVST_SHARED_ASSETS_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

juce_add_plugin(UWdeVST_perc
    COMPANY_NAME "unicorn who dev / Charli Billabert"
    BUNDLE_ID "com.uwdevst.perc"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MQPC
    PLUGIN_CODE MPC1
    FORMATS ${MPC_PLUGIN_FORMATS}
    PRODUCT_NAME "uwdevst_perc"
    ICON_BIG   "${ASSETS_DIR}/logo,favic/logo_perc.png"
    ICON_SMALL "${ASSETS_DIR}/logo,favic/logo_perc.png"
)

juce_generate_juce_header(UWdeVST_perc)

target_sources(UWdeVST_perc PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Engine/PercDefs.cpp
    Source/Engine/PercDefs.h
    Source/Engine/PercInstrumentModel.cpp
    Source/Engine/PercInstrumentModel.h
    Source/Engine/PercVoice.cpp
    Source/Engine/PercVoice.h
    Source/Engine/SinTable.h
    Source/Engine/FxProcessors.h
    Source/Engine/FactoryPresets.cpp
    Source/Engine/FactoryPresets.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.h"
)

target_compile_definitions(UWdeVST_perc PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

if (MSVC)
    target_compile_options(UWdeVST_perc PRIVATE
        /utf-8
        $<$<CONFIG:Release>:/O2 /fp:fast /GL>
    )
    target_link_options(UWdeVST_perc PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
endif()

juce_add_binary_data(PercAssets SOURCES
    "${ASSETS_DIR}/fond_perc.png"
    "${ASSETS_DIR}/icons/00_communes/001_level_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/002_tune_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/003_attack_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/004_decay_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/005_sustain_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/006_release_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/007_cutoff_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/008_pan_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/009_brightness_00001_.png"
    "${ASSETS_DIR}/icons/00_communes/006_gain_volume.png"
    "${ASSETS_DIR}/icons/00_communes/012_output_routing.png"
    "${ASSETS_DIR}/icons/06_mpc/074_famille_percussions.png"
    "${ASSETS_DIR}/icons/06_mpc/075_famille_ambiance.png"
    "${ASSETS_DIR}/icons/06_mpc/079_noise.png"
    "${ASSETS_DIR}/icons/06_mpc/080_color.png"
    "${ASSETS_DIR}/icons/06_mpc/020_shimmer_00001_.png"
    "${ASSETS_DIR}/icons/06_mpc/021_stereo_width_perc_00001_.png"
    "${ASSETS_DIR}/icons/06_mpc/082_macro_resonance.png"
    "${ASSETS_DIR}/icons/06_mpc/083_macro_espace.png"
    "${ASSETS_DIR}/icons/06_mpc/084_macro_couleur.png"
    "${ASSETS_DIR}/icons/09_fx/024_threshold_00001_.png"
    "${ASSETS_DIR}/icons/09_fx/025_ratio_00001_.png"
    "${ASSETS_DIR}/icons/09_fx/026_makeup_gain_00001_.png"
    "${ASSETS_DIR}/icons/09_fx/037_drive_saturation_00001_.png"
    "${ASSETS_DIR}/icons/09_fx/038_blend_mix_00001_.png"
)

target_link_libraries(UWdeVST_perc PRIVATE
    PercAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)
