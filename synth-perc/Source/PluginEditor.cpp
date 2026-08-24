#include "PluginEditor.h"
#include "BinaryData.h"
#include "../../Shared/PresetManifest.h"
#include <cmath>

// =============================================================================
// Layout constants — 1340 x 820
// =============================================================================
namespace lay
{
    constexpr int W = 1100, H = 780;
}

namespace
{
using EnvUiProfile = synthui::InstrumentUiProfile<14>;

struct PercLayoutMetrics
{
    bool compact = false;
    bool roomy = false;
    int outerMargin = 24;
    int gutter = 16;
    int headerH = 96;
    int selectorY = 0;
    int selectorH = 80;
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

PercLayoutMetrics computeLayoutMetrics(int width, int height)
{
    PercLayoutMetrics layout;
    layout.compact = width < 1120 || height < 700;
    layout.roomy = width > 1600 || height > 940;
    layout.outerMargin = layout.compact ? 16 : 24;
    layout.gutter = layout.compact ? 10 : (layout.roomy ? 22 : 16);
    layout.headerH = layout.compact ? 88 : 96;
    layout.selectorH = layout.compact ? 86 : 92;
    layout.kbH = layout.compact
        ? juce::jlimit(70, 90, static_cast<int>(height * 0.13f))
        : juce::jlimit(80, 114, static_cast<int>(height * (layout.roomy ? 0.135f : 0.145f)));

    const int maxContentW = juce::jmin(width - layout.outerMargin * 2, 1680);
    layout.contentW = juce::jmax(900, maxContentW);
    layout.contentX = (width - layout.contentW) / 2;

    layout.selectorY = layout.outerMargin + layout.headerH + 8;
    layout.kbY = height - layout.kbH - layout.outerMargin;
    layout.bodyY = layout.selectorY + layout.selectorH + layout.gutter;
    layout.bodyH = juce::jmax(250, layout.kbY - layout.bodyY - 10);

    layout.colW = (layout.contentW - layout.gutter * 2) / 3;
    layout.col1X = layout.contentX;
    layout.col2X = layout.col1X + layout.colW + layout.gutter;
    layout.col3X = layout.col2X + layout.colW + layout.gutter;
    return layout;
}

#if defined(UWDEVST_PERC_TEST_BUILD)
juce::Rectangle<int> unionBounds(std::initializer_list<juce::Rectangle<int>> rectangles)
{
    juce::Rectangle<int> result;
    for (const auto& rectangle : rectangles)
    {
        if (!rectangle.isEmpty())
            result = result.isEmpty() ? rectangle : result.getUnion(rectangle);
    }
    return result;
}

template <typename ComponentType>
juce::Rectangle<int> unionVisibleBounds(const std::initializer_list<const ComponentType*>& components)
{
    juce::Rectangle<int> result;
    for (const auto* component : components)
    {
        if (component != nullptr && component->isVisible() && !component->getBounds().isEmpty())
            result = result.isEmpty() ? component->getBounds() : result.getUnion(component->getBounds());
    }
    return result;
}
#endif

const synthui::MacroLabelProfile<4>& macroLabelsForFamily(const mpc::Family family)
{
    static const synthui::MacroLabelProfile<4> percussions = { "Punch", "Body", "Space", "Tone" };
    static const synthui::MacroLabelProfile<4> ambiance    = { "Bloom", "Air", "Space", "Shimmer" };
    static const synthui::MacroLabelProfile<4> metalliques = { "Sparkle", "Ring", "Space", "Alloy" };

    switch (family)
    {
        case mpc::Family::Percussions: return percussions;
        case mpc::Family::Ambiance:    return ambiance;
        case mpc::Family::Metalliques: return metalliques;
    }
    return percussions;
}

constexpr const char* kRightPanelSectionLabels[3] = { "Macro / LFO", "MOD MATRIX", "FX" };

juce::String trimValueString(double value, int decimals = 2)
{
    auto text = juce::String(value, decimals);
    while (text.contains(".") && (text.endsWith("0") || text.endsWith(".")))
    {
        if (text.endsWith("."))
        {
            text = text.dropLastCharacters(1);
            break;
        }

        text = text.dropLastCharacters(1);
    }

    return text;
}

double parseUiNumber(const juce::String& text)
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return 0.0;

    juce::String filtered;
    bool seenDecimal = false;
    bool seenSign = false;
    for (auto ch : trimmed)
    {
        if (juce::CharacterFunctions::isDigit(ch))
        {
            filtered << juce::String::charToString(ch);
            continue;
        }

        if (ch == '.' && !seenDecimal)
        {
            filtered << ".";
            seenDecimal = true;
            continue;
        }

        if ((ch == '-' || ch == '+') && filtered.isEmpty() && !seenSign)
        {
            filtered << juce::String::charToString(ch);
            seenSign = true;
        }
    }

    return filtered.isNotEmpty() ? filtered.getDoubleValue() : 0.0;
}

juce::String formatPercent01(double value)
{
    return juce::String(juce::roundToInt(juce::jlimit(0.0, 1.0, value) * 100.0)) + "%";
}

juce::String formatSignedPercent(double value)
{
    const auto rounded = juce::roundToInt(juce::jlimit(-1.0, 1.0, value) * 100.0);
    if (std::abs(rounded) < 1)
        return "0%";

    return juce::String(rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatSemitones(double value)
{
    if (std::abs(value) < 0.005)
        return "0 st";

    const auto body = trimValueString(value, std::abs(value) < 10.0 ? 1 : 0);
    return juce::String(value > 0.0 ? "+" : "") + body + " st";
}

juce::String formatTimeSeconds(double seconds)
{
    if (seconds < 1.0)
        return juce::String(juce::roundToInt(seconds * 1000.0)) + " ms";

    return trimValueString(seconds, seconds < 10.0 ? 2 : 1) + " s";
}

juce::String formatMilliseconds(double milliseconds)
{
    if (milliseconds >= 1000.0)
        return trimValueString(milliseconds / 1000.0, milliseconds < 10000.0 ? 2 : 1) + " s";

    return juce::String(juce::roundToInt(milliseconds)) + " ms";
}

juce::String formatFrequency(double hz)
{
    if (hz >= 1000.0)
        return trimValueString(hz / 1000.0, hz < 10000.0 ? 1 : 0) + " kHz";

    return juce::String(juce::roundToInt(hz)) + " Hz";
}

juce::String formatPanValue(double value)
{
    const auto pan = juce::jlimit(-1.0, 1.0, value);
    const auto amount = juce::roundToInt(std::abs(pan) * 100.0);
    if (amount < 2)
        return "Center";

    return juce::String(pan < 0.0 ? "L " : "R ") + juce::String(amount) + "%";
}

juce::String formatDb(double value)
{
    return trimValueString(value, std::abs(value) < 10.0 ? 1 : 0) + " dB";
}

juce::String formatRatio(double value)
{
    return trimValueString(value, value < 10.0 ? 1 : 0) + ":1";
}

juce::String formatMultiplier(double value)
{
    return trimValueString(value, value < 10.0 ? 1 : 0) + "x";
}

void paintHeaderCaption(juce::Graphics& g, juce::Rectangle<int> area,
                        const juce::String& text, juce::Colour accent)
{
    auto band = area.withHeight(12).translated(0, -1).toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRoundedRectangle(band.translated(0.0f, 1.0f), 4.0f);
    g.setColour(accent.withAlpha(0.08f));
    g.fillRoundedRectangle(band, 4.0f);
    g.setColour(synthcol::textDim.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.drawText(text, area.removeFromTop(12), juce::Justification::centredLeft);
}

struct PercChromeTheme
{
    juce::Colour accent;
    juce::Colour headerTint;
    juce::Colour panelBaseTint;
    juce::Colour panelCavityTint;
    juce::Colour panelHeaderTint;
    juce::Colour keyboardTint;
    juce::Colour knobAccent;
    juce::Colour knobGlow;
    juce::Colour knobBezel;
    juce::Colour knobCollar;
    juce::Colour knobCapAccent;
};

PercChromeTheme makePercChromeTheme(juce::Colour accent)
{
    PercChromeTheme theme;
    theme.accent = accent;
    theme.headerTint = accent.brighter(0.04f).withMultipliedSaturation(0.44f);
    theme.panelBaseTint = accent.darker(0.86f).withMultipliedSaturation(0.22f);
    theme.panelCavityTint = accent.darker(0.98f).withMultipliedSaturation(0.28f);
    theme.panelHeaderTint = accent.darker(0.48f).withMultipliedSaturation(0.34f);
    theme.keyboardTint = accent.darker(0.92f).withMultipliedSaturation(0.18f);
    theme.knobAccent = juce::Colour(0xffA9B4BF)
        .interpolatedWith(accent, 0.18f)
        .withMultipliedSaturation(0.56f);
    theme.knobGlow = accent.brighter(0.12f).withMultipliedSaturation(0.42f);
    theme.knobBezel = juce::Colour(0xff243039)
        .interpolatedWith(accent.darker(0.52f), 0.10f)
        .withMultipliedSaturation(0.46f);
    theme.knobCollar = juce::Colour(0xff1A2229)
        .interpolatedWith(accent.darker(0.34f), 0.08f)
        .withMultipliedSaturation(0.42f);
    theme.knobCapAccent = juce::Colour(0xffD7DEE6)
        .interpolatedWith(accent, 0.12f)
        .withMultipliedSaturation(0.52f);
    return theme;
}

void applyKnobChrome(juce::Slider& slider, const PercChromeTheme& theme)
{
    slider.setColour(juce::Slider::rotarySliderFillColourId, theme.knobAccent);
    slider.setColour(juce::Slider::textBoxTextColourId,
                     juce::Colour(0xffE6ECF2).interpolatedWith(theme.accent, 0.08f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId,
                     juce::Colour(0xff0D1115).interpolatedWith(theme.panelCavityTint, 0.10f).withAlpha(0.96f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, theme.accent.withAlpha(0.26f));
    slider.setColour(SynthLookAndFeel::knobGlowColourId, theme.knobGlow);
    slider.setColour(SynthLookAndFeel::knobBezelColourId, theme.knobBezel);
    slider.setColour(SynthLookAndFeel::knobCollarColourId, theme.knobCollar);
    slider.setColour(SynthLookAndFeel::knobCapAccentColourId, theme.knobCapAccent);
}

const EnvUiProfile& envProfileForInstrument(const int instrIndex)
{
    static const EnvUiProfile percussions = {{
        { "Level",    "Output volume",       "Sets the percussion output level in the mix" },
        { "Tune",     "Fine tuning",         "Adjusts overall instrument pitch" },
        { "Strike",   "Attack brightness",   "Hardens or softens the mallet or skin attack" },
        { "Attack",   "Attack time",         "Controls how quickly the sound arrives" },
        { "Decay",    "Decay time",          "Shortens or lengthens the main decay" },
        { "Hold",     "Sustain level",       "Holds part of the level while held" },
        { "Release",  "Release time",        "Controls the end of the sound after release" },
        { "Mute",     "Damping",             "Brakes high frequencies and shortens resonance" },
        { "Shell Res.", "Body resonance",    "Blends shell, skin or resonant body" },
        { "Noise",      "Noise amount",      "Adds breath, friction or skin texture" },
        { "Width",      "Stereo width",      "Widens the instrument in the stereo field" },
        { "Tone",       "Timbral colour",    "Shifts the timbre between wood, skin and metal" },
        { "Tone Cut", "Low-pass filter",     "Darkens or opens the spectrum" },
        { "Pan",      "Pan",                 "Places the instrument left or right" }
    }};

    static const EnvUiProfile ambiance = {{
        { "Level",     "Output volume",    "Sets the ambience texture output level" },
        { "Tune",      "Fine tuning",      "Shifts the apparent pitch of the texture" },
        { "Air",       "Air brightness",   "Adds air, breath or shimmer" },
        { "Bloom",     "Bloom time",       "Makes the ambience emerge slower or faster" },
        { "Tail",      "Tail length",      "Lengthens or shortens the sonic trail" },
        { "Float",     "Sustain level",    "Maintains a more stable sound bed" },
        { "Release",   "Release time",     "Lets the ambience fade more gently" },
        { "Absorb",    "Damping",          "Brakes brightness and tightens resonances" },
        { "Resonance", "Body resonance",   "Brings out the bowl, tube or cavity resonance" },
        { "Breath",    "Breath amount",    "Adds wind, sand or air texture" },
        { "Width",     "Stereo width",     "Spreads the ambience across the space" },
        { "Shimmer",   "Timbral colour",   "Makes the timbre more shimmering or more matte" },
        { "Soften",    "Low-pass filter",  "Softens highs for a darker ambience" },
        { "Pan",       "Pan",              "Places the ambience in the stereo field" }
    }};

    static const EnvUiProfile metalliques = {{
        { "Level",     "Output volume",    "Sets the metallic instrument output level" },
        { "Tune",      "Fine tuning",      "Adjusts the pitch of the metal or bell" },
        { "Shine",     "Brightness",       "Adds sparkle and metallic highs" },
        { "Strike",    "Attack time",      "Hardens or softens the stick impact" },
        { "Ring",      "Ring time",        "Controls the length of the metallic resonance" },
        { "Hold",      "Sustain level",    "Holds more of the resonant body" },
        { "Release",   "Release time",     "Extends the ring tail after release" },
        { "Damping",   "Damping",          "Mutes the metal for a drier sound" },
        { "Resonance", "Body resonance",   "Blends the metallic body resonance" },
        { "Stick",     "Noise amount",     "Adds the noise of the hit or friction" },
        { "Spread",    "Stereo width",     "Widens the stereo perception of the metal" },
        { "Alloy",     "Timbral colour",   "Shifts the timbre toward denser or lighter metal" },
        { "Tone Cut",  "Low-pass filter",  "Darkens or opens the metallic ring" },
        { "Pan",       "Pan",              "Places the instrument left or right" }
    }};

    switch (mpc::getFamily(instrIndex))
    {
        case mpc::Family::Percussions: return percussions;
        case mpc::Family::Ambiance:    return ambiance;
        case mpc::Family::Metalliques: return metalliques;
    }
    return percussions;
}

void glazePercChrome(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     juce::Colour accent,
                     float radius,
                     float intensity)
{
    const auto percTop = juce::Colour(0xff394247).withAlpha(0.028f * intensity);
    const auto percMid = juce::Colour(0xff232A2E).withAlpha(0.052f * intensity);
    const auto percBottom = juce::Colour(0xff101316).withAlpha(0.14f * intensity);

    juce::ColourGradient glaze(percTop, area.getCentreX(), area.getY(),
                               percBottom, area.getCentreX(), area.getBottom(), false);
    glaze.addColour(0.42, percMid);
    g.setGradientFill(glaze);
    g.fillRoundedRectangle(area, radius);

    auto grainArea = area.reduced(8.0f, 7.0f);
    const float fineStep = juce::jlimit(5.0f, 11.0f, grainArea.getHeight() * 0.024f);
    const float coarseStep = juce::jlimit(11.0f, 22.0f, grainArea.getHeight() * 0.051f);
    const float fineInset = juce::jlimit(8.0f, 14.0f, grainArea.getWidth() * 0.022f);
    const float coarseInset = juce::jlimit(14.0f, 24.0f, grainArea.getWidth() * 0.040f);
    g.setColour(juce::Colours::white.withAlpha(0.006f * intensity));
    for (float y = grainArea.getY() + fineStep * 0.85f; y < grainArea.getBottom() - fineStep * 0.85f; y += fineStep)
        g.drawHorizontalLine(static_cast<int>(std::round(y)), grainArea.getX() + fineInset, grainArea.getRight() - fineInset);

    g.setColour(juce::Colours::black.withAlpha(0.020f * intensity));
    for (float y = grainArea.getY() + coarseStep * 0.70f; y < grainArea.getBottom() - coarseStep * 0.45f; y += coarseStep)
        g.drawHorizontalLine(static_cast<int>(std::round(y)), grainArea.getX() + coarseInset, grainArea.getRight() - coarseInset);

    auto sheen = area.reduced(2.4f).withHeight(juce::jmax(8.0f, area.getHeight() * 0.16f));
    juce::ColourGradient highlight(juce::Colours::white.withAlpha(0.012f * intensity), sheen.getCentreX(), sheen.getY(),
                                   juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
    g.setGradientFill(highlight);
    g.fillRoundedRectangle(sheen, juce::jmax(0.0f, radius - 2.0f));

    g.setColour(accent.withAlpha(0.026f * intensity));
    g.drawRoundedRectangle(area.reduced(1.0f), juce::jmax(0.0f, radius - 1.0f), 0.9f);
}

void paintPercMatteTexture(juce::Graphics& g,
                           juce::Rectangle<float> area,
                           juce::Colour accent,
                           float radius,
                           float intensity)
{
    juce::Graphics::ScopedSaveState scoped(g);
    auto face = area.reduced(4.0f, 4.0f);
    g.reduceClipRegion(face.toNearestInt());

    juce::ColourGradient wash(juce::Colour(0xff111418).withAlpha(0.07f * intensity),
                              face.getCentreX(), face.getY(),
                              juce::Colour(0xff090B0E).withAlpha(0.11f * intensity),
                              face.getCentreX(), face.getBottom(), false);
    wash.addColour(0.34, juce::Colour(0xff191E22).withAlpha(0.04f * intensity));
    wash.addColour(0.68, juce::Colour(0xff0D1013).withAlpha(0.08f * intensity));
    g.setGradientFill(wash);
    g.fillRoundedRectangle(face, juce::jmax(0.0f, radius - 2.0f));

    const float fineStep = juce::jlimit(5.0f, 11.0f, face.getHeight() * 0.024f);
    const float coarseStep = juce::jlimit(11.0f, 22.0f, face.getHeight() * 0.051f);
    const float verticalStep = juce::jlimit(18.0f, 34.0f, face.getWidth() * 0.080f);
    const float fineInset = juce::jlimit(8.0f, 14.0f, face.getWidth() * 0.022f);
    const float coarseInset = juce::jlimit(14.0f, 24.0f, face.getWidth() * 0.040f);
    const float verticalInset = juce::jlimit(12.0f, 18.0f, face.getWidth() * 0.035f);

    g.setColour(juce::Colours::white.withAlpha(0.006f * intensity));
    for (float y = face.getY() + fineStep * 0.90f; y < face.getBottom() - fineStep * 0.85f; y += fineStep)
        g.drawHorizontalLine(static_cast<int>(std::round(y)), face.getX() + fineInset, face.getRight() - fineInset);

    g.setColour(juce::Colours::black.withAlpha(0.022f * intensity));
    for (float y = face.getY() + coarseStep * 0.72f; y < face.getBottom() - coarseStep * 0.52f; y += coarseStep)
        g.drawHorizontalLine(static_cast<int>(std::round(y)), face.getX() + coarseInset, face.getRight() - coarseInset);

    g.setColour(accent.withAlpha(0.006f * intensity));
    for (float x = face.getX() + verticalStep * 0.62f; x < face.getRight() - verticalStep * 0.50f; x += verticalStep)
        g.drawVerticalLine(static_cast<int>(std::round(x)), face.getY() + verticalInset, face.getBottom() - verticalInset);
}
}

// =============================================================================
// Static tables
// =============================================================================
const std::array<PercSynthAudioProcessorEditor::CtrlDef,
                 PercSynthAudioProcessorEditor::kEnvN>
    PercSynthAudioProcessorEditor::kEnvCtrls = {{
        { "Level",        "level" },
        { "Tune",         "tune" },
        { "Brightness",   "brightness" },
        { "Attack",       "attack" },
        { "Decay",        "decay" },
        { "Sustain",      "sustain" },
        { "Release",      "release" },
        { "Damping",      "damping" },
        { "Body",         "body" },
        { "Noise",        "noise" },
        { "Stereo Width", "stereo_width" },
        { "Color",        "color" },
        { "Cutoff",       "cutoff" },
        { "Pan",          "pan" },
        { "One-Shot",     "oneShot" },
        { "O.S. Decay",   "oneShotDecayMs" }
    }};

const std::array<PercSynthAudioProcessorEditor::FxDef,
                 PercSynthAudioProcessorEditor::kMacroTotal>
    PercSynthAudioProcessorEditor::kMacroCtrls = {{
        { "Transient", "macro_impact" },
        { "Body",      "macro_resonance" },
        { "Space",     "macro_space" },
        { "Color",     "macro_couleur" }
    }};

const std::array<PercSynthAudioProcessorEditor::FxDef,
                 PercSynthAudioProcessorEditor::kFxN>
    PercSynthAudioProcessorEditor::kFxCtrls = {{
        // SAT (0-1)
        { "Drive",      "sat_drive" },
        { "Mix",        "sat_mix" },
        // TRANS (2-4)
        { "Attack",     "transient_attack" },
        { "Sustain",    "transient_sustain" },
        { "Mix",        "transient_mix" },
        // COMP (5-10)
        { "Threshold",  "comp_threshold" },
        { "Ratio",      "comp_ratio" },
        { "Attack",     "comp_attack" },
        { "Release",    "comp_release" },
        { "Makeup",     "comp_makeup" },
        { "Mix",        "comp_mix" },
        // REVERB (11-15)
        { "Size",       "reverb_size" },
        { "Damping",    "reverb_damping" },
        { "Width",      "reverb_width" },
        { "Mix",        "reverb_mix" },
        { "Pre-Delay",  "reverb_predelay" },
        // EQ (16-22)
        { "Low Freq",   "eq_low_freq" },
        { "Low Gain",   "eq_low_gain" },
        { "Mid Freq",   "eq_mid_freq" },
        { "Mid Gain",   "eq_mid_gain" },
        { "Mid Q",      "eq_mid_q" },
        { "High Freq",  "eq_high_freq" },
        { "High Gain",  "eq_high_gain" },
        // CHORUS (23-25)
        { "Rate",       "chorus_rate" },
        { "Depth",      "chorus_depth" },
        { "Mix",        "chorus_mix" },
        // DELAY (26-28)
        { "Time",       "delay_time" },
        { "Feedback",   "delay_feedback" },
        { "Mix",        "delay_mix" },
        // LIMITER (29-30)
        { "Threshold",  "limiter_threshold" },
        { "Release",    "limiter_release" }
    }};

const char* PercSynthAudioProcessorEditor::kFxRackSummaries[kFxTabs] = {
    "Space", "Color", "Attack", "Dynamics", "Tone", "Width", "Echo", "Output"
};

const char* PercSynthAudioProcessorEditor::kFxTabNames[kFxTabs] = {
    "REVERB", "SAT", "TRANS", "COMP", "EQ", "CHORUS", "DELAY", "LIMITER"
};

const char* PercSynthAudioProcessorEditor::kFxTabLabels[kFxTabs][kFxPerTab] = {
    { "Size",      "Damping",  "Width",    "Mix",      "Pre-Delay", "",        "" },
    { "Drive",     "Mix",      "",         "",         "",          "",        "" },
    { "Attack",    "Sustain",  "Mix",      "",         "",          "",        "" },
    { "Threshold", "Ratio",    "Attack",   "Release",  "Makeup",    "Mix",     "" },
    { "Low Freq",  "Low Gain", "Mid Freq", "Mid Gain", "Mid Q",     "High Freq", "High Gain" },
    { "Rate",      "Depth",    "Mix",      "",         "",          "",        "" },
    { "Time",      "Feedback", "Mix",      "",         "",          "",        "" },
    { "Threshold", "Release",  "",         "",         "",          "",        "" }
};

const char* PercSynthAudioProcessorEditor::kFxBypassParamIds[kFxTabs] = {
    "fx_tab3_en", "fx_tab0_en", "fx_tab1_en", "fx_tab2_en",
    "fx_eq_en", "fx_chorus_en", "fx_delay_en", "fx_limiter_en"
};

// =============================================================================
// Tooltip texts  (index order: envDials[0..15], lfoRate, lfoDepth,
//                 macroDials[0..3], fxDials[0..30], gainDial)
// =============================================================================
const char* PercSynthAudioProcessorEditor::kTooltipsShort[kTooltipCount] = {
    // env 0-15
    "Percussion output volume (0 - 100 %)",
    "Fine tuning in semitones (-24 to +24)",
    "Brightness - harmonic content of the modes",
    "Envelope attack time (0 - 2 s)",
    "Decay time after peak (0.1 - 10 s)",
    "Envelope sustain level (0 - 100 %)",
    "Release time after key release",
    "Damping of resonant modes",
    "Body / resonator resonance",
    "Excitation noise amount",
    "Stereo width of the sound",
    "Timbral colour - metal / wood",
    "Low-pass filter cutoff frequency (120 - 18 000 Hz)",
    "Percussion stereo position (-1 left, +1 right)",
    "One-shot mode (short rhythmic decay)",
    "One-shot decay time (10 - 500 ms)",
    // lfo 16-17
    "LFO speed (0.05 - 12 Hz)",
    "LFO modulation depth",
    // macro 16-19
    "Macro Sparkle - attack sparkle and brightness",
    "Macro Air - body resonance and space",
    "Macro Space - stereo width and depth",
    "Macro Color - overall timbral colour",
    // fx 20-50
    "Harmonic saturation intensity",
    "Saturation dry / wet balance",
    "Transient attack emphasis",
    "Transient sustain control",
    "Transient shaper dry / wet balance",
    "Compressor threshold (-60 to 0 dB)",
    "Compression ratio (1:1 to 20:1)",
    "Compressor attack time (0.1 - 100 ms)",
    "Compressor release time (5 - 500 ms)",
    "Compressor makeup gain (0 - 24 dB)",
    "Compressor dry / wet balance",
    "Reverb room size",
    "High-frequency damping in the reverb",
    "Reverb stereo width",
    "Reverb dry / wet balance",
    "Reverb pre-delay (ms)",
    "Low shelf filter frequency",
    "Low shelf gain (dB)",
    "Mid peak filter frequency",
    "Mid peak gain (dB)",
    "Mid filter Q factor - bandwidth",
    "High shelf filter frequency",
    "High shelf gain (dB)",
    "Chorus modulation speed (0.1 - 5 Hz)",
    "Chorus modulation depth",
    "Chorus dry / wet balance",
    "Delay time (1 - 2000 ms)",
    "Delay feedback (0 - 95 %)",
    "Delay dry / wet balance",
    "Output limiter threshold (-12 to 0 dB)",
    "Limiter release time (1 - 200 ms)",
    // gain 51
    "Global output gain (-24 to +12 dB)"
};

const char* PercSynthAudioProcessorEditor::kTooltipsNovice[kTooltipCount] = {
    // env 0-13
    "Level - Selected percussion volume. Turn up for louder, down for softer.",
    "Tune - Fine-tunes the percussion in semitones. Useful to match the key of the track.",
    "Brightness - Resonant mode brightness. Higher = metallic and clear, lower = matte and woody.",
    "Attack - Speed at which the sound rises. Short = sharp hit, long = soft entry.",
    "Decay - How long the sound descends after the initial peak before sustain.",
    "Sustain - Level held while you hold the key. 100% = no decay.",
    "Release - Time for the sound to fade after releasing the key. Short = tight, long = resonant.",
    "Damping - Mode damping. Higher = shorter and more muted resonance.",
    "Body - Body / acoustic resonator resonance. Higher = fuller, deeper sound.",
    "Noise - Excitation noise amount (hit, friction). Higher = more texture.",
    "Stereo Width - Widens the sound in the stereo field. 0 = mono, 1 = full stereo.",
    "Color - Timbral colour. Toward 0 = woody and matte, toward 1 = metallic and bright.",
    "Low Pass - Low-pass filter frequency. Lower to soften the sound, higher to let highs through.",
    "Pan - Position in the stereo field. -1 = hard left, 0 = centre, +1 = hard right.",
    "One-Shot - Enable one-shot mode for short rhythmic decays (drum machines, hi-hats).",
    "O.S. Decay - When one-shot is on, time before the sound stops (10 - 500 ms).",
    // lfo 16-17
    "LFO Rate - Low-frequency oscillator speed. Controls vibrato or tremolo rate.",
    "LFO Depth - Modulation intensity. Higher = more pronounced effect.",
    // macro 16-19
    "Macro Sparkle - Attack sparkle and brightness. Turn right for a more glittering sound.",
    "Macro Air - Body resonance and space. Higher = more airy and open.",
    "Macro Space - Stereo width and depth. Higher = more spatial and immersive.",
    "Macro Color - Overall timbral colour. Shifts the character from wood to metal.",
    // fx 20-50
    "Sat Drive - Harmonic saturation amount. Adds warmth and grain.",
    "Sat Mix - Balance between clean and saturated signal.",
    "Transient Attack - Emphasises the initial hit. Great for punchier percussion.",
    "Transient Sustain - Controls sustain after the attack. Lower = shorter, more percussive.",
    "Transient Mix - Transient shaper balance. 0% = no effect.",
    "Comp Threshold - Level above which the compressor acts. Lower = more compression.",
    "Comp Ratio - Compression strength. 2:1 = gentle, 10:1 = heavy, 20:1 = near-limiter.",
    "Comp Attack - Compressor reaction speed. Short = preserves transients, long = smooth.",
    "Comp Release - Compressor release time. Short = pumping, long = natural.",
    "Comp Makeup - Makeup gain after compression to compensate for volume reduction.",
    "Comp Mix - Parallel compressor balance. 50% = parallel compression (NY style).",
    "Reverb Size - Simulated room size. Large = long tail, small = short ambience.",
    "Reverb Damping - High-frequency absorption in the reverb. Higher = darker reverb.",
    "Reverb Width - Reverb stereo opening. 0 = mono, 1 = wide.",
    "Reverb Mix - Balance between direct and reverb. 0% = dry, 100% = fully wet.",
    "Reverb Pre-delay - Time before the reverb starts. Adds clarity between attack and reverb.",
    "EQ Low Freq - Low shelf filter cutoff. Typical: 80-300 Hz.",
    "EQ Low Gain - Boosts or cuts bass. Positive = more bass, negative = less.",
    "EQ Mid Freq - Mid peak centre. Adjust to target body (400 Hz) or presence (2 kHz).",
    "EQ Mid Gain - Boosts or cuts midrange. Useful for sculpting percussion character.",
    "EQ Mid Q - Mid bandwidth. Low Q = wide and gentle, high Q = narrow and precise.",
    "EQ High Freq - High shelf filter frequency. Typical: 3-12 kHz.",
    "EQ High Gain - Boosts or cuts highs. Positive = more brilliance, negative = darker.",
    "Chorus Rate - Chorus modulation speed. Slow = gentle ripple, fast = vibrato.",
    "Chorus Depth - Modulation intensity. Higher = more pronounced effect.",
    "Chorus Mix - Balance between dry and chorused signal. 50% is a good starting point.",
    "Delay Time - Time between repeats. Short = slapback, long = spaced echoes.",
    "Delay Feedback - Number of repeats. Higher = more repeats (watch the feedback!).",
    "Delay Mix - Balance between direct and echo. Light for rhythm, more for atmosphere.",
    "Limiter Threshold - Final limiter ceiling. Prevents the signal from exceeding this level.",
    "Limiter Release - Limiter release speed. Short = transparent, long = smooth.",
    // gain 52
    "Output Gain - Final plugin output volume. Adjust to match the level in your mix."
};

// =============================================================================
// Helpers
// =============================================================================
juce::Colour PercSynthAudioProcessorEditor::familyColour(int familyIndex)
{
    switch (familyIndex)
    {
        case 0: return juce::Colour(0xff7b746c);
        case 1: return juce::Colour(0xff6e7f88);
        case 2: return juce::Colour(0xff888d96);
        default: return juce::Colour(0xff7d7d84);
    }
}

juce::Colour PercSynthAudioProcessorEditor::instrCatColour(int instrIndex)
{
    const int familyIndex = static_cast<int>(mpc::getFamily(instrIndex));
    const int first = mpc::kFamilyStart[familyIndex];
    const int count = mpc::kFamilySize[familyIndex];
    const float t = count > 1 ? static_cast<float>(instrIndex - first) / static_cast<float>(count - 1) : 0.0f;

    auto base = familyColour(familyIndex).withMultipliedSaturation(0.45f);
    auto dark = base.darker(0.35f).withMultipliedSaturation(0.60f);
    auto light = base.brighter(0.18f).withMultipliedSaturation(0.75f);
    return dark.interpolatedWith(light, juce::jlimit(0.0f, 1.0f, t));
}

int PercSynthAudioProcessorEditor::selectedInstrFromParam() const
{
    if (auto* raw = proc.getAPVTS().getRawParameterValue("selected_instr"))
        return juce::jlimit(0, mpc::kNumInstruments - 1,
                            static_cast<int>(std::round(raw->load())));
    return 0;
}

// =============================================================================
// Virtual bridge methods
// =============================================================================
juce::StringArray PercSynthAudioProcessorEditor::hostGetFactoryNames()
    { return proc.getFactoryPresetNames(); }

juce::Array<juce::File> PercSynthAudioProcessorEditor::hostScanUserPresets()
    { return proc.scanUserPresets(); }

bool PercSynthAudioProcessorEditor::hostIsUserPreset()
    { return proc.isCurrentPresetUser(); }

juce::File PercSynthAudioProcessorEditor::hostCurrentUserFile()
    { return proc.getCurrentUserPresetFile(); }

int PercSynthAudioProcessorEditor::hostCurrentFactoryIdx()
    { return proc.getCurrentFactoryPresetIndex(); }

void PercSynthAudioProcessorEditor::hostApplyFactory(int idx)
    { proc.applyFactoryPreset(idx); }

void PercSynthAudioProcessorEditor::hostLoadUser(const juce::File& f)
    { proc.loadUserPreset(f); }

bool PercSynthAudioProcessorEditor::hostSaveUser(const juce::String& name)
    { return proc.saveUserPreset(name); }

void PercSynthAudioProcessorEditor::hostUpdateUser(const juce::File& f)
    { proc.updateUserPreset(f); }

void PercSynthAudioProcessorEditor::hostSaveFactory(int idx)
    { proc.saveFactoryPreset(idx); }

void PercSynthAudioProcessorEditor::hostDeleteUser(const juce::File& f)
    { proc.deleteUserPreset(f); }

juce::File PercSynthAudioProcessorEditor::hostGetUserPresetsDir()
    { return PercSynthAudioProcessor::getUserPresetsDirectory(proc.getSelectedInstrIndex()); }

juce::File PercSynthAudioProcessorEditor::hostGetUserPresetsDirForIndex(int instrumentIndex)
    { return PercSynthAudioProcessor::getUserPresetsDirectory(instrumentIndex); }

juce::String PercSynthAudioProcessorEditor::hostPresetInstrumentAttr() const
    { return "instrIndex"; }

juce::String PercSynthAudioProcessorEditor::hostFormatFactoryPresetLabel(int presetIndex,
                                                                         const juce::String& displayName) const
{
    if (const auto* preset = proc.getFactoryPresetDefinition(presetIndex))
    {
        return displayName
            + " | " + juce::String(preset->metadata.mixRole.c_str()).toUpperCase()
            + " | " + juce::String(preset->metadata.nominalPeakDb, 1) + " dB"
            + " | " + juce::String(preset->metadata.outputProfile.c_str());
    }
    return displayName;
}

juce::String PercSynthAudioProcessorEditor::hostFormatUserPresetLabel(const juce::File&,
                                                                      const juce::String& displayName) const
{
    return displayName + " | USER";
}

juce::String PercSynthAudioProcessorEditor::hostFactoryPresetSearchText(int presetIndex,
                                                                        const juce::String& displayName) const
{
    juce::String text = displayName;
    if (const auto* preset = proc.getFactoryPresetDefinition(presetIndex))
    {
        text << " " << juce::String(preset->metadata.mixRole.c_str());
        text << " " << juce::String(preset->metadata.familyLabel.c_str());
        text << " " << juce::String(preset->metadata.description.c_str());
        text << " " << juce::String(preset->metadata.outputProfile.c_str());
        for (const auto& tag : preset->metadata.tags)
            text << " " << juce::String(tag.c_str());
    }
    return text;
}

juce::String PercSynthAudioProcessorEditor::hostUserPresetSearchText(const juce::File& presetFile,
                                                                     const juce::String& displayName) const
{
    juce::String text = displayName + " user " + juce::String(juce::CharPointer_UTF8(mpc::getInstrName(selectedInstrFromParam())));
    musique::preset::PresetManifest manifest;
    if (musique::preset::loadManifestFromFile(musique::preset::manifestFileForPresetFile(presetFile), manifest))
    {
        text << " " << manifest.instrumentName << " " << manifest.synthType << " " << manifest.sourceModel;
    }
    return text;
}

bool PercSynthAudioProcessorEditor::hostShouldIncludeFactoryPreset(int presetIndex) const
{
    if (presetSourceFilter.getSelectedId() == 3)
        return false;

    const auto* preset = proc.getFactoryPresetDefinition(presetIndex);
    if (preset == nullptr)
        return true;

    const auto selectedFamily = presetFamilyFilter.getText().trim();
    if (presetFamilyFilter.getSelectedId() > 1
        && !selectedFamily.equalsIgnoreCase(juce::String(preset->metadata.familyLabel.c_str())))
        return false;

    const auto selectedRole = presetRoleFilter.getText().trim();
    if (presetRoleFilter.getSelectedId() > 1
        && !selectedRole.equalsIgnoreCase(juce::String(preset->metadata.mixRole.c_str())))
        return false;

    const auto selectedTag = presetTagFilter.getText().trim();
    if (presetTagFilter.getSelectedId() > 1)
    {
        bool tagMatch = false;
        for (const auto& tag : preset->metadata.tags)
        {
            if (selectedTag.equalsIgnoreCase(juce::String(tag.c_str())))
            {
                tagMatch = true;
                break;
            }
        }
        if (!tagMatch)
            return false;
    }

    return true;
}

bool PercSynthAudioProcessorEditor::hostShouldIncludeUserPreset(const juce::File&) const
{
    return presetSourceFilter.getSelectedId() != 2;
}

#if defined(UWDEVST_PERC_TEST_BUILD)
PercSynthAudioProcessorEditor::VisualLayoutSnapshot
PercSynthAudioProcessorEditor::computeVisualLayoutSnapshot(int width, int height) const
{
    VisualLayoutSnapshot snapshot;
    snapshot.compact = computeLayoutMetrics(width, height).compact;
    return snapshot;
}

PercSynthAudioProcessorEditor::LayoutSnapshot
PercSynthAudioProcessorEditor::captureLayoutSnapshotForTests() const
{
    LayoutSnapshot snapshot;
    const auto visual = computeVisualLayoutSnapshot(getWidth(), getHeight());
    const auto layout = computeLayoutMetrics(getWidth(), getHeight());
    snapshot.compact = visual.compact;
    snapshot.editorBounds = getLocalBounds();
    snapshot.headerBounds = unionBounds({
        presetBox.getBounds(),
        presetSearch.getBounds(),
        prevPresetBtn.getBounds(),
        nextPresetBtn.getBounds(),
        savePresetBtn.getBounds(),
        saveAsPresetBtn.getBounds(),
        deletePresetBtn.getBounds(),
        familySelectorLbl.getBounds(),
        familySelector.getBounds(),
        qualitySelector.getBounds(),
        tooltipModeBtn.getBounds(),
        midiCCPageLabel.getBounds(),
        gainDial.getBounds()
    });
    snapshot.selectorPanelBounds = unionBounds({
        modelSelectorLbl.getBounds(),
        modelSelector.getBounds(),
        presetSourceFilter.getBounds(),
        presetFamilyFilter.getBounds(),
        presetRoleFilter.getBounds(),
        presetTagFilter.getBounds(),
        presetMetaLabel.getBounds(),
        familyTabs[0].getBounds(),
        familyTabs[1].getBounds(),
        familyTabs[2].getBounds(),
        presetCards[0].getBounds(),
        presetCards[1].getBounds(),
        presetCards[2].getBounds(),
        presetCards[3].getBounds(),
        presetCards[4].getBounds(),
        presetCards[5].getBounds(),
        presetCards[6].getBounds(),
        presetCards[7].getBounds(),
        presetCards[8].getBounds()
    });
    snapshot.selectorCardBounds = { layout.contentX, layout.selectorY, layout.contentW, layout.selectorH - 8 };
    snapshot.modelSelectorBounds = modelSelector.getBounds();
    snapshot.presetMetaBounds = presetMetaLabel.getBounds();
    snapshot.qualitySelectorBounds = qualitySelector.getBounds();
    snapshot.statusControlsBounds = unionBounds({
        qualitySelector.getBounds(),
        tooltipModeBtn.getBounds(),
        midiCCPageLabel.getBounds()
    });
    snapshot.outputSelectorBounds = gainDial.getBounds();
    snapshot.outputBayBounds = outputBayBounds;
    snapshot.rightPanelTabsBounds = unionVisibleBounds<SynthEffectTab>({
        &rightPanelTabs[0], &rightPanelTabs[1], &rightPanelTabs[2]
    });
    snapshot.macroControlsBounds = unionBounds({
        unionVisibleBounds<juce::Label>({
            &macroLbls[0], &macroLbls[1], &macroLbls[2], &macroLbls[3]
        }),
        unionVisibleBounds<juce::Slider>({
            &macroDials[0], &macroDials[1], &macroDials[2], &macroDials[3]
        })
    });
    snapshot.lfoVisualBounds = lfoVisual.getBounds();
    snapshot.fxLockBounds = {};
    snapshot.fxLockVisible = false;
    snapshot.modMatrixContentBounds = unionBounds({
        unionVisibleBounds<juce::Label>({
            &modSlotLabels[0], &modSlotLabels[1], &modSlotLabels[2], &modSlotLabels[3],
            &modSlotLabels[4], &modSlotLabels[5], &modSlotLabels[6], &modSlotLabels[7],
            &modLfo2RateLabel, &modLfo2WaveLabel
        }),
        unionVisibleBounds<juce::ComboBox>({
            &modSourceBoxes[0], &modSourceBoxes[1], &modSourceBoxes[2], &modSourceBoxes[3],
            &modSourceBoxes[4], &modSourceBoxes[5], &modSourceBoxes[6], &modSourceBoxes[7],
            &modDestBoxes[0], &modDestBoxes[1], &modDestBoxes[2], &modDestBoxes[3],
            &modDestBoxes[4], &modDestBoxes[5], &modDestBoxes[6], &modDestBoxes[7],
            &modLfo2WaveSelector
        }),
        unionVisibleBounds<juce::Slider>({
            &modAmountSliders[0], &modAmountSliders[1], &modAmountSliders[2], &modAmountSliders[3],
            &modAmountSliders[4], &modAmountSliders[5], &modAmountSliders[6], &modAmountSliders[7],
            &modLfo2RateDial
        })
    });
    snapshot.keyboardBounds = keyboard != nullptr ? keyboard->getBounds() : juce::Rectangle<int>();
    snapshot.octaveControlsBounds = unionBounds({
        octaveDownBtn.getBounds(),
        octaveUpBtn.getBounds()
    });
    return snapshot;
}

void PercSynthAudioProcessorEditor::setRightPanelSectionForTests(int sectionIndex)
{
    switchRightPanelSection(juce::jlimit(0, kRightPanelSections - 1, sectionIndex));
}

juce::String PercSynthAudioProcessorEditor::formatToneCutValueForTests(double hz)
{
    return envDials[12].getTextFromValue(hz);
}
#endif

// =============================================================================
// Constructor
// =============================================================================
PercSynthAudioProcessorEditor::PercSynthAudioProcessorEditor(
    PercSynthAudioProcessor& processor)
    : CommonSynthEditor(processor,
                        processor.getAPVTS(),
                        processor.getKeyboardState(),
                        juce::Colour(0xff4DB6AC),
                        36, 84, 38.0f)
    , proc(processor)
{
    familySelectorLbl.setText("PERC TYPE", juce::dontSendNotification);
    modelSelectorLbl.setText("MODEL", juce::dontSendNotification);

    familySelector.addItem("PERCUSSIONS", 1);
    familySelector.addItem("AMBIANCE", 2);
    familySelector.addItem("METALLIQUES", 3);

    familySelector.onChange = [this]
    {
        const int fi = juce::jlimit(0, mpc::kNumFamilies - 1,
                                    familySelector.getSelectedId() - 1);
        activeFamilyIndex = fi;
        rebuildModelSelectorForFamily(activeFamilyIndex);
        const int selectedId = modelSelector.getSelectedId();
        if (selectedId > 0)
            instrSelector.setSelectedId(selectedId);
    };

    modelSelector.onChange = [this]
    {
        const int selectedId = modelSelector.getSelectedId();
        if (selectedId > 0)
            instrSelector.setSelectedId(selectedId);
    };

    static const char* kFamilyNames[] = { "PERCUSSIONS", "AMBIANCE", "METALLIQUES" };
    for (int f = 0; f < mpc::kNumFamilies; ++f)
    {
        familyTabs[(size_t)f].configure(f, kFamilyNames[f], familyColour(f));
        familyTabs[(size_t)f].onClicked = [this](int idx) { familySelector.setSelectedId(idx + 1, juce::sendNotificationSync); };
        familyTabs[(size_t)f].setVisible(false);
        addChildComponent(familyTabs[(size_t)f]);
    }

    instrSelector.setVisible(false);
    addChildComponent(instrSelector);
    for (int i = 0; i < mpc::kNumInstruments; ++i)
        instrSelector.addItem(juce::String(juce::CharPointer_UTF8(mpc::getInstrName(i))), i + 1);
    selInstrAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(), "selected_instr", instrSelector);
    instrSelector.onChange = [this] {
        rebuildInstrAttachments();
        syncSelectionUiFromInstr();
    };

    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        auto& card = presetCards[(size_t)i];
        card.configure(i, juce::String(juce::CharPointer_UTF8(mpc::getInstrName(i))), instrCatColour(i));
        card.onClicked = [this](int idx) { instrSelector.setSelectedId(idx + 1); };
        addChildComponent(card);
    }

    for (int i = 0; i < kEnvN; ++i)
    {
        auto si = (size_t)i;
        switch (i)
        {
            case 0:
            case 2:
            case 5:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                setupDial(envDials[si], accent_);
                break;
            case 1:
                setupDial(envDials[si], accent_);
                break;
            case 3:
            case 4:
            case 6:
                setupDial(envDials[si], accent_);
                break;
            case 12:
                setupGrandDial(envDials[si], accent_, " Hz");
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
    envVisual.bindAdsr(&envDials[3], &envDials[4], &envDials[5], &envDials[6]);
    addAndMakeVisible(envVisual);

    lfoVisual.setAccent(accent_);
    lfoVisual.setTitle("INTERACTIVE LFO");
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

    qualitySelector.addItem("LIVE", 1);
    qualitySelector.addItem("STUDIO", 2);
    qualityAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "quality_mode", qualitySelector);
    addAndMakeVisible(qualitySelector);

    for (int i = 0; i < kMacroTotal; ++i)
    {
        auto si = (size_t)i;
        macroAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), kMacroCtrls[si].paramId, macroDials[si]);
        setupDial(macroDials[si], accent_);
        if (i < kMacroVisible)
        {
            addAndMakeVisible(macroDials[si]);
            macroLbls[si].setText(kMacroCtrls[si].label, juce::dontSendNotification);
            macroLbls[si].setJustificationType(juce::Justification::centred);
            macroLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
            macroLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
            addAndMakeVisible(macroLbls[si]);
        }
        else addChildComponent(macroDials[si]);
    }

    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.configure(sectionIndex, kRightPanelSectionLabels[sectionIndex], accent_);
        tab.setSelected(sectionIndex == activeRightPanelSection);
        tab.onClicked = [this](int idx) { switchRightPanelSection(idx); };
        addAndMakeVisible(tab);
    }

    for (int i = 0; i < kFxN; ++i)
    {
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

    for (int t = 0; t < kFxTabs; ++t)
    {
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
    fxDetailTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    fxDetailTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(fxDetailTitle);

    fxUnavailableLbl.setText("Unavailable for the selected instrument", juce::dontSendNotification);
    fxUnavailableLbl.setJustificationType(juce::Justification::centred);
    fxUnavailableLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(10.6f)));
    fxUnavailableLbl.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.70f));
    addChildComponent(fxUnavailableLbl);

    delaySyncLabel.setText("SYNC", juce::dontSendNotification);
    delayDivisionLabel.setText("DIV", juce::dontSendNotification);
    delaySyncSelector.addItem("OFF", 1);
    delaySyncSelector.addItem("HOST", 2);
    delayDivisionSelector.addItem("1/4", 1);
    delayDivisionSelector.addItem("1/8", 2);
    delayDivisionSelector.addItem("1/8D", 3);
    delayDivisionSelector.addItem("1/8T", 4);
    delayDivisionSelector.addItem("1/16", 5);
    delayDivisionSelector.addItem("1/16D", 6);
    delaySyncAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "delay_sync", delaySyncSelector);
    delayDivisionAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "delay_division", delayDivisionSelector);
    addChildComponent(delaySyncLabel);
    addChildComponent(delayDivisionLabel);
    addChildComponent(delaySyncSelector);
    addChildComponent(delayDivisionSelector);

    // --- Tooltip mode button ---
    tooltipModeBtn.setButtonText("Tips: Short");
    tooltipModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A32));
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffBBBBCC));
    tooltipModeBtn.onClick = [this] { cycleTooltipMode(); };
    addAndMakeVisible(tooltipModeBtn);

    advancedModBtn.setButtonText("MOD MATRIX");
    advancedModBtn.setClickingTogglesState(true);
    advancedModBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    advancedModBtn.setColour(juce::TextButton::textColourOffId, accent_);
    advancedModBtn.onClick = [this]
    {
        advancedModBtn.setButtonText(advancedModBtn.getToggleState() ? "MOD MATRIX ON" : "MOD MATRIX");
        resized();
        repaint();
    };
    addChildComponent(advancedModBtn);

    presetSourceLabel.setText("SOURCE", juce::dontSendNotification);
    presetFamilyLabel.setText("FAMILY", juce::dontSendNotification);
    presetRoleLabel.setText("ROLE", juce::dontSendNotification);
    presetTagLabel.setText("TAG", juce::dontSendNotification);
    for (auto* label : { &presetSourceLabel, &presetFamilyLabel, &presetRoleLabel, &presetTagLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setColour(juce::Label::textColourId, synthcol::textSec);
        label->setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        addAndMakeVisible(*label);
    }
    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);
    presetMetaLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    presetMetaLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f)));
    presetMetaLabel.setMinimumHorizontalScale(0.85f);
    addAndMakeVisible(presetMetaLabel);

    presetSourceFilter.addItem("ALL", 1);
    presetSourceFilter.addItem("FACTORY", 2);
    presetSourceFilter.addItem("USER", 3);
    presetSourceFilter.setSelectedId(1, juce::dontSendNotification);
    for (auto* combo : { &presetSourceFilter, &presetFamilyFilter, &presetRoleFilter, &presetTagFilter })
        addAndMakeVisible(*combo);
    auto refreshPresetFilters = [this]
    {
        refreshPresetList();
        updatePresetMetadataSummary();
    };
    presetSourceFilter.onChange = refreshPresetFilters;
    presetFamilyFilter.onChange = refreshPresetFilters;
    presetRoleFilter.onChange = refreshPresetFilters;
    presetTagFilter.onChange = refreshPresetFilters;
    applyTooltips();

    // --- MIDI CC page indicator (FLkey Mini) ---
    midiCCPageLabel.setText("CC: ---", juce::dontSendNotification);
    midiCCPageLabel.setJustificationType(juce::Justification::centred);
    midiCCPageLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    midiCCPageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffBBBBCC));
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    midiCCPageLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(midiCCPageLabel);

    modLfo2RateLabel.setText("LFO2 RATE", juce::dontSendNotification);
    modLfo2WaveLabel.setText("LFO2 WAVE", juce::dontSendNotification);
    setupSmallDial(modLfo2RateDial, accent_);
    modLfo2WaveSelector.addItem("SINE", 1);
    modLfo2WaveSelector.addItem("TRI", 2);
    modLfo2WaveSelector.addItem("SAW", 3);
    modLfo2WaveSelector.addItem("SQR", 4);
    addChildComponent(modLfo2RateLabel);
    addChildComponent(modLfo2WaveLabel);
    addChildComponent(modLfo2RateDial);
    addChildComponent(modLfo2WaveSelector);

    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        auto& label = modSlotLabels[static_cast<std::size_t>(slotIndex)];
        auto& src = modSourceBoxes[static_cast<std::size_t>(slotIndex)];
        auto& dst = modDestBoxes[static_cast<std::size_t>(slotIndex)];
        auto& amt = modAmountSliders[static_cast<std::size_t>(slotIndex)];

        label.setText("S" + juce::String(slotIndex + 1), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
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

    modLfo2RateDial.setRange(0.05, 12.0, 0.01);
    modLfo2RateDial.onValueChange = [this]
    {
        proc.setModMatrixLfo2Rate(static_cast<float>(modLfo2RateDial.getValue()));
    };
    modLfo2WaveSelector.onChange = [this]
    {
        proc.setModMatrixLfo2Wave(juce::jmax(0, modLfo2WaveSelector.getSelectedId() - 1));
    };

    rebuildInstrAttachments();
    syncSelectionUiFromInstr();
    syncFxAvailability();
    syncAdvancedModUi();
    switchEffectTab(0);
    backgroundImage_ = juce::ImageCache::getFromMemory(
        BinaryData::fond_perc_png, BinaryData::fond_perc_pngSize);
    applyInstrumentTheme(selectedInstrFromParam());
    initCommon();
    presetSearch.setTextToShowWhenEmpty("Search preset", synthcol::textDim);
    octaveDownBtn.setButtonText("-");
    octaveUpBtn.setButtonText("+");
    octaveDownBtn.setTooltip("Preview one octave down");
    octaveUpBtn.setTooltip("Preview one octave up");
    configureValueDisplays();
    refreshPresetFilterChoices();
    updatePresetMetadataSummary();

    proc.getAPVTS().addParameterListener("selected_instr", this);
    for (const auto* paramId : kFxBypassParamIds)
        proc.getAPVTS().addParameterListener(paramId, this);

    startTimerHz(12);
    setResizable(true, true);
    setResizeLimits(960, 700, 2560, 1600);
    setSize(lay::W, lay::H);
}

PercSynthAudioProcessorEditor::~PercSynthAudioProcessorEditor()
{
    cancelPendingUpdate();
    proc.getAPVTS().removeParameterListener("selected_instr", this);
    for (const auto* paramId : kFxBypassParamIds)
        proc.getAPVTS().removeParameterListener(paramId, this);
}

// =============================================================================
// Timer
// =============================================================================
void PercSynthAudioProcessorEditor::timerCallback()
{
    rebuildInstrAttachments();
    syncPresetBox();
    if (activeRightPanelSection == 1)
        syncAdvancedModUi();
    updatePresetMetadataSummary();

    const int page = proc.getMidiCCPage();
    if (page != cachedMidiCCPage)
    {
        cachedMidiCCPage = page;
        midiCCPageLabel.setText(juce::String("CC: ") + PercSynthAudioProcessor::getCCPageName(page),
                                juce::dontSendNotification);
    }

    repaint();
}

void PercSynthAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void PercSynthAudioProcessorEditor::handleAsyncUpdate()
{
    rebuildInstrAttachments();
    syncSelectionUiFromInstr();
    syncFxAvailability();
    syncPresetBox();
    syncAdvancedModUi();
    refreshPresetFilterChoices();
    configureValueDisplays();
    updatePresetMetadataSummary();
}

// =============================================================================
// Paint
// =============================================================================
void PercSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    paintBackground(g);
    g.setColour(synthcol::bg.withAlpha(0.18f));
    g.fillAll();
    const auto layout = computeLayoutMetrics(getWidth(), getHeight());
    const auto headerZones = computeHeaderZones(layout.headerH, layout.contentX, layout.contentW);
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
    auto outputBay = outputBayBounds;
    if (outputBay.isEmpty())
    {
        const int fallbackBayW = layout.compact ? 56 : 64;
        outputBay = { headerZones.statusZone.getRight() - fallbackBayW - 6,
                      headerZones.statusZone.getY(),
                      fallbackBayW,
                      headerZones.statusZone.getHeight() };
    }

    auto statusPrimaryRow = headerZones.statusPrimaryRow;
    auto statusSecondaryRow = headerZones.statusSecondaryRow;
    statusPrimaryRow.setRight(juce::jmax(statusPrimaryRow.getX(), outputBay.getX() - 8));
    statusSecondaryRow.setRight(juce::jmax(statusSecondaryRow.getX(), outputBay.getX() - 8));

    paintHeader(g, layout.headerH, layout.contentX, layout.contentW);
    glazePercChrome(g, headerRect.reduced(1.5f, 1.5f), accent_, 13.0f, 0.88f);

    paintHeaderCaption(g,
                       { headerZones.identityZone.getX(), headerZones.headerBounds.getY() + 3,
                         headerZones.identityZone.getWidth(), 10 },
                       "BRAND", accent_);
    paintHeaderCaption(g,
                       { headerZones.presetZone.getX(), headerZones.headerBounds.getY() + 3,
                         headerZones.presetZone.getWidth(), 10 },
                       "PRESET BROWSER", accent_);
    paintHeaderCaption(g,
                       { headerZones.statusZone.getX(), headerZones.headerBounds.getY() + 3,
                         headerZones.statusZone.getWidth(), 10 },
                       "SESSION / OUTPUT", accent_);

    g.setColour(accent_.withAlpha(0.04f));
    g.fillRoundedRectangle(statusPrimaryRow.toFloat().withWidth(juce::jmin(120.0f, statusPrimaryRow.getWidth() * 0.34f)), 7.0f);

    const auto gainAnchor = outputBay.toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRoundedRectangle(gainAnchor.translated(0.0f, 2.0f), 9.0f);
    g.setColour(accent_.withAlpha(0.04f));
    g.fillRoundedRectangle(gainAnchor, 9.0f);
    g.setColour(accent_.withAlpha(0.16f));
    g.drawRoundedRectangle(gainAnchor.reduced(0.5f), 9.0f, 0.9f);
    paintStatusChip(g,
                    { static_cast<int>(gainAnchor.getX()) + 3, static_cast<int>(gainAnchor.getY()) + 3,
                      static_cast<int>(gainAnchor.getWidth()) - 6, 13 },
                    "OUTPUT",
                    synthcol::surfHi.withAlpha(0.90f),
                    accent_.withAlpha(0.55f));

    paintCard(g, layout.contentX, layout.outerMargin + layout.headerH + 8,
              layout.contentW, layout.selectorH - 8, "Family / Model");
    paintCard(g, layout.col1X, layout.bodyY, layout.colW, layout.bodyH, "Source & Envelope");
    paintCard(g, layout.col2X, layout.bodyY, layout.colW, layout.bodyH, "Tone Shaping");
    paintCard(g, layout.col3X, layout.bodyY, layout.colW, layout.bodyH, "Macro / LFO / FX");
    glazePercChrome(g, selectorRect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.74f);
    glazePercChrome(g, col1Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.60f);
    glazePercChrome(g, col2Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.60f);
    glazePercChrome(g, col3Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.60f);
    paintPercMatteTexture(g, selectorRect.reduced(10.0f, 10.0f).withTrimmedTop(18.0f), accent_, 8.0f, 0.70f);
    paintPercMatteTexture(g, col1Rect.reduced(10.0f, 10.0f).withTrimmedTop(18.0f), accent_, 8.0f, 0.78f);
    paintPercMatteTexture(g, col2Rect.reduced(10.0f, 10.0f).withTrimmedTop(18.0f), accent_, 8.0f, 0.78f);
    paintPercMatteTexture(g, col3Rect.reduced(10.0f, 10.0f).withTrimmedTop(18.0f), accent_, 8.0f, 0.78f);

    const int meterY = static_cast<int>(gainAnchor.getBottom()) - 14;
    const int meterLeft = static_cast<int>(gainAnchor.getX()) - 48;
    if (meterLeft >= statusSecondaryRow.getX() + 160)
    {
        paintMeterBar(g, { meterLeft, meterY, 20, 6 }, proc.getMainMeterLevel(0), accent_);
        paintMeterBar(g, { meterLeft + 24, meterY, 20, 6 }, proc.getMainMeterLevel(1), accent_.brighter(0.22f));
        g.setColour(synthcol::textDim);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.8f)));
        g.drawText("L", juce::Rectangle<int>(meterLeft - 9, meterY - 3, 9, 12), juce::Justification::centredLeft);
        g.drawText("R", juce::Rectangle<int>(meterLeft + 15, meterY - 3, 9, 12), juce::Justification::centredLeft);
    }

    if (activeRightPanelSection == 0)
    {
        const int cPad = layout.compact ? 13 : 16;
        const int rightPad = cPad + (layout.compact ? 3 : 2);
        const int rightTabH = layout.compact ? 23 : 26;
        const int sectionContentY = layout.bodyY + cPad + 28 + rightTabH + (layout.compact ? 10 : 12);
        const int macroGap = layout.compact ? 7 : 9;
        const int macroW = (layout.colW - rightPad * 2 - macroGap * 3) / 4;
        const int macroH = juce::jlimit(layout.compact ? 44 : 50,
                                        layout.roomy ? 78 : 68,
                                        juce::jmin(macroW, juce::jmax(40, (layout.bodyH - 180) / 3)));
        const int dividerY = sectionContentY + macroH + 24;
        auto divider = juce::Rectangle<float>(static_cast<float>(layout.col3X + rightPad),
                                              static_cast<float>(dividerY),
                                              static_cast<float>(layout.colW - rightPad * 2),
                                              2.0f);
        juce::ColourGradient dividerGrad(juce::Colours::white.withAlpha(0.015f), divider.getCentreX(), divider.getY(),
                                         juce::Colours::black.withAlpha(0.07f), divider.getCentreX(), divider.getBottom(), false);
        g.setGradientFill(dividerGrad);
        g.fillRoundedRectangle(divider, 1.0f);
    }

    g.setColour(accent_.withAlpha(0.12f));
    g.drawLine(static_cast<float>(layout.contentX + 18), static_cast<float>(layout.kbY - 8),
               static_cast<float>(layout.contentX + layout.contentW - 18), static_cast<float>(layout.kbY - 8),
               1.0f);

    paintKeyboardDock(g, layout.contentX, layout.kbY, layout.contentW, layout.kbH);
    glazePercChrome(g, keyboardRect.reduced(2.0f, 2.0f), accent_, 11.0f, 0.84f);
    paintStatusChip(g,
                    { layout.contentX + layout.contentW - 126, layout.kbY + 10, 104, 16 },
                    "PREVIEW KEYS",
                    synthcol::surfHi.withAlpha(0.90f),
                    accent_.withAlpha(0.48f));
}

// =============================================================================
// Resized
// =============================================================================
void PercSynthAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const auto layout = computeLayoutMetrics(w, getHeight());
    const auto headerZones = computeHeaderZones(layout.headerH, layout.contentX, layout.contentW);

    const int ctrlH = layout.compact ? 34 : 36;
    const int gainSize = layout.compact ? 38 : 44;
    const int outputBayW = layout.compact ? 56 : 64;
    outputBayBounds = {
        headerZones.statusZone.getRight() - outputBayW - 6,
        headerZones.statusZone.getY(),
        outputBayW,
        headerZones.statusZone.getHeight()
    };
    const auto presetPrimaryRow = headerZones.presetPrimaryRow.reduced(0, 1);
    const auto presetSecondaryRow = headerZones.presetSecondaryRow.reduced(0, 1);
    auto statusSecondaryRow = headerZones.statusSecondaryRow.reduced(0, 1);
    statusSecondaryRow.setRight(juce::jmax(statusSecondaryRow.getX(), outputBayBounds.getX() - 8));
    const int topRowY = presetPrimaryRow.getY() + (presetPrimaryRow.getHeight() - ctrlH) / 2;

    const int gainX = outputBayBounds.getX() + (outputBayBounds.getWidth() - gainSize) / 2;
    const int gainY = outputBayBounds.getY() + (layout.compact ? 17 : 18);
    gainDial.setBounds(gainX, gainY, gainSize, gainSize);

    const int navW = 26;
    int searchW = juce::jlimit(layout.compact ? 112 : 120,
                               layout.compact ? 144 : 164,
                               presetPrimaryRow.getWidth() / 5);
    int x = presetPrimaryRow.getX();
    const int presetW = juce::jmax(layout.compact ? 220 : 320,
                                   presetPrimaryRow.getRight() - x - searchW - navW * 2 - 16);
    presetSearch.setBounds(x, topRowY, searchW, ctrlH); x += searchW + 8;
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH); x += navW + 4;
    presetBox.setBounds(x, topRowY, presetW, ctrlH); x += presetW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    const int actionBtnH = layout.compact ? 20 : 22;
    const int actionY = presetSecondaryRow.getY() + juce::jmax(0, (presetSecondaryRow.getHeight() - actionBtnH) / 2);
    const int saveW = layout.compact ? 56 : 64;
    const int saveAsW = layout.compact ? 66 : 78;
    const int deleteW = layout.compact ? 62 : 70;
    const int importW = layout.compact ? 62 : 70;
    const int btnGap = 8;
    int actionX = presetSecondaryRow.getX();
    savePresetBtn.setBounds(actionX, actionY, saveW, actionBtnH); actionX += saveW + btnGap;
    saveAsPresetBtn.setBounds(actionX, actionY, saveAsW, actionBtnH); actionX += saveAsW + btnGap;
    deletePresetBtn.setBounds(actionX, actionY, deleteW, actionBtnH); actionX += deleteW + btnGap;
    importPresetsBtn.setBounds(actionX, actionY, importW, actionBtnH);

    const int statusBtnH = layout.compact ? 20 : 22;
    const int statusY = statusSecondaryRow.getY() + juce::jmax(0, (statusSecondaryRow.getHeight() - statusBtnH) / 2);
    int statusX = statusSecondaryRow.getX();
    const int statusGap = 6;
    qualitySelector.setBounds(statusX, statusY, 68, statusBtnH); statusX += 68 + statusGap;
    tooltipModeBtn.setBounds(statusX, statusY, 76, statusBtnH); statusX += 76 + statusGap;
    midiCCPageLabel.setBounds(statusX, statusY,
                              juce::jmax(0, statusSecondaryRow.getRight() - statusX), statusBtnH);

    const int selPad = layout.compact ? 12 : 14;
    const int selectorInnerX = layout.contentX + selPad;
    const int selectorInnerW = layout.contentW - selPad * 2;
    const int selectorTopY = layout.selectorY + (layout.compact ? 19 : 22);
    const int selectorRowH = layout.compact ? 26 : 28;
    const int selectorGap = layout.compact ? 10 : 12;
    const int tabsZoneW = static_cast<int>(selectorInnerW * (layout.compact ? 0.62f : 0.66f));
    const int comboZoneW = selectorInnerW - tabsZoneW - selectorGap;
    const int comboSafeInsetRight = layout.compact ? 4 : 2;
    const int tabGap = layout.compact ? 6 : 8;
    const int tabW = (tabsZoneW - tabGap * (mpc::kNumFamilies - 1)) / mpc::kNumFamilies;
    for (int familyIndex = 0; familyIndex < mpc::kNumFamilies; ++familyIndex)
    {
        auto& tab = familyTabs[(size_t)familyIndex];
        tab.setBounds(selectorInnerX + familyIndex * (tabW + tabGap), selectorTopY, tabW, selectorRowH);
        tab.setVisible(true);
        tab.setSelected(familyIndex == activeFamilyIndex);
    }
    familySelectorLbl.setVisible(false);
    familySelectorLbl.setBounds(0, 0, 0, 0);
    familySelector.setVisible(false);
    familySelector.setBounds(0, 0, 0, 0);
    instrSelector.setVisible(false);
    instrSelector.setBounds(0, 0, 0, 0);
    modelSelectorLbl.setVisible(false);
    modelSelectorLbl.setBounds(0, 0, 0, 0);
    modelSelector.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorTopY,
                            juce::jmax(80, comboZoneW - comboSafeInsetRight), selectorRowH);
    presetSourceLabel.setVisible(false);
    presetSourceLabel.setBounds(0, 0, 0, 0);
    presetFamilyLabel.setVisible(false);
    presetFamilyLabel.setBounds(0, 0, 0, 0);
    presetRoleLabel.setVisible(false);
    presetRoleLabel.setBounds(0, 0, 0, 0);
    presetTagLabel.setVisible(false);
    presetTagLabel.setBounds(0, 0, 0, 0);
    presetSourceFilter.setVisible(false);
    presetSourceFilter.setBounds(0, 0, 0, 0);
    presetFamilyFilter.setVisible(false);
    presetFamilyFilter.setBounds(0, 0, 0, 0);
    presetRoleFilter.setVisible(false);
    presetRoleFilter.setBounds(0, 0, 0, 0);
    presetTagFilter.setVisible(false);
    presetTagFilter.setBounds(0, 0, 0, 0);
    const int metaH = layout.compact ? 16 : 18;
    const int metaY = selectorTopY + selectorRowH + (layout.compact ? 8 : 10);
    presetMetaLabel.setBounds(selectorInnerX + 8, metaY, selectorInnerW - 16, metaH);
    for (auto& card : presetCards)
        card.setBounds(0, 0, 0, 0);

    const int cPad = layout.compact ? 13 : 16;
    const int knobGapX = layout.compact ? 7 : (layout.roomy ? 12 : 10);
    const int knobGapY = layout.compact ? 8 : (layout.roomy ? 12 : 10);
    const int knobW = (layout.colW - cPad * 2 - knobGapX * 2) / 3;
    const int lblH = layout.compact ? 12 : 14;
    const int graphTargetH = layout.compact ? 86 : (layout.roomy ? 178 : 126);
    const int knobH = juce::jlimit(layout.compact ? 50 : 58,
                                   layout.roomy ? 98 : 84,
                                   (layout.bodyH - graphTargetH - cPad * 2 - lblH * 3 - knobGapY * 3) / 3);
    const int protectedKeyboardTop = layout.kbY - (layout.compact ? 14 : 18);

    const int sourceInnerX = layout.col1X + cPad;
    const int sourceInnerW = layout.colW - cPad * 2;
    const int sourceTopY = layout.bodyY + cPad + 28;
    const int sourceBottomY = protectedKeyboardTop - 8;
    const int envH = juce::jlimit(layout.compact ? 132 : 154,
                                  layout.roomy ? 250 : 212,
                                  static_cast<int>((sourceBottomY - sourceTopY) * 0.47f));
    envVisual.setVisible(true);
    envVisual.setBounds(sourceInnerX, sourceTopY, sourceInnerW, envH);

    const int sourceControlsY = envVisual.getBottom() + (layout.compact ? 8 : 10);
    const int adsrGapX = layout.compact ? 6 : 8;
    const int adsrGapY = layout.compact ? 8 : 10;
    const int remainingH = sourceBottomY - sourceControlsY;
    const int adsrW = (sourceInnerW - adsrGapX * 3) / 4;
    const int adsrKnobH = juce::jlimit(layout.compact ? 44 : 50,
                                       layout.roomy ? 78 : 64,
                                       juce::jmax(layout.compact ? 44 : 50,
                                                  (remainingH - lblH * 4 - adsrGapY * 2) / 2));
    const int secondaryGapX = layout.compact ? 8 : 10;
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
    layoutSourceDial(3, sourceInnerX, sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(4, sourceInnerX + (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(5, sourceInnerX + 2 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(6, sourceInnerX + 3 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    const int sourceTailY = sourceControlsY + lblH + adsrKnobH + adsrGapY;
    layoutSourceDial(7, sourceInnerX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(0, sourceInnerX + secondaryW + secondaryGapX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(1, sourceInnerX + 2 * (secondaryW + secondaryGapX), sourceTailY, secondaryW, smallSourceKnobH);

    int toneIdx[] = { 8, 9, 10, 2, 13, 11 };
    const int col2StartY = layout.bodyY + cPad + 28;
    for (int i = 0; i < 6; ++i)
    {
        int row = i / 3, col = i % 3;
        int xk = layout.col2X + cPad + col * (knobW + knobGapX);
        int yk = col2StartY + row * (knobH + lblH + knobGapY);
        auto si = (size_t)toneIdx[i];
        envLabels[si].setBounds(xk, yk, knobW, lblH);
        envDials[si].setBounds(xk, yk + lblH, knobW, knobH);
    }
    const int cutoffSize = juce::jlimit(layout.compact ? 66 : 72,
                                        layout.roomy ? 116 : 104,
                                        knobH + (layout.compact ? 14 : 18));
    const int cutoffX = layout.col2X + (layout.colW - cutoffSize) / 2;
    const int cutoffY = col2StartY + 2 * (knobH + lblH + knobGapY) + (layout.compact ? 6 : 10);
    envLabels[12].setBounds(cutoffX, cutoffY, cutoffSize, lblH);
    envDials[12].setBounds(cutoffX, cutoffY + lblH, cutoffSize, cutoffSize);

    lfoRateDial.setVisible(false);
    lfoDepthDial.setVisible(false);
    lfoWaveSelector.setVisible(false);
    lfoRateDial.setBounds(0, 0, 0, 0);
    lfoDepthDial.setBounds(0, 0, 0, 0);
    lfoWaveSelector.setBounds(0, 0, 0, 0);
    advancedModBtn.setVisible(false);
    advancedModBtn.setBounds(0, 0, 0, 0);
    lfoVisual.setVisible(false);
    lfoVisual.setBounds(0, 0, 0, 0);

    const int col3StartY = layout.bodyY + cPad + 28;
    const int rightPad = cPad + (layout.compact ? 3 : 2);
    const int rightTabGap = layout.compact ? 6 : 8;
    const int rightTabH = layout.compact ? 23 : 26;
    const int rightTabW = (layout.colW - rightPad * 2 - rightTabGap * (kRightPanelSections - 1)) / kRightPanelSections;
    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.setBounds(layout.col3X + rightPad + sectionIndex * (rightTabW + rightTabGap), col3StartY, rightTabW, rightTabH);
        tab.setSelected(sectionIndex == activeRightPanelSection);
    }
    const int sectionContentY = col3StartY + rightTabH + (layout.compact ? 10 : 12);

    const int macroGap = layout.compact ? 7 : 9;
    const int macroW = (layout.colW - rightPad * 2 - macroGap * 3) / 4;
    const int macroH = juce::jlimit(layout.compact ? 44 : 50,
                                    layout.roomy ? 78 : 68,
                                    juce::jmin(macroW, knobH - (layout.compact ? 6 : 10)));
    for (int i = 0; i < kMacroVisible; ++i)
    {
        auto si = (size_t)i;
        macroLbls[si].setVisible(activeRightPanelSection == 0);
        macroDials[si].setVisible(activeRightPanelSection == 0);
        if (activeRightPanelSection == 0)
        {
            const int xk = layout.col3X + rightPad + i * (macroW + macroGap);
            macroLbls[si].setBounds(xk, sectionContentY, macroW, lblH);
            macroDials[si].setBounds(xk, sectionContentY + lblH, macroW, macroH);
        }
        else
        {
            macroLbls[si].setBounds(0, 0, 0, 0);
            macroDials[si].setBounds(0, 0, 0, 0);
        }
    }

    if (activeRightPanelSection == 0)
    {
        const int lfoVisualY = sectionContentY + macroH + lblH + (layout.compact ? 10 : 12);
        const int lfoVisualH = juce::jmax(84, protectedKeyboardTop - lfoVisualY - 8);
        lfoVisual.setVisible(true);
        lfoVisual.setBounds(layout.col3X + rightPad, lfoVisualY, layout.colW - rightPad * 2, lfoVisualH);
    }

    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        modSlotLabels[(size_t)slotIndex].setVisible(false);
        modSourceBoxes[(size_t)slotIndex].setVisible(false);
        modDestBoxes[(size_t)slotIndex].setVisible(false);
        modAmountSliders[(size_t)slotIndex].setVisible(false);
        modSlotLabels[(size_t)slotIndex].setBounds(0, 0, 0, 0);
        modSourceBoxes[(size_t)slotIndex].setBounds(0, 0, 0, 0);
        modDestBoxes[(size_t)slotIndex].setBounds(0, 0, 0, 0);
        modAmountSliders[(size_t)slotIndex].setBounds(0, 0, 0, 0);
    }
    modLfo2RateLabel.setVisible(false);
    modLfo2WaveLabel.setVisible(false);
    modLfo2RateDial.setVisible(false);
    modLfo2WaveSelector.setVisible(false);
    modLfo2RateLabel.setBounds(0, 0, 0, 0);
    modLfo2WaveLabel.setBounds(0, 0, 0, 0);
    modLfo2RateDial.setBounds(0, 0, 0, 0);
    modLfo2WaveSelector.setBounds(0, 0, 0, 0);

    if (activeRightPanelSection == 1)
    {
        const int modAreaX = layout.col3X + rightPad;
        const int modAreaW = layout.colW - rightPad * 2;
        const int matrixY = sectionContentY;
        const int controlGap = layout.compact ? 5 : 6;
        const int modAreaH = juce::jmax(80, protectedKeyboardTop - matrixY - 10);
        const int footerReserve = layout.compact ? 86 : 96;
        const int rowGap = layout.compact ? 4 : 5;
        const int matrixRowsH = juce::jmax(96, modAreaH - footerReserve);
        const int rowH = juce::jlimit(20, 28,
                                      (matrixRowsH - rowGap * juce::jmax(0, kModSlots - 1)) / kModSlots);
        const int amountSize = juce::jlimit(layout.compact ? 22 : 24,
                                            layout.compact ? 28 : 30,
                                            rowH + 2);
        const int labelW = 18;
        const int comboAreaW = modAreaW - labelW - amountSize - controlGap * 3;
        const int sourceW = juce::jmax(74, comboAreaW / 2);
        const int destW = juce::jmax(74, comboAreaW - sourceW);
        const int comboH = juce::jlimit(20, 22, rowH);
        const int maxRowBottom = matrixY + matrixRowsH;
        int visibleSlotCount = 0;
        for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
        {
            const int rowY = matrixY + slotIndex * (rowH + rowGap);
            if (rowY + rowH > maxRowBottom)
                break;
            const int comboY = rowY + juce::jmax(0, (rowH - comboH) / 2);
            const int amountY = rowY + juce::jmax(0, (rowH - amountSize) / 2);
            modSlotLabels[(size_t)slotIndex].setBounds(modAreaX, rowY + juce::jmax(0, (rowH - 16) / 2), labelW, 16);
            modSourceBoxes[(size_t)slotIndex].setBounds(modAreaX + labelW + controlGap, comboY, sourceW, comboH);
            modDestBoxes[(size_t)slotIndex].setBounds(modAreaX + labelW + controlGap * 2 + sourceW, comboY, destW, comboH);
            modAmountSliders[(size_t)slotIndex].setBounds(modAreaX + modAreaW - amountSize, amountY, amountSize, amountSize);
            ++visibleSlotCount;
        }
        for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
        {
            const bool vis = slotIndex < visibleSlotCount;
            modSlotLabels[(size_t)slotIndex].setVisible(vis);
            modSourceBoxes[(size_t)slotIndex].setVisible(vis);
            modDestBoxes[(size_t)slotIndex].setVisible(vis);
            modAmountSliders[(size_t)slotIndex].setVisible(vis);
        }
        const int footerY = matrixY + visibleSlotCount * (rowH + rowGap) + 10;
        if (footerY + 40 <= protectedKeyboardTop)
        {
            const int footerLabelW = modAreaW / 2 - controlGap;
            modLfo2RateLabel.setBounds(modAreaX, footerY, footerLabelW, 14);
            modLfo2WaveLabel.setBounds(modAreaX + footerLabelW + controlGap, footerY, footerLabelW, 14);
            modLfo2RateDial.setBounds(modAreaX, footerY + 12, footerLabelW, juce::jmin(66, modAreaW / 2));
            modLfo2WaveSelector.setBounds(modAreaX + footerLabelW + controlGap, footerY + 18, footerLabelW, 22);
            modLfo2RateLabel.setVisible(true);
            modLfo2WaveLabel.setVisible(true);
            modLfo2RateDial.setVisible(true);
            modLfo2WaveSelector.setVisible(true);
        }
    }

    const int instrIdx = selectedInstrFromParam();
    const bool fxAvailableForInstr = isFxTabAvailable(activeFxTab, instrIdx);
    fxDetailTitle.setVisible(activeRightPanelSection == 2);
    fxUnavailableLbl.setVisible(activeRightPanelSection == 2 && !fxAvailableForInstr);
    delaySyncLabel.setVisible(false);
    delayDivisionLabel.setVisible(false);
    delaySyncSelector.setVisible(false);
    delayDivisionSelector.setVisible(false);
    for (int i = 0; i < kFxN; ++i)
    {
        fxDials[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setBounds(0, 0, 0, 0);
        fxLbls[(size_t)i].setBounds(0, 0, 0, 0);
    }

    const int fxAreaY = sectionContentY + 24;
    const int fxAreaH = juce::jmax(156, protectedKeyboardTop - fxAreaY - 10);
    constexpr int kBypassW = 38;
    constexpr int kRackGap = 10;
    constexpr int kRackRowGap = 4;
    const int rackTotalW = juce::jlimit(layout.compact ? 112 : 124,
                                        layout.roomy ? 166 : 152,
                                        layout.colW / 3 + 12);
    const int rackItemW = rackTotalW - kBypassW - 8;
    const int rackRowH = juce::jlimit(layout.compact ? 18 : 20,
                                      layout.roomy ? 32 : 28,
                                      (fxAreaH - kRackRowGap * (kFxTabs - 1)) / kFxTabs);
    int availableFxTabs = 0;
    for (int t = 0; t < kFxTabs; ++t)
        if (isFxTabAvailable(t, instrIdx)) ++availableFxTabs;
    const int rackBlockH = availableFxTabs > 0
        ? availableFxTabs * rackRowH + kRackRowGap * (availableFxTabs - 1) : 0;
    const int rackStartY = fxAreaY + juce::jmax(0, (fxAreaH - rackBlockH) / 2);
    int visibleRow = 0;
    for (int t = 0; t < kFxTabs; ++t)
    {
        const bool available = activeRightPanelSection == 2 && isFxTabAvailable(t, instrIdx);
        fxRackItems[(size_t)t].setVisible(available);
        fxBypassBtns[(size_t)t].setVisible(available);
        if (!available)
        {
            fxRackItems[(size_t)t].setBounds(0, 0, 0, 0);
            fxBypassBtns[(size_t)t].setBounds(0, 0, 0, 0);
            continue;
        }
        const int rowY = rackStartY + visibleRow * (rackRowH + kRackRowGap);
        fxRackItems[(size_t)t].setBounds(layout.col3X + rightPad, rowY, rackItemW, rackRowH);
        fxBypassBtns[(size_t)t].setBounds(layout.col3X + rightPad + rackItemW + 6, rowY + (rackRowH - 18) / 2, kBypassW, 18);
        ++visibleRow;
    }

    const int detailX = layout.col3X + rightPad + rackTotalW + kRackGap + 2;
    const int detailW = juce::jmax(120, layout.col3X + layout.colW - rightPad - detailX);
    fxDetailTitle.setBounds(detailX, fxAreaY, detailW, 16);
    fxUnavailableLbl.setBounds(detailX, fxAreaY + 24, detailW, juce::jmax(44, fxAreaH - 28));
    if (activeRightPanelSection == 2)
    {
        fxDetailTitle.setText(juce::String("DETAIL: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
        if (!fxAvailableForInstr)
        {
            fxDetailTitle.setBounds(detailX, fxAreaY + juce::jmax(0, fxAreaH / 2 - 30), detailW, 16);
            fxUnavailableLbl.setBounds(detailX, fxDetailTitle.getBottom() + 8, detailW, 36);
        }
        int visibleCount = 0;
        for (int k = 0; k < kFxPerTab; ++k)
            if (kFxTabMap[activeFxTab][k] >= 0)
                ++visibleCount;
        int detailCols = 1;
        if (visibleCount >= 2)
            detailCols = 2;
        if (detailW > 250 && visibleCount >= 5)
            detailCols = 3;
        const int detailRows = juce::jmax(1, (visibleCount + detailCols - 1) / detailCols);
        const int detailGap = layout.compact ? 6 : 8;
        int detailGridY = fxAreaY + 28;
        if (activeFxTab == 6)
        {
            delaySyncLabel.setBounds(detailX, fxAreaY + 18, 40, 12);
            delaySyncSelector.setBounds(detailX, fxAreaY + 30, juce::jmin(84, detailW / 2), 22);
            delayDivisionLabel.setBounds(delaySyncSelector.getRight() + detailGap, fxAreaY + 18, 36, 12);
            delayDivisionSelector.setBounds(delaySyncSelector.getRight() + detailGap, fxAreaY + 30,
                                            juce::jmin(90, juce::jmax(72, detailW / 2 - 20)), 22);
            delaySyncLabel.setVisible(true);
            delaySyncSelector.setVisible(true);
            delayDivisionLabel.setVisible(true);
            delayDivisionSelector.setVisible(true);
            detailGridY += 36;
        }
        const int detailAvailH = juce::jmax(72, protectedKeyboardTop - detailGridY - 10);
        const int detailCellH = (detailAvailH - detailGap * (detailRows - 1)) / detailRows;
        const int detailKnobW = (detailW - detailGap * (detailCols - 1)) / detailCols;
        const int detailKnobH = juce::jlimit(layout.compact ? 32 : 40,
                                             layout.roomy ? 120 : 96,
                                             juce::jmin(detailKnobW, detailCellH - lblH));
        const int detailRowStride = lblH + detailKnobH + detailGap;
        const int detailGridTotalH = detailRows * (lblH + detailKnobH) + (detailRows - 1) * detailGap;
        const int detailStartY = detailGridY + juce::jmax(0, (detailAvailH - detailGridTotalH) / 2);
        int visibleIndex = 0;
        for (int k = 0; k < kFxPerTab; ++k)
        {
            const int fi = kFxTabMap[activeFxTab][k];
            if (fi < 0)
                continue;
            auto si = static_cast<std::size_t>(fi);
            const int row = visibleIndex / detailCols;
            const int col = visibleIndex % detailCols;
            const int xk = detailX + col * (detailKnobW + detailGap);
            const int yk = detailStartY + row * detailRowStride;
            fxLbls[si].setBounds(xk, yk, detailKnobW, lblH);
            fxDials[si].setBounds(xk, yk + lblH, detailKnobW, detailKnobH);
            fxLbls[si].setVisible(true);
            fxDials[si].setVisible(true);
            ++visibleIndex;
        }
    }

    const int keyboardInsetLeft = 72;
    const int keyboardInsetRight = 12;
    const int keyboardInsetTop = layout.compact ? 6 : 8;
    const int keyboardH = juce::jmax(36, layout.kbH - keyboardInsetTop * 2);
    const int keyboardCenterY = layout.kbY + keyboardInsetTop + keyboardH / 2;
    keyboard->setBounds(layout.contentX + keyboardInsetLeft, layout.kbY + keyboardInsetTop,
                        layout.contentW - keyboardInsetLeft - keyboardInsetRight, keyboardH);
    const int octaveButtonW = 24;
    const int octaveButtonGap = 6;
    const int octaveButtonsX = layout.contentX + 6 + (60 - (octaveButtonW * 2 + octaveButtonGap)) / 2;
    octaveDownBtn.setBounds(octaveButtonsX, keyboardCenterY - 14, octaveButtonW, 26);
    octaveUpBtn.setBounds(octaveButtonsX + octaveButtonW + octaveButtonGap, keyboardCenterY - 14, octaveButtonW, 26);

    auto applyLbl = [layout](juce::Label& l) {
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(layout.compact ? 11.0f : 12.0f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, synthcol::textSec);
    };
    for (auto& l : envLabels) applyLbl(l);
    for (auto& l : macroLbls) applyLbl(l);
    for (auto& l : fxLbls) applyLbl(l);
    for (auto& l : modSlotLabels) applyLbl(l);
    applyLbl(delaySyncLabel);
    applyLbl(delayDivisionLabel);
    applyLbl(modLfo2RateLabel);
    applyLbl(modLfo2WaveLabel);
}

// =============================================================================
// Switch effect tab
// =============================================================================
void PercSynthAudioProcessorEditor::switchEffectTab(int tabIndex)
{
    const int instrIdx = selectedInstrFromParam();
    if (!isFxTabAvailable(tabIndex, instrIdx))
        tabIndex = firstAvailableFxTab(instrIdx);

    const bool tabChanged = activeFxTab != tabIndex;
    activeFxTab = tabIndex;
    fxDetailTitle.setText(juce::String("DETAIL: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);

    for (int i = 0; i < kFxN; ++i)
    {
        fxDials[(size_t)i].setVisible(false);
        fxLbls [(size_t)i].setVisible(false);
    }

    for (int k = 0; k < kFxPerTab; ++k)
    {
        int fi  = kFxTabMap[activeFxTab][k];
        if (fi < 0)
            continue;
        auto si = (size_t)fi;
        fxDials[si].setVisible(true);
        fxLbls [si].setVisible(true);
        fxLbls [si].setText(kFxCtrls[si].label, juce::dontSendNotification);
    }

    const bool fxAvailableForInstr = isFxTabAvailable(activeFxTab, selectedInstrFromParam());
    fxUnavailableLbl.setVisible(!fxAvailableForInstr);

    configureValueDisplays();
    syncFxRackState();
    if (tabChanged)
        resized();
}

void PercSynthAudioProcessorEditor::switchRightPanelSection(int sectionIndex)
{
    sectionIndex = juce::jlimit(0, kRightPanelSections - 1, sectionIndex);
    if (activeRightPanelSection == sectionIndex)
        return;

    activeRightPanelSection = sectionIndex;
    if (activeRightPanelSection == 2)
        syncFxAvailability();
    if (activeRightPanelSection == 1)
        syncAdvancedModUi();

    configureValueDisplays();
    resized();
    repaint();
}

void PercSynthAudioProcessorEditor::syncFxRackState()
{
    const int instrIdx = selectedInstrFromParam();
    for (int t = 0; t < kFxTabs; ++t)
    {
        if (!isFxTabAvailable(t, instrIdx)) continue; // item est masqué dans resized()
        auto& rackItem = fxRackItems[(size_t)t];
        auto& bypass = fxBypassBtns[(size_t)t];
        rackItem.setEnabled(true);
        rackItem.setSelected(t == activeFxTab);
        rackItem.setEnabledState(bypass.getToggleState());
    }
}

void PercSynthAudioProcessorEditor::applyInstrumentTheme(int instrIndex)
{
    const auto theme = makePercChromeTheme(instrCatColour(instrIndex));
    const auto catC = theme.accent;
    const auto controlText = juce::Colour(0xffE3E8ED).interpolatedWith(catC, 0.10f);
    const auto panelBg = juce::Colour(0xff161B1E).interpolatedWith(theme.panelCavityTint, 0.10f);
    const auto actionBg = juce::Colour(0xff20272D).interpolatedWith(theme.panelHeaderTint, 0.14f);
    const auto destructiveBg = juce::Colour(0xff27191B).interpolatedWith(juce::Colour(0xff9E4040), 0.30f);
    const auto badgeBg = juce::Colour(0xff12161A).interpolatedWith(theme.panelBaseTint, 0.12f);
    const auto modeBg = juce::Colour(0xff171F23).interpolatedWith(theme.panelHeaderTint, 0.18f);

    setAccentTheme(catC);
    setChromePalette(theme.headerTint, theme.panelBaseTint, theme.panelCavityTint, theme.panelHeaderTint, theme.keyboardTint);
    envVisual.setAccent(catC);
    lfoVisual.setAccent(catC);

    for (auto& dial : envDials)
        applyKnobChrome(dial, theme);

    for (auto& dial : macroDials)
        applyKnobChrome(dial, theme);

    for (auto& dial : fxDials)
        applyKnobChrome(dial, theme);

    applyKnobChrome(modLfo2RateDial, theme);
    applyKnobChrome(gainDial, theme);
    for (auto& dial : modAmountSliders)
        applyKnobChrome(dial, theme);

    fxDetailTitle.setColour(juce::Label::textColourId, catC.brighter(0.26f));
    fxUnavailableLbl.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.72f));
    presetMetaLabel.setColour(juce::Label::textColourId, catC.brighter(0.15f));
    presetSearch.setColour(juce::TextEditor::backgroundColourId, badgeBg);
    presetSearch.setColour(juce::TextEditor::outlineColourId, catC.withAlpha(0.18f));
    qualitySelector.setColour(juce::ComboBox::backgroundColourId, modeBg);
    qualitySelector.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.48f));
    delaySyncSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    delaySyncSelector.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    delayDivisionSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    delayDivisionSelector.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    presetSourceFilter.setColour(juce::ComboBox::backgroundColourId, panelBg);
    presetSourceFilter.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    presetFamilyFilter.setColour(juce::ComboBox::backgroundColourId, panelBg);
    presetFamilyFilter.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    presetRoleFilter.setColour(juce::ComboBox::backgroundColourId, panelBg);
    presetRoleFilter.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    presetTagFilter.setColour(juce::ComboBox::backgroundColourId, panelBg);
    presetTagFilter.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    modLfo2WaveSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    modLfo2WaveSelector.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    for (auto& combo : modSourceBoxes)
    {
        combo.setColour(juce::ComboBox::backgroundColourId, panelBg);
        combo.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    }
    for (auto& combo : modDestBoxes)
    {
        combo.setColour(juce::ComboBox::backgroundColourId, panelBg);
        combo.setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    }
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, badgeBg);
    prevPresetBtn.setColour(juce::TextButton::textColourOffId, synthcol::textSec);
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, badgeBg);
    nextPresetBtn.setColour(juce::TextButton::textColourOffId, synthcol::textSec);
    savePresetBtn.setColour(juce::TextButton::buttonColourId, actionBg);
    savePresetBtn.setColour(juce::TextButton::textColourOffId, synthcol::text);
    saveAsPresetBtn.setColour(juce::TextButton::buttonColourId, actionBg);
    saveAsPresetBtn.setColour(juce::TextButton::textColourOffId, synthcol::text);
    importPresetsBtn.setColour(juce::TextButton::buttonColourId, actionBg);
    importPresetsBtn.setColour(juce::TextButton::textColourOffId, synthcol::text);
    deletePresetBtn.setColour(juce::TextButton::buttonColourId, destructiveBg);
    deletePresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffF2CCCC));
    advancedModBtn.setColour(juce::TextButton::buttonColourId, panelBg);
    advancedModBtn.setColour(juce::TextButton::textColourOffId, catC);
    tooltipModeBtn.setColour(juce::TextButton::buttonColourId, badgeBg);
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId, synthcol::textDim);
    octaveDownBtn.setColour(juce::TextButton::buttonColourId, badgeBg);
    octaveDownBtn.setColour(juce::TextButton::textColourOffId, controlText);
    octaveUpBtn.setColour(juce::TextButton::buttonColourId, badgeBg);
    octaveUpBtn.setColour(juce::TextButton::textColourOffId, controlText);
    midiCCPageLabel.setColour(juce::Label::textColourId, controlText);
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, badgeBg.withAlpha(0.88f));
    midiCCPageLabel.setColour(juce::Label::outlineColourId, catC.withAlpha(0.24f));
    for (auto& tab : rightPanelTabs)
        tab.setAccent(catC);

    for (auto& rackItem : fxRackItems)
        rackItem.setAccent(catC);

    syncFxRackState();
    configureValueDisplays();
}

void PercSynthAudioProcessorEditor::syncAdvancedModUi()
{
    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        const auto slot = proc.getModMatrixSlot(slotIndex);
        modSourceBoxes[static_cast<std::size_t>(slotIndex)].setSelectedId(static_cast<int>(slot.source) + 1,
                                                                          juce::dontSendNotification);
        modDestBoxes[static_cast<std::size_t>(slotIndex)].setSelectedId(static_cast<int>(slot.destination) + 1,
                                                                        juce::dontSendNotification);
        modAmountSliders[static_cast<std::size_t>(slotIndex)].setValue(slot.amount, juce::dontSendNotification);
        modAmountSliders[static_cast<std::size_t>(slotIndex)].setTooltip("Mod amount: " + formatSignedPercent(slot.amount));
    }

    modLfo2RateDial.setValue(proc.getModMatrixLfo2Rate(), juce::dontSendNotification);
    modLfo2WaveSelector.setSelectedId(proc.getModMatrixLfo2Wave() + 1, juce::dontSendNotification);
}

bool PercSynthAudioProcessorEditor::isFxTabAvailable(int tabIndex, int instrIndex) const
{
    switch (tabIndex)
    {
        case 0: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Reverb);
        case 1: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Saturator);
        case 2: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Transient);
        case 3: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Compressor);
        case 4: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Eq);
        case 5: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Chorus);
        case 6: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Delay);
        case 7: return mpc::isFxAvailable(instrIndex, mpc::GlobalFxSlot::Limiter);
        default: return true;
    }
}

int PercSynthAudioProcessorEditor::firstAvailableFxTab(int instrIndex) const
{
    for (int tabIndex = 0; tabIndex < kFxTabs; ++tabIndex)
        if (isFxTabAvailable(tabIndex, instrIndex))
            return tabIndex;
    return 0;
}

void PercSynthAudioProcessorEditor::syncFxAvailability()
{
    const int instrIdx = selectedInstrFromParam();
    for (int tabIndex = 0; tabIndex < kFxTabs; ++tabIndex)
    {
        const bool available = isFxTabAvailable(tabIndex, instrIdx);
        const auto tooltip = available
            ? juce::String(kFxTabNames[tabIndex]) + " available for " + juce::String(juce::CharPointer_UTF8(mpc::getInstrName(instrIdx)))
            : juce::String(kFxTabNames[tabIndex]) + " unavailable for " + juce::String(juce::CharPointer_UTF8(mpc::getInstrName(instrIdx)));
        fxRackItems[(size_t)tabIndex].setTooltip(tooltip);
        fxBypassBtns[(size_t)tabIndex].setTooltip(tooltip);
        fxBypassBtns[(size_t)tabIndex].setEnabled(available);
        fxBypassBtns[(size_t)tabIndex].setButtonText(available ? "ON" : "N/A");
    }

    if (!isFxTabAvailable(activeFxTab, instrIdx))
        switchEffectTab(firstAvailableFxTab(instrIdx));
    else
        syncFxRackState();
}

// =============================================================================
// Instr attachment management
// =============================================================================
void PercSynthAudioProcessorEditor::rebuildInstrAttachments()
{
    auto instrIdx = selectedInstrFromParam();
    if (instrIdx == cachedInstrIdx) return;
    cachedInstrIdx = instrIdx;
    const auto family = mpc::getFamily(instrIdx);
    const auto& profile = envProfileForInstrument(instrIdx);
    const auto& macroLabels = macroLabelsForFamily(family);

    for (auto& a : envAttach) a.reset();

    for (int i = 0; i < kEnvN; ++i)
    {
        auto si = (size_t)i;
        auto id = PercSynthAudioProcessor::makeInstrParamId(
            cachedInstrIdx, kEnvCtrls[si].suffix);
        envAttach[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), id, envDials[si]);
    }

    for (int i = 0; i < kEnvN; ++i)
    {
        const auto si = static_cast<std::size_t>(i);
        switch (i)
        {
            case 0:
            case 2:
            case 5:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                setupDial(envDials[si], accent_);
                break;
            case 1:
                setupDial(envDials[si], accent_);
                break;
            case 3:
            case 4:
            case 6:
                setupDial(envDials[si], accent_);
                break;
            case 12:
                setupGrandDial(envDials[si], accent_, " Hz");
                break;
            case 13:
                setupDial(envDials[si], accent_);
                break;
            default:
                setupDial(envDials[si], accent_);
                break;
        }
    }

    configureValueDisplays();
    synthui::applyLabelProfile(profile, envLabels);
    synthui::applyMacroLabelProfile(macroLabels, macroLbls);

    // T5: Hide Damping knob (index 7) for instruments without body resonator
    if (!mpc::hasBodyResonator(instrIdx))
    {
        envDials[7].setVisible(false);
        envDials[7].setBounds(0, 0, 0, 0);
        envLabels[7].setVisible(false);
        envLabels[7].setBounds(0, 0, 0, 0);
        envAttach[7].reset(); // detach to prevent host automation of hidden param
    }
    else
    {
        envDials[7].setVisible(true);
    }

    // T6: Hide Body knob (index 8) for instruments without body resonator
    if (!mpc::hasBodyResonator(instrIdx))
    {
        envDials[8].setVisible(false);
        envDials[8].setBounds(0, 0, 0, 0);
        envLabels[8].setVisible(false);
        envLabels[8].setBounds(0, 0, 0, 0);
        envAttach[8].reset();
    }
    else
    {
        envDials[8].setVisible(true);
    }

    // T7: oneShotDecayMs (index 15) — show only when oneShot is enabled
    const auto& apvts = proc.getAPVTS();
    const auto oneShotParam = apvts.getParameter(PercSynthAudioProcessor::makeInstrParamId(instrIdx, "oneShot"));
    const bool oneShotActive = oneShotParam != nullptr && oneShotParam->getValue() >= 0.5f;
    if (!oneShotActive)
    {
        envDials[15].setVisible(false);
        envDials[15].setBounds(0, 0, 0, 0);
        envLabels[15].setVisible(false);
        envLabels[15].setBounds(0, 0, 0, 0);
        envAttach[15].reset();
    }
    else
    {
        envDials[15].setVisible(true);
    }

    applyInstrumentTheme(instrIdx);

    activeFamilyIndex = static_cast<int>(family);
    syncSelectionUiFromInstr();
    syncFxAvailability();
    applyTooltips();
    refreshPresetFilterChoices();
    syncAdvancedModUi();

    // Refresh preset browser to show presets for the newly selected instrument
    refreshPresetList();
    updatePresetMetadataSummary();

    repaint();
}

void PercSynthAudioProcessorEditor::configureValueDisplays()
{
    const auto percentFromText = [](const juce::String& text) { return parseUiNumber(text) / 100.0; };
    const auto signedPercentFromText = [](const juce::String& text) { return parseUiNumber(text) / 100.0; };
    const auto secondsFromText = [](const juce::String& text)
    {
        const auto raw = parseUiNumber(text);
        return text.containsIgnoreCase("ms") ? raw / 1000.0 : raw;
    };
    const auto millisecondsFromText = [](const juce::String& text)
    {
        const auto raw = parseUiNumber(text);
        return text.containsIgnoreCase("ms") || !text.containsIgnoreCase("s") ? raw : raw * 1000.0;
    };
    const auto frequencyFromText = [](const juce::String& text)
    {
        const auto lowered = text.toLowerCase();
        const auto raw = parseUiNumber(lowered);
        return lowered.containsChar('k') ? raw * 1000.0 : raw;
    };
    const auto panFromText = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.equalsIgnoreCase("Center") || trimmed.equalsIgnoreCase("C"))
            return 0.0;
        if (trimmed.startsWithIgnoreCase("L"))
            return -juce::jlimit(0.0, 100.0, parseUiNumber(trimmed)) / 100.0;
        if (trimmed.startsWithIgnoreCase("R"))
            return juce::jlimit(0.0, 100.0, parseUiNumber(trimmed)) / 100.0;
        return juce::jlimit(-1.0, 1.0, parseUiNumber(trimmed));
    };
    const auto clearSuffix = [](juce::Slider& slider) { slider.setTextValueSuffix({}); };

    auto setPercentDisplay = [&](juce::Slider& slider, int width = 64)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, width, 18);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatPercent01(value); };
        slider.valueFromTextFunction = percentFromText;
    };

    auto setSignedPercentDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatSignedPercent(value); };
        slider.valueFromTextFunction = signedPercentFromText;
    };

    auto setSecondsDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatTimeSeconds(value); };
        slider.valueFromTextFunction = secondsFromText;
    };

    auto setMillisecondsDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatMilliseconds(value); };
        slider.valueFromTextFunction = millisecondsFromText;
    };

    auto setFrequencyDisplay = [&](juce::Slider& slider, int width = 90)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, width, 18);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatFrequency(value); };
        slider.valueFromTextFunction = frequencyFromText;
    };

    clearSuffix(envDials[0]);
    envDials[0].textFromValueFunction = [](double value) { return formatPercent01(value); };
    envDials[0].valueFromTextFunction = percentFromText;
    envDials[0].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);

    clearSuffix(envDials[1]);
    envDials[1].textFromValueFunction = [](double value) { return formatSemitones(value); };
    envDials[1].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-24.0, 24.0, parseUiNumber(text)); };
    envDials[1].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);

    setPercentDisplay(envDials[2]);
    setSecondsDisplay(envDials[3]);
    setSecondsDisplay(envDials[4]);
    setPercentDisplay(envDials[5]);
    setSecondsDisplay(envDials[6]);
    setPercentDisplay(envDials[7]);
    setPercentDisplay(envDials[8]);
    setPercentDisplay(envDials[9]);
    setPercentDisplay(envDials[10]);
    setPercentDisplay(envDials[11]);
    setFrequencyDisplay(envDials[12], 92);

    clearSuffix(envDials[13]);
    envDials[13].textFromValueFunction = [](double value) { return formatPanValue(value); };
    envDials[13].valueFromTextFunction = panFromText;
    envDials[13].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);

    // oneShot (index 14) — bool; display as ON/OFF
    envDials[14].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    clearSuffix(envDials[14]);
    envDials[14].textFromValueFunction = [](double value) { return value >= 0.5 ? "ON" : "OFF"; };
    envDials[14].valueFromTextFunction = [](const juce::String& text)
        { return text.equalsIgnoreCase("ON") ? 1.0 : 0.0; };

    // oneShotDecayMs (index 15) — milliseconds
    setMillisecondsDisplay(envDials[15]);

    for (auto& slider : macroDials)
        setPercentDisplay(slider, 66);

    setMillisecondsDisplay(lfoRateDial);
    clearSuffix(lfoRateDial);
    lfoRateDial.textFromValueFunction = [](double value) { return trimValueString(value, value < 10.0 ? 2 : 1) + " Hz"; };
    lfoRateDial.valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.05, 12.0, parseUiNumber(text)); };
    setPercentDisplay(lfoDepthDial, 62);

    fxDials[0].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    clearSuffix(fxDials[0]);
    fxDials[0].textFromValueFunction = [](double value) { return formatMultiplier(value); };
    fxDials[0].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(1.0, 16.0, parseUiNumber(text)); };
    setPercentDisplay(fxDials[1]);
    setSignedPercentDisplay(fxDials[2]);
    setSignedPercentDisplay(fxDials[3]);
    setPercentDisplay(fxDials[4]);

    fxDials[5].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[5]);
    fxDials[5].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[5].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-60.0, 0.0, parseUiNumber(text)); };
    fxDials[6].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    clearSuffix(fxDials[6]);
    fxDials[6].textFromValueFunction = [](double value) { return formatRatio(value); };
    fxDials[6].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(1.0, 20.0, parseUiNumber(text)); };
    setMillisecondsDisplay(fxDials[7]);
    setMillisecondsDisplay(fxDials[8]);
    fxDials[9].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[9]);
    fxDials[9].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[9].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.0, 24.0, parseUiNumber(text)); };
    setPercentDisplay(fxDials[10]);

    setPercentDisplay(fxDials[11]);
    setPercentDisplay(fxDials[12]);
    setPercentDisplay(fxDials[13]);
    setPercentDisplay(fxDials[14]);
    setMillisecondsDisplay(fxDials[15]);
    setFrequencyDisplay(fxDials[16], 82);
    fxDials[17].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[17]);
    fxDials[17].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[17].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-12.0, 12.0, parseUiNumber(text)); };
    setFrequencyDisplay(fxDials[18], 82);
    fxDials[19].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[19]);
    fxDials[19].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[19].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-12.0, 12.0, parseUiNumber(text)); };
    fxDials[20].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
    clearSuffix(fxDials[20]);
    fxDials[20].textFromValueFunction = [](double value) { return "Q " + trimValueString(value, value < 10.0 ? 2 : 1); };
    fxDials[20].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.1, 10.0, parseUiNumber(text)); };
    setFrequencyDisplay(fxDials[21], 82);
    fxDials[22].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[22]);
    fxDials[22].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[22].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-12.0, 12.0, parseUiNumber(text)); };
    fxDials[23].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    clearSuffix(fxDials[23]);
    fxDials[23].textFromValueFunction = [](double value) { return trimValueString(value, value < 10.0 ? 2 : 1) + " Hz"; };
    fxDials[23].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.1, 5.0, parseUiNumber(text)); };
    setPercentDisplay(fxDials[24]);
    setPercentDisplay(fxDials[25]);
    setMillisecondsDisplay(fxDials[26]);
    fxDials[27].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    clearSuffix(fxDials[27]);
    fxDials[27].textFromValueFunction = [](double value)
    {
        return juce::String(juce::roundToInt(juce::jlimit(0.0, 0.95, value) * 100.0)) + "%";
    };
    fxDials[27].valueFromTextFunction = [](const juce::String& text)
    {
        return juce::jlimit(0.0, 0.95, parseUiNumber(text) / 100.0);
    };
    setPercentDisplay(fxDials[28]);
    fxDials[29].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    clearSuffix(fxDials[29]);
    fxDials[29].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[29].valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(-12.0, 0.0, parseUiNumber(text)); };
    setMillisecondsDisplay(fxDials[30]);

    modLfo2RateDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    clearSuffix(modLfo2RateDial);
    modLfo2RateDial.textFromValueFunction = [](double value) { return trimValueString(value, value < 10.0 ? 2 : 1) + " Hz"; };
    modLfo2RateDial.valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.05, 12.0, parseUiNumber(text)); };

    for (auto& slider : modAmountSliders)
    {
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        clearSuffix(slider);
        slider.textFromValueFunction = [](double value) { return formatSignedPercent(value); };
        slider.valueFromTextFunction = signedPercentFromText;
    }

    for (auto& slider : envDials)
        slider.updateText();
    lfoRateDial.updateText();
    lfoDepthDial.updateText();
    for (auto& slider : macroDials)
        slider.updateText();
    for (auto& slider : fxDials)
        slider.updateText();
    modLfo2RateDial.updateText();
    for (auto& slider : modAmountSliders)
        slider.updateText();
}

// =============================================================================
// Selection UI
// =============================================================================
void PercSynthAudioProcessorEditor::rebuildModelSelectorForFamily(
    int familyIndex, int preferredInstr)
{
    familyIndex = juce::jlimit(0, mpc::kNumFamilies - 1, familyIndex);
    modelSelector.clear(juce::dontSendNotification);

    const int first = mpc::kFamilyStart[familyIndex];
    const int count = mpc::kFamilySize[familyIndex];

    for (int i = 0; i < count; ++i)
        modelSelector.addItem(juce::String(juce::CharPointer_UTF8(mpc::getInstrName(first + i))), first + i + 1);

    int target = preferredInstr;
    if (target < first || target >= first + count) target = first;
    modelSelector.setSelectedId(target + 1, juce::dontSendNotification);
}

void PercSynthAudioProcessorEditor::syncSelectionUiFromInstr()
{
    const int instrIndex  = selectedInstrFromParam();
    const int familyIndex = static_cast<int>(mpc::getFamily(instrIndex));
    static const char* familyTabNames[] = { "PERCUSSIONS", "AMBIANCE", "METALLIQUES" };

    activeFamilyIndex = familyIndex;

    if (familySelector.getSelectedId() != familyIndex + 1)
        familySelector.setSelectedId(familyIndex + 1, juce::dontSendNotification);

    rebuildModelSelectorForFamily(familyIndex, instrIndex);

    for (int tabIndex = 0; tabIndex < mpc::kNumFamilies; ++tabIndex)
    {
        familyTabs[(size_t)tabIndex].configure(tabIndex,
                                               familyTabNames[tabIndex],
                                               tabIndex == familyIndex ? instrCatColour(instrIndex) : familyColour(tabIndex));
        familyTabs[(size_t)tabIndex].setSelected(tabIndex == familyIndex);
    }
}

const mpc::InstrumentPreset* PercSynthAudioProcessorEditor::currentFactoryPresetDefinition() const noexcept
{
    return proc.getFactoryPresetDefinition(proc.getCurrentFactoryPresetIndex());
}

void PercSynthAudioProcessorEditor::refreshPresetFilterChoices()
{
    const auto restoreSelection = [](juce::ComboBox& box, const juce::String& previous)
    {
        if (previous.isNotEmpty())
        {
            for (int itemIndex = 0; itemIndex < box.getNumItems(); ++itemIndex)
            {
                const auto itemId = box.getItemId(itemIndex);
                if (box.getItemText(itemIndex).equalsIgnoreCase(previous))
                {
                    box.setSelectedId(itemId, juce::dontSendNotification);
                    return;
                }
            }
        }
        if (box.getNumItems() > 0 && box.getSelectedId() == 0)
            box.setSelectedItemIndex(0, juce::dontSendNotification);
    };

    const auto previousFamily = presetFamilyFilter.getText();
    const auto previousRole = presetRoleFilter.getText();
    const auto previousTag = presetTagFilter.getText();

    juce::StringArray families;
    juce::StringArray roles;
    juce::StringArray tags;
    if (const auto names = proc.getFactoryPresetNames(); !names.isEmpty())
    {
        for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
        {
            if (const auto* preset = proc.getFactoryPresetDefinition(presetIndex))
            {
                families.addIfNotAlreadyThere(juce::String(preset->metadata.familyLabel.c_str()));
                roles.addIfNotAlreadyThere(juce::String(preset->metadata.mixRole.c_str()));
                for (const auto& tag : preset->metadata.tags)
                    tags.addIfNotAlreadyThere(juce::String(tag.c_str()));
            }
        }
    }

    auto populateFilter = [](juce::ComboBox& box, const juce::String& allLabel, const juce::StringArray& values)
    {
        box.clear(juce::dontSendNotification);
        box.addItem(allLabel, 1);
        for (int valueIndex = 0; valueIndex < values.size(); ++valueIndex)
            box.addItem(values[valueIndex], valueIndex + 2);
    };

    populateFilter(presetFamilyFilter, "ALL", families);
    populateFilter(presetRoleFilter, "ALL", roles);
    populateFilter(presetTagFilter, "ALL", tags);
    restoreSelection(presetFamilyFilter, previousFamily);
    restoreSelection(presetRoleFilter, previousRole);
    restoreSelection(presetTagFilter, previousTag);
}

juce::String PercSynthAudioProcessorEditor::currentPresetMetadataSummary() const
{
    if (proc.isCurrentPresetUser())
    {
        const auto file = proc.getCurrentUserPresetFile();
        juce::String summary = "User preset";
        musique::preset::PresetManifest manifest;
        if (musique::preset::loadManifestFromFile(musique::preset::manifestFileForPresetFile(file), manifest))
            summary << " · " << manifest.instrumentName << " · " << manifest.sourceModel;
        else
            summary << " · " << file.getFileNameWithoutExtension();
        return summary;
    }

    if (const auto* preset = currentFactoryPresetDefinition())
    {
        juce::String summary = juce::String(preset->metadata.description.c_str());
        if (summary.length() > 64)
            summary = summary.substring(0, 61).trimEnd() + "...";

        return summary
            + " · " + juce::String(preset->metadata.mixRole.c_str())
            + " · " + trimValueString(preset->metadata.nominalPeakDb, 1) + " dB";
    }

    return {};
}

void PercSynthAudioProcessorEditor::updatePresetMetadataSummary()
{
    const auto summary = currentPresetMetadataSummary();
    presetMetaLabel.setTooltip(summary);
    if (presetMetaLabel.getText() != summary)
        presetMetaLabel.setText(summary, juce::dontSendNotification);
}

// =============================================================================
// Tooltip mode cycling
// =============================================================================
void PercSynthAudioProcessorEditor::cycleTooltipMode()
{
    switch (tooltipMode)
    {
        case TooltipMode::Off:    tooltipMode = TooltipMode::Short;  break;
        case TooltipMode::Short:  tooltipMode = TooltipMode::Novice; break;
        case TooltipMode::Novice: tooltipMode = TooltipMode::Off;    break;
    }

    switch (tooltipMode)
    {
        case TooltipMode::Off:
        case TooltipMode::Short:
        case TooltipMode::Novice:
            tooltipModeBtn.setButtonText(tooltipMode == TooltipMode::Off ? "Tips: Off"
                                                                         : tooltipMode == TooltipMode::Short ? "Tips: Short"
                                                                                                              : "Tips: Novice");
            break;
    }

    tooltipWindow.setVisible(tooltipMode != TooltipMode::Off);
    applyTooltips();
}

// =============================================================================
// Apply tooltips according to current mode
// =============================================================================
void PercSynthAudioProcessorEditor::applyTooltips()
{
    const char** src = nullptr;
    if (tooltipMode == TooltipMode::Short)  src = kTooltipsShort;
    if (tooltipMode == TooltipMode::Novice) src = kTooltipsNovice;

    const auto& profile = envProfileForInstrument(cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam());
    const auto sharedTooltipMode = tooltipMode == TooltipMode::Short ? synthui::TooltipMode::Short
                                 : tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                                      : synthui::TooltipMode::Off;
    synthui::applyTooltipProfile(profile, envDials, sharedTooltipMode);

    int idx = kEnvN;

    lfoRateDial .setTooltip(src ? juce::String(src[idx])     : juce::String()); ++idx;
    lfoDepthDial.setTooltip(src ? juce::String(src[idx])     : juce::String()); ++idx;

    for (int i = 0; i < kMacroTotal; ++i, ++idx)
        macroDials[static_cast<std::size_t>(i)].setTooltip(src ? juce::String(src[idx]) : juce::String());

    for (int i = 0; i < kFxN; ++i, ++idx)
        fxDials[static_cast<std::size_t>(i)].setTooltip(src ? juce::String(src[idx]) : juce::String());

    gainDial.setTooltip(src ? juce::String(src[idx]) : juce::String());
}
