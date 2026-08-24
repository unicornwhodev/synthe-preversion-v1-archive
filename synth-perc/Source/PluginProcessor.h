#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "Engine/PercVoice.h"
#include "Engine/FactoryPresets.h"
#include "Engine/FxProcessors.h"
#include "../../Shared/PitchBendState.h"
#include "../../Shared/ModulationMatrix.h"
#include "../../Shared/PresetManifest.h"

class PercSynthAudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::AsyncUpdater
{
public:
    enum class QualityMode : int
    {
        Live = 0,
        Studio
    };

    static constexpr int kNumAuxOutputs = 4;
    static constexpr int kMaxVoices     = 32;
    static constexpr const char* kProcessorName = "UWdeVST_perc";
    // Max concurrent voices per family: Percussions(0-2), Ambiance(3-5), Métalliques(6-8)
    static constexpr std::array<int, 3> kMaxVoicesPerFamily = { 14, 8, 10 };

    PercSynthAudioProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String makeInstrParamId(int instrIndex, const juce::String& suffix);
    static auto createBusLayout() -> BusesProperties;

    ~PercSynthAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
#if defined(JucePlugin_Name)
        return JucePlugin_Name;
#else
        return kProcessorName;
#endif
    }
    bool acceptsMidi()    const override { return true; }
    bool producesMidi()   const override { return false; }
    bool isMidiEffect()   const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return parameters; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    // ── FLkey Mini CC page system ──────────────────────────────────────
    static constexpr int kNumCCPages = 7;
    int  getMidiCCPage() const noexcept { return midiCCPage.load(std::memory_order_relaxed); }
    static const char* getCCPageName(int page) noexcept;

    juce::StringArray getFactoryPresetNames() const;
    int  getCurrentFactoryPresetIndex() const noexcept;
    void applyFactoryPreset(int presetIndex);
    bool saveFactoryPreset(int presetIndex);

    static juce::File getUserPresetsDirectory(int instrIndex);
    static juce::File getFactoryOverridesDirectory();
    juce::Array<juce::File> scanUserPresets() const;
    bool saveUserPreset(const juce::String& name);
    bool updateUserPreset(const juce::File& file);
    bool deleteUserPreset(const juce::File& file);
    bool loadUserPreset(const juce::File& file);
    bool isCurrentPresetUser() const noexcept;
    juce::File getCurrentUserPresetFile() const noexcept;

    int getSelectedInstrIndex() const;
    QualityMode getQualityMode() const noexcept;
    bool isDelaySyncEnabled() const noexcept;
    int getDelayDivisionIndex() const noexcept;
    float getLastKnownHostTempoBpm() const noexcept;
    float getMainMeterLevel(int channel) const noexcept;
    float getAuxMeterLevel(int auxIndex) const noexcept;
    bool isClipLatched() const noexcept;
    void clearClipLatch() noexcept;

    const mpc::InstrumentPreset* getFactoryPresetDefinition(int presetIndex) const noexcept;

    modmatrix::ModSlot getModMatrixSlot(int index) const;
    void setModMatrixSlot(int index, modmatrix::Source source,
                          modmatrix::Destination destination, float amount);
    float getModMatrixLfo2Rate() const noexcept;
    int getModMatrixLfo2Wave() const noexcept;
    void setModMatrixLfo2Rate(float rateHz);
    void setModMatrixLfo2Wave(int waveformIndex);

private:
    struct VoiceSlot
    {
        std::array<std::unique_ptr<mpc::PercVoice>, mpc::kNumInstruments> voiceBank;
        std::array<std::unique_ptr<mpc::PercVoice>, mpc::kNumInstruments> dyingVoiceBank;
        mpc::PercVoice* active = nullptr;
        mpc::PercVoice* dying  = nullptr;   // cross-instrument steal fade-out
        int dyingBus   = 0;
        int midiNote   = -1;
        int instrIndex = 0;
        float velocity = 0.0f;
        float dyingVelocity = 0.0f;
        uint64_t activationAge = 0;
    };

    struct ParamBinding
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* ranged = nullptr;
    };

    struct InstrParamRefs
    {
        ParamBinding level;
        ParamBinding tune;
        ParamBinding brightness;
        ParamBinding attack;
        ParamBinding decay;
        ParamBinding sustain;
        ParamBinding release;
        ParamBinding damping;
        ParamBinding body;
        ParamBinding noise;
        ParamBinding stereoWidth;
        ParamBinding color;
        ParamBinding cutoff;
        ParamBinding pan;
        ParamBinding output;
        ParamBinding oneShot;
        ParamBinding oneShotDecayMs;
    };

    struct GlobalParamRefs
    {
        ParamBinding selectedInstr;
        ParamBinding outputGain;
        ParamBinding qualityMode;
        ParamBinding delaySync;
        ParamBinding delayDivision;
        ParamBinding lfoRate;
        ParamBinding lfoDepth;
        ParamBinding lfoWave;
        ParamBinding macroImpact;
        ParamBinding macroResonance;
        ParamBinding macroSpace;
        ParamBinding macroCouleur;
        ParamBinding compThreshold;
        ParamBinding compRatio;
        ParamBinding compAttack;
        ParamBinding compRelease;
        ParamBinding compMakeup;
        ParamBinding compMix;
        ParamBinding satDrive;
        ParamBinding satMix;
        ParamBinding transientAttack;
        ParamBinding transientSustain;
        ParamBinding transientMix;
        ParamBinding reverbSize;
        ParamBinding reverbDamping;
        ParamBinding reverbWidth;
        ParamBinding reverbMix;
        ParamBinding reverbPredelay;
        ParamBinding eqLowFreq;
        ParamBinding eqLowGain;
        ParamBinding eqMidFreq;
        ParamBinding eqMidGain;
        ParamBinding eqMidQ;
        ParamBinding eqHighFreq;
        ParamBinding eqHighGain;
        ParamBinding chorusRate;
        ParamBinding chorusDepth;
        ParamBinding chorusMix;
        ParamBinding delayTime;
        ParamBinding delayFeedback;
        ParamBinding delayMix;
        ParamBinding limiterThreshold;
        ParamBinding limiterRelease;
        ParamBinding fxSatEnable;
        ParamBinding fxTransientEnable;
        ParamBinding fxCompEnable;
        ParamBinding fxReverbEnable;
        ParamBinding fxEqEnable;
        ParamBinding fxChorusEnable;
        ParamBinding fxDelayEnable;
        ParamBinding fxLimiterEnable;
    };

    struct GlobalBlockState
    {
        int selectedInstrument = 0;
        int qualityMode = 0;
        int delayDivision = 1;
        float lfoRate = 0.0f;
        float lfoDepth = 0.0f;
        int lfoWave = 0;
        float hostBpm = 120.0f;
        float outputGainDb = 0.0f;
        bool delaySyncToHost = false;
        mpc::GlobalFxSettings fx;
        modmatrix::ModContext baseModContext;
        modmatrix::ModResult sharedModResult;
    };

public:
    struct PersistedPresetMetadata
    {
        juce::String mixRole = "custom";
        juce::String family;
        juce::String tags;
        juce::String description;
        juce::String outputProfile;
        float nominalPeakDb = -12.0f;
    };

    struct PresetPersistenceState
    {
        juce::String name;
        int instrIndex = 0;
        int presetIndex = -1;
        int outputBus = 0;
        mpc::InstrSettings settings;
        mpc::GlobalFxSettings fx;
        int qualityMode = 0;
        int delaySync = 0;
        int delayDivision = 1;
        float lfoRate = 1.8f;
        float lfoDepth = 0.0f;
        int lfoWave = 0;
        float macroImpact = 0.5f;
        float macroResonance = 0.5f;
        float macroSpace = 0.5f;
        float macroCouleur = 0.5f;
        modmatrix::MatrixState modMatrix {};
        PersistedPresetMetadata metadata {};
    };

private:
    float getParamValue(const juce::String& paramId) const;
    float readCachedParamValue(const ParamBinding& binding, float fallback = 0.0f) const noexcept;
    void  setParamValue(const juce::String& paramId, float value);
    void  setParamValueInternal(const juce::String& paramId, float value, bool notifyHost);
    void  queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue);
    float sanitizeParameterValue(const juce::String& paramId, float value, float fallback, int* warningCount = nullptr) const;
    void  resolveParameterPointers();
    void  rebuildMidiCCBindings() noexcept;
    void  markOutputBusCacheDirty() noexcept;
    void  refreshOutputBusCacheIfNeeded() noexcept;
    void  sanitizeAllParameters();
    mpc::InstrSettings sanitizeInstrSettings(int instrIndex, const mpc::InstrSettings& settings) const;
    mpc::GlobalFxSettings sanitizeFxSettings(const mpc::GlobalFxSettings& fx) const;
    double readHostTempoBpm() const;
    GlobalBlockState buildGlobalBlockState();
    mpc::InstrSettings captureBaseInstrSettings(int instrIndex) const;
    mpc::InstrSettings snapshotInstrSettings(int instrIndex) const;
    int captureInstrOutputBus(int instrIndex) const;
    mpc::GlobalFxSettings snapshotFxSettings() const;
    PresetPersistenceState captureCurrentPresetState(int instrIndex) const;
    PresetPersistenceState makeFactoryPresetState(int instrIndex,
                                                  int presetIndex,
                                                  const mpc::InstrumentPreset& preset) const;
    void applyPresetPersistenceState(const PresetPersistenceState& state,
                                     bool notifyHost = false);
    void applyFxToParams(int instrIndex, const mpc::GlobalFxSettings& fx, bool notifyHost = false);
    void applyPerformanceMacros(int instrIndex, mpc::InstrSettings& s) const;
    int  findFreeVoice(int instrIndex) const;
    int  countActiveVoicesForFamily(int familyIdx) const;
    void triggerNoteOn(int instrIndex, int midiNote, float velocity);
    void triggerNoteOff(int instrIndex, int midiNote);
    void panicAllVoices();
    void releaseVoices(int midiChannel, bool immediate);
    void clearSustainedNotes() noexcept;
    bool addSustainedNote(int instrIndex, int midiNote) noexcept;
    void releaseSustainedNotes();
    void handleMidiCC(int ccNumber, int ccValue, int instrIndex);
    void storeCurrentInstrumentFxState(int instrIndex);
    void restoreInstrumentFxState(int instrIndex);
    bool isFxAvailableForCurrentInstrument(mpc::GlobalFxSlot slot) const;
    void processMasterFxChain(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalTransient(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalEQ(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalChorus(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalDelay(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalReverb(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void updateOutputMeters(juce::AudioBuffer<float>& fullBuffer, const juce::AudioBuffer<float>& mainBuffer);
    void loadFactoryOverrides();
    void applyInstrPresetSettings(int instrIndex, const mpc::InstrSettings& s, bool notifyHost = false);
    bool writePresetManifest(const juce::File& presetFile, const juce::String& presetName,
                             int instrIndex, const juce::String& sourceModel) const;
    void backfillUserPresetLibrary(int instrIndex) const;
    void backfillAllUserPresetLibraries() const;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    std::array<InstrParamRefs, mpc::kNumInstruments> instrParamRefs {};
    GlobalParamRefs globalParamRefs {};
    std::array<std::vector<mpc::InstrumentPreset>, mpc::kNumInstruments> factoryPresetBanks;
    std::array<std::vector<PresetPersistenceState>, mpc::kNumInstruments> factoryPresetStates;

    std::array<VoiceSlot, kMaxVoices> voices;
    uint64_t voiceAgeCounter = 0;

    mpc::fx::TransientShaper   fxTransient;
    mpc::fx::Saturator          fxSaturator;
    mpc::fx::ParametricEQ3Band  fxEQ;
    juce::dsp::Compressor<float> compressor;
    mpc::fx::StereoChorus       fxChorus;
    mpc::fx::StereoDelay        fxDelay;
    mpc::fx::DattorroPlateReverb fxReverb;
    mpc::fx::OutputLimiter      fxLimiter;

    juce::AudioBuffer<float> fxDryBuffer;
    juce::AudioBuffer<float> mainDryBuffer;
    juce::AudioBuffer<float> voiceScratchBuffer;
    juce::MidiBuffer oversizedChunkMidi;
    juce::dsp::Oversampling<float> satOversamplingMono { 1, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };
    juce::dsp::Oversampling<float> satOversamplingStereo { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };
    double preparedSampleRate = 44100.0;
    float lfoPhase = 0.0f;
    std::array<int, mpc::kNumInstruments> outputBusCache {};
    std::atomic<bool> outputBusCacheDirty { true };
    modmatrix::ModulationMatrix modulationMatrix;
    modmatrix::ModResult cachedModResult {};

    struct CompressorCache
    {
        float threshold =  1.0f;
        float ratio     = -1.0f;
        float attack    = -1.0f;
        float release   = -1.0f;
    } compCache;

    PitchBendState pitchBend;
    VelocityCurve  velocityCurve = VelocityCurve::Linear;
    std::atomic<int> midiCCPage { 0 };  // FLkey Mini CC page (0..kNumCCPages-1)
    std::atomic<float> lastKnownHostTempoBpm { 120.0f };
    std::array<std::atomic<float>, 2> mainMeterLevels { 0.0f, 0.0f };
    std::array<std::atomic<float>, kNumAuxOutputs> auxMeterLevels { 0.0f, 0.0f, 0.0f, 0.0f };
    std::atomic<bool> clipLatched { false };

    float satDriveCurrent = 1.5f;
    float satMixCurrent = 0.0f;
    std::array<float, 2> saturatorPrevInput { 0.0f, 0.0f };
    std::array<float, 2> saturatorPrevAdaaInput { 0.0f, 0.0f };

    static constexpr int kNumMidiCCKnobs = 8;
    std::array<std::array<juce::RangedAudioParameter*, kNumMidiCCKnobs>, kNumCCPages> ccGlobalBindings {};
    std::array<std::array<std::array<juce::RangedAudioParameter*, kNumMidiCCKnobs>, kNumCCPages>, mpc::kNumInstruments> ccInstrumentBindings {};

    bool sustainHeld = false;
    static constexpr int kMaxSustainedNotes = 128;
    std::array<std::pair<int, int>, kMaxSustainedNotes> sustainedNotes {};
    int sustainedNoteCount = 0;

    std::array<int,        mpc::kNumInstruments> currentPresetIndices;
    std::array<juce::File, mpc::kNumInstruments> currentUserPresetFiles;
    std::array<mpc::GlobalFxSettings, mpc::kNumInstruments> instrumentFxStates;
    struct PendingParamUpdate
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };
    static constexpr int kPendingParamQueueSize = 32;
    juce::AbstractFifo pendingParamUpdateFifo { kPendingParamQueueSize };
    std::array<PendingParamUpdate, kPendingParamQueueSize> pendingParamUpdates {};
    mutable std::mutex stateMutex;
    std::atomic<bool> isRestoringState { false };
    std::atomic<int> pendingSelectedInstrumentIndex { 0 };
    int cachedSelectedInstrumentIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PercSynthAudioProcessor)
};
