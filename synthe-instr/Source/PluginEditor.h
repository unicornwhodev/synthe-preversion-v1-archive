#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "../../Shared/SynthCommon.h"

class InstrSynthAudioProcessorEditor : public CommonSynthEditor,
                                       private juce::Timer
{
public:
    explicit InstrSynthAudioProcessorEditor(InstrSynthAudioProcessor&);

    juce::String            pluginNamespace()  const override { return "RARE"; }
    juce::String            pluginTitle()      const override { return "UWdeVST_Rare"; }
    juce::StringArray       hostGetFactoryNames()              override;
    juce::Array<juce::File> hostScanUserPresets()              override;
    bool                    hostIsUserPreset()                 override;
    juce::File              hostCurrentUserFile()              override;
    int                     hostCurrentFactoryIdx()            override;
    void                    hostApplyFactory(int idx)          override;
    void                    hostLoadUser(const juce::File& f)  override;
    bool                    hostSaveUser(const juce::String& n)override;
    void                    hostUpdateUser(const juce::File& f)override;
    void                    hostSaveFactory(int idx)           override;
    void                    hostDeleteUser(const juce::File& f)override;
    juce::File              hostGetUserPresetsDir()            override;
    juce::File              hostGetUserPresetsDirForIndex(int instrumentIndex) override;
    juce::String            hostPresetInstrumentAttr() const   override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshUiForTesting();

#if defined(UWDEVST_INSTR_TEST_BUILD)
    struct LayoutSnapshot
    {
        bool compact = false;
        juce::Rectangle<int> editorBounds;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> selectorPanelBounds;
        juce::Rectangle<int> presetSearchBounds;
        juce::Rectangle<int> presetBoxBounds;
        juce::Rectangle<int> presetMetaBounds;
        juce::Rectangle<int> modelSelectorBounds;
        juce::Rectangle<int> rightPanelTabsBounds;
        juce::Rectangle<int> fxDetailTitleBounds;
        juce::Rectangle<int> modMatrixContentBounds;
        juce::Rectangle<int> physicalControlsBounds;
        juce::Rectangle<int> keyboardBounds;
        bool fxDetailVisible = false;
    };

    LayoutSnapshot captureLayoutSnapshotForTests() const;
    void setRightPanelSectionForTests(int sectionIndex);
#endif

private:
    using APVTS          = juce::AudioProcessorValueTreeState;
    using SliderAttach   = APVTS::SliderAttachment;
    using ComboBoxAttach = APVTS::ComboBoxAttachment;

    struct CtrlDef { const char* label; const char* suffix; };
    struct FxDef   { const char* label; const char* paramId; };

    void timerCallback() override;
    void rebuildInstrAttachments();
    void rebuildModelSelectorForFamily(int familyIndex, int preferredInstr = -1);
    void syncSelectionUiFromInstr();
    void syncInstrumentUiProfile();
    void syncFxAvailability();
    void syncFxRackState();
    void switchEffectTab(int tabIndex);
    void switchRightPanelSection(int sectionIndex);
    void applyInstrumentTheme(int instrIndex);
    void updateOutputGainUi();
    void updatePresetMetadataSummary();
    void syncAdvancedModUi();
    juce::String currentPresetMetadataSummary() const;
    bool isFxTabAvailable(int tabIndex, int instrIndex) const;
    int  firstAvailableFxTab(int instrIndex) const;
    int  selectedInstrFromParam() const;

    static juce::Colour familyColour(int familyIndex);
    static juce::Colour instrCatColour(int instrIndex);

    struct VisualLayoutSnapshot
    {
        bool compact = false;
        bool roomy = false;
        int headerH = 0;
        int contentX = 0;
        int contentW = 0;
        int selectorY = 0;
        int selectorH = 0;
        int bodyY = 0;
        int bodyH = 0;
        int kbY = 0;
        int kbH = 0;
        int col1X = 0;
        int col2X = 0;
        int col3X = 0;
        int colW = 0;
        HeaderZones headerZones;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> selectorPanelBounds;
    };

    VisualLayoutSnapshot computeVisualLayoutSnapshot(int width, int height) const;

    InstrSynthAudioProcessor& proc;

    static constexpr int kEnvN         = 14;
    static constexpr int kFxN          = 31;
    static constexpr int kMacroTotal   = 4;
    static constexpr int kMacroVisible = 4;
    static constexpr int kFxPerTab     = 7;
    static constexpr int kFxTabs       = 8;
    static constexpr int kRightPanelSections = 3;
    static constexpr int kModSlots = modmatrix::kMaxSlots;

    std::array<SynthFamilyTab,  mis::kNumFamilies>    familyTabs;
    std::array<SynthPresetCard, mis::kNumInstruments> presetCards;

    juce::ComboBox instrSelector;
    std::unique_ptr<ComboBoxAttach> selInstrAtt;

    std::array<juce::Slider, kEnvN> envDials;
    std::array<juce::Label,  kEnvN> envLabels;
    std::array<std::unique_ptr<SliderAttach>, kEnvN> envAttach;
    EnvelopeDisplay envVisual;
    LfoModulationDisplay lfoVisual;
    juce::Slider lfoRateDial, lfoDepthDial;
    juce::ComboBox lfoWaveSelector;
    std::unique_ptr<SliderAttach> lfoRateAtt, lfoDepthAtt;
    std::unique_ptr<ComboBoxAttach> lfoWaveAtt;
    std::array<juce::Slider, 3> physicalDials;
    std::array<juce::Label,  3> physicalLabels;
    std::array<std::unique_ptr<SliderAttach>, 3> physicalAttach;

    std::array<juce::Slider, kMacroTotal> macroDials;
    std::array<juce::Label,  kMacroTotal> macroLbls;
    std::array<std::unique_ptr<SliderAttach>, kMacroTotal> macroAtt;
    std::array<SynthEffectTab, kRightPanelSections> rightPanelTabs;

    std::array<juce::Slider, kFxN> fxDials;
    std::array<juce::Label,  kFxN> fxLbls;
    std::array<std::unique_ptr<SliderAttach>, kFxN> fxAtt;
    std::array<SynthFxRackItem, kFxTabs> fxRackItems;
    std::array<juce::ToggleButton, kFxTabs> fxBypassBtns;
    using BtnAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::array<std::unique_ptr<BtnAttach>, kFxTabs> fxBypassAtts;
    juce::Label fxDetailTitle;
    juce::Label fxUnavailableLbl;
    juce::Label physicalSectionTitle;
    juce::Label physicalSectionHint;
    juce::Label modMatrixTitle;
    juce::Label modMatrixPlaceholderLabel;
    juce::Label modMatrixHintLabel;
    std::array<juce::Label, kModSlots>    modSlotLabels;
    std::array<juce::ComboBox, kModSlots> modSourceBoxes;
    std::array<juce::ComboBox, kModSlots> modDestBoxes;
    std::array<juce::Slider, kModSlots>   modAmountSliders;
    juce::Label    modLfo2RateLabel;
    juce::Label    modLfo2WaveLabel;
    juce::Slider   modLfo2RateDial;
    juce::ComboBox modLfo2WaveSelector;
    juce::Label outputGainLabel;
    juce::Label presetMetaLabel;

    int activeFamilyIndex = 0;
    int activeFxTab       = 0;
    int activeRightPanelSection = 0;
    int cachedInstrIdx    = -1;

    static constexpr int kFxTabMap[kFxTabs][kFxPerTab] = {
        { 11, 12, 13, 14, 15, -1, -1 },
        {  0,  1, -1, -1, -1, -1, -1 },
        {  2,  3,  4, -1, -1, -1, -1 },
        {  5,  6,  7,  8,  9, 10, -1 },
        { 16, 17, 18, 19, 20, 21, 22 },
        { 23, 24, 25, -1, -1, -1, -1 },
        { 26, 27, 28, -1, -1, -1, -1 },
        { 29, 30, -1, -1, -1, -1, -1 }
    };

    static const std::array<CtrlDef, kEnvN>       kEnvCtrls;
    static const std::array<FxDef,   kMacroTotal> kMacroCtrls;
    static const std::array<FxDef,   kFxN>        kFxCtrls;

    static const char* kFxRackSummaries[kFxTabs];
    static const char* kFxBypassParamIds[kFxTabs];
    static const char* kFxTabNames[kFxTabs];
    static const char* kFxTabLabels[kFxTabs][kFxPerTab];

    // ── MIDI CC page indicator (FLkey Mini) ────────────────────────────
    juce::Label midiCCPageLabel;
    int cachedMidiCCPage = -1;

    // ── Tooltip mode system ────────────────────────────────────────────
    enum class TooltipMode { Off, Short, Novice };
    TooltipMode tooltipMode = TooltipMode::Short;

    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::TextButton    tooltipModeBtn;

    void cycleTooltipMode();
    void applyTooltips();

    static constexpr int kTooltipCount = kEnvN + 2 + kMacroTotal + kFxN + 1; // 14+2+4+31+1 = 52
    static const char* kTooltipsShort[kTooltipCount];
    static const char* kTooltipsNovice[kTooltipCount];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrSynthAudioProcessorEditor)
};
