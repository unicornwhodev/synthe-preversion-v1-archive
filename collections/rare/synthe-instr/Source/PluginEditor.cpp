#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace lay { constexpr int W = 1100, H = 720; }

namespace
{
juce::String formatSignedValue(double value, const juce::String& suffix, int decimals = 1)
{
    juce::String text;
    if (value > 0.0)
        text << "+";
    text << juce::String(value, decimals);
    if (suffix.isNotEmpty())
        text << suffix;
    return text;
}

juce::String formatPercentFromNormalised(const juce::Slider& slider, double value)
{
    const auto percent = juce::roundToInt(const_cast<juce::Slider&>(slider).valueToProportionOfLength(value) * 100.0);
    return juce::String(percent) + "%";
}

juce::String formatPanDisplay(const juce::Slider& slider, double value)
{
    juce::ignoreUnused(slider);
    if (std::abs(value) < 0.005)
        return "Center";

    const auto side = value < 0.0 ? "L " : "R ";
    return juce::String(side) + juce::String(juce::roundToInt(std::abs(value) * 100.0)) + "%";
}

juce::String formatCutoffDisplay(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, 2) + " kHz";

    return juce::String(value, value >= 100.0 ? 0 : 1) + " Hz";
}

void applyValueFormatter(juce::Slider& slider,
                         std::function<juce::String(double)> toText,
                         std::function<double(const juce::String&)> toValue = {})
{
    slider.textFromValueFunction = std::move(toText);
    if (toValue)
        slider.valueFromTextFunction = std::move(toValue);
}

juce::String formatSignedPercent(double value)
{
    const auto amount = juce::roundToInt(std::abs(value) * 100.0);
    if (amount == 0)
        return "0%";
    return juce::String(value > 0.0 ? "+" : "-") + juce::String(amount) + "%";
}

double parseSignedPercentText(const juce::String& text)
{
    return juce::jlimit(-1.0, 1.0, text.retainCharacters("0123456789+-.").getDoubleValue() / 100.0);
}

void configureCutoffDial(juce::Slider& slider, juce::Colour fill)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 126, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setDoubleClickReturnValue(true, 0.5);
    slider.setTextValueSuffix({});
    applyValueFormatter(slider, [](double value) { return formatCutoffDisplay(value); });
}

constexpr const char* kRightPanelSectionLabels[3] = {
    "MACRO+LFO", "FX", "MOD MATRIX"
};

struct InstrLayoutMetrics
{
    bool compact = false;
    bool roomy = false;
    int outerMargin = 24;
    int gutter = 16;
    int headerH = 96;
    int selectorY = 0;
    int selectorH = 68;
    int bodyY = 0;
    int bodyH = 0;
    int kbY = 0;
    int kbH = 96;
    int contentX = 0;
    int contentW = 0;
    int colW = 0;
    int col1X = 0;
    int col2X = 0;
    int col3X = 0;
};

float layoutDensity(const bool compact, const bool roomy)
{
    if (compact)
        return -1.0f;
    if (roomy)
        return 1.0f;
    return 0.0f;
}

int interpolateGap(const float density, const int compactValue, const int normalValue, const int roomyValue)
{
    if (density <= 0.0f)
    {
        return juce::roundToInt(juce::jmap(density,
                                           -1.0f,
                                           0.0f,
                                           static_cast<float>(compactValue),
                                           static_cast<float>(normalValue)));
    }

    return juce::roundToInt(juce::jmap(density,
                                       0.0f,
                                       1.0f,
                                       static_cast<float>(normalValue),
                                       static_cast<float>(roomyValue)));
}

InstrLayoutMetrics computeLayoutMetrics(int width, int height)
{
    InstrLayoutMetrics layout;
    layout.compact = width < 1120 || height < 700;
    layout.roomy = width > 1600 || height > 940;
    const float density = layoutDensity(layout.compact, layout.roomy);
    layout.outerMargin = layout.compact ? 16 : 24;
    layout.gutter = interpolateGap(density, 10, 16, 22);
    layout.headerH = layout.compact ? 88 : 96;
    layout.selectorH = layout.compact ? 62 : 66;
    layout.kbH = layout.compact
        ? juce::jlimit(72, 96, static_cast<int>(height * 0.14f))
        : juce::jlimit(84, 126, static_cast<int>(height * (layout.roomy ? 0.145f : 0.16f)));

    const int maxContentW = juce::jmin(width - layout.outerMargin * 2, 1680);
    layout.contentW = juce::jmax(900, maxContentW);
    layout.contentX = (width - layout.contentW) / 2;

    layout.selectorY = layout.outerMargin + layout.headerH + 8;
    layout.kbY = height - layout.kbH - layout.outerMargin;
    layout.bodyY = layout.selectorY + layout.selectorH + layout.gutter;
    layout.bodyH = juce::jmax(250, layout.kbY - layout.bodyY - 14);

    layout.colW = (layout.contentW - layout.gutter * 2) / 3;
    layout.col1X = layout.contentX;
    layout.col2X = layout.col1X + layout.colW + layout.gutter;
    layout.col3X = layout.col2X + layout.colW + layout.gutter;
    return layout;
}

juce::Image loadNamedBinaryImage(const char* resourceName)
{
    int resourceSize = 0;
    if (auto* resourceData = BinaryData::getNamedResource(resourceName, resourceSize))
        return juce::ImageCache::getFromMemory(resourceData, resourceSize);

    return {};
}

using PhysicalControlSpec = synthui::PhysicalControlUiDef;
using RareInstrumentUiProfile = synthui::PhysicalControlUiProfile<3>;

constexpr PhysicalControlSpec kHiddenPhysicalControl {};

constexpr RareInstrumentUiProfile makeInstrumentUiProfile(const RareInstrumentUiProfile& physicalControls)
{
    return physicalControls;
}

const RareInstrumentUiProfile& getUiProfileForInstrument(const int instrIndex)
{
    static const RareInstrumentUiProfile bowedProfile = makeInstrumentUiProfile({{
        { "Speed", "bow_speed", "Bow speed", "Controls the bow friction speed on the string." },
        { "Pressure", "bow_pressure", "Bow pressure", "Controls bow pressure, affecting grip and roughness." },
        { "Brightness", "brightness", "Spectral brightness", "Opens or darkens the base timbre of the instrument." }
    }});
    static const RareInstrumentUiProfile pluckedProfile = makeInstrumentUiProfile({{
        { "Position", "strike_position", "Pick position", "Moves the plucking point along the string to shift harmonics." },
        { "Brightness", "brightness", "Spectral brightness", "Opens or darkens the base timbre of the instrument." },
        kHiddenPhysicalControl
    }});
    static const RareInstrumentUiProfile blownProfile = makeInstrumentUiProfile({{
        { "Breath", "breath_pressure", "Air pressure", "Controls breath intensity and air column sustain." },
        { "Brightness", "brightness", "Spectral brightness", "Opens or darkens the base timbre of the instrument." },
        kHiddenPhysicalControl
    }});
    static const RareInstrumentUiProfile struckProfile = makeInstrumentUiProfile({{
        { "Position", "strike_position", "Strike position", "Changes the impact zone to shift body resonances." },
        { "Brightness", "brightness", "Spectral brightness", "Opens or darkens the base timbre of the instrument." },
        { "Mallet", "exciter", "Mallet hardness", "Shapes the mallet hardness and attack density for struck instruments." }
    }});
    static const RareInstrumentUiProfile electronicProfile = makeInstrumentUiProfile({{
        { "Brightness", "brightness", "Spectral brightness", "Controls the amount of highs and timbre clarity." },
        kHiddenPhysicalControl,
        kHiddenPhysicalControl
    }});

    switch (mis::getCharacteristics(instrIndex).synthesisMode)
    {
        case mis::SynthesisMode::Bowed:      return bowedProfile;
        case mis::SynthesisMode::Plucked:    return pluckedProfile;
        case mis::SynthesisMode::Blown:      return blownProfile;
        case mis::SynthesisMode::Struck:     return struckProfile;
        case mis::SynthesisMode::Electronic: return electronicProfile;
    }

    return electronicProfile;
}

void glazeRareChrome(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     juce::Colour accent,
                     float radius,
                     float intensity)
{
    juce::ignoreUnused(accent);

    const auto rareTop = juce::Colour(0xff44484F).withAlpha(0.024f * intensity);
    const auto rareMid = juce::Colour(0xff252930).withAlpha(0.056f * intensity);
    const auto rareBottom = juce::Colour(0xff0C1015).withAlpha(0.18f * intensity);

    juce::ColourGradient glaze(rareTop, area.getCentreX(), area.getY(),
                               rareBottom, area.getCentreX(), area.getBottom(), false);
    glaze.addColour(0.40, rareMid);
    g.setGradientFill(glaze);
    g.fillRoundedRectangle(area, radius);

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        g.reduceClipRegion(area.toNearestInt());
        auto textureArea = area.reduced(4.0f);
        auto addCloud = [&](juce::Point<float> centre, float radiusX, float radiusY, juce::Colour colour)
        {
            juce::ColourGradient cloud(colour, centre.x, centre.y,
                                       juce::Colours::transparentBlack, centre.x + radiusX, centre.y + radiusY, true);
            g.setGradientFill(cloud);
            g.fillEllipse(centre.x - radiusX, centre.y - radiusY, radiusX * 2.0f, radiusY * 2.0f);
        };

        addCloud({ textureArea.getX() + textureArea.getWidth() * 0.24f,
                   textureArea.getY() + textureArea.getHeight() * 0.22f },
                 textureArea.getWidth() * 0.30f, textureArea.getHeight() * 0.16f,
                 juce::Colours::white.withAlpha(0.006f * intensity));
        addCloud({ textureArea.getX() + textureArea.getWidth() * 0.78f,
                   textureArea.getY() + textureArea.getHeight() * 0.36f },
                 textureArea.getWidth() * 0.24f, textureArea.getHeight() * 0.18f,
                 juce::Colours::white.withAlpha(0.004f * intensity));
        addCloud({ textureArea.getCentreX(),
                   textureArea.getBottom() - textureArea.getHeight() * 0.10f },
                 textureArea.getWidth() * 0.42f, textureArea.getHeight() * 0.14f,
                 juce::Colours::black.withAlpha(0.050f * intensity));
    }

    auto sheen = area.reduced(3.0f).withHeight(juce::jmax(7.0f, area.getHeight() * 0.12f));
    juce::ColourGradient highlight(juce::Colours::white.withAlpha(0.010f * intensity), sheen.getCentreX(), sheen.getY(),
                                   juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
    g.setGradientFill(highlight);
    g.fillRoundedRectangle(sheen, juce::jmax(0.0f, radius - 2.0f));

    g.setColour(juce::Colour(0xff575E68).withAlpha(0.058f * intensity));
    g.drawRoundedRectangle(area.reduced(1.0f), juce::jmax(0.0f, radius - 1.0f), 0.9f);
}

void applyKnobPalette(juce::Slider& slider, juce::Colour accent)
{
    const auto fill = accent.withMultipliedSaturation(0.70f)
                            .withMultipliedBrightness(1.02f)
                            .interpolatedWith(juce::Colour(0xffD6DEE7), 0.16f);
    const auto glow = accent.withMultipliedSaturation(0.56f)
                            .withMultipliedBrightness(1.00f)
                            .withAlpha(0.70f);
    const auto bezel = juce::Colour(0xff2D343D)
                           .interpolatedWith(accent.withMultipliedSaturation(0.28f)
                                                       .withMultipliedBrightness(0.74f), 0.18f)
                           .withAlpha(0.94f);
    const auto collar = juce::Colour(0xff161C23)
                            .interpolatedWith(accent.withMultipliedSaturation(0.34f)
                                                      .withMultipliedBrightness(0.82f), 0.16f)
                            .withAlpha(0.88f);
    const auto cap = accent.interpolatedWith(juce::Colour(0xffE7EDF4), 0.22f)
                           .withMultipliedSaturation(0.62f)
                           .withMultipliedBrightness(0.98f)
                           .withAlpha(0.72f);

    slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
    slider.setColour(SynthLookAndFeel::knobGlowColourId, glow);
    slider.setColour(SynthLookAndFeel::knobBezelColourId, bezel);
    slider.setColour(SynthLookAndFeel::knobCollarColourId, collar);
    slider.setColour(SynthLookAndFeel::knobCapAccentColourId, cap);
}
}

const std::array<InstrSynthAudioProcessorEditor::CtrlDef, InstrSynthAudioProcessorEditor::kEnvN>
    InstrSynthAudioProcessorEditor::kEnvCtrls = {{
        { "Level",       "level" },
        { "Tune",        "tune" },
        { "Attack",      "attack" },
        { "Decay",       "decay" },
        { "Sustain",     "sustain" },
        { "Release",     "release" },
        { "Exciter",     "exciter" },
        { "Body",        "body" },
        { "Sympathetic", "sympathetic" },
        { "Noise",       "noise" },
        { "Drive",       "drive" },
        { "Cutoff",      "cutoff" },
        { "Filter Q",    "filter_q" },
        { "Pan",         "pan" }
    }};

const std::array<InstrSynthAudioProcessorEditor::FxDef, InstrSynthAudioProcessorEditor::kMacroTotal>
    InstrSynthAudioProcessorEditor::kMacroCtrls = {{
        { "Warmth",     "macro_warmth" },
        { "Brightness", "macro_brightness" },
        { "Expression", "macro_expression" },
        { "Texture",    "macro_texture" }
    }};

const std::array<InstrSynthAudioProcessorEditor::FxDef, InstrSynthAudioProcessorEditor::kFxN>
    InstrSynthAudioProcessorEditor::kFxCtrls = {{
        { "Drive",      "sat_drive" },
        { "Mix",        "sat_mix" },
        { "Attack",     "transient_attack" },
        { "Sustain",    "transient_sustain" },
        { "Mix",        "transient_mix" },
        { "Threshold",  "comp_threshold" },
        { "Ratio",      "comp_ratio" },
        { "Attack",     "comp_attack" },
        { "Release",    "comp_release" },
        { "Makeup",     "comp_makeup" },
        { "Mix",        "comp_mix" },
        { "Size",       "reverb_size" },
        { "Damping",    "reverb_damping" },
        { "Width",      "reverb_width" },
        { "Mix",        "reverb_mix" },
        { "Predelay",   "reverb_predelay" },
        { "Lo Freq",    "eq_low_freq" },
        { "Lo Gain",    "eq_low_gain" },
        { "Mid Freq",   "eq_mid_freq" },
        { "Mid Gain",   "eq_mid_gain" },
        { "Mid Q",      "eq_mid_q" },
        { "Hi Freq",    "eq_high_freq" },
        { "Hi Gain",    "eq_high_gain" },
        { "Rate",       "chorus_rate" },
        { "Depth",      "chorus_depth" },
        { "Mix",        "chorus_mix" },
        { "Time",       "delay_time" },
        { "Feedback",   "delay_feedback" },
        { "Mix",        "delay_mix" },
        { "Threshold",  "limiter_threshold" },
        { "Release",    "limiter_release" }
    }};

const char* InstrSynthAudioProcessorEditor::kFxRackSummaries[kFxTabs] = {
    "Space", "Color", "Attack", "Dynamics", "Tone", "Width", "Echo", "Output"
};

const char* InstrSynthAudioProcessorEditor::kFxBypassParamIds[kFxTabs] = {
    "fx_tab3_en",     // idx 0 = Reverb    → "Space"
    "fx_tab0_en",     // idx 1 = Saturator → "Color"
    "fx_tab1_en",     // idx 2 = Transient → "Attack"
    "fx_tab2_en",     // idx 3 = Compressor→ "Dynamics"
    "fx_eq_en",       // idx 4 = EQ        → "Tone"
    "fx_chorus_en",   // idx 5 = Chorus    → "Width"
    "fx_delay_en",    // idx 6 = Delay     → "Echo"
    "fx_limiter_en"   // idx 7 = Limiter   → "Output"
};

const char* InstrSynthAudioProcessorEditor::kFxTabNames[kFxTabs] = {
    "REVERB", "SAT", "TRANS", "COMP", "EQ", "CHORUS", "DELAY", "LIMITER"
};
const char* InstrSynthAudioProcessorEditor::kFxTabLabels[kFxTabs][kFxPerTab] = {
    { "Size", "Damp", "Width", "Mix", "Predly", "", "" },
    { "Drive", "Mix", "", "", "", "", "" },
    { "Attack", "Sustain", "Mix", "", "", "", "" },
    { "Thresh", "Ratio", "Attack", "Release", "Makeup", "Mix", "" },
    { "Lo F", "Lo G", "Mid F", "Mid G", "Mid Q", "Hi F", "Hi G" },
    { "Rate", "Depth", "Mix", "", "", "", "" },
    { "Time", "Feedbk", "Mix", "", "", "", "" },
    { "Thresh", "Release", "", "", "", "", "" }
};

// ── Tooltip text arrays ────────────────────────────────────────────────
// Order: env(14) + lfo(2) + macro(4) + fx(31) + gain(1) = 52
const char* InstrSynthAudioProcessorEditor::kTooltipsShort[kTooltipCount] = {
    // env 0-13
    "Main volume",
    "Tuning +/- semitones",
    "Attack time",
    "Decay time",
    "Sustain level",
    "Release time",
    "Exciter energy",
    "Body resonance",
    "Sympathetic strings",
    "Noise layer",
    "Internal saturation",
    "Filter cutoff frequency",
    "Filter resonance",
    "Pan L/R",
    // lfo 14-15
    "LFO speed",
    "LFO depth",
    // macro 16-19
    "Warmth - tonal warmth",
    "Spectral brightness",
    "Expression - dynamic expression",
    "Texture - sonic character",
    // fx 20-50 (31 FX: sat(2) trans(3) comp(6) reverb(5) eq(7) chorus(3) delay(3) limiter(2))
    "Saturation drive",
    "Saturation mix",
    "Transient attack",
    "Transient sustain",
    "Transient mix",
    "Compressor threshold",
    "Compression ratio",
    "Compressor attack",
    "Compressor release",
    "Makeup gain",
    "Compressor mix",
    "Reverb size",
    "Reverb damping",
    "Reverb stereo width",
    "Reverb mix",
    "Reverb pre-delay",
    "EQ low frequency",
    "EQ low gain",
    "EQ mid frequency",
    "EQ mid gain",
    "EQ mid Q",
    "EQ high frequency",
    "EQ high gain",
    "Chorus speed",
    "Chorus depth",
    "Chorus mix",
    "Delay time",
    "Delay feedback",
    "Delay mix",
    "Limiter threshold",
    "Limiter release",
    // gain 51
    "Output volume"
};

const char* InstrSynthAudioProcessorEditor::kTooltipsNovice[kTooltipCount] = {
    // env 0-13
    "Instrument volume (0 = silence, 1 = loud)",
    "Fine tuning in semitones (negative = lower, positive = higher)",
    "How long the sound takes to start (short = percussive, long = soft)",
    "Speed at which the sound descends after the initial peak",
    "Sound level held while the key is held",
    "Fade-out time after releasing the key",
    "Exciter energy amount (friction, breath, strike...)",
    "Body resonance intensity",
    "Volume of sympathetic strings resonating in sympathy",
    "Noise layer added to the sound (breath, texture)",
    "Internal oscillator saturation (soft distortion)",
    "Low-pass filter cutoff frequency (low = dark, high = bright)",
    "Filter resonance - peak at the cutoff point",
    "Position in the stereo field (left / right)",
    // lfo 14-15
    "Low-frequency oscillator speed (periodic modulation)",
    "LFO modulation intensity on the sound",
    // macro 16-19
    "Adds warmth and roundness to the timbre",
    "Increases brightness and presence of the sound",
    "Controls the expressive intensity of the playing",
    "Modifies the texture and overall sonic character",
    // fx 20-50
    "Amount of harmonic distortion added",
    "Proportion of saturated signal in the mix",
    "Emphasises the percussive attack of the sound",
    "Emphasises or reduces the sustained part of the sound",
    "Proportion of the transient shaper in the mix",
    "Level above which compression begins",
    "Dynamic reduction intensity (2:1, 4:1...)",
    "Compressor reaction speed to peaks",
    "Compressor return speed to normal level",
    "Gain added after compression to compensate for volume loss",
    "Proportion of compressed signal in the mix",
    "Simulated room size (small = chamber, large = cathedral)",
    "High-frequency absorption in the reverb",
    "Reverb stereo width (0 = mono, 1 = very wide)",
    "Reverb proportion in the final mix",
    "Delay before the reverb starts (simulates distance)",
    "Centre frequency of the EQ low band",
    "Boosts or reduces low frequencies",
    "Centre frequency of the EQ mid band",
    "Boosts or reduces midrange",
    "Mid bandwidth (narrow = surgical, wide = musical)",
    "Centre frequency of the EQ high band",
    "Boosts or reduces high frequencies",
    "Chorus modulation speed (slow = subtle, fast = vibrato)",
    "Chorus modulation depth",
    "Chorus proportion in the mix",
    "Delay repeat time in ms",
    "Amount of signal fed back into the delay (repeating echo)",
    "Delay proportion in the final mix",
    "Maximum peak level allowed before limiting",
    "Limiter return speed after a peak",
    // gain 51
    "Global plugin output volume in dB"
};

juce::Colour InstrSynthAudioProcessorEditor::familyColour(int familyIndex)
{
    switch (familyIndex)
    {
        case 0: return juce::Colour(0xff77838f);
        case 1: return juce::Colour(0xff6f837c);
        case 2: return juce::Colour(0xff8a766e);
        case 3: return juce::Colour(0xff78758e);
        default: return juce::Colour(0xff7d7d84);
    }
}

juce::Colour InstrSynthAudioProcessorEditor::instrCatColour(int instrIndex)
{
    const int familyIndex = static_cast<int>(mis::getFamily(instrIndex));
    const int first = mis::kFamilyStart[familyIndex];
    const int count = mis::kFamilySize[familyIndex];
    const float t = count > 1 ? static_cast<float>(instrIndex - first) / static_cast<float>(count - 1) : 0.0f;

    auto base = familyColour(familyIndex).withMultipliedSaturation(0.45f);
    auto dark = base.darker(0.35f).withMultipliedSaturation(0.60f);
    auto light = base.brighter(0.18f).withMultipliedSaturation(0.75f);
    return dark.interpolatedWith(light, juce::jlimit(0.0f, 1.0f, t));
}

int InstrSynthAudioProcessorEditor::selectedInstrFromParam() const {
    if (auto* raw = proc.getAPVTS().getRawParameterValue("selected_instrument"))
        return juce::jlimit(0, mis::kNumInstruments - 1, static_cast<int>(std::round(raw->load())));
    return 0;
}

InstrSynthAudioProcessorEditor::VisualLayoutSnapshot
InstrSynthAudioProcessorEditor::computeVisualLayoutSnapshot(int width, int height) const
{
    const auto layout = computeLayoutMetrics(width, height);

    VisualLayoutSnapshot snapshot;
    snapshot.compact = layout.compact;
    snapshot.roomy = layout.roomy;
    snapshot.headerH = layout.headerH;
    snapshot.contentX = layout.contentX;
    snapshot.contentW = layout.contentW;
    snapshot.selectorY = layout.selectorY;
    snapshot.selectorH = layout.selectorH;
    snapshot.bodyY = layout.bodyY;
    snapshot.bodyH = layout.bodyH;
    snapshot.kbY = layout.kbY;
    snapshot.kbH = layout.kbH;
    snapshot.col1X = layout.col1X;
    snapshot.col2X = layout.col2X;
    snapshot.col3X = layout.col3X;
    snapshot.colW = layout.colW;
    snapshot.headerZones = computeHeaderZones(layout.headerH);
    snapshot.headerBounds = snapshot.headerZones.headerBounds;
    snapshot.selectorPanelBounds = { layout.contentX, layout.selectorY, layout.contentW, layout.selectorH - 8 };
    return snapshot;
}

void InstrSynthAudioProcessorEditor::refreshUiForTesting()
{
    syncSelectionUiFromInstr();
    syncInstrumentUiProfile();
    syncFxAvailability();
    syncFxRackState();
    syncAdvancedModUi();
    resized();
    repaint();
}

#if defined(UWDEVST_INSTR_TEST_BUILD)
InstrSynthAudioProcessorEditor::LayoutSnapshot
InstrSynthAudioProcessorEditor::captureLayoutSnapshotForTests() const
{
    const auto layout = computeVisualLayoutSnapshot(getWidth(), getHeight());

    LayoutSnapshot snapshot;
    snapshot.compact = layout.compact;
    snapshot.editorBounds = getLocalBounds();
    snapshot.headerBounds = layout.headerBounds;
    snapshot.selectorPanelBounds = layout.selectorPanelBounds;
    snapshot.presetSearchBounds = presetSearch.getBounds();
    snapshot.presetBoxBounds = presetBox.getBounds();
    if (presetMetaLabel.isVisible())
        snapshot.presetMetaBounds = presetMetaLabel.getBounds();
    snapshot.modelSelectorBounds = modelSelector.getBounds();
    snapshot.fxDetailTitleBounds = fxDetailTitle.getBounds();
    snapshot.fxDetailVisible = fxDetailTitle.isVisible();
    if (keyboard != nullptr)
        snapshot.keyboardBounds = keyboard->getBounds();

    for (const auto& tab : rightPanelTabs)
    {
        if (!tab.isVisible())
            continue;

        snapshot.rightPanelTabsBounds = snapshot.rightPanelTabsBounds.isEmpty()
            ? tab.getBounds()
            : snapshot.rightPanelTabsBounds.getUnion(tab.getBounds());
    }

    auto includeVisibleBounds = [&snapshot](const juce::Component& component)
    {
        if (!component.isVisible() || component.getBounds().isEmpty())
            return;

        snapshot.modMatrixContentBounds = snapshot.modMatrixContentBounds.isEmpty()
            ? component.getBounds()
            : snapshot.modMatrixContentBounds.getUnion(component.getBounds());
    };

    includeVisibleBounds(modMatrixTitle);
    includeVisibleBounds(modLfo2RateLabel);
    includeVisibleBounds(modLfo2WaveLabel);
    includeVisibleBounds(modLfo2RateDial);
    includeVisibleBounds(modLfo2WaveSelector);
    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        includeVisibleBounds(modSlotLabels[static_cast<std::size_t>(slotIndex)]);
        includeVisibleBounds(modSourceBoxes[static_cast<std::size_t>(slotIndex)]);
        includeVisibleBounds(modDestBoxes[static_cast<std::size_t>(slotIndex)]);
        includeVisibleBounds(modAmountSliders[static_cast<std::size_t>(slotIndex)]);
    }

    auto includePhysicalBounds = [&snapshot](const juce::Component& component)
    {
        if (!component.isVisible() || component.getBounds().isEmpty())
            return;

        snapshot.physicalControlsBounds = snapshot.physicalControlsBounds.isEmpty()
            ? component.getBounds()
            : snapshot.physicalControlsBounds.getUnion(component.getBounds());
    };

    includePhysicalBounds(physicalSectionTitle);
    includePhysicalBounds(physicalSectionHint);
    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
    {
        includePhysicalBounds(physicalLabels[static_cast<std::size_t>(i)]);
        includePhysicalBounds(physicalDials[static_cast<std::size_t>(i)]);
    }

    return snapshot;
}

void InstrSynthAudioProcessorEditor::setRightPanelSectionForTests(int sectionIndex)
{
    switchRightPanelSection(sectionIndex);
}
#endif

juce::StringArray InstrSynthAudioProcessorEditor::hostGetFactoryNames()
    { return proc.getFactoryPresetNames(); }
juce::Array<juce::File> InstrSynthAudioProcessorEditor::hostScanUserPresets()
    { return proc.scanUserPresets(); }
bool InstrSynthAudioProcessorEditor::hostIsUserPreset()
    { return proc.isCurrentPresetUser(); }
juce::File InstrSynthAudioProcessorEditor::hostCurrentUserFile()
    { return proc.getCurrentUserPresetFile(); }
int InstrSynthAudioProcessorEditor::hostCurrentFactoryIdx()
    { return proc.getCurrentFactoryPresetIndex(); }
void InstrSynthAudioProcessorEditor::hostApplyFactory(int idx)
    { proc.applyFactoryPreset(idx); }
void InstrSynthAudioProcessorEditor::hostLoadUser(const juce::File& f)
    { proc.loadUserPreset(f); }
bool InstrSynthAudioProcessorEditor::hostSaveUser(const juce::String& name)
    { return proc.saveUserPreset(name); }
void InstrSynthAudioProcessorEditor::hostUpdateUser(const juce::File& f)
    { proc.updateUserPreset(f); }
void InstrSynthAudioProcessorEditor::hostSaveFactory(int idx)
    { proc.saveFactoryPreset(idx); }
void InstrSynthAudioProcessorEditor::hostDeleteUser(const juce::File& f)
    { proc.deleteUserPreset(f); }

juce::File InstrSynthAudioProcessorEditor::hostGetUserPresetsDir()
    { return InstrSynthAudioProcessor::getUserPresetsDirectory(proc.getSelectedInstrumentIndex()); }

juce::File InstrSynthAudioProcessorEditor::hostGetUserPresetsDirForIndex(int instrumentIndex)
    { return InstrSynthAudioProcessor::getUserPresetsDirectory(instrumentIndex); }

juce::String InstrSynthAudioProcessorEditor::hostPresetInstrumentAttr() const
    { return "inst"; }

InstrSynthAudioProcessorEditor::InstrSynthAudioProcessorEditor(
    InstrSynthAudioProcessor& processor)
    : CommonSynthEditor(processor, processor.getAPVTS(), processor.getKeyboardState(),
                        juce::Colour(0xff42C8CC), 36, 84, 38.0f)
    , proc(processor)
{
    familySelectorLbl.setText("INSTRUMENT TYPE", juce::dontSendNotification);
    modelSelectorLbl.setText("MODEL", juce::dontSendNotification);

    familySelector.addItem("STRINGS",     1);
    familySelector.addItem("WINDS",       2);
    familySelector.addItem("PERCUSSION",  3);
    familySelector.addItem("CONCEPTUAL",  4);

    familySelector.onChange = [this] {
        const int fi = juce::jlimit(0, mis::kNumFamilies - 1,
                                    familySelector.getSelectedId() - 1);
        activeFamilyIndex = fi;
        rebuildModelSelectorForFamily(activeFamilyIndex);
        const int selectedId = modelSelector.getSelectedId();
        if (selectedId > 0) instrSelector.setSelectedId(selectedId);
    };

    modelSelector.onChange = [this] {
        const int selectedId = modelSelector.getSelectedId();
        if (selectedId > 0) instrSelector.setSelectedId(selectedId);
    };

    static const char* kFamilyNames[] = {
        "STRINGS", "WINDS", "PERCUSSION", "CONCEPTUAL" };
    for (int f = 0; f < mis::kNumFamilies; ++f) {
        familyTabs[(size_t)f].configure(f, kFamilyNames[f], familyColour(f));
        familyTabs[(size_t)f].onClicked = [this](int idx) { familySelector.setSelectedId(idx + 1); };
        familyTabs[(size_t)f].setVisible(false);
        addChildComponent(familyTabs[(size_t)f]);
    }

    instrSelector.setVisible(false);
    addChildComponent(instrSelector);
    for (int i = 0; i < mis::kNumInstruments; ++i)
        instrSelector.addItem(mis::getInstrumentName(i), i + 1);
    selInstrAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(), "selected_instrument", instrSelector);
    instrSelector.onChange = [this] {
        rebuildInstrAttachments(); syncSelectionUiFromInstr(); };

    for (int i = 0; i < mis::kNumInstruments; ++i) {
        auto& card = presetCards[(size_t)i];
        card.configure(i, mis::getInstrumentName(i), instrCatColour(i));
        card.onClicked = [this](int idx) { instrSelector.setSelectedId(idx + 1); };
        addChildComponent(card);
    }

    for (int i = 0; i < kEnvN; ++i) {
        auto si = (size_t)i;
        switch (i)
        {
            case 0:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
                setupDial(envDials[si], accent_);
                break;
            case 1:
                setupDial(envDials[si], accent_);
                break;
            case 2:
            case 3:
            case 5:
                setupDial(envDials[si], accent_);
                break;
            case 10:
                setupDial(envDials[si], accent_);
                break;
            case 11:
                configureCutoffDial(envDials[si], accent_);
                break;
            case 13:
                setupDial(envDials[si], accent_);
                break;
            default:
                setupDial(envDials[si], accent_);
                break;
        }
        addAndMakeVisible(envDials[si]);
        envLabels[si].setText(kEnvCtrls[si].label, juce::dontSendNotification);
        envLabels[si].setJustificationType(juce::Justification::centred);
        envLabels[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
        envLabels[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
        addAndMakeVisible(envLabels[si]);
    }
    envVisual.setAccent(accent_);
    envVisual.bindAdsr(&envDials[2], &envDials[3], &envDials[4], &envDials[5]);
    addAndMakeVisible(envVisual);

    lfoVisual.setAccent(accent_);
    lfoVisual.setTitle("LFO MOD");
    setupSmallDial(lfoRateDial, accent_);
    setupSmallDial(lfoDepthDial, accent_);
    addChildComponent(lfoRateDial);
    addChildComponent(lfoDepthDial);
    addChildComponent(lfoWaveSelector);
    lfoWaveSelector.addItem("SINE", 1);
    lfoWaveSelector.addItem("TRI", 2);
    lfoWaveSelector.addItem("SAW", 3);
    lfoWaveSelector.addItem("SQR", 4);
    lfoRateAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "lfo_rate", lfoRateDial);
    lfoDepthAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "lfo_depth", lfoDepthDial);
    lfoWaveAtt  = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "lfo_wave", lfoWaveSelector);
    lfoVisual.bindRateDepth(&lfoRateDial, &lfoDepthDial);
    lfoVisual.setWaveformIndex(juce::jmax(0, lfoWaveSelector.getSelectedId() - 1));
    lfoWaveSelector.onChange = [this] {
        lfoVisual.setWaveformIndex(juce::jmax(0, lfoWaveSelector.getSelectedId() - 1));
    };
    lfoVisual.onWaveformChanged = [this](int wi) {
        lfoWaveSelector.setSelectedId(wi + 1, juce::sendNotificationSync);
    };
    addAndMakeVisible(lfoVisual);

    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
    {
        auto si = static_cast<std::size_t>(i);
        setupSmallDial(physicalDials[si], accent_);
        addAndMakeVisible(physicalDials[si]);
        physicalLabels[si].setJustificationType(juce::Justification::centred);
        physicalLabels[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f)));
        physicalLabels[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.88f));
        addAndMakeVisible(physicalLabels[si]);
    }

    for (int i = 0; i < kMacroTotal; ++i) {
        auto si = (size_t)i;
        macroAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), kMacroCtrls[si].paramId, macroDials[si]);
        setupDial(macroDials[si], accent_);
        if (i < kMacroVisible) {
            addAndMakeVisible(macroDials[si]);
            macroLbls[si].setText(kMacroCtrls[si].label, juce::dontSendNotification);
            macroLbls[si].setJustificationType(juce::Justification::centred);
            macroLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
            macroLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
            addAndMakeVisible(macroLbls[si]);
        } else addChildComponent(macroDials[si]);
    }

    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.configure(sectionIndex, kRightPanelSectionLabels[sectionIndex], accent_);
        tab.setSelected(sectionIndex == activeRightPanelSection);
        tab.onClicked = [this](int idx) { switchRightPanelSection(idx); };
        addAndMakeVisible(tab);
    }

    for (int i = 0; i < kFxN; ++i) {
        auto si = (size_t)i;
        fxAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), kFxCtrls[si].paramId, fxDials[si]);
        setupSmallDial(fxDials[si], accent_);
        addChildComponent(fxDials[si]);
        fxLbls[si].setText(kFxCtrls[si].label, juce::dontSendNotification);
        fxLbls[si].setJustificationType(juce::Justification::centred);
        fxLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        fxLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
        addChildComponent(fxLbls[si]);
    }

    for (int t = 0; t < kFxTabs; ++t) {
        auto& item = fxRackItems[(size_t)t];
        item.configure(t, kFxTabNames[t], kFxRackSummaries[t], accent_);
        item.onClicked = [this](int ti) { switchEffectTab(ti); };
        addAndMakeVisible(item);
    }

    for (int t = 0; t < kFxTabs; ++t)
    {
        auto& btn = fxBypassBtns[(size_t)t];
        btn.setButtonText("ON");
        btn.setClickingTogglesState(true);
        btn.setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(btn);
        fxBypassAtts[(size_t)t] = std::make_unique<BtnAttach>(
            proc.getAPVTS(), kFxBypassParamIds[t], btn);
        btn.onClick = [this] { syncFxRackState(); };
    }

    fxDetailTitle.setJustificationType(juce::Justification::centredLeft);
    fxDetailTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f).withStyle("Bold")));
    fxDetailTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(fxDetailTitle);

    fxUnavailableLbl.setText("Not available for this instrument", juce::dontSendNotification);
    fxUnavailableLbl.setJustificationType(juce::Justification::centred);
    fxUnavailableLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    fxUnavailableLbl.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.55f));
    addChildComponent(fxUnavailableLbl);

    physicalSectionTitle.setText("Instrument Response", juce::dontSendNotification);
    physicalSectionTitle.setJustificationType(juce::Justification::centredLeft);
    physicalSectionTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.2f).withStyle("Bold")));
    physicalSectionTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(physicalSectionTitle);

    physicalSectionHint.setText("Model-specific controls keep the Rare instrument identity.", juce::dontSendNotification);
    physicalSectionHint.setJustificationType(juce::Justification::centredLeft);
    physicalSectionHint.setFont(juce::Font(juce::FontOptions{}.withHeight(10.6f)));
    physicalSectionHint.setColour(juce::Label::textColourId, synthcol::textDim);
    addAndMakeVisible(physicalSectionHint);

    modMatrixTitle.setText("MOD MATRIX", juce::dontSendNotification);
    modMatrixTitle.setJustificationType(juce::Justification::centredLeft);
    modMatrixTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f).withStyle("Bold")));
    modMatrixTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addChildComponent(modMatrixTitle);

    modLfo2RateLabel.setText("LFO2 RATE", juce::dontSendNotification);
    modLfo2RateLabel.setJustificationType(juce::Justification::centredLeft);
    modLfo2RateLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
    modLfo2RateLabel.setColour(juce::Label::textColourId, synthcol::textSec);
    modLfo2WaveLabel.setText("LFO2 WAVE", juce::dontSendNotification);
    modLfo2WaveLabel.setJustificationType(juce::Justification::centredLeft);
    modLfo2WaveLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
    modLfo2WaveLabel.setColour(juce::Label::textColourId, synthcol::textSec);
    setupSmallDial(modLfo2RateDial, accent_);
    modLfo2RateDial.setRange(0.05, 12.0, 0.01);
    modLfo2RateDial.textFromValueFunction = [](double v)
    {
        return juce::String(v, v < 10.0 ? 2 : 1) + " Hz";
    };
    modLfo2RateDial.valueFromTextFunction = [](const juce::String& text)
    {
        return juce::jlimit(0.05, 12.0, text.retainCharacters("0123456789.").getDoubleValue());
    };
    modLfo2RateDial.onValueChange = [this]
    {
        proc.setModMatrixLfo2Rate(static_cast<float>(modLfo2RateDial.getValue()));
    };
    modLfo2WaveSelector.addItem("SINE", 1);
    modLfo2WaveSelector.addItem("TRI", 2);
    modLfo2WaveSelector.addItem("SAW", 3);
    modLfo2WaveSelector.addItem("SQR", 4);
    modLfo2WaveSelector.onChange = [this]
    {
        proc.setModMatrixLfo2Wave(juce::jmax(0, modLfo2WaveSelector.getSelectedId() - 1));
    };
    addChildComponent(modLfo2RateLabel);
    addChildComponent(modLfo2WaveLabel);
    addChildComponent(modLfo2RateDial);
    addChildComponent(modLfo2WaveSelector);

    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        auto si = static_cast<std::size_t>(slotIndex);
        auto& label = modSlotLabels[si];
        auto& src   = modSourceBoxes[si];
        auto& dst   = modDestBoxes[si];
        auto& amt   = modAmountSliders[si];

        label.setText("S" + juce::String(slotIndex + 1), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
        label.setColour(juce::Label::textColourId, synthcol::textSec);
        addChildComponent(label);

        for (int sourceIndex = 0; sourceIndex < modmatrix::kSourceCount; ++sourceIndex)
            src.addItem(modmatrix::getSourceName(static_cast<modmatrix::Source>(sourceIndex)), sourceIndex + 1);
        for (int destIndex = 0; destIndex < modmatrix::kDestCount; ++destIndex)
            dst.addItem(modmatrix::getDestinationName(static_cast<modmatrix::Destination>(destIndex)), destIndex + 1);

        src.onChange = [this, slotIndex, &src, &dst, &amt]
        {
            proc.setModMatrixSlot(slotIndex,
                                  static_cast<modmatrix::Source>(juce::jmax(0, src.getSelectedId() - 1)),
                                  static_cast<modmatrix::Destination>(juce::jmax(0, dst.getSelectedId() - 1)),
                                  static_cast<float>(amt.getValue()));
        };
        dst.onChange = [this, slotIndex, &src, &dst, &amt]
        {
            proc.setModMatrixSlot(slotIndex,
                                  static_cast<modmatrix::Source>(juce::jmax(0, src.getSelectedId() - 1)),
                                  static_cast<modmatrix::Destination>(juce::jmax(0, dst.getSelectedId() - 1)),
                                  static_cast<float>(amt.getValue()));
        };

        setupSmallDial(amt, accent_);
        amt.setRange(-1.0, 1.0, 0.01);
        amt.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        amt.setDoubleClickReturnValue(true, 0.0);
        amt.setTooltip("Mod amount");
        amt.textFromValueFunction = [](double v) { return formatSignedPercent(v); };
        amt.valueFromTextFunction = parseSignedPercentText;
        amt.onValueChange = [this, slotIndex, &src, &dst, &amt]
        {
            proc.setModMatrixSlot(slotIndex,
                                  static_cast<modmatrix::Source>(juce::jmax(0, src.getSelectedId() - 1)),
                                  static_cast<modmatrix::Destination>(juce::jmax(0, dst.getSelectedId() - 1)),
                                  static_cast<float>(amt.getValue()));
            amt.setTooltip("Mod amount: " + formatSignedPercent(amt.getValue()));
        };

        addChildComponent(src);
        addChildComponent(dst);
        addChildComponent(amt);
    }

    syncAdvancedModUi();

    rebuildInstrAttachments();
    syncSelectionUiFromInstr();
    syncFxAvailability();
    switchEffectTab(0);

    if (auto rareBackground = loadNamedBinaryImage("fond_rare.png"); rareBackground.isValid())
        backgroundImage_ = rareBackground;
    else
        backgroundImage_ = juce::ImageCache::getFromMemory(
            BinaryData::fond_instr_png, BinaryData::fond_instr_pngSize);

    if (auto headerLogo = loadNamedBinaryImage("logo_instr.png"); headerLogo.isValid())
        setHeaderLogo(headerLogo);

    applyInstrumentTheme(selectedInstrFromParam());

    // ── Tooltip button ─────────────────────────────────────────────────
    tooltipModeBtn.setButtonText("TIP: SHORT");
    tooltipModeBtn.onClick = [this] { cycleTooltipMode(); };
    addAndMakeVisible(tooltipModeBtn);

    // ── MIDI CC page label ─────────────────────────────────────────────
    midiCCPageLabel.setText("CC: ---", juce::dontSendNotification);
    midiCCPageLabel.setJustificationType(juce::Justification::centredLeft);
    midiCCPageLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.2f).withStyle("Bold")));
    addAndMakeVisible(midiCCPageLabel);

    outputGainLabel.setText("0.0 dB", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centredRight);
    outputGainLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    outputGainLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    addAndMakeVisible(outputGainLabel);

    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);
    presetMetaLabel.setMinimumHorizontalScale(0.68f);
    presetMetaLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    presetMetaLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.2f)));
    addAndMakeVisible(presetMetaLabel);

    initCommon();
    if (keyboard != nullptr)
        keyboard->setScrollButtonsVisible(false);

    presetSearch.setTextToShowWhenEmpty("Search...", synthcol::textDim);
    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, accent_.withAlpha(0.50f));
    presetSearch.setColour(juce::TextEditor::highlightColourId, accent_.withAlpha(0.20f));
    presetSearch.setColour(juce::TextEditor::outlineColourId, accent_.withAlpha(0.22f));

    applyValueFormatter(envDials[2], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[3], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[4], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[5], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[0], [this](double value) { return formatPercentFromNormalised(envDials[0], value); });
    applyValueFormatter(envDials[1], [](double value) { return formatSignedValue(value, " st", 1); });
    applyValueFormatter(envDials[6], [this](double value) { return formatPercentFromNormalised(envDials[6], value); });
    applyValueFormatter(envDials[7], [this](double value) { return formatPercentFromNormalised(envDials[7], value); });
    applyValueFormatter(envDials[8], [this](double value) { return formatPercentFromNormalised(envDials[8], value); });
    applyValueFormatter(envDials[9], [this](double value) { return formatPercentFromNormalised(envDials[9], value); });
    applyValueFormatter(envDials[10], [this](double value) { return formatPercentFromNormalised(envDials[10], value); });
    applyValueFormatter(envDials[12], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[13], [this](double value) { return formatPanDisplay(envDials[13], value); });
    applyValueFormatter(lfoRateDial, [](double value) { return juce::String(value, 2) + " Hz"; });
    applyValueFormatter(lfoDepthDial, [this](double value) { return formatPercentFromNormalised(lfoDepthDial, value); });
    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
        applyValueFormatter(physicalDials[(size_t)i], [this, i](double value)
        {
            return formatPercentFromNormalised(physicalDials[(size_t)i], value);
        });
    for (int i = 0; i < kMacroTotal; ++i)
        applyValueFormatter(macroDials[(size_t)i], [this, i](double value) { return formatPercentFromNormalised(macroDials[(size_t)i], value); });
    for (int i = 0; i < kFxN; ++i)
        applyValueFormatter(fxDials[(size_t)i], [](double value) { return juce::String(value, 2); });

    applyTooltips();
    updatePresetMetadataSummary();
    updateOutputGainUi();

    startTimerHz(30);
    setResizable(true, true);
    setResizeLimits(960, 600, 2560, 1600);
    setSize(lay::W, lay::H);
}

void InstrSynthAudioProcessorEditor::timerCallback() {
    rebuildInstrAttachments();
    syncSelectionUiFromInstr();
    syncFxAvailability();
    syncPresetBox();
    updatePresetMetadataSummary();
    updateOutputGainUi();

    if (activeRightPanelSection == 2)
        syncAdvancedModUi();

    const int page = proc.getMidiCCPage();
    if (page != cachedMidiCCPage)
    {
        cachedMidiCCPage = page;
        midiCCPageLabel.setText(juce::String("CC: ") + InstrSynthAudioProcessor::getCCPageName(page),
                                juce::dontSendNotification);
    }

    repaint();
}

void InstrSynthAudioProcessorEditor::paint(juce::Graphics& g) {
    paintBackground(g);
    const auto layout = computeLayoutMetrics(getWidth(), getHeight());
    const auto headerZones = computeHeaderZones(layout.headerH);
    const auto headerRect = juce::Rectangle<float>(static_cast<float>(layout.contentX),
                                                   static_cast<float>(layout.outerMargin),
                                                   static_cast<float>(layout.contentW),
                                                   static_cast<float>(layout.headerH));
    const auto selectorRect = juce::Rectangle<float>(static_cast<float>(layout.contentX),
                                                     static_cast<float>(layout.outerMargin + layout.headerH + 8),
                                                     static_cast<float>(layout.contentW),
                                                     static_cast<float>(layout.selectorH - 8));
    const auto col1Rect = juce::Rectangle<float>(static_cast<float>(layout.col1X),
                                                 static_cast<float>(layout.bodyY),
                                                 static_cast<float>(layout.colW),
                                                 static_cast<float>(layout.bodyH));
    const auto col2Rect = juce::Rectangle<float>(static_cast<float>(layout.col2X),
                                                 static_cast<float>(layout.bodyY),
                                                 static_cast<float>(layout.colW),
                                                 static_cast<float>(layout.bodyH));
    const auto col3Rect = juce::Rectangle<float>(static_cast<float>(layout.col3X),
                                                 static_cast<float>(layout.bodyY),
                                                 static_cast<float>(layout.colW),
                                                 static_cast<float>(layout.bodyH));
    const auto keyboardRect = juce::Rectangle<float>(static_cast<float>(layout.contentX),
                                                     static_cast<float>(layout.kbY),
                                                     static_cast<float>(layout.contentW),
                                                     static_cast<float>(layout.kbH));
    const int visiblePhysicalCount = static_cast<int>(physicalDials[0].isVisible())
                                   + static_cast<int>(physicalDials[1].isVisible())
                                   + static_cast<int>(physicalDials[2].isVisible());
    const auto physicalLoad = juce::jlimit(0.0f, 1.0f, visiblePhysicalCount / 3.0f);
    const auto outputLoad = static_cast<float>(juce::jmap(gainDial.getValue(), -24.0, 12.0, 0.0, 1.0));
    const int gainSize = layout.compact ? 38 : 42;
    const auto statusPrimaryRow = headerZones.statusPrimaryRow.reduced(0, 1);
    const auto statusSecondaryRow = headerZones.statusSecondaryRow.reduced(0, 1);

    paintHeader(g, layout.headerH);
    glazeRareChrome(g, headerRect.reduced(1.5f, 1.5f), accent_, 13.0f, 1.0f);

    g.setColour(accent_.withAlpha(0.04f));
    g.fillRoundedRectangle(statusPrimaryRow.toFloat().withWidth(juce::jmin(128.0f, statusPrimaryRow.getWidth() * 0.36f)), 7.0f);

    const int gainX = headerZones.statusZone.getRight() - gainSize - 6;
    const int meterBlockW = layout.compact ? 106 : 118;
    const int meterLeft = juce::jmax(statusSecondaryRow.getX() + 92, gainX - meterBlockW - 12);
    const int meterY = statusSecondaryRow.getCentreY() - 4;
    if (meterLeft >= statusSecondaryRow.getX() + 84)
    {
        paintMeterBar(g, { meterLeft, meterY, 52, 8 }, physicalLoad, accent_);
        paintMeterBar(g, { meterLeft + 60, meterY, 52, 8 }, outputLoad, accent_.brighter(0.22f));
        g.setColour(synthcol::textDim);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        g.drawText("P", juce::Rectangle<int>(meterLeft - 10, meterY - 2, 10, 12), juce::Justification::centredLeft);
        g.drawText("G", juce::Rectangle<int>(meterLeft + 50, meterY - 2, 10, 12), juce::Justification::centredLeft);
    }

    paintCard(g, layout.contentX, layout.outerMargin + layout.headerH + 8,
              layout.contentW, layout.selectorH - 8, "Family / Model");
    paintCard(g, layout.col1X, layout.bodyY, layout.colW, layout.bodyH, "Source / Envelope");
    paintCard(g, layout.col2X, layout.bodyY, layout.colW, layout.bodyH, "Tone Shaping");
    paintCard(g, layout.col3X, layout.bodyY, layout.colW, layout.bodyH, "Performance / Routing / FX");
    glazeRareChrome(g, selectorRect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.96f);
    glazeRareChrome(g, col1Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);
    glazeRareChrome(g, col2Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);
    glazeRareChrome(g, col3Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);

    g.setColour(accent_.withAlpha(0.12f));
    g.drawLine(static_cast<float>(layout.contentX + 18), static_cast<float>(layout.kbY - 8),
               static_cast<float>(layout.contentX + layout.contentW - 18), static_cast<float>(layout.kbY - 8),
               1.0f);
    paintKeyboardDock(g, layout.contentX, layout.kbY, layout.contentW, layout.kbH);
    glazeRareChrome(g, keyboardRect.reduced(2.0f, 2.0f), accent_, 11.0f, 1.04f);
}

void InstrSynthAudioProcessorEditor::resized() {
    const auto layout = computeLayoutMetrics(getWidth(), getHeight());
    const float gapDensity = layoutDensity(layout.compact, layout.roomy);
    const auto headerZones = computeHeaderZones(layout.headerH);
    const int ctrlH = layout.compact ? 32 : 34;
    const int gainSize = layout.compact ? 38 : 42;
    const auto presetPrimaryRow = headerZones.presetPrimaryRow.reduced(0, 1);
    const auto presetSecondaryRow = headerZones.presetSecondaryRow.reduced(0, 1);
    const auto statusPrimaryRow = headerZones.statusPrimaryRow.reduced(0, 1);
    const auto statusSecondaryRow = headerZones.statusSecondaryRow.reduced(0, 1);
    const int topRowY = presetPrimaryRow.getY() + (presetPrimaryRow.getHeight() - ctrlH) / 2;

    const int navW = layout.compact ? 24 : 26;
    const int searchW = juce::jlimit(layout.compact ? 118 : 112,
                                     layout.compact ? 150 : 150,
                                     presetPrimaryRow.getWidth() / (layout.compact ? 4 : 5));
    int x = presetPrimaryRow.getX();
    const int presetAvailableW = presetPrimaryRow.getRight() - x - searchW - navW * 2 - 16;
    const int presetW = juce::jlimit(layout.compact ? 180 : 300,
                                     layout.compact ? 300 : 520,
                                     presetAvailableW);
    presetSearch.setBounds(x, topRowY, searchW, ctrlH);   x += searchW + 8;
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH);     x += navW + 4;
    presetBox.setBounds(x, topRowY, presetW, ctrlH);      x += presetW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    const int actionBtnH = layout.compact ? 22 : 24;
    const int actionY = presetSecondaryRow.getY() + juce::jmax(0, (presetSecondaryRow.getHeight() - actionBtnH) / 2);
    const int saveW = layout.compact ? 52 : 64;
    const int saveAsW = layout.compact ? 62 : 78;
    const int deleteW = layout.compact ? 56 : 70;
    const int importW = layout.compact ? 64 : 70;
    const int btnGap = layout.compact ? 6 : 8;
    const int actionX = presetSecondaryRow.getX();
    savePresetBtn.setBounds(actionX, actionY, saveW, actionBtnH);
    saveAsPresetBtn.setBounds(savePresetBtn.getRight() + btnGap, actionY, saveAsW, actionBtnH);
    deletePresetBtn.setBounds(saveAsPresetBtn.getRight() + btnGap, actionY, deleteW, actionBtnH);
    importPresetsBtn.setBounds(deletePresetBtn.getRight() + btnGap, actionY, importW, actionBtnH);
    const int metaX = importPresetsBtn.getRight() + (layout.compact ? 8 : 12);
    const int metaW = juce::jmax(0, presetSecondaryRow.getRight() - metaX);
    presetMetaLabel.setBounds(metaX, actionY - 1, metaW, actionBtnH + 2);
    presetMetaLabel.setVisible(metaW > (layout.compact ? 150 : 110));

    const int statusBtnH = layout.compact ? 22 : 24;
    const int statusPrimaryY = statusPrimaryRow.getY() + juce::jmax(0, (statusPrimaryRow.getHeight() - statusBtnH) / 2);
    int statusX = statusPrimaryRow.getX();
    const int tipW = layout.compact ? 86 : 94;
    tooltipModeBtn.setBounds(statusX, statusPrimaryY, tipW, statusBtnH);
    statusX += tipW + 8;
    midiCCPageLabel.setBounds(statusX, statusPrimaryY,
                              juce::jmax(92, statusPrimaryRow.getRight() - statusX), statusBtnH);

    const int secondaryCentreY = statusSecondaryRow.getCentreY();
    const int gainX = headerZones.statusZone.getRight() - gainSize - 6;
    const int gainY = secondaryCentreY - gainSize / 2;
    gainDial.setBounds(gainX, gainY, gainSize, gainSize);
    const int meterBlockW = layout.compact ? 106 : 118;
    const int meterBlockX = juce::jmax(statusSecondaryRow.getX() + 92, gainX - meterBlockW - 12);
    outputGainLabel.setBounds(statusSecondaryRow.getX(), secondaryCentreY - 10,
                              juce::jmax(78, meterBlockX - statusSecondaryRow.getX() - 10), 20);

    const int selPad = layout.compact ? 12 : 14;
    const int selectorInnerX = layout.contentX + selPad;
    const int selectorInnerW = layout.contentW - selPad * 2;
    const int selectorTopY = layout.selectorY + (layout.compact ? 20 : 22);
    const int selectorRowH = layout.compact ? 22 : 24;
    const int selectorGap = layout.compact ? 10 : 12;
    const int tabsZoneW = static_cast<int>(selectorInnerW * (layout.compact ? 0.58f : 0.66f));
    const int comboZoneW = selectorInnerW - tabsZoneW - selectorGap;
    const int tabGap = interpolateGap(gapDensity, 6, 8, 10);
    const int tabW = (tabsZoneW - tabGap * (mis::kNumFamilies - 1)) / mis::kNumFamilies;
    for (int familyIndex = 0; familyIndex < mis::kNumFamilies; ++familyIndex)
        familyTabs[(size_t)familyIndex].setBounds(selectorInnerX + familyIndex * (tabW + tabGap), selectorTopY, tabW, selectorRowH);

    familySelectorLbl.setVisible(false);
    familySelectorLbl.setBounds(0, 0, 0, 0);
    familySelector.setVisible(false);
    familySelector.setBounds(0, 0, 0, 0);
    modelSelectorLbl.setVisible(false);
    modelSelectorLbl.setBounds(0, 0, 0, 0);
    modelSelector.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorTopY, comboZoneW, selectorRowH);

    const int cPad = layout.compact ? 14 : 18;
    const int knobGapX = interpolateGap(gapDensity, 8, 11, 13);
    const int knobGapY = interpolateGap(gapDensity, 7, 12, 14);
    const int knobW = (layout.colW - cPad * 2 - knobGapX * 2) / 3;
    const int lblH = layout.compact ? 12 : 14;
    const int graphTargetH = layout.compact ? 86 : (layout.roomy ? 178 : 126);
    const int knobH = juce::jlimit(layout.compact ? 42 : 58,
                                   layout.roomy ? 98 : (layout.compact ? 64 : 84),
                                   (layout.bodyH - graphTargetH - cPad * 2 - lblH * 3 - knobGapY * 3) / 3);
    const int protectedKeyboardTop = layout.kbY - (layout.compact ? 18 : 24);

    const int sourceInnerX = layout.col1X + cPad;
    const int sourceInnerW = layout.colW - cPad * 2;
    const int sourceTopY = layout.bodyY + cPad + 28;
    const int sourceBottomY = protectedKeyboardTop - 12;
    const int envH = juce::jlimit(layout.compact ? 132 : 154,
                                  layout.roomy ? 250 : 212,
                                  static_cast<int>((sourceBottomY - sourceTopY) * 0.50f));
    envVisual.setVisible(true);
    envVisual.setBounds(sourceInnerX, sourceTopY, sourceInnerW, envH);

    const int sourceControlsY = envVisual.getBottom() + (layout.compact ? 8 : 10);
    const int adsrGapX = interpolateGap(gapDensity, 6, 8, 10);
    const int adsrGapY = interpolateGap(gapDensity, 8, 10, 12);
    const int remainingH = sourceBottomY - sourceControlsY;
    const int adsrW = (sourceInnerW - adsrGapX * 3) / 4;
    const int adsrKnobH = juce::jlimit(layout.compact ? 44 : 50,
                                       layout.roomy ? 78 : 64,
                                       juce::jmax(layout.compact ? 44 : 50,
                                                  (remainingH - lblH * 4 - adsrGapY * 2) / 2));
    const int secondaryGapX = interpolateGap(gapDensity, 8, 10, 12);
    const int secondaryW = (sourceInnerW - secondaryGapX * 2) / 3;
    const int smallSourceKnobH = juce::jlimit(layout.compact ? 42 : 48,
                                              layout.roomy ? 76 : 62,
                                              juce::jmax(layout.compact ? 42 : 48,
                                                         remainingH - adsrKnobH - lblH * 2 - adsrGapY - 4));

    auto layoutSourceDial = [this, lblH](int paramIndex, int xk, int yk, int width, int height)
    {
        auto si = static_cast<std::size_t>(paramIndex);
        envLabels[si].setBounds(xk, yk, width, lblH);
        envDials[si].setBounds(xk, yk + lblH, width, height);
    };

    layoutSourceDial(2, sourceInnerX, sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(3, sourceInnerX + (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(4, sourceInnerX + 2 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(5, sourceInnerX + 3 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);

    const int sourceTailY = sourceControlsY + lblH + adsrKnobH + adsrGapY;
    layoutSourceDial(6, sourceInnerX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(0, sourceInnerX + secondaryW + secondaryGapX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(1, sourceInnerX + 2 * (secondaryW + secondaryGapX), sourceTailY, secondaryW, smallSourceKnobH);

    int toneIdx[] = { 7, 8, 9, 10, 13, 12 };
    const int col2StartY = layout.bodyY + cPad + (layout.compact ? 18 : 28);
    for (int i = 0; i < 6; ++i) {
        int row = i / 3, col = i % 3;
        int xk = layout.col2X + cPad + col * (knobW + knobGapX);
        int yk = col2StartY + row * (knobH + lblH + knobGapY);
        auto si = static_cast<std::size_t>(toneIdx[i]);
        envLabels[si].setBounds(xk, yk, knobW, lblH);
        envDials[si].setBounds(xk, yk + lblH, knobW, knobH);
    }

    const int cutoffSize = juce::jlimit(layout.compact ? 54 : 76,
                                        layout.roomy ? 124 : 112,
                                        knobH + (layout.compact ? 8 : 18));
    const int cutoffX = layout.col2X + (layout.colW - cutoffSize) / 2;
    const int cutoffY = col2StartY + 2 * (knobH + lblH + knobGapY) + (layout.compact ? 2 : 8);
    envLabels[11].setBounds(cutoffX, cutoffY, cutoffSize, lblH);
    envDials[11].setBounds(cutoffX, cutoffY + lblH, cutoffSize, cutoffSize);

    const int physicalTitleH = layout.compact ? 13 : 16;
    const int physicalHintH = layout.compact ? 0 : 13;
    const int physicalTitleGap = layout.compact ? 0 : 3;
    const int physicalHintGap = layout.compact ? 1 : 5;
    const int physicalTitleY = envDials[11].getBottom() + (layout.compact ? 4 : 10);
    physicalSectionTitle.setBounds(layout.col2X + cPad, physicalTitleY, layout.colW - cPad * 2, physicalTitleH);
    physicalSectionHint.setBounds(layout.col2X + cPad, physicalSectionTitle.getBottom() + physicalTitleGap,
                                  layout.colW - cPad * 2, physicalHintH);
    physicalSectionHint.setVisible(!layout.compact);

    const int physicalBlockY = physicalSectionHint.getBottom() + physicalHintGap;
    const int physicalBottomY = protectedKeyboardTop - (layout.compact ? 4 : 8);
    const int physicalKnobW = juce::jlimit(layout.compact ? 70 : 78,
                                           layout.roomy ? 100 : 90,
                                           juce::jmax(70, (layout.colW - cPad * 2 - 24) / 3));
    const int physicalGap = interpolateGap(gapDensity, 10, 12, 14);
    const int physicalAvailableH = juce::jmax(layout.compact ? 30 : 28, physicalBottomY - physicalBlockY - lblH);
    const int physicalKnobH = juce::jlimit(layout.compact ? 30 : 38,
                                           layout.roomy ? 70 : 58,
                                           physicalAvailableH);

    std::array<int, 3> visiblePhysicalSlots { -1, -1, -1 };
    int visiblePhysicalSlotsCount = 0;
    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
    {
        if (physicalDials[(size_t)i].isVisible())
            visiblePhysicalSlots[(size_t)visiblePhysicalSlotsCount++] = i;
        else
        {
            physicalLabels[(size_t)i].setVisible(false);
            physicalLabels[(size_t)i].setBounds(0, 0, 0, 0);
            physicalDials[(size_t)i].setVisible(false);
            physicalDials[(size_t)i].setBounds(0, 0, 0, 0);
        }
    }

    if (visiblePhysicalSlotsCount > 0)
    {
        const int totalWidth = visiblePhysicalSlotsCount * physicalKnobW + (visiblePhysicalSlotsCount - 1) * physicalGap;
        const int physicalStartX = layout.col2X + (layout.colW - totalWidth) / 2;
        for (int visualIndex = 0; visualIndex < visiblePhysicalSlotsCount; ++visualIndex)
        {
            const auto si = static_cast<std::size_t>(visiblePhysicalSlots[(size_t)visualIndex]);
            const int xk = physicalStartX + visualIndex * (physicalKnobW + physicalGap);
            physicalLabels[si].setBounds(xk, physicalBlockY, physicalKnobW, lblH);
            physicalDials[si].setBounds(xk, physicalBlockY + lblH, physicalKnobW, physicalKnobH);
        }
    }

    lfoRateDial.setVisible(false);
    lfoDepthDial.setVisible(false);
    lfoWaveSelector.setVisible(false);
    lfoRateDial.setVisible(false);
    lfoRateDial.setBounds(0, 0, 0, 0);
    lfoDepthDial.setVisible(false);
    lfoDepthDial.setBounds(0, 0, 0, 0);
    lfoWaveSelector.setVisible(false);
    lfoWaveSelector.setBounds(0, 0, 0, 0);

    const int col3StartY = layout.bodyY + cPad + 28;
    const int rightTabGap = interpolateGap(gapDensity, 6, 8, 10);
    const int rightTabH = layout.compact ? 23 : 26;
    const int rightTabW = (layout.colW - cPad * 2 - rightTabGap * (kRightPanelSections - 1)) / kRightPanelSections;
    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.setBounds(layout.col3X + cPad + sectionIndex * (rightTabW + rightTabGap),
                      col3StartY, rightTabW, rightTabH);
        tab.setSelected(sectionIndex == activeRightPanelSection);
    }

    const int sectionContentY = col3StartY + rightTabH + (layout.compact ? 12 : 14);
    const int macroGap = interpolateGap(gapDensity, 7, 9, 11);
    const int macroW = (layout.colW - cPad * 2 - macroGap * 3) / 4;
    const int macroH = juce::jlimit(layout.compact ? 44 : 50,
                                    layout.roomy ? 78 : 68,
                                    juce::jmin(macroW, knobH - (layout.compact ? 6 : 10)));
    for (int i = 0; i < kMacroVisible; ++i) {
        auto si = (size_t)i;
        const bool showMacro = activeRightPanelSection == 0;
        macroLbls[si].setVisible(showMacro);
        macroDials[si].setVisible(showMacro);
        if (showMacro) {
            const int xk = layout.col3X + cPad + i * (macroW + macroGap);
            const int yk = sectionContentY;
            macroLbls[si].setBounds(xk, yk, macroW, lblH);
            macroDials[si].setBounds(xk, yk + lblH, macroW, macroH);
        } else {
            macroLbls[si].setVisible(false);
            macroLbls[si].setBounds(0, 0, 0, 0);
            macroDials[si].setVisible(false);
            macroDials[si].setBounds(0, 0, 0, 0);
        }
    }

    const bool showMacroLfo = activeRightPanelSection == 0;
    lfoVisual.setVisible(showMacroLfo);
    if (showMacroLfo) {
        const int lfoY = sectionContentY + macroH + lblH + (layout.compact ? 14 : 16);
        const int lfoH = juce::jmax(110, protectedKeyboardTop - lfoY - 12);
        lfoVisual.setBounds(layout.col3X + cPad, lfoY, layout.colW - cPad * 2, lfoH);
    } else {
        lfoVisual.setVisible(false);
        lfoVisual.setBounds(0, 0, 0, 0);
    }

    // ── MOD MATRIX section (index 2) ────────────────────────────────
    const bool showModMatrix = activeRightPanelSection == 2;
    modMatrixTitle.setVisible(false);
    modMatrixPlaceholderLabel.setVisible(false);
    modMatrixHintLabel.setVisible(false);
    modMatrixTitle.setBounds(0, 0, 0, 0);
    modMatrixPlaceholderLabel.setBounds(0, 0, 0, 0);
    modMatrixHintLabel.setBounds(0, 0, 0, 0);
    // hide all mod matrix ui first
    for (int si = 0; si < kModSlots; ++si)
    {
        auto s = static_cast<std::size_t>(si);
        modSlotLabels[s].setVisible(false);   modSlotLabels[s].setBounds(0, 0, 0, 0);
        modSourceBoxes[s].setVisible(false);  modSourceBoxes[s].setBounds(0, 0, 0, 0);
        modDestBoxes[s].setVisible(false);    modDestBoxes[s].setBounds(0, 0, 0, 0);
        modAmountSliders[s].setVisible(false); modAmountSliders[s].setBounds(0, 0, 0, 0);
    }
    modLfo2RateLabel.setVisible(false);   modLfo2RateLabel.setBounds(0, 0, 0, 0);
    modLfo2WaveLabel.setVisible(false);   modLfo2WaveLabel.setBounds(0, 0, 0, 0);
    modLfo2RateDial.setVisible(false);    modLfo2RateDial.setBounds(0, 0, 0, 0);
    modLfo2WaveSelector.setVisible(false); modLfo2WaveSelector.setBounds(0, 0, 0, 0);

    if (showModMatrix)
    {
        const int modAreaX = layout.col3X + cPad;
        const int modAreaW = layout.colW - cPad * 2;
        const int matrixY = sectionContentY;
        const int controlGap = layout.compact ? 5 : 6;
        const int modAreaH = juce::jmax(80, protectedKeyboardTop - matrixY - 10);
        const int footerReserve = layout.compact ? 86 : 96;
        const int rowGap = layout.compact ? 4 : 5;
        const int matrixRowsH = juce::jmax(96, modAreaH - footerReserve);
        const int rowH = juce::jlimit(20, 28,
                                      (matrixRowsH - rowGap * juce::jmax(0, kModSlots - 1)) / kModSlots);
        const int amountSize = juce::jlimit(layout.compact ? 22 : 24,
                                            layout.compact ? 28 : 30, rowH + 2);
        const int labelW = 18;
        const int comboAreaW = modAreaW - labelW - amountSize - controlGap * 3;
        const int sourceW = juce::jmax(74, comboAreaW / 2);
        const int destW = juce::jmax(74, comboAreaW - sourceW);
        const int comboH = juce::jlimit(20, 22, rowH);
        const int maxRowBottom = matrixY + matrixRowsH;
        int visibleSlotCount = 0;
        for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
        {
            auto si = static_cast<std::size_t>(slotIndex);
            const int rowY = matrixY + slotIndex * (rowH + rowGap);
            if (rowY + rowH > maxRowBottom)
                break;
            const int comboY = rowY + juce::jmax(0, (rowH - comboH) / 2);
            const int amountY = rowY + juce::jmax(0, (rowH - amountSize) / 2);
            modSlotLabels[si].setBounds(modAreaX, rowY + (rowH - 16) / 2, labelW, 16);
            modSourceBoxes[si].setBounds(modAreaX + labelW + controlGap, comboY, sourceW, comboH);
            modDestBoxes[si].setBounds(modAreaX + labelW + controlGap * 2 + sourceW, comboY, destW, comboH);
            modAmountSliders[si].setBounds(modAreaX + modAreaW - amountSize, amountY, amountSize, amountSize);
            ++visibleSlotCount;
        }
        for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
        {
            auto si = static_cast<std::size_t>(slotIndex);
            const bool vis = slotIndex < visibleSlotCount;
            modSlotLabels[si].setVisible(vis);
            modSourceBoxes[si].setVisible(vis);
            modDestBoxes[si].setVisible(vis);
            modAmountSliders[si].setVisible(vis);
        }
        // LFO2 footer below slots
        const int footerY = matrixY + visibleSlotCount * (rowH + rowGap) + 10;
        if (footerY + 40 <= protectedKeyboardTop)
        {
            const int footerLabelW = modAreaW / 2 - controlGap;
            modLfo2RateLabel.setBounds(modAreaX, footerY, footerLabelW, 14);
            modLfo2WaveLabel.setBounds(modAreaX + footerLabelW + controlGap, footerY, footerLabelW, 14);
            modLfo2RateDial.setBounds(modAreaX, footerY + 12, footerLabelW, juce::jmin(70, modAreaW / 2));
            modLfo2WaveSelector.setBounds(modAreaX + footerLabelW + controlGap, footerY + 18, footerLabelW, 22);
            modLfo2RateLabel.setVisible(true);   modLfo2WaveLabel.setVisible(true);
            modLfo2RateDial.setVisible(true);     modLfo2WaveSelector.setVisible(true);
        }
    }

    const int instrIdx = selectedInstrFromParam();
    const int fxAreaY = sectionContentY;
    const int fxAreaBottom = protectedKeyboardTop - 12;
    const int fxAreaH = juce::jmax(layout.compact ? 156 : 176, fxAreaBottom - fxAreaY);
    constexpr int kBypassW = 34;
    const int kRackGap = layout.compact ? 14 : 16;
    constexpr int kRackRowGap = 4;
    const int minDetailW = layout.compact ? 158 : 176;
    const int rackPreferredW = juce::jlimit(layout.compact ? 100 : 108,
                                            layout.roomy ? 148 : 132,
                                            layout.colW / 3);
    const int rackMaxW = juce::jmax(88, layout.colW - cPad * 2 - kRackGap - minDetailW);
    const int rackTotalW = juce::jmin(rackPreferredW, rackMaxW);
    const int rackItemW = rackTotalW - kBypassW - 6;
    const int rackRowH = juce::jlimit(layout.compact ? 20 : 22,
                                      layout.roomy ? 32 : 28,
                                      (fxAreaH - kRackRowGap * (kFxTabs - 1)) / kFxTabs);

    const int rackStartY = fxAreaY + 26;
    int visibleRow = 0;
    for (int t = 0; t < kFxTabs; ++t)
    {
        const bool available = activeRightPanelSection == 1 && isFxTabAvailable(t, instrIdx);
        fxRackItems[(size_t)t].setVisible(available);
        fxBypassBtns[(size_t)t].setVisible(available);
        if (!available) {
            fxRackItems[(size_t)t].setVisible(false);
            fxRackItems[(size_t)t].setBounds(0, 0, 0, 0);
            fxBypassBtns[(size_t)t].setVisible(false);
            fxBypassBtns[(size_t)t].setBounds(0, 0, 0, 0);
            continue;
        }
        const int rowY = rackStartY + visibleRow * (rackRowH + kRackRowGap);
        fxRackItems[(size_t)t].setBounds(layout.col3X + cPad, rowY, rackItemW, rackRowH);
        fxBypassBtns[(size_t)t].setBounds(layout.col3X + cPad + rackItemW + 6, rowY + (rackRowH - 18) / 2, kBypassW, 18);
        ++visibleRow;
    }

    const int detailX = layout.col3X + cPad + rackTotalW + kRackGap;
    const int detailW = layout.colW - cPad * 2 - rackTotalW - kRackGap;
    const int currentFxTab = (activeFxTab >= 0 && activeFxTab < kFxTabs) ? activeFxTab : firstAvailableFxTab(instrIdx);
    const bool showFxUnavailable = activeRightPanelSection == 1 && !isFxTabAvailable(currentFxTab, instrIdx);
    fxDetailTitle.setVisible(activeRightPanelSection == 1);
    fxUnavailableLbl.setVisible(showFxUnavailable);
    fxDetailTitle.setBounds(detailX, fxAreaY, detailW, 16);
    fxUnavailableLbl.setBounds(detailX, fxAreaY + 22, detailW, 20);

    for (int i = 0; i < kFxN; ++i) {
        fxDials[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setBounds(0, 0, 0, 0);
        fxLbls[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setBounds(0, 0, 0, 0);
    }

    int visibleCount = 0;
    for (int k = 0; k < kFxPerTab; ++k)
        if (currentFxTab >= 0 && kFxTabMap[currentFxTab][k] >= 0)
            ++visibleCount;

    const bool denseFxTab = currentFxTab == 3 || currentFxTab == 4;
    int detailCols = visibleCount >= 2 ? 2 : 1;
    if (!denseFxTab && detailW > (layout.compact ? 304 : 324) && visibleCount >= 5)
        detailCols = 3;
    else if (denseFxTab && detailW > 380 && visibleCount >= 6)
        detailCols = 3;

    const int detailRows = juce::jmax(1, (visibleCount + detailCols - 1) / detailCols);
    const int detailGap = layout.compact ? 8 : 10;
    const int detailGridY = fxAreaY + (showFxUnavailable ? 50 : 28);
    const int detailAvailH = juce::jmax(88, fxAreaBottom - detailGridY);
    const int detailCellH = (detailAvailH - detailGap * (detailRows - 1)) / detailRows;
    const int detailKnobW = (detailW - detailGap * (detailCols - 1)) / detailCols;
    const int detailKnobH = juce::jlimit(denseFxTab ? (layout.compact ? 44 : 52)
                                                    : (layout.compact ? 40 : 46),
                                         layout.roomy ? 124 : 100,
                                         juce::jmin(detailKnobW, detailCellH - lblH));
    const int detailRowStride = lblH + detailKnobH + detailGap;
    const int detailGridTotalH = detailRows * (lblH + detailKnobH) + (detailRows - 1) * detailGap;
    const int detailStartY = detailGridY + juce::jmax(0, (detailAvailH - detailGridTotalH) / 2);

    int visibleIndex = 0;
    for (int k = 0; k < kFxPerTab; ++k) {
        const int fi = currentFxTab >= 0 ? kFxTabMap[currentFxTab][k] : -1;
        if (fi < 0)
            continue;

        auto si = (size_t)fi;
        const int row = visibleIndex / detailCols;
        const int col = visibleIndex % detailCols;
        const int xk = detailX + col * (detailKnobW + detailGap);
        const int yk = detailStartY + row * detailRowStride;
        if (activeRightPanelSection == 1) {
            fxLbls[si].setBounds(xk, yk, detailKnobW, lblH);
            fxDials[si].setBounds(xk, yk + lblH, detailKnobW, detailKnobH);
            fxLbls[si].setVisible(true);
            fxDials[si].setVisible(true);
        }
        ++visibleIndex;
    }

    const int keyboardInsetLeft = 60;
    const int keyboardInsetTop = layout.compact ? 6 : 8;
    const int keyboardH = juce::jmax(36, layout.kbH - keyboardInsetTop * 2);
    const int keyboardCenterY = layout.kbY + keyboardInsetTop + keyboardH / 2;
    keyboard->setBounds(layout.contentX + keyboardInsetLeft, layout.kbY + keyboardInsetTop,
                        layout.contentW - keyboardInsetLeft - 4, keyboardH);
    octaveDownBtn.setBounds(layout.contentX + 10, keyboardCenterY - 14, 20, 26);
    octaveUpBtn.setBounds(layout.contentX + 34, keyboardCenterY - 14, 20, 26);

    auto applyLbl = [layout](juce::Label& l) {
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(layout.compact ? 11.0f : 12.0f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, synthcol::textSec);
    };
    for (auto& l : envLabels) applyLbl(l);
    for (auto& l : physicalLabels) applyLbl(l);
    for (auto& l : macroLbls) applyLbl(l);
    for (auto& l : fxLbls)    applyLbl(l);
}

void InstrSynthAudioProcessorEditor::switchEffectTab(int tabIndex) {
    const int instrIdx = selectedInstrFromParam();
    if (!isFxTabAvailable(tabIndex, instrIdx))
        tabIndex = firstAvailableFxTab(instrIdx);

    activeFxTab = tabIndex;
    fxDetailTitle.setText(juce::String("FX Detail: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);

    for (int i=0; i<kFxN; ++i) {
        fxDials[(size_t)i].setVisible(false);
        fxLbls [(size_t)i].setVisible(false);
    }
    for (int k=0; k<kFxPerTab; ++k) {
        int fi=kFxTabMap[activeFxTab][k];
        if (fi < 0)
            continue;
        auto si=(size_t)fi;
        fxDials[si].setVisible(true);
        fxLbls [si].setVisible(true);
        fxLbls [si].setText(kFxCtrls[si].label, juce::dontSendNotification);
    }

    syncFxRackState();
    resized();
}

void InstrSynthAudioProcessorEditor::switchRightPanelSection(int sectionIndex)
{
    if (sectionIndex < 0 || sectionIndex >= kRightPanelSections || activeRightPanelSection == sectionIndex)
        return;

    activeRightPanelSection = sectionIndex;
    resized();
    repaint();
}

void InstrSynthAudioProcessorEditor::syncAdvancedModUi()
{
    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        auto si = static_cast<std::size_t>(slotIndex);
        const auto slot = proc.getModMatrixSlot(slotIndex);
        auto& sourceBox    = modSourceBoxes[si];
        auto& destBox      = modDestBoxes[si];
        auto& amountSlider = modAmountSliders[si];

        const auto sourceId = static_cast<int>(slot.source) + 1;
        const auto destId   = static_cast<int>(slot.destination) + 1;

        if (sourceBox.getSelectedId() != sourceId)
            sourceBox.setSelectedId(sourceId, juce::dontSendNotification);
        if (destBox.getSelectedId() != destId)
            destBox.setSelectedId(destId, juce::dontSendNotification);
        if (std::abs(amountSlider.getValue() - static_cast<double>(slot.amount)) > 1.0e-6)
            amountSlider.setValue(static_cast<double>(slot.amount), juce::dontSendNotification);

        amountSlider.setTooltip("Mod amount: " + formatSignedPercent(static_cast<double>(slot.amount)));
    }

    const auto lfo2Rate   = proc.getModMatrixLfo2Rate();
    const auto lfo2WaveId = proc.getModMatrixLfo2Wave() + 1;
    if (std::abs(modLfo2RateDial.getValue() - static_cast<double>(lfo2Rate)) > 1.0e-6)
        modLfo2RateDial.setValue(static_cast<double>(lfo2Rate), juce::dontSendNotification);
    if (modLfo2WaveSelector.getSelectedId() != lfo2WaveId)
        modLfo2WaveSelector.setSelectedId(lfo2WaveId, juce::dontSendNotification);
}

void InstrSynthAudioProcessorEditor::syncFxRackState()
{
    const int instrIdx = selectedInstrFromParam();
    for (int t = 0; t < kFxTabs; ++t)
    {
        if (!isFxTabAvailable(t, instrIdx)) continue; // item est masqué dans resized()
        auto& rackItem = fxRackItems[(size_t)t];
        rackItem.setSelected(t == activeFxTab);
        rackItem.setEnabledState(fxBypassBtns[(size_t)t].getToggleState());
        rackItem.setTooltip(juce::String(kFxRackSummaries[t]));
    }
}

void InstrSynthAudioProcessorEditor::syncInstrumentUiProfile()
{
    const int instrIdx = cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam();
    const auto& profile = getUiProfileForInstrument(instrIdx);

    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
    {
        auto si = static_cast<std::size_t>(i);
        physicalAttach[si].reset();

        const auto& control = profile[si];
        const bool visible = control.suffix != nullptr;
        physicalDials[si].setVisible(visible);
        physicalLabels[si].setVisible(visible);

        if (!visible)
        {
            physicalLabels[si].setText(juce::String(), juce::dontSendNotification);
            physicalDials[si].setTooltip(juce::String());
            continue;
        }

        physicalLabels[si].setText(control.label, juce::dontSendNotification);
        physicalAttach[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(),
            InstrSynthAudioProcessor::makeInstParamId(instrIdx, control.suffix),
            physicalDials[si]);

        if (tooltipMode == TooltipMode::Off)
            physicalDials[si].setTooltip(juce::String());
        else
            physicalDials[si].setTooltip(synthui::tooltipForMode(
                control,
                tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                   : synthui::TooltipMode::Short));
    }
}

void InstrSynthAudioProcessorEditor::applyInstrumentTheme(int instrIndex)
{
    const auto catC = instrCatColour(instrIndex);
    const auto controlText = catC.brighter(0.18f);
    const auto panelBg = juce::Colour(0xff1A1B20).interpolatedWith(catC.withAlpha(0.14f), 0.12f);
    const auto pillBg = juce::Colour(0xff14161B).withAlpha(0.84f);
    const auto headerTint = juce::Colour(0xff363940);
    const auto panelBaseTint = juce::Colour(0xff171B21);
    const auto panelCavityTint = juce::Colour(0xff0F1318);
    const auto panelHeaderTint = juce::Colour(0xff232730);
    const auto keyboardTint = juce::Colour(0xff0E1116);

    setAccentTheme(catC);
    setChromePalette(headerTint, panelBaseTint, panelCavityTint, panelHeaderTint, keyboardTint);
    envVisual.setAccent(catC);
    lfoVisual.setAccent(catC);

    for (auto& dial : envDials)
        applyKnobPalette(dial, catC);

    for (auto& dial : physicalDials)
        applyKnobPalette(dial, catC);

    for (auto& dial : macroDials)
        applyKnobPalette(dial, catC);

    for (auto& dial : fxDials)
        applyKnobPalette(dial, catC);

    applyKnobPalette(lfoRateDial, catC);
    applyKnobPalette(lfoDepthDial, catC);
    applyKnobPalette(gainDial, catC);

    for (auto* combo : { &modelSelector, &lfoWaveSelector })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, panelBg);
        combo->setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    }

    tooltipModeBtn.setColour(juce::TextButton::buttonColourId, panelBg);
    tooltipModeBtn.setColour(juce::TextButton::buttonOnColourId, panelBg.brighter(0.05f));
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId, controlText);
    tooltipModeBtn.setColour(juce::TextButton::textColourOnId, controlText.brighter(0.08f));

    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, catC.withAlpha(0.50f));
    presetSearch.setColour(juce::TextEditor::highlightColourId, catC.withAlpha(0.20f));
    presetSearch.setColour(juce::TextEditor::outlineColourId, catC.withAlpha(0.22f));

    fxDetailTitle.setColour(juce::Label::textColourId, catC.brighter(0.30f));
    physicalSectionTitle.setColour(juce::Label::textColourId, catC.brighter(0.24f));
    physicalSectionHint.setColour(juce::Label::textColourId, controlText.withAlpha(0.82f));
    modMatrixTitle.setColour(juce::Label::textColourId, catC.brighter(0.25f));
    modMatrixPlaceholderLabel.setColour(juce::Label::textColourId, controlText);
    modMatrixHintLabel.setColour(juce::Label::textColourId, controlText.withAlpha(0.82f));
    presetMetaLabel.setColour(juce::Label::textColourId, catC.brighter(0.12f));
    outputGainLabel.setColour(juce::Label::textColourId, controlText);
    fxUnavailableLbl.setColour(juce::Label::textColourId, controlText.withAlpha(0.58f));
    midiCCPageLabel.setColour(juce::Label::textColourId, controlText);
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, pillBg);
    midiCCPageLabel.setColour(juce::Label::outlineColourId, catC.withAlpha(0.32f));

    for (auto& tab : rightPanelTabs)
        tab.setAccent(catC);

    for (auto& rackItem : fxRackItems)
        rackItem.setAccent(catC);

    updateOutputGainUi();
    syncFxRackState();
}

bool InstrSynthAudioProcessorEditor::isFxTabAvailable(int tabIndex, int instrIndex) const {
    switch (tabIndex) {
        case 0: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Reverb);
        case 1: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Saturator);
        case 2: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Transient);
        case 3: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Compressor);
        case 4: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Eq);
        case 5: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Chorus);
        case 6: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Delay);
        case 7: return mis::isFxAvailable(instrIndex, mis::GlobalFxSlot::Limiter);
        default: return true;
    }
}

int InstrSynthAudioProcessorEditor::firstAvailableFxTab(int instrIndex) const {
    for (int tabIndex = 0; tabIndex < kFxTabs; ++tabIndex)
        if (isFxTabAvailable(tabIndex, instrIndex))
            return tabIndex;
    return 0;
}

void InstrSynthAudioProcessorEditor::syncFxAvailability() {
    const int instrIdx = selectedInstrFromParam();
    for (int tabIndex = 0; tabIndex < kFxTabs; ++tabIndex) {
        const bool available = isFxTabAvailable(tabIndex, instrIdx);
        fxBypassBtns[(size_t)tabIndex].setEnabled(available);
        if (!available)
            fxBypassBtns[(size_t)tabIndex].setButtonText("OFF");
        else
            fxBypassBtns[(size_t)tabIndex].setButtonText("ON");
    }

    const int fallbackTab = firstAvailableFxTab(instrIdx);
    if (activeFxTab < 0 || activeFxTab >= kFxTabs || !isFxTabAvailable(activeFxTab, instrIdx))
    {
        activeFxTab = fallbackTab;
        fxDetailTitle.setText(juce::String("FX Detail: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
        syncFxRackState();
        resized();
        repaint();
        return;
    }

    fxDetailTitle.setText(juce::String("FX Detail: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
    syncFxRackState();
}

void InstrSynthAudioProcessorEditor::rebuildInstrAttachments() {
    auto instrIdx = selectedInstrFromParam();
    if (instrIdx == cachedInstrIdx) return;
    cachedInstrIdx = instrIdx;
    const bool struckInstrument = mis::getCharacteristics(instrIdx).synthesisMode == mis::SynthesisMode::Struck;

    for (auto& a : envAttach) a.reset();
    for (auto& a : physicalAttach) a.reset();

    for (int i=0; i<kEnvN; ++i) {
        auto si=(size_t)i;
        auto id = InstrSynthAudioProcessor::makeInstParamId(
            cachedInstrIdx, kEnvCtrls[si].suffix);
        envAttach[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), id, envDials[si]);

        juce::String label = kEnvCtrls[si].label;
        if (struckInstrument)
        {
            if (i == 6)      label = "Mallet";
            else if (i == 7) label = "Cavity";
            else if (i == 8) label = "Coupling";
        }
        envLabels[si].setText(label, juce::dontSendNotification);
    }

    configureCutoffDial(envDials[11], accent_);

    syncInstrumentUiProfile();
    applyInstrumentTheme(instrIdx);
    activeFamilyIndex = static_cast<int>(mis::getFamily(instrIdx));
    syncSelectionUiFromInstr();
    syncFxAvailability();
    refreshPresetList();
    updatePresetMetadataSummary();
    repaint();
}

void InstrSynthAudioProcessorEditor::rebuildModelSelectorForFamily(
    int familyIndex, int preferredInstr) {
    familyIndex = juce::jlimit(0, mis::kNumFamilies-1, familyIndex);
    modelSelector.clear(juce::dontSendNotification);
    const int first = mis::kFamilyStart[familyIndex];
    const int count = mis::kFamilySize[familyIndex];
    for (int i=0; i<count; ++i)
        modelSelector.addItem(mis::getInstrumentName(first+i), first+i+1);
    int target = preferredInstr;
    if (target < first || target >= first+count) target = first;
    modelSelector.setSelectedId(target+1, juce::dontSendNotification);
}

void InstrSynthAudioProcessorEditor::syncSelectionUiFromInstr() {
    const int instrIndex  = selectedInstrFromParam();
    const int familyIndex = static_cast<int>(mis::getFamily(instrIndex));
    if (familySelector.getSelectedId() != familyIndex+1)
        familySelector.setSelectedId(familyIndex+1, juce::dontSendNotification);
    rebuildModelSelectorForFamily(familyIndex, instrIndex);
    for (int f = 0; f < mis::kNumFamilies; ++f)
    {
        auto& tab = familyTabs[(size_t)f];
        tab.setSelected(f == familyIndex);
        tab.setVisible(true);
    }
}

void InstrSynthAudioProcessorEditor::updateOutputGainUi()
{
    if (auto* raw = proc.getAPVTS().getRawParameterValue("output_gain"))
        outputGainLabel.setText(formatSignedValue(raw->load(), " dB", 1), juce::dontSendNotification);
}

juce::String InstrSynthAudioProcessorEditor::currentPresetMetadataSummary() const
{
    const auto instrumentName = juce::String(mis::getInstrumentName(selectedInstrFromParam()));
    if (proc.isCurrentPresetUser())
    {
        const auto file = proc.getCurrentUserPresetFile();
        return "User | " + instrumentName + " | " + (file.existsAsFile() ? file.getFileNameWithoutExtension()
                                                                          : juce::String("User preset"));
    }

    const auto factoryNames = proc.getFactoryPresetNames();
    const int presetIndex = proc.getCurrentFactoryPresetIndex();
    if (juce::isPositiveAndBelow(presetIndex, factoryNames.size()))
    {
        const auto factoryName = factoryNames[presetIndex];
        if (factoryName.startsWithIgnoreCase(instrumentName))
            return "Factory | " + factoryName;

        return "Factory | " + instrumentName + " | " + factoryName;
    }

    return "Factory | " + instrumentName;
}

void InstrSynthAudioProcessorEditor::updatePresetMetadataSummary()
{
    presetMetaLabel.setText(currentPresetMetadataSummary(), juce::dontSendNotification);
}

// ── Tooltip helpers ────────────────────────────────────────────────────────
void InstrSynthAudioProcessorEditor::cycleTooltipMode()
{
    switch (tooltipMode)
    {
    case TooltipMode::Short:  tooltipMode = TooltipMode::Novice; break;
    case TooltipMode::Novice: tooltipMode = TooltipMode::Off;    break;
    case TooltipMode::Off:    tooltipMode = TooltipMode::Short;  break;
    }
    switch (tooltipMode)
    {
    case TooltipMode::Short:  tooltipModeBtn.setButtonText("TIP: SHORT");  break;
    case TooltipMode::Novice: tooltipModeBtn.setButtonText("TIP: NOVICE"); break;
    case TooltipMode::Off:    tooltipModeBtn.setButtonText("TIP: OFF");    break;
    }
    tooltipWindow.setVisible(tooltipMode != TooltipMode::Off);
    applyTooltips();
}

void InstrSynthAudioProcessorEditor::applyTooltips()
{
    const bool off = (tooltipMode == TooltipMode::Off);
    const auto* table = (tooltipMode == TooltipMode::Novice) ? kTooltipsNovice : kTooltipsShort;
    const bool struckInstrument = mis::getCharacteristics(
        cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam()).synthesisMode == mis::SynthesisMode::Struck;

    int idx = 0;
    for (int i = 0; i < kEnvN; ++i)
    {
        juce::String tooltip = off ? juce::String() : juce::String(table[idx]);
        if (!off && struckInstrument)
        {
            if (i == 6)
                tooltip = "Mallet hardness and impact density for struck instruments";
            else if (i == 7)
                tooltip = "Cavity/body bloom that sets modal mass and resonant sustain";
            else if (i == 8)
                tooltip = "Modal coupling between tuned fields and secondary resonators";
        }
        envDials[(size_t)i].setTooltip(tooltip);
        ++idx;
    }

    lfoRateDial .setTooltip(off ? juce::String() : table[idx++]);
    lfoDepthDial.setTooltip(off ? juce::String() : table[idx++]);

    for (int i = 0; i < kMacroTotal; ++i)
        macroDials[(size_t)i].setTooltip(off ? juce::String() : table[idx++]);

    for (int i = 0; i < kFxN; ++i)
        fxDials[(size_t)i].setTooltip(off ? juce::String() : table[idx++]);

    gainDial.setTooltip(off ? juce::String() : table[idx++]);

    const auto& profile = getUiProfileForInstrument(cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam());
    for (int i = 0; i < static_cast<int>(physicalDials.size()); ++i)
    {
        const auto& control = profile[static_cast<std::size_t>(i)];
        if (control.suffix == nullptr || off)
            physicalDials[static_cast<std::size_t>(i)].setTooltip(juce::String());
        else
            physicalDials[static_cast<std::size_t>(i)].setTooltip(synthui::tooltipForMode(
                control,
                tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                   : synthui::TooltipMode::Short));
    }
}
