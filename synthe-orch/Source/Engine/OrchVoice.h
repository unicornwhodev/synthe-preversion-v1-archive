#pragma once

#include <JuceHeader.h>
#include "OrchDefs.h"
#include "Models/InstrumentModel.h"

#include <array>
#include <memory>

namespace mos
{

class OrchVoice
{
public:
    virtual ~OrchVoice() = default;

    void noteOn(const InstrSettings& settings,
                int midiNote, float velocity, double sampleRate,
                float portamentoSeconds = 0.0f,
                float roundRobinAmount = 0.0f,
                uint32_t noteSeed = 0,
                float previousNoteFrequency = 0.0f,
                float legatoAmount = 1.0f);
    void noteOff();
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool isActive()    const noexcept { return envState != EnvState::Off; }
    bool isReleasing() const noexcept { return envState == EnvState::Release; }
    int  getMidiNote() const noexcept { return midiNote; }
    float getEnvelopeLevelEstimate() const noexcept { return envLevel; }
    virtual int getInstrumentIndex() const noexcept = 0;
    const char* getInstrumentModelName() const noexcept;
    bool isUsingV2InstrumentModel() const noexcept;

    void setInstrumentModel(std::unique_ptr<v2::InstrumentModel> model) noexcept;
    void setLegacyCoreGainOverride(float gain) noexcept;
    void clearLegacyCoreGainOverride() noexcept;
    void setPitchBendFactor(float f) noexcept { pitchBendFactor = f; }
    void setPerformanceControls(float expressionGain,
                                float timbreCutoffScale,
                                float vibratoScale,
                                float pitchScale = 1.0f,
                                float panOffset = 0.0f) noexcept;
    void setReleaseTimeSeconds(float seconds) noexcept;
    void forceQuickRelease() noexcept;

protected:
    virtual const InstrCharacteristics& getCharacteristics() const noexcept = 0;
    virtual void customizeNoteOn(const InstrSettings& newSettings,
                                 int newMidiNote, float newVelocity)
    {
        juce::ignoreUnused(newSettings, newMidiNote, newVelocity);
    }

    // ---- Family-specific render hooks (called per-sample from render()) ----
    // Override in family base classes to specialise synthesis.
    struct SampleContext
    {
        float pitchMult;
        float chorusMod;
        float envOut;
        int   sampleIndex;
    };

    // Generate raw oscillator signal into signalL/signalR
    virtual void renderOscillators(float& signalL, float& signalR,
                                   const SampleContext& ctx) = 0;

    // Add transient layers (bow noise, breath noise, pluck, etc.)
    virtual void renderTransients(float& signalL, float& signalR,
                                  const SampleContext& ctx)
    {
        juce::ignoreUnused(signalL, signalR, ctx);
    }

    // Post-filter family coloring (brass bloom, string body, modal edge, etc.)
    virtual void renderColor(float& signalL, float& signalR,
                             const SampleContext& ctx)
    {
        juce::ignoreUnused(signalL, signalR, ctx);
    }

    void setVibratoDelaySeconds(float seconds);
    void scalePitchTransient(float amount, float timeScale = 1.0f);
    void scaleBowNoise(float levelScale, float decayTimeScale = 1.0f);
    void scaleBreathNoise(float levelScale, float decayTimeScale = 1.0f);
    void scalePluck(float levelScale, float decayTimeScale = 1.0f);
    void scaleBodyResonance(float primaryScale, float secondaryScale = 1.0f);
    void scaleFormantGains(float gainScale);
    void scaleFilterCutoff(float cutoffScale);
    void scaleChorus(float depthScale, float rateScale = 1.0f);
    void scaleBrassBloom(float bloomScale);
    void setPanOffset(float delta);
    void scaleMaxAge(float ageScale);
    void scalePartial(int partialIndex, float amplitudeScale, float decayTimeScale = 1.0f);
    void tiltActivePartials(float tilt);

    // ---- Shared state (accessible by family subclasses) ----
    enum class EnvState { Off, Attack, Decay1, Sustain, Decay2, Release };

    float readComb(const float* buf, int bufSize, int writePos, float delaySamples) const;
    static float polyBlep(float t, float dt);

    InstrSettings         settings{};
    InstrCharacteristics  chars{};
    double sr       = 44100.0;
    float  vel      = 0.0f;
    int    midiNote = -1;

    EnvState envState = EnvState::Off;

    // Multi-oscillator (saw/sine/square with unison)
    static constexpr int kMaxOsc = 4;
    struct OscState
    {
        float phase    = 0.0f;
        float phaseInc = 0.0f;
        float panL     = 0.7071f;
        float panR     = 0.7071f;
    };
    std::array<OscState, kMaxOsc> oscs{};
    int numOscs = 1;

    // Additive partials
    static constexpr int kMaxPartials = 24;
    struct PartialState
    {
        float phase      = 0.0f;
        float phaseInc   = 0.0f;
        float amplitude  = 0.0f;
        float decayCoeff = 1.0f;
    };
    std::array<PartialState, kMaxPartials> partials{};
    int numActivePartials = 0;

    // Vibrato LFO
    float vibratoPhase    = 0.0f;
    float vibratoPhaseInc = 0.0f;
    float vibratoDepth    = 0.0f;   // frequency ratio
    float vibratoDepthCurrent = 0.0f;
    float vibratoDepthTarget  = 0.0f;
    float vibratoReleaseCoeff = 1.0f;
    float vibratoDriftPhase   = 0.0f;
    float vibratoDriftPhaseInc = 0.0f;
    float vibratoDriftDepth   = 0.0f;

    // Short pitch transient for more natural wind/brass attacks
    float pitchTransient      = 1.0f;
    float pitchTransientCoeff = 1.0f;

    // Amplitude envelope
    float envLevel     = 0.0f;
    float attackRate   = 0.0f;
    float attackShape  = 0.0f;   // 0=linear, >0=slow bow shape
    float decay1Coeff  = 1.0f;
    float decay1Target = 0.0f;
    float decay2Coeff  = 1.0f;
    float releaseCoeff = 1.0f;

    float baseFreq = 0.0f;

    // Pluck transient
    float pluckLevel      = 0.0f;
    float pluckDecayCoeff = 1.0f;

    // SVF filter state
    float svfLow     = 0.0f;
    float svfBand    = 0.0f;
    float filterF    = 0.0f;
    float filterQinv = 0.0f;

    // DC blocker state
    float dcX1 = 0.0f, dcY1 = 0.0f;

    // Body resonator (comb filter)
    static constexpr int kBodyBufSize = 8192;
    float bodyBuf[kBodyBufSize] = {};
    int   bodyWritePos     = 0;
    float bodyDelaySamples = 100.0f;
    float bodyFeedback     = 0.0f;
    float bodyDampState    = 0.0f;

    // Pan
    float panL = 0.7071f;
    float panR = 0.7071f;

    // Ensemble chorus (for stereo spread)
    float chorusPhase    = 0.0f;
    float chorusPhaseInc = 0.0f;
    float chorusDepth    = 0.0f;

    // Velocity brightness envelope (additive instruments)
    float brightCutoffTarget  = 1000.0f;  // settled rolloff cutoff frequency
    float brightCutoffCurrent = 1000.0f;  // current (starts higher on hard hits)
    float brightDecayCoeff    = 1.0f;

    // Vibrato delay (strings + choir — vibrato starts after 300ms)
    int vibratoDelaySamples = 0;
    int vibratoDelayCounter = 0;

    // Bow noise transient (Section Cordes)
    float bowNoiseLevel = 0.0f;
    float bowNoiseDecay = 1.0f;
    float bowNoiseState = 0.0f;

    // Breath noise transient (Bois)
    float breathNoiseLevel = 0.0f;
    float breathNoiseDecay = 1.0f;
    float breathNoiseFilt  = 0.0f;
    float breathReleaseTailLevel = 0.0f;
    float breathReleaseTailDecay = 1.0f;
    float brassTransientLevel = 0.0f;
    float brassTransientDecay = 1.0f;
    float brassTransientHpState = 0.0f;

    // Secondary body modes — parallel BP resonators (Harpe)
    float body2State1 = 0.0f, body2State2 = 0.0f;
    float body2CoeffA = 0.0f, body2CoeffB = 0.0f, body2Gain = 0.0f;
    float body3State1 = 0.0f, body3State2 = 0.0f;
    float body3CoeffA = 0.0f, body3CoeffB = 0.0f, body3Gain = 0.0f;

    // Formant filters — parallel BP banks (Choeur)
    float form1State1 = 0.0f, form1State2 = 0.0f;
    float form1CoeffA = 0.0f, form1CoeffB = 0.0f, form1Gain = 0.0f;
    float form2State1 = 0.0f, form2State2 = 0.0f;
    float form2CoeffA = 0.0f, form2CoeffB = 0.0f, form2Gain = 0.0f;
    float form3State1 = 0.0f, form3State2 = 0.0f;
    float form3CoeffA = 0.0f, form3CoeffB = 0.0f, form3Gain = 0.0f;

    // Noise
    juce::Random rng;

    float pitchBendFactor = 1.0f;
    float realtimeExpressionGain = 1.0f;
    float realtimeTimbreCutoffScale = 1.0f;
    float realtimeVibratoScale = 1.0f;
    float realtimePitchScale = 1.0f;
    float realtimePanOffset = 0.0f;
    float roundRobinNoiseScale = 1.0f;
    float dynamicTimbreCutoffScale = 1.0f;
    float dynamicPartialTilt = 0.0f;
    float dynamicNoiseScale = 1.0f;
    float brassBloomScale = 1.0f;
    float formantRegisterScaleApplied = 1.0f;
    float legatoOnsetScale = 1.0f;
    float legatoAmountActive = 0.0f;
    bool legatoTransitionActive = false;
    float portamentoPitchMult = 1.0f;
    float portamentoPitchStep = 0.0f;
    int portamentoSamplesRemaining = 0;
    std::unique_ptr<v2::InstrumentModel> instrumentModel;
    float legacyCoreGainOverride = -1.0f;

    int ageSamples    = 0;
    int maxAgeSamples = 0;
};

// =========================================================================
// Family base classes — each overrides the render hooks
// =========================================================================

/// Bowed strings: Saw polyBLEP oscillators, bow noise, body resonator
class BowedStringVoiceBase : public OrchVoice
{
protected:
    void renderOscillators(float& signalL, float& signalR,
                           const SampleContext& ctx) override;
    void renderTransients(float& signalL, float& signalR,
                          const SampleContext& ctx) override;
    void renderColor(float& signalL, float& signalR,
                     const SampleContext& ctx) override;
};

/// Plucked strings: Additive partials, pluck transient, body resonator + secondary modes
class PluckedStringVoiceBase : public OrchVoice
{
protected:
    void renderOscillators(float& signalL, float& signalR,
                           const SampleContext& ctx) override;
    void renderTransients(float& signalL, float& signalR,
                          const SampleContext& ctx) override;
};

/// Woodwinds: Additive partials with breath noise, formants, sustained air
class WoodwindVoiceBase : public OrchVoice
{
protected:
    void renderOscillators(float& signalL, float& signalR,
                           const SampleContext& ctx) override;
    void renderTransients(float& signalL, float& signalR,
                          const SampleContext& ctx) override;
    void renderColor(float& signalL, float& signalR,
                     const SampleContext& ctx) override;
};

/// Brass: Additive partials with pitch transient, formants, brass bloom
class BrassVoiceBase : public OrchVoice
{
protected:
    void renderOscillators(float& signalL, float& signalR,
                           const SampleContext& ctx) override;
    void renderTransients(float& signalL, float& signalR,
                          const SampleContext& ctx) override;
    void renderColor(float& signalL, float& signalR,
                     const SampleContext& ctx) override;
};

/// Percussion: Modal synthesis, pluck transient, AM vibrato, shimmer
class PercussionVoiceBase : public OrchVoice
{
protected:
    void renderOscillators(float& signalL, float& signalR,
                           const SampleContext& ctx) override;
    void renderTransients(float& signalL, float& signalR,
                          const SampleContext& ctx) override;
    void renderColor(float& signalL, float& signalR,
                     const SampleContext& ctx) override;
};

// =========================================================================
// Leaf voice declarations — each inherits from its family base
// =========================================================================

#define MOS_DECLARE_VOICE(className, familyBase, instrumentIndex) \
class className final : public familyBase \
{ \
public: \
    int getInstrumentIndex() const noexcept override { return instrumentIndex; } \
\
private: \
    const InstrCharacteristics& getCharacteristics() const noexcept override; \
    void customizeNoteOn(const InstrSettings& settings, int midiNote, float velocity) override; \
};

// Cordes (Bowed)
MOS_DECLARE_VOICE(ViolonVoice,       BowedStringVoiceBase,   0)
MOS_DECLARE_VOICE(AltoVoice,         BowedStringVoiceBase,   1)
MOS_DECLARE_VOICE(VioloncelleVoice,  BowedStringVoiceBase,   2)
MOS_DECLARE_VOICE(ContrebasseVoice,  BowedStringVoiceBase,   3)

// Cordes (Plucked)
MOS_DECLARE_VOICE(HarpeVoice,        PluckedStringVoiceBase, 4)

// Bois
MOS_DECLARE_VOICE(FluteVoice,        WoodwindVoiceBase,      5)
MOS_DECLARE_VOICE(HautboisVoice,     WoodwindVoiceBase,      6)
MOS_DECLARE_VOICE(ClarinetteVoice,   WoodwindVoiceBase,      7)
MOS_DECLARE_VOICE(BassonVoice,       WoodwindVoiceBase,      8)
MOS_DECLARE_VOICE(PiccoloVoice,      WoodwindVoiceBase,      9)
MOS_DECLARE_VOICE(CorAnglaisVoice,   WoodwindVoiceBase,     10)
MOS_DECLARE_VOICE(ClarinetteBasseVoice, WoodwindVoiceBase,  11)

// Cuivres
MOS_DECLARE_VOICE(CorFrancaisVoice,  BrassVoiceBase,        12)
MOS_DECLARE_VOICE(TrompetteVoice,    BrassVoiceBase,        13)
MOS_DECLARE_VOICE(TromboneVoice,     BrassVoiceBase,        14)
MOS_DECLARE_VOICE(TubaVoice,         BrassVoiceBase,        15)

// Percussions
MOS_DECLARE_VOICE(TimbalesVoice,     PercussionVoiceBase,   16)
MOS_DECLARE_VOICE(CelestaVoice,      PercussionVoiceBase,   17)
MOS_DECLARE_VOICE(SnareVoice,        PercussionVoiceBase,   18)
MOS_DECLARE_VOICE(XylophoneVoice,    PercussionVoiceBase,   19)

#undef MOS_DECLARE_VOICE

std::unique_ptr<OrchVoice> createVoiceForInstrument(int instrIndex);

} // namespace mos
