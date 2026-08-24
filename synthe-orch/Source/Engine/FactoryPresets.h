#pragma once

#include "OrchDefs.h"

#include <array>
#include <string>
#include <vector>

namespace mos
{

struct PresetMetadata
{
    std::string mixRole = "production";
    std::vector<std::string> tags { "orch", "factory" };
    std::string familyLabel = "orch";
    std::string description = "Orchestral preset";
    std::string outputProfile = "main-score";
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

} // namespace mos
