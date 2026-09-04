#pragma once

#include "InstrumentDefs.h"

#include <array>
#include <string>
#include <vector>

namespace mis
{

struct PerformanceSettings
{
    float lfoRate = 1.4f;
    float lfoDepth = 0.0f;
    int lfoWave = 0;
    float macroWarmth = 0.5f;
    float macroBrightness = 0.5f;
    float macroExpression = 0.5f;
    float macroTexture = 0.3f;
};

struct InstrumentPreset
{
    std::string           name;
    InstrumentSettings    settings;
    GlobalFxSettings      fx;
    int                   outputBus = 0;
    PerformanceSettings   performance {};
    float                 nominalPeakDb = -12.0f;
};

const std::array<std::vector<InstrumentPreset>, kNumInstruments>& getFactoryPresetBanks();
std::size_t getTotalFactoryPresetCount();

} // namespace mis
