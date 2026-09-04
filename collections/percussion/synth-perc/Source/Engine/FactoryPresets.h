#pragma once

#include "PercDefs.h"
#include <array>
#include <string>
#include <vector>

namespace mpc
{

struct PresetMetadata
{
    std::string mixRole = "production";
    std::vector<std::string> tags { "perc", "factory" };
    std::string familyLabel = "perc";
    std::string description = "Percussion preset";
    std::string outputProfile = "main-dry";
    float nominalPeakDb = -12.0f;
};

struct InstrumentPreset
{
    std::string      name;
    InstrSettings    settings;
    GlobalFxSettings fx;
    int              outputBus = 0;
    PresetMetadata   metadata {};
};

const std::array<std::vector<InstrumentPreset>, kNumInstruments>& getFactoryPresetBanks();
std::size_t getTotalFactoryPresetCount();

} // namespace mpc
