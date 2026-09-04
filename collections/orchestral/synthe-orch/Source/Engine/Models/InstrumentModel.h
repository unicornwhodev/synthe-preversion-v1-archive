#pragma once

#include "../OrchDefs.h"

#include <JuceHeader.h>
#include <memory>

namespace mos::v2
{

enum class InstrumentArticulation
{
    Sustain = 0,
    Staccato,
    Marcato,
    Soft
};

struct InstrumentModelNoteContext
{
    int instrumentIndex = 0;
    Family family = Family::Cordes;
    int midiNote = 60;
    float velocity = 0.0f;
    float baseFrequencyHz = 440.0f;
    double sampleRate = 44100.0;
    float expression = 1.0f;
    float tone = 0.5f;
    float motion = 0.0f;
    float articulation = 0.5f;
    InstrumentArticulation articulationMode = InstrumentArticulation::Sustain;
    bool legatoTransition = false;
    float legatoAmount = 0.0f;
    float legatoOnsetScale = 1.0f;
    float legatoSourceFrequencyHz = 0.0f;
    InstrSettings settings{};
    InstrCharacteristics characteristics{};
};

struct InstrumentModelFrame
{
    float pitchMult = 1.0f;
    float chorusMod = 0.0f;
    float envOut = 0.0f;
    int sampleIndex = 0;
    int midiNote = 60;
    float velocity = 0.0f;
    float baseFrequencyHz = 440.0f;
    float sampleRate = 44100.0f;
    float expression = 1.0f;
    float tone = 0.5f;
    float motion = 0.0f;
    float articulation = 0.5f;
    InstrumentArticulation articulationMode = InstrumentArticulation::Sustain;
    bool legatoTransition = false;
    float legatoAmount = 0.0f;
    float legatoOnsetScale = 1.0f;
    float legatoSourceFrequencyHz = 0.0f;
};

class InstrumentModel
{
public:
    virtual ~InstrumentModel() = default;

    virtual const char* name() const noexcept = 0;
    virtual bool isV2() const noexcept { return false; }
    virtual float legacyCoreGain() const noexcept { return 1.0f; }

    virtual void noteOn(const InstrumentModelNoteContext& context);
    virtual void renderPreFilter(const InstrumentModelFrame& frame,
                                 float& signalL,
                                 float& signalR) noexcept;
    virtual void renderPostColor(const InstrumentModelFrame& frame,
                                 float& signalL,
                                 float& signalR) noexcept;
};

std::unique_ptr<InstrumentModel> createInstrumentModel(int instrumentIndex);
std::unique_ptr<InstrumentModel> createLegacyInstrumentModel();
const char* instrumentModelName(int instrumentIndex) noexcept;
const char* instrumentModelAlgorithm(int instrumentIndex) noexcept;
float instrumentLegacyCoreGain(int instrumentIndex) noexcept;
bool instrumentUsesV2Model(int instrumentIndex) noexcept;
const char* instrumentArticulationName(InstrumentArticulation articulation) noexcept;
InstrumentArticulation inferInstrumentArticulation(const InstrSettings& settings,
                                                   const InstrCharacteristics& characteristics,
                                                   float velocity) noexcept;

} // namespace mos::v2
