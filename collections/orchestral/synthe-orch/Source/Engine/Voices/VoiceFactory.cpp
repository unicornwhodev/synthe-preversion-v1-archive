#include "../OrchVoice.h"
#include "../Models/InstrumentModel.h"

namespace mos
{

std::unique_ptr<OrchVoice> createVoiceForInstrument(const int instrIndex)
{
    auto attachModel = [] (std::unique_ptr<OrchVoice> voice)
    {
        if (voice != nullptr)
            voice->setInstrumentModel(v2::createInstrumentModel(voice->getInstrumentIndex()));
        return voice;
    };

    switch (instrIndex)
    {
        case 0:  return attachModel(std::make_unique<ViolonVoice>());
        case 1:  return attachModel(std::make_unique<AltoVoice>());
        case 2:  return attachModel(std::make_unique<VioloncelleVoice>());
        case 3:  return attachModel(std::make_unique<ContrebasseVoice>());
        case 4:  return attachModel(std::make_unique<HarpeVoice>());
        case 5:  return attachModel(std::make_unique<FluteVoice>());
        case 6:  return attachModel(std::make_unique<HautboisVoice>());
        case 7:  return attachModel(std::make_unique<ClarinetteVoice>());
        case 8:  return attachModel(std::make_unique<BassonVoice>());
        case 9:  return attachModel(std::make_unique<PiccoloVoice>());
        case 10: return attachModel(std::make_unique<CorAnglaisVoice>());
        case 11: return attachModel(std::make_unique<ClarinetteBasseVoice>());
        case 12: return attachModel(std::make_unique<CorFrancaisVoice>());
        case 13: return attachModel(std::make_unique<TrompetteVoice>());
        case 14: return attachModel(std::make_unique<TromboneVoice>());
        case 15: return attachModel(std::make_unique<TubaVoice>());
        case 16: return attachModel(std::make_unique<TimbalesVoice>());
        case 17: return attachModel(std::make_unique<CelestaVoice>());
        case 18: return attachModel(std::make_unique<SnareVoice>());
        case 19: return attachModel(std::make_unique<XylophoneVoice>());
        default: break;
    }

    return {};
}

} // namespace mos
