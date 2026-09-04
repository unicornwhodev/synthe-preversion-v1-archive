#pragma once
	
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/OrchVoice.h"
#include "Engine/FxProcessors.h"
#include "Engine/FactoryPresets.h"
#include "../../Shared/PitchBendState.h"

class OrchSynthAudioProcessor : public juce::AudioProcessor,
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
    static constexpr int kNumModMatrixSlots = 16;  // FIX: was 8 — increased for more modulation options (P16)
    static constexpr const char* kProcessorName = "UWdeVST_orch";

    OrchSynthAudioProcessor();
    ~OrchSynthAudioProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String makeInstrParamId(int instrIndex, const juce::String& suffix);
    static juce::String makeModMatrixParamId(int slotIndex, const juce::String& suffix);
    static auto createBusLayout() -> BusesProperties;

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
    bool shouldStopNotesOnKeyRelease() const noexcept;
    void setStopNotesOnKeyRelease(bool shouldStop) noexcept;
    bool isDelaySyncEnabled() const noexcept;
    int getDelayDivisionIndex() const noexcept;
    float getLastKnownHostTempoBpm() const noexcept;
    float getMainMeterLevel(int channel) const noexcept;
    float getAuxMeterLevel(int auxIndex) const noexcept;
    bool isClipLatched() const noexcept;
    void clearClipLatch() noexcept;
    bool isFxAvailableForCurrentInstr(mos::GlobalFxSlot slot) const;
    const mos::InstrumentPreset* getFactoryPresetDefinition(int presetIndex) const noexcept;
    int getActiveVoiceCount() const noexcept;
    void randomizePreset(float amount = 0.15f);
    void requestPanicAllVoices() noexcept;

    static constexpr int kNumCCPages = 7;
    int  getMidiCCPage() const noexcept { return midiCCPage.load(std::memory_order_relaxed); }
    static const char* getCCPageName(int page) noexcept;

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
        mos::InstrSettings settings;
        mos::GlobalFxSettings fx;
        int qualityMode = 0;
        int delaySync = 0;
        int delayDivision = 1;
        float lfoRate = 1.1f;
        float lfoDepth = 0.0f;
        int lfoWave = 0;
        int velocityCurve = 0;
        float portamentoSeconds = 0.0f;
        float legatoAmount = 1.0f;
        float roundRobinAmount = 0.5f;
        float macroWarmth = 0.5f;
        float macroBrillance = 0.5f;
        float macroSpace = 0.5f;
        float macroExpression = 0.5f;
        std::array<int, kNumModMatrixSlots> modSources {};
        std::array<int, kNumModMatrixSlots> modDestinations {};
        std::array<float, kNumModMatrixSlots> modAmounts {};
        std::array<bool, 8> fxEnables { true, true, true, true, true, true, true, true };
        bool fxLock = false;
        PersistedPresetMetadata metadata {};
    };

private:
    struct VoiceSlot
    {
        std::array<std::unique_ptr<mos::OrchVoice>, mos::kNumInstruments> voiceBank;
        std::array<std::unique_ptr<mos::OrchVoice>, mos::kNumInstruments> alternateVoiceBank;
        mos::OrchVoice* active = nullptr;   // points into voiceBank, no ownership
        mos::OrchVoice* dying  = nullptr;   // voice doing a quick fade-out after steal
        int dyingBus         = 0;
        int dyingMidiChannel = 0;
        int sourceMidiNote = -1;
        int renderMidiNote = -1;
        int instrIndex     = 0;
        int midiChannel    = 0;
        bool deferredNoteOff = false;
        bool heldAfterKeyRelease = false;
        float activeTailHintSeconds = 0.0f;
        float dyingTailHintSeconds = 0.0f;
        uint64_t activationAge = 0;
    };

    float getParamValue(const juce::String& paramId) const;
    float sanitizeParameterValue(const juce::String& paramId,
                                 float value,
                                 float fallback,
                                 int* warningCount = nullptr) const;
    void  setParamValueInternal(const juce::String& paramId, float value, bool notifyHost);
    void  setParamValue(const juce::String& paramId, float value);
    void  queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue);
    mos::InstrSettings sanitizeInstrSettings(int instrIndex, const mos::InstrSettings& settings) const;
    mos::GlobalFxSettings sanitizeFxSettings(const mos::GlobalFxSettings& fx) const;
    double readHostTempoBpm() const;
    void sanitizeAllParameters();
    mos::InstrSettings captureBaseInstrSettings(int instrIndex) const;
    mos::InstrSettings snapshotInstrSettings(int instrIndex) const;
    PresetPersistenceState captureCurrentPresetState(int instrIndex) const;
    PresetPersistenceState makeFactoryPresetState(int instrIndex,
                                                  int presetIndex,
                                                  const mos::InstrumentPreset& preset) const;
    void applyPresetPersistenceState(const PresetPersistenceState& state,
                                     bool notifyHost = false);
    void applyPerformanceMacros(int instrIndex, mos::InstrSettings& s) const;
    int  findFreeVoice() const;
    void clearVoice(VoiceSlot& slot);
    void panicAllVoices();
    void triggerNoteOn(int instrIndex, int midiNote, float velocity, int midiChannel = 0);
    void triggerNoteOff(int instrIndex, int midiNote, int midiChannel = 0);
    void releaseSustainedVoices(int channel, float pedalPosition);
    void resetMidiPerformanceState();
    struct ModMatrixSlotSnapshot
    {
        int source = 0;
        int destination = 0;
        float amount = 0.0f;
    };

    struct RealtimePerformanceBlock
    {
        std::array<ModMatrixSlotSnapshot, kNumModMatrixSlots> slots {};
        bool hasExplicitModWheelGain = false;
        float lfoDepth = 0.0f;
        int lfoWave = 0;
        float lfoValue = 0.0f;
    };

    RealtimePerformanceBlock captureRealtimePerformanceBlock() const;
    void applyRealtimePerformance(mos::OrchVoice& voice, int midiChannel) const;
    void applyRealtimePerformance(mos::OrchVoice& voice,
                                  int midiChannel,
                                  const RealtimePerformanceBlock& block) const;
    float releaseSecondsForNoteOff(int instrIndex, int midiChannel) const;
    float releaseSecondsForPedal(int instrIndex, float pedalPosition, int midiChannel) const;
    void sumAuxBusesIntoMain(juce::AudioBuffer<float>& buffer,
                             juce::AudioBuffer<float>& mainBuffer,
                             int outputBusCount);
    void updateGlobalEffectParameters();
    void processGlobalTransient(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalEQ(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalChorus(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalDelay(juce::AudioBuffer<float>& mainBuffer);
    void applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalReverb(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer);
    void updateOutputMeters(juce::AudioBuffer<float>& fullBuffer,
                            const juce::AudioBuffer<float>& mainBuffer);
    void loadFactoryOverrides();
    void applyInstrPresetSettings(int instrIndex, const mos::InstrSettings& s);
    bool writePresetWithManifestRollback(const juce::File& presetFile,
                                         const juce::XmlElement& presetXml,
                                         const juce::String& presetName,
                                         int instrIndex,
                                         const juce::String& sourceModel) const;
    bool writePresetManifest(const juce::File& presetFile,
                             const juce::String& presetName,
                             int instrIndex,
                             const juce::String& sourceModel) const;
    void backfillUserPresetLibrary(int instrIndex) const;
    void applyFxSettingsToParams(const mos::GlobalFxSettings& fx, bool notifyHost = false);
    void handleMidiCC(int ccNumber, int ccValue, int instrIndex);
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    std::array<std::vector<mos::InstrumentPreset>, mos::kNumInstruments> factoryPresetBanks;
    std::array<std::vector<PresetPersistenceState>, mos::kNumInstruments> factoryPresetStates;

    std::array<VoiceSlot, kMaxVoices> voices;
    uint64_t voiceAgeCounter = 0;

    mos::fx::TransientShaper   transientShaper;
    mos::fx::Saturator           saturator;
    mos::fx::ParametricEQ3Band   eq;
    juce::dsp::Compressor<float> compressor;
    mos::fx::StereoChorus        chorus;
    mos::fx::StereoDelay         stereoDelay;
    mos::fx::DattorroPlateReverb reverb;
    mos::fx::DiffuseHallReverb   hallReverb;
    mos::fx::OutputLimiter       limiter;
    juce::AudioBuffer<float>     fxDryBuffer;
    juce::dsp::Oversampling<float> satOversampling { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };
    double preparedSampleRate = 44100.0;
    float lfoPhase = 0.0f;
    float globalLfoDepthCurrent = 0.0f;
    RealtimePerformanceBlock realtimePerformanceBlock {};

    struct CompressorCache
    {
        float threshold =  1.0f;
        float ratio     = -1.0f;
        float attack    = -1.0f;
        float release   = -1.0f;
    } compCache;

    std::array<PitchBendState, 16> pitchBendPerChannel;
    VelocityCurve  velocityCurve = VelocityCurve::Linear;
    juce::SmoothedValue<float> outputGainSmoother;

    struct ChannelPerformanceState
    {
        float expression = 1.0f;
        float modWheel = 0.0f;
        bool modWheelSeen = false;
        float breath = 0.0f;
        float aftertouch = 0.0f;
        float lastNoteVelocity = 1.0f;  // FIX: P8 - for velocity modulation source
    };

    enum class ModMatrixSource
    {
        Off = 0,
        ModWheel,
        CC11,
        Breath,
        Aftertouch,
        Velocity,
        LFO,
        PitchBend,
        Envelope
    };

    enum class ModMatrixDestination
    {
        Off = 0,
        Gain,
        Timbre,
        Vibrato,
        Release,
        Aftertouch,
        Cutoff,
        Pan,
        Pitch,
        Attack,
        Decay,
        EqMidFreq,
        EqMidGain
    };

    struct RealtimePerformanceState
    {
        float gain = 1.0f;
        float timbre = 1.0f;
        float vibrato = 1.0f;
        float release = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        float attack = 1.0f;
        float decay = 1.0f;
        float eqMidFreq = 1.0f;
        float eqMidGainDb = 0.0f;
    };

    RealtimePerformanceState evaluateRealtimePerformanceState(int midiChannel, float envelopeLevel = 1.0f) const;
    RealtimePerformanceState evaluateRealtimePerformanceState(int midiChannel,
                                                             float envelopeLevel,
                                                             const RealtimePerformanceBlock& block) const;
    static ModMatrixSource modulationSourceFromParam(float value) noexcept;
    static ModMatrixDestination modulationDestinationFromParam(float value) noexcept;

    std::array<bool, 16>  sustainPedalDown = {};
    std::array<float, 16> damperPosition   = {};
    std::array<ChannelPerformanceState, 16> channelPerformance = {};

    std::atomic<int> midiCCPage { 0 };
    std::atomic<float> lastKnownHostTempoBpm { 120.0f };
    std::atomic<bool> stopNotesOnKeyRelease { true };
    std::array<std::atomic<float>, 2> mainMeterLevels { 0.0f, 0.0f };
    std::array<std::atomic<float>, kNumAuxOutputs> auxMeterLevels { 0.0f, 0.0f, 0.0f, 0.0f };
    std::atomic<bool> clipLatched { false };

    std::array<int, mos::kNumInstruments> currentPresetIndices;
    std::array<juce::File, mos::kNumInstruments> currentUserPresetFiles;

    // FX lock / per-instrument FX cache
    std::array<mos::GlobalFxSettings, mos::kNumInstruments> cachedFxPerInstr;
    std::atomic<int> pendingSelectedInstrIndex { 0 };
    std::atomic<int> pendingFxRecallInstrIndex { -1 };
    std::atomic<int> liveVoiceCount { 0 };
    std::atomic<int> displayedVoiceCount { 0 };
    std::atomic<bool> pendingPanicAllVoices { false };
    std::atomic<int> currentFxOwnerInstr { 0 };
    struct PendingParamUpdate
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };
    static constexpr int kPendingParamQueueSize = 32;
    juce::AbstractFifo pendingParamUpdateFifo { kPendingParamQueueSize };
    std::array<PendingParamUpdate, kPendingParamQueueSize> pendingParamUpdates {};
    bool isRestoringState = false;
    int cachedSelectedInstrIndex = 0;
    float satDriveCurrent = 1.5f;
    float satMixCurrent = 0.0f;
    std::array<float, 2> saturatorPrevInput { 0.0f, 0.0f };
    mos::GlobalFxSettings snapshotFx() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrchSynthAudioProcessor)
};
