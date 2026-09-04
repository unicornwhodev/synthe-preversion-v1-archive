#pragma once

#include "PercDefs.h"
#include "PercInstrumentModel.h"
#include <array>
#include <cstdint>
#include <memory>
#include <cmath>

namespace mpc
{

struct VoiceModulation
{
    float cutoffMul = 1.0f;
    float resonanceAdd = 0.0f;
    float panAdd = 0.0f;
    float attackScale = 1.0f;
    float decayScale = 1.0f;
    float pitchSemi = 0.0f;
    float levelMul = 1.0f;
    float duckingMul = 1.0f; // transient level reduction when a new note steals the slot
};

// =========================================================================
// Percussion voice — modal synthesis with noise excitation
// =========================================================================
class PercVoice
{
public:
    virtual ~PercVoice() = default;

    void noteOn(const InstrSettings& settings, int midiNote, float velocity, double sampleRate);
    void noteOff();
    void forceQuickRelease() noexcept;
    void reset() noexcept;

    // Render next sample (stereo)
    void render(float& outL, float& outR, double sampleRate);
    void renderBlock(float* outL, float* outR, int numSamples, double sampleRate);

    bool isActive()    const noexcept { return active; }
    bool isReleasing() const noexcept { return envStage == Release; }
    float getEnvelopeLevel() const noexcept { return envLevel; }
    float debugReadCombForTests(float delaySamples) const { return readComb(delaySamples); }

    void setPitchBendFactor(float f) noexcept
    {
        pitchBendFactor = std::isfinite(f) ? juce::jlimit(0.0625f, 16.0f, f) : 1.0f;
    }
    void setVoiceModulation(const VoiceModulation& modulation, double sampleRate) noexcept;

    // Voice modulation is audio-thread-owned. UI state is snapshotted by the
    // processor before reaching the voice, so render never blocks on a mutex.

protected:
    virtual const InstrCharacteristics& getCharacteristics() const noexcept = 0;
    virtual int getInstrumentIndex() const noexcept = 0;

private:
    static constexpr int kMaxModes = 16;
    static constexpr int kBodyBufSize = 4096;

    // Per-mode state
    struct ModeState
    {
        float phase     = 0.0f;
        float phaseInc  = 0.0f;     // current safe frequency as phase increment
        float basePhaseInc = 0.0f;  // unbent frequency as phase increment
        float amplitude = 0.0f;     // initial amplitude
        float decayCoef = 1.0f;     // per-sample decay multiplier
        float currentAmp = 0.0f;    // current decaying amplitude
        float panL      = 0.707f;
        float panR      = 0.707f;
    };

    // Velocity brightness envelope (animated timbral rolloff)
    float brightCutoffTarget  = 5000.0f;
    float brightCutoffCurrent = 5000.0f;
    float brightDecayCoeff    = 1.0f;

    // Attack click transient (mallet/stick impact)
    float clickLevel    = 0.0f;
    float clickDecay    = 1.0f;
    float clickFilt     = 0.0f;
    float clickHpState  = 0.0f;  // DC-blocking HP state
    float clickFiltCoeff = 0.4f; // LP filter coeff for click (adaptive per instrument)

    // Noise excitation
    float noiseLevel     = 0.0f;
    float noiseDecayCoef = 1.0f;
    float noiseCurrent   = 0.0f;
    float noiseBright    = 0.5f;
    float noisePrev      = 0.0f;     // for simple LP

    // Body resonator (comb filter with 2-pole SVF damping)
    std::array<float, kBodyBufSize> bodyBuf = {};
    int   bodyWritePos   = 0;
    float bodyDelay      = 0.0f;
    float bodyFeedback   = 0.0f;
    float bodyDampBand   = 0.0f;   // SVF band state
    float bodyDampLow    = 0.0f;   // SVF low state (LP output)
    float bodyDampF      = 0.0f;   // SVF frequency coefficient
    float bodyDampQinv   = 0.707f; // SVF Q inverse

    // SVF filter
    float svfBand        = 0.0f;
    float svfLow         = 0.0f;
    float filterF        = 0.0f;
    float filterQinv     = 0.707f;
    float filterBaseF    = 0.0f;    // resting cutoff coefficient
    float filterCurrentF = 0.0f;   // current animated cutoff
    float filterDecayCoeff = 1.0f; // decay rate towards base
    float filterMaxF     = 0.0f;   // stability limit

    // ADSR envelope
    enum EnvStage { Off, Attack, Decay, Sustain, Release };
    EnvStage envStage  = Off;
    float envLevel     = 0.0f;
    float envAttackInc = 0.0f;
    float envDecayMul  = 1.0f;
    float envSustain   = 0.0f;
    float envRelMul    = 1.0f;

    // Stereo
    float panL = 0.707f, panR = 0.707f;
    float stereoWidth = 0.0f;

    // Color / metallic shift
    float colorShift = 0.0f;

    // General
    float velocity     = 0.0f;
    float levelGain    = 1.0f;
    float pitchFollowing = 1.0f;
    float randomization  = 0.0f;

    // Round-robin: cycles through 2-4 timbral variations
    int roundRobinCount = 0;

    // DC blocker state
    float dcX1 = 0.0f;
    float dcY1 = 0.0f;

    int   numActiveModes = 0;
    std::array<ModeState, kMaxModes> modes = {};

    bool  active       = false;
    float age          = 0.0f;
    float maxAge       = 30.0f;
    float storedSampleRate = 44100.0f;
    float pitchBendFactor = 1.0f;
    float modPitchFactor  = 1.0f;
    float baseBodyFeedback = 0.0f;
    float baseFilterBaseF  = 0.0f;
    float baseFilterQinv   = 0.707f;
    float baseAttackSeconds = 0.005f;
    float baseDecaySeconds  = 1.5f;
    float baseReleaseSeconds = 0.2f;
    bool quickReleaseForced = false;
    float basePan = 0.0f;
    float baseLevelGain = 1.0f;
    VoiceModulation currentModulation {};
    VoiceModulation nextNoteModulation {}; // carried across reset only for pre-note ducking

    int instrumentIndex = 0;
    PercInstrumentAlgorithm percAlgorithm = PercInstrumentAlgorithm::TimbalesMembraneBessel;
    bool percDedicatedActive = false;
    bool percModelOnly = false;
    float dedicatedPhaseA = 0.0f;
    float dedicatedPhaseB = 0.0f;
    float dedicatedPhaseC = 0.0f;
    float dedicatedPhaseD = 0.0f;
    float dedicatedStateA = 0.0f;
    float dedicatedStateB = 0.0f;
    float dedicatedStateC = 0.0f;
    float dedicatedPulse = 0.0f;
    float dedicatedEnv = 0.0f;
    float dedicatedDecay = 1.0f;
    float dedicatedGain = 0.0f;
    float dedicatedPan = 0.0f;
    float dedicatedRateA = 0.0f;
    float dedicatedRateB = 0.0f;
    float dedicatedRateC = 0.0f;
    float dedicatedRateD = 0.0f;

    // RNG
    uint32_t rngState  = 12345u;
    float nextRandom();

    float readComb(float delaySamples) const;
    void updateEnvelopeCoefficients(float sampleRate) noexcept;
    void updatePanFromModulation() noexcept;
    void updateFilterFromModulation(float sampleRate) noexcept;
    bool usesPercDedicatedAlgorithm() const noexcept;
    void applyDedicatedNoteProfile(float fundHz, float sr, float vel, const InstrSettings& settings) noexcept;
    void applyDedicatedRenderSignature(float& signalL, float& signalR, float noiseSig, float sampleRate) noexcept;
};

#define MPC_DECLARE_PERC_VOICE(className, instrumentIndex) \
class className final : public PercVoice \
{ \
protected: \
    const InstrCharacteristics& getCharacteristics() const noexcept override; \
    int getInstrumentIndex() const noexcept override { return instrumentIndex; } \
};

MPC_DECLARE_PERC_VOICE(TimbalesVoice, 0)
MPC_DECLARE_PERC_VOICE(MarimbaVoice, 1)
MPC_DECLARE_PERC_VOICE(DjembeVoice, 2)
MPC_DECLARE_PERC_VOICE(RainstickVoice, 3)
MPC_DECLARE_PERC_VOICE(SingingBowlVoice, 4)
MPC_DECLARE_PERC_VOICE(WindChimesVoice, 5)
MPC_DECLARE_PERC_VOICE(TubularBellVoice, 6)
MPC_DECLARE_PERC_VOICE(TriangleVoice, 7)
MPC_DECLARE_PERC_VOICE(GlockenspielVoice, 8)

#undef MPC_DECLARE_PERC_VOICE

std::unique_ptr<PercVoice> createVoiceForInstrument(int instrIndex);

} // namespace mpc
