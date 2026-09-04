#include "PluginEditor.h"
#include "BinaryData.h"
#include "../../Shared/PresetManifest.h"
#include <cmath>
#include <numeric>

// =============================================================================
// Layout constants — 1340 x 820
// =============================================================================
namespace lay
{
    constexpr int W = 1100, H = 720;
}

namespace
{
using EnvUiProfile = synthui::InstrumentUiProfile<14>;

juce::String utf8Text(const char* text)
{
    return juce::String(juce::CharPointer_UTF8(text));
}

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

juce::String compactCcPageLabel(int page)
{
    auto pageName = juce::String(OrchSynthAudioProcessor::getCCPageName(page)).trim().toUpperCase();
    pageName = pageName.replaceCharacter('/', '-')
                       .replace("REVERB", "REV")
                       .replace("DELAY", "DLY")
                       .replace("ENVELOPE", "ENV")
                       .replace("DYNAMICS", "DYN")
                       .replace("LIMITER", "LIM");

    return "CC: " + pageName;
}

bool isModalInstrumentForUi(const int instrIndex)
{
    return mos::getCharacteristics(instrIndex).oscMode == mos::OscMode::Modal;
}

juce::String browserGuideForInstrument(int instrIndex, bool compact)
{
    const auto family = juce::String(mos::getFamilyName(static_cast<int>(mos::getFamily(instrIndex)))).toUpperCase();
    const bool modalInstrument = isModalInstrumentForUi(instrIndex);

    if (compact)
        return family + (modalInstrument ? ": CUES>TEXT" : ": ROLES>TEXT");

    return modalInstrument
        ? "BROWSER: " + family + " | PRIMARY CUES FIRST | TEXTURES SECOND"
        : "BROWSER: " + family + " | PRIMARY ROLES FIRST | TEXTURES SECOND";
}

juce::String browserPlaceholderForInstrument(int instrIndex, bool compact)
{
    if (compact)
        return "Search...";

    return juce::String(mos::getFamilyName(static_cast<int>(mos::getFamily(instrIndex))))
        + " / solo / texture / role / tag...";
}

juce::String compactVoiceCountLabel(int activeVoices)
{
    return "V " + juce::String(activeVoices);
}

void applyValueFormatter(juce::Slider& slider,
                         std::function<juce::String(double)> toText,
                         std::function<double(const juce::String&)> toValue = {})
{
    slider.textFromValueFunction = std::move(toText);
    if (toValue)
        slider.valueFromTextFunction = std::move(toValue);
}

juce::String formatPortamentoDisplay(double value)
{
    if (value < 0.001)
        return "Off";
    if (value < 1.0)
        return juce::String(juce::roundToInt(value * 1000.0)) + " ms";
    return juce::String(value, 2) + " s";
}

constexpr std::array<mos::GlobalFxSlot, 8> kOrchFxSlots = {{
    mos::GlobalFxSlot::Reverb,
    mos::GlobalFxSlot::Saturator,
    mos::GlobalFxSlot::Transient,
    mos::GlobalFxSlot::Compressor,
    mos::GlobalFxSlot::Eq,
    mos::GlobalFxSlot::Chorus,
    mos::GlobalFxSlot::Delay,
    mos::GlobalFxSlot::Limiter
}};

constexpr const char* kModMatrixSourceItems[9] = {
    "OFF", "WHEEL", "CC11", "BREATH", "AFT", "VEL", "LFO", "PBEND", "ENV"
};

constexpr const char* kModMatrixDestItems[13] = {
    "OFF", "GAIN", "TIMBRE", "VIBRATO", "RELEASE", "AT", "CUTOFF", "PAN", "PITCH", "ATTACK", "DECAY", "EQ FREQ", "EQ GAIN"
};

constexpr const char* kRightPanelSectionLabels[3] = {
    "MACRO+LFO", "MOD MATRIX", "FX"
};

constexpr bool kShowAdvancedFrontControls = false;

juce::String compactOutputRouteLabel(int selectedId)
{
    if (selectedId <= 1)
        return "MAIN";

    return "S" + juce::String(selectedId - 1);
}

bool useCompactHeaderCopy(int width, int height) noexcept
{
    return width < 1120 || height < 700;
}

bool isSecondaryMixRole(const std::string& mixRole) noexcept
{
    return mixRole == "trailer-layer"
        || mixRole == "cinematic-section"
        || mixRole == "atmospheric-layer"
        || mixRole == "stereo-layer";
}

juce::String compactRoleLabel(const std::string& mixRole)
{
    if (mixRole == "solo-core") return "SOLO CORE";
    if (mixRole == "support-layer") return "SOLO SOFT";
    if (mixRole == "low-mid-support") return "SOLO DARK";
    if (mixRole == "short-articulation") return "SOLO STAC";
    if (mixRole == "accent-layer") return "SOLO MRCT";
    if (mixRole == "trailer-layer") return "TEXT TRLR";
    if (mixRole == "cinematic-section") return "TEXT CINE";
    if (mixRole == "atmospheric-layer") return "TEXT AMBI";
    if (mixRole == "stereo-layer") return "TEXT WIDE";
    return "PRESET";
}

juce::String presetTierLabel(const mos::PresetMetadata& metadata)
{
    return isSecondaryMixRole(metadata.mixRole) ? "SECONDARY" : "PRIMARY";
}

juce::String compactTierLabel(const mos::PresetMetadata& metadata)
{
    return isSecondaryMixRole(metadata.mixRole) ? "SEC" : "PRI";
}

juce::String compactOutputProfileLabel(const std::string& outputProfile)
{
    if (outputProfile == "main-core" || outputProfile == "main-score"
        || outputProfile == "main-space" || outputProfile == "main-support"
        || outputProfile == "main-articulation")
        return "MAIN";
    if (outputProfile == "main-plus-aux1-articulation")
        return "AUX ART";
    if (outputProfile == "main-plus-aux2-score")
        return "AUX SCORE";
    if (outputProfile == "main-plus-aux3-space")
        return "AUX SPACE";
    if (outputProfile == "main-plus-aux4-support")
        return "AUX SUPPORT";
    return {};
}

juce::String browserSearchAliases(const mos::PresetMetadata& metadata)
{
    juce::String text;
    const bool secondary = isSecondaryMixRole(metadata.mixRole);

    text << " " << compactRoleLabel(metadata.mixRole);
    text << " solo single voice single-voice";
    text << (secondary
        ? " texture texture-role secondary role secondary-role layer"
        : " primary role primary-role anchor core" );

    if (metadata.mixRole == "solo-core")
        text << " solo core concert anchor";
    else if (metadata.mixRole == "support-layer")
        text << " solo soft support";
    else if (metadata.mixRole == "low-mid-support")
        text << " solo dark support";
    else if (metadata.mixRole == "short-articulation")
        text << " solo staccato short articulation";
    else if (metadata.mixRole == "accent-layer")
        text << " solo marcato accent articulation";
    else if (metadata.mixRole == "trailer-layer")
        text << " texture trailer layer";
    else if (metadata.mixRole == "cinematic-section")
        text << " texture cinematic score layer";
    else if (metadata.mixRole == "atmospheric-layer")
        text << " texture ambient atmospheric layer";
    else if (metadata.mixRole == "stereo-layer")
        text << " texture wide stereo layer";

    return text;
}

int presetRoleDisplayRank(const mos::PresetMetadata& metadata)
{
    if (metadata.mixRole == "solo-core") return 0;
    if (metadata.mixRole == "support-layer") return 1;
    if (metadata.mixRole == "low-mid-support") return 2;
    if (metadata.mixRole == "short-articulation") return 3;
    if (metadata.mixRole == "accent-layer") return 4;
    if (metadata.mixRole == "cinematic-section") return 5;
    if (metadata.mixRole == "trailer-layer") return 6;
    if (metadata.mixRole == "atmospheric-layer") return 7;
    if (metadata.mixRole == "stereo-layer") return 8;
    return 9;
}

const synthui::MacroLabelProfile<4>& macroLabelsForFamily(const mos::Family family)
{
    static const synthui::MacroLabelProfile<4> strings     = { "Depth", "Motion", "Space", "Expression" };
    static const synthui::MacroLabelProfile<4> woodwinds   = { "Air", "Motion", "Space", "Expression" };
    static const synthui::MacroLabelProfile<4> brass       = { "Weight", "Brass", "Space", "Expression" };
    static const synthui::MacroLabelProfile<4> percussion  = { "Impact", "Tone", "Image", "Artic" };

    switch (family)
    {
        case mos::Family::Cordes:      return strings;
        case mos::Family::Bois:        return woodwinds;
        case mos::Family::Cuivres:     return brass;
        case mos::Family::Percussions: return percussion;
    }

    return strings;
}

struct OrchLayoutMetrics
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

OrchLayoutMetrics computeLayoutMetrics(int width, int height)
{
    OrchLayoutMetrics m;
    m.compact = width < 1120 || height < 700;
    m.roomy = width > 1600 || height > 940;
    m.outerMargin = m.compact ? 16 : 24;
    m.gutter = m.compact ? 10 : (m.roomy ? 22 : 16);
    m.headerH = m.compact ? 88 : 96;
    m.selectorH = m.compact ? 74 : 78;
    m.kbH = m.compact
        ? juce::jlimit(72, 96, static_cast<int>(height * 0.14f))
        : juce::jlimit(84, 126, static_cast<int>(height * (m.roomy ? 0.145f : 0.16f)));

    const int maxContentW = juce::jmin(width - m.outerMargin * 2, 1680);
    m.contentW = juce::jmax(900, maxContentW);
    m.contentX = (width - m.contentW) / 2;

    m.selectorY = m.outerMargin + m.headerH + 8;
    m.kbY = height - m.kbH - m.outerMargin;
    m.bodyY = m.selectorY + m.selectorH + m.gutter;
    m.bodyH = juce::jmax(250, m.kbY - m.bodyY - 8);

    m.colW = (m.contentW - m.gutter * 2) / 3;
    m.col1X = m.contentX;
    m.col2X = m.col1X + m.colW + m.gutter;
    m.col3X = m.col2X + m.colW + m.gutter;
    return m;
}

const EnvUiProfile& envProfileForInstrument(const int instrIndex)
{
    static const EnvUiProfile bowedStrings = {{
        { "Level", "Volume level", "How loud this instrument plays" },
        { "Tune", "Pitch offset (semitones)", "Shifts the pitch up or down in semitones" },
        { "Brightness", "Tonal brightness", "Higher values make the sound brighter and more open" },
        { "Attack", "Attack time", "How quickly the sound reaches full volume after a note" },
        { "Decay", "Decay time", "How fast the sound fades after the attack peak" },
        { "Sustain", "Sustain level", "The volume held while a note is pressed" },
        { "Release", "Release time", "How long the tail rings after releasing a note" },
        { "Vibrato", "Vibrato depth", "Adds pitch wobble for expressive sustain" },
        { "Warmth", "Warmth amount", "Adds body warmth and low-mid richness" },
        { "Detune", "Detune spread", "Slight pitch spread for a wider, thicker sound" },
        { "Stereo Width", "Stereo width", "How wide the sound is placed in stereo" },
        { "Character", "Character color", "Subtle tonal shaping unique to each instrument" },
        { "Cutoff", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the sound left or right in the stereo field" }
    }};

    static const EnvUiProfile harp = {{
        { "Level", "Volume level", "How loud the harp sits in the arrangement" },
        { "Tune", "Pitch offset (semitones)", "Shifts the harp pitch up or down in semitones" },
        { "Pluck", "Pluck brightness", "Higher values make the pluck brighter and more defined" },
        { "Onset", "Pluck attack", "How immediate the pluck feels at note start" },
        { "Decay", "Resonance decay", "How fast the string resonance falls after the strike" },
        { "Ring", "Resonance hold", "Amount of body that remains while the note rings" },
        { "Release", "Tail release", "How long the harp tail survives after note release" },
        { "Motion", "Decorative motion", "Reserved motion shaping; kept restrained for harp presets" },
        { "Body", "Low-mid body", "Adds resonance body without pushing the harp forward" },
        { "Spread", "Layer spread", "Very small pitch spread for doubled harp color" },
        { "Stereo Image", "Stereo image", "Controls how wide the harp sits in stereo" },
        { "Character", "Character color", "Subtle tonal shading for softer or firmer plucks" },
        { "Tone", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the harp left or right in the stereo field" }
    }};

    static const EnvUiProfile woodwinds = {{
        { "Level", "Volume level", "How loud this instrument plays" },
        { "Tune", "Pitch offset (semitones)", "Shifts the pitch up or down in semitones" },
        { "Air", "Tonal brightness", "Higher values make the sound brighter and more open" },
        { "Attack", "Attack time", "How quickly the sound reaches full volume after a note" },
        { "Decay", "Decay time", "How fast the sound fades after the attack peak" },
        { "Sustain", "Sustain level", "The volume held while a note is pressed" },
        { "Release", "Release time", "How long the tail rings after releasing a note" },
        { "Vibrato", "Vibrato depth", "Adds pitch wobble for expressive sustain" },
        { "Warmth", "Warmth amount", "Adds body warmth and low-mid richness" },
        { "Detune", "Detune spread", "Slight pitch spread for a wider, thicker sound" },
        { "Stereo Width", "Stereo width", "How wide the sound is placed in stereo" },
        { "Character", "Character color", "Subtle tonal shaping unique to each instrument" },
        { "Cutoff", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the sound left or right in the stereo field" }
    }};

    static const EnvUiProfile brass = {{
        { "Level", "Volume level", "How loud this instrument plays" },
        { "Tune", "Pitch offset (semitones)", "Shifts the pitch up or down in semitones" },
        { "Brightness", "Tonal brightness", "Higher values make the sound brighter and more open" },
        { "Attack", "Attack time", "How quickly the sound reaches full volume after a note" },
        { "Decay", "Decay time", "How fast the sound fades after the attack peak" },
        { "Sustain", "Sustain level", "The volume held while a note is pressed" },
        { "Release", "Release time", "How long the tail rings after releasing a note" },
        { "Vibrato", "Vibrato depth", "Adds pitch wobble for expressive sustain" },
        { "Weight", "Warmth amount", "Adds body warmth and low-mid richness" },
        { "Detune", "Detune spread", "Slight pitch spread for a wider, thicker sound" },
        { "Stereo Width", "Stereo width", "How wide the sound is placed in stereo" },
        { "Character", "Character color", "Subtle tonal shaping unique to each instrument" },
        { "Cutoff", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the sound left or right in the stereo field" }
    }};

    static const EnvUiProfile timpani = {{
        { "Level", "Volume level", "How loud the timpani sits in the arrangement" },
        { "Tune", "Pitch offset (semitones)", "Fine pitch transposition for the drum body" },
        { "Strike", "Stick brightness", "Higher values emphasize stick attack and upper harmonics" },
        { "Mallet", "Onset speed", "How fast the mallet energy blooms at note start" },
        { "Body", "Body decay", "How long the shell resonance develops after the hit" },
        { "Ring", "Ring amount", "How much low body remains while the drum rings" },
        { "Tail", "Release tail", "How long the final tail survives after note release" },
        { "LOCKED", "Pitch wobble disabled", "Pitch wobble is disabled for finalized timpani presets" },
        { "Shell", "Shell weight", "Adds controlled low-mid shell mass" },
        { "LOCKED", "Detune spread disabled", "Pitch spread is disabled for finalized timpani presets" },
        { "Stereo Image", "Stereo image", "Keeps the timpani image controlled and centered" },
        { "Resonance", "Resonance color", "Shapes shell resonance without turning into a wash" },
        { "Tone", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the timpani left or right in the stereo field" }
    }};

    static const EnvUiProfile celesta = {{
        { "Level", "Volume level", "How loud the celesta sits in the arrangement" },
        { "Tune", "Pitch offset (semitones)", "Shifts the celesta pitch up or down in semitones" },
        { "Bell", "Bell brightness", "Higher values emphasize bell attack and upper chime" },
        { "Hammer", "Hammer onset", "How immediate the hammer strike feels at note start" },
        { "Decay", "Bell decay", "How fast the celesta body decays after the strike" },
        { "Ring", "Bell hold", "Amount of chime that remains while the note rings" },
        { "Tail", "Release tail", "How long the celesta tail remains after note release" },
        { "LOCKED", "Pitch wobble disabled", "Pitch wobble is disabled for finalized celesta presets" },
        { "Body", "Body weight", "Adds body without making the celesta cloudy" },
        { "LOCKED", "Detune spread disabled", "Pitch spread is disabled for finalized celesta presets" },
        { "Stereo Image", "Stereo image", "Controls image width while keeping pitch definition stable" },
        { "Resonance", "Resonance color", "Shapes bell resonance and metallic tail" },
        { "Tone", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the celesta left or right in the stereo field" }
    }};

    static const EnvUiProfile snare = {{
        { "Level", "Volume level", "How loud the snare sits in the arrangement" },
        { "Tune", "Pitch offset (semitones)", "Tuning offset for the drum body" },
        { "Snap", "Snap brightness", "Higher values emphasize wire and stick attack" },
        { "Hit", "Hit onset", "How immediate the strike feels at note start" },
        { "Body", "Body decay", "How long the drum shell resonance remains" },
        { "Ring", "Ring amount", "Amount of tone held after the strike" },
        { "Tail", "Release tail", "How long the final tail remains after note release" },
        { "LOCKED", "Pitch wobble disabled", "Pitch wobble is disabled for finalized snare presets" },
        { "Shell", "Shell weight", "Adds controlled mid shell mass" },
        { "LOCKED", "Detune spread disabled", "Pitch spread is disabled for finalized snare presets" },
        { "Stereo Image", "Stereo image", "Controls image width while keeping impact focused" },
        { "Character", "Character color", "Shapes snap and shell emphasis" },
        { "Tone", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the snare left or right in the stereo field" }
    }};

    static const EnvUiProfile xylophone = {{
        { "Level", "Volume level", "How loud the xylophone sits in the arrangement" },
        { "Tune", "Pitch offset (semitones)", "Shifts the xylophone pitch up or down in semitones" },
        { "Bar", "Bar brightness", "Higher values emphasize stick attack and upper partials" },
        { "Mallet", "Mallet onset", "How immediate the mallet strike feels at note start" },
        { "Decay", "Bar decay", "How fast the wooden bar resonance falls" },
        { "Ring", "Ring hold", "Amount of tone held after the strike" },
        { "Tail", "Release tail", "How long the final tail remains after note release" },
        { "LOCKED", "Pitch wobble disabled", "Pitch wobble is disabled for finalized xylophone presets" },
        { "Body", "Body weight", "Adds controlled wooden body" },
        { "LOCKED", "Detune spread disabled", "Pitch spread is disabled for finalized xylophone presets" },
        { "Stereo Image", "Stereo image", "Controls image width while keeping pitch definition stable" },
        { "Resonance", "Resonance color", "Shapes wooden resonance and attack edge" },
        { "Tone", "Filter cutoff freq", "Low-pass filter: lower = darker, higher = brighter" },
        { "Pan", "Pan position", "Moves the xylophone left or right in the stereo field" }
    }};

    if (instrIndex == 4)
        return harp;
    if (instrIndex == 16)
        return timpani;
    if (instrIndex == 17)
        return celesta;
    if (instrIndex == 18)
        return snare;
    if (instrIndex == 19)
        return xylophone;

    switch (mos::getFamily(instrIndex))
    {
        case mos::Family::Cordes:      return bowedStrings;
        case mos::Family::Bois:        return woodwinds;
        case mos::Family::Cuivres:     return brass;
        case mos::Family::Percussions: return timpani;
    }

    return bowedStrings;
}

void glazeOrchChrome(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     juce::Colour accent,
                     float radius,
                     float intensity)
{
    juce::ignoreUnused(accent);

    const auto orchTop = juce::Colour(0xff3E4147).withAlpha(0.028f * intensity);
    const auto orchMid = juce::Colour(0xff23272D).withAlpha(0.062f * intensity);
    const auto orchBottom = juce::Colour(0xff0B0E12).withAlpha(0.20f * intensity);

    juce::ColourGradient glaze(orchTop, area.getCentreX(), area.getY(),
                               orchBottom, area.getCentreX(), area.getBottom(), false);
    glaze.addColour(0.40, orchMid);
    g.setGradientFill(glaze);
    g.fillRoundedRectangle(area, radius);

    auto sheen = area.reduced(3.0f).withHeight(juce::jmax(7.0f, area.getHeight() * 0.12f));
    juce::ColourGradient highlight(juce::Colours::white.withAlpha(0.010f * intensity), sheen.getCentreX(), sheen.getY(),
                                   juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
    g.setGradientFill(highlight);
    g.fillRoundedRectangle(sheen, juce::jmax(0.0f, radius - 2.0f));

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        auto brushedArea = area.reduced(5.0f);
        g.reduceClipRegion(brushedArea.toNearestInt());

        auto addCloud = [&](juce::Point<float> centre, float radiusX, float radiusY, juce::Colour colour)
        {
            juce::ColourGradient cloud(colour, centre.x, centre.y,
                                       juce::Colours::transparentBlack, centre.x + radiusX, centre.y + radiusY, true);
            g.setGradientFill(cloud);
            g.fillEllipse(centre.x - radiusX, centre.y - radiusY, radiusX * 2.0f, radiusY * 2.0f);
        };

        addCloud({ brushedArea.getX() + brushedArea.getWidth() * 0.32f,
                   brushedArea.getY() + brushedArea.getHeight() * 0.18f },
                 brushedArea.getWidth() * 0.30f, brushedArea.getHeight() * 0.16f,
                 juce::Colours::white.withAlpha(0.006f * intensity));
        addCloud({ brushedArea.getX() + brushedArea.getWidth() * 0.74f,
                   brushedArea.getY() + brushedArea.getHeight() * 0.30f },
                 brushedArea.getWidth() * 0.26f, brushedArea.getHeight() * 0.18f,
                 juce::Colours::white.withAlpha(0.004f * intensity));
        addCloud({ brushedArea.getX() + brushedArea.getWidth() * 0.64f,
                   brushedArea.getY() + brushedArea.getHeight() * 0.46f },
                 brushedArea.getWidth() * 0.24f, brushedArea.getHeight() * 0.16f,
                 juce::Colours::black.withAlpha(0.012f * intensity));
        addCloud({ brushedArea.getCentreX(),
                   brushedArea.getBottom() - brushedArea.getHeight() * 0.10f },
                 brushedArea.getWidth() * 0.42f, brushedArea.getHeight() * 0.14f,
                 juce::Colours::black.withAlpha(0.050f * intensity));
    }

    g.setColour(juce::Colour(0xff565D67).withAlpha(0.058f * intensity));
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

// =============================================================================
// Static tables
// =============================================================================
const std::array<OrchSynthAudioProcessorEditor::CtrlDef,
                 OrchSynthAudioProcessorEditor::kEnvN>
    OrchSynthAudioProcessorEditor::kEnvCtrls = {{
        { "Level",        "level" },
        { "Tune",         "tune" },
        { "Brightness",   "brightness" },
        { "Attack",       "attack" },
        { "Decay",        "decay" },
        { "Sustain",      "sustain" },
        { "Release",      "release" },
        { "Vibrato",      "vibrato" },
        { "Warmth",       "warmth" },
        { "Detune",       "detune" },
        { "Stereo Width", "stereo_width" },
        { "Character",    "character" },
        { "Cutoff",       "cutoff" },
        { "Pan",          "pan" }
    }};

const std::array<OrchSynthAudioProcessorEditor::FxDef,
                 OrchSynthAudioProcessorEditor::kMacroTotal>
    OrchSynthAudioProcessorEditor::kMacroCtrls = {{
        { "Depth",      "macro_warmth" },
        { "Motion",     "macro_brillance" },
        { "Space",      "macro_space" },
        { "Expression", "macro_expression" }
    }};

const std::array<OrchSynthAudioProcessorEditor::FxDef,
                 OrchSynthAudioProcessorEditor::kFxN>
    OrchSynthAudioProcessorEditor::kFxCtrls = {{
        { "Size",       "reverb_size" },
        { "Damping",    "reverb_damping" },
        { "Width",      "reverb_width" },
        { "Mix",        "reverb_mix" },
        { "Predelay",   "reverb_predelay" },
        { "Drive",      "sat_drive" },
        { "Mix",        "sat_mix" },
        { "Attack",     "transient_attack" },
        { "Sustain",    "transient_sustain" },
        { "Mix",        "transient_mix" },
        { "Threshold",  "comp_threshold" },
        { "Ratio",      "comp_ratio" },
        { "Attack",     "comp_attack" },
        { "Release",    "comp_release" },
        { "Mix",        "comp_mix" },
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

const char* OrchSynthAudioProcessorEditor::kFxTabNames[kFxTabs] = {
    "REVERB", "SAT", "TRANS", "COMP", "EQ", "CHORUS", "DELAY", "LIMITER"
};

const char* OrchSynthAudioProcessorEditor::kFxRackSummaries[kFxTabs] = {
    "Space", "Color", "Attack", "Dynamics", "Tone", "Width", "Echo", "Output"
};

const char* OrchSynthAudioProcessorEditor::kFxTabLabels[kFxTabs][kFxPerTab] = {
    { "Size",      "Damping",  "Width",   "Mix",     "Predly", "",       "" },
    { "Drive",     "Mix",      "",        "",        "",       "",       "" },
    { "Attack",    "Sustain",  "Mix",     "",        "",       "",       "" },
    { "Thresh",    "Ratio",    "Attack",  "Release", "Mix",    "",       "" },
    { "Lo F",      "Lo G",     "Mid F",   "Mid G",   "Mid Q",  "Hi F",   "Hi G" },
    { "Rate",      "Depth",    "Mix",     "",        "",       "",       "" },
    { "Time",      "Feedbk",   "Mix",     "",        "",       "",       "" },
    { "Thresh",    "Release",  "",        "",        "",       "",       "" }
};

const char* OrchSynthAudioProcessorEditor::kFxBypassParamIds[kFxTabs] = {
    "fx_tab0_en",      // idx 0 = Reverb     → "Space"
    "fx_tab1_en",      // idx 1 = Saturator  → "Color"
    "fx_tab2_en",      // idx 2 = Transient  → "Attack"
    "fx_tab3_en",      // idx 3 = Compressor → "Dynamics"
    "fx_eq_en",        // idx 4 = EQ         → "Tone"
    "fx_chorus_en",    // idx 5 = Chorus     → "Width"
    "fx_delay_en",     // idx 6 = Delay      → "Echo"
    "fx_limiter_en"    // idx 7 = Limiter    → "Output"
};

// =============================================================================
// Tooltip arrays (14 env + 2 lfo + 4 macro + 30 fx + 1 gain = 51)
// =============================================================================
const char* OrchSynthAudioProcessorEditor::kTooltipsShort[kTooltipCount] = {
    // Env (14)
    "Volume level", "Pitch offset (semitones)", "Tonal brightness",
    "Attack time", "Decay time", "Sustain level", "Release time",
    "Vibrato depth", "Warmth amount", "Detune spread",
    "Stereo width", "Character color", "Filter cutoff freq", "Pan position",
    // LFO (2)
    "LFO speed", "LFO intensity",
    // Macros (4)
    "Depth control", "Motion sweep", "Space ambience", "Expression dynamics",
    // FX (30)
    "Room size", "HF damping", "Stereo spread", "Wet/dry reverb", "Pre-delay ms",
    "Saturation drive", "Sat wet/dry",
    "Transient attack boost", "Transient sustain", "Transient mix",
    "Comp threshold", "Comp ratio", "Comp attack", "Comp release", "Comp mix",
    "Low EQ freq", "Low EQ gain", "Mid EQ freq", "Mid EQ gain", "Mid Q width",
    "High EQ freq", "High EQ gain",
    "Chorus speed", "Chorus depth", "Chorus mix",
    "Delay time", "Delay feedback", "Delay mix",
    "Limiter ceiling", "Limiter release",
    // Gain (1)
    "Master output level"
};

const char* OrchSynthAudioProcessorEditor::kTooltipsNovice[kTooltipCount] = {
    // Env (14)
    "How loud this instrument plays",
    "Shifts the pitch up or down in semitones",
    "Higher values make the sound brighter and more open",
    "How quickly the sound reaches full volume after a note",
    "How fast the sound fades after the attack peak",
    "The volume held while a note is pressed",
    "How long the tail rings after releasing a note",
    "Adds pitch wobble for expressive sustain",
    "Adds body warmth and low-mid richness",
    "Slight pitch spread for a wider, thicker sound",
    "How wide the sound is placed in stereo",
    "Subtle tonal shaping unique to each instrument",
    "Low-pass filter: lower = darker, higher = brighter",
    "Moves the sound left or right in the stereo field",
    // LFO (2)
    "Speed of the modulation oscillator (Hz)",
    "How strongly the LFO affects the target parameter",
    // Macros (4)
    "Master depth: controls warmth and body across the instrument",
    "Master motion: sweeps brightness and movement",
    "Master space: controls reverb and spatial width",
    "Master expression: adjusts dynamics and articulation",
    // FX (30)
    "Reverb room size: small rooms to large halls",
    "How quickly high frequencies decay in reverb",
    "Stereo width of the reverb tail",
    "Balance between dry signal and reverb",
    "Delay before reverb starts (adds depth)",
    "Distortion intensity (subtle warmth to aggressive)",
    "Balance between clean and saturated signal",
    "Boosts the punch of note attacks",
    "Controls how the sustain portion is affected",
    "Balance between original and transient-shaped signal",
    "Level where compression begins (-60 to 0 dB)",
    "Compression strength (1:1 = off, 20:1 = hard limiting)",
    "How fast the compressor reacts to loud signals",
    "How fast the compressor stops compressing",
    "Balance between uncompressed and compressed signal",
    "Center frequency of the low EQ band",
    "Boost/cut amount for the low band",
    "Center frequency of the mid EQ band",
    "Boost/cut amount for the mid band",
    "Width of the mid EQ bell (higher = narrower)",
    "Center frequency of the high EQ band",
    "Boost/cut amount for the high band",
    "Speed of the chorus modulation",
    "Intensity of the chorus pitch wobble",
    "Balance between dry and chorus-effected signal",
    "Echo repeat interval in milliseconds",
    "Portion of echo fed back for repeats",
    "Balance between dry and delayed signal",
    "Output ceiling before hard limiting",
    "How fast the limiter recovers after clamping",
    // Gain (1)
    "Final output volume in dB (affects everything)"
};

// =============================================================================
// Helpers
// =============================================================================
juce::Colour OrchSynthAudioProcessorEditor::familyColour(int familyIndex)
{
    switch (familyIndex)
    {
        case 0: return juce::Colour(0xff8C5A3C);
        case 1: return juce::Colour(0xff3E7C73);
        case 2: return juce::Colour(0xffA55233);
        case 3: return juce::Colour(0xff4E5AA7);
        default: return juce::Colour(0xff8A6D4B);
    }
}

juce::Colour OrchSynthAudioProcessorEditor::instrCatColour(int instrIndex)
{
    const auto familyIndex = static_cast<int>(mos::getFamily(instrIndex));
    const auto base = familyColour(familyIndex);
    const auto familyStart = mos::kFamilyStart[familyIndex];
    const auto familySize = juce::jmax(1, mos::kFamilySize[familyIndex]);
    const auto localIndex = juce::jlimit(0, familySize - 1, instrIndex - familyStart);
    const float t = familySize > 1
        ? static_cast<float>(localIndex) / static_cast<float>(familySize - 1)
        : 0.5f;

    auto hue = base.getHue() + (t - 0.5f) * 0.055f;
    while (hue < 0.0f) hue += 1.0f;
    while (hue >= 1.0f) hue -= 1.0f;

    const auto sat = juce::jlimit(0.35f, 0.92f,
        base.getSaturation() * (0.90f + t * 0.18f));
    const auto bri = juce::jlimit(0.24f, 0.92f,
        base.getBrightness() * (0.82f + t * 0.26f));

    return juce::Colour::fromHSV(hue, sat, bri, 1.0f);
}

int OrchSynthAudioProcessorEditor::selectedInstrFromParam() const
{
    if (auto* raw = proc.getAPVTS().getRawParameterValue("selected_instr"))
        return juce::jlimit(0, mos::kNumInstruments - 1,
                            static_cast<int>(std::round(raw->load())));
    return 0;
}

// =============================================================================
// Virtual bridge methods
// =============================================================================
juce::StringArray OrchSynthAudioProcessorEditor::hostGetFactoryNames()
{
    rebuildFactoryPresetDisplayOrder();
    juce::StringArray names;
    const auto rawNames = proc.getFactoryPresetNames();
    for (const int actualIndex : factoryPresetDisplayOrder)
    {
        if (juce::isPositiveAndBelow(actualIndex, rawNames.size()))
            names.add(rawNames[actualIndex]);
    }
    return names;
}

juce::Array<juce::File> OrchSynthAudioProcessorEditor::hostScanUserPresets()
    { return proc.scanUserPresets(); }

bool OrchSynthAudioProcessorEditor::hostIsUserPreset()
    { return proc.isCurrentPresetUser(); }

juce::File OrchSynthAudioProcessorEditor::hostCurrentUserFile()
    { return proc.getCurrentUserPresetFile(); }

int OrchSynthAudioProcessorEditor::hostCurrentFactoryIdx()
{
    rebuildFactoryPresetDisplayOrder();
    return resolveFactoryActualIndex(proc.getCurrentFactoryPresetIndex());
}

void OrchSynthAudioProcessorEditor::hostApplyFactory(int idx)
    { proc.applyFactoryPreset(resolveFactoryDisplayIndex(idx)); }

void OrchSynthAudioProcessorEditor::hostLoadUser(const juce::File& f)
    { proc.loadUserPreset(f); }

bool OrchSynthAudioProcessorEditor::hostSaveUser(const juce::String& name)
    { return proc.saveUserPreset(name); }

void OrchSynthAudioProcessorEditor::hostUpdateUser(const juce::File& f)
    { proc.updateUserPreset(f); }

void OrchSynthAudioProcessorEditor::hostSaveFactory(int idx)
    { proc.saveFactoryPreset(idx); }

void OrchSynthAudioProcessorEditor::hostDeleteUser(const juce::File& f)
    { proc.deleteUserPreset(f); }

juce::File OrchSynthAudioProcessorEditor::hostGetUserPresetsDir()
    { return OrchSynthAudioProcessor::getUserPresetsDirectory(proc.getSelectedInstrIndex()); }

juce::File OrchSynthAudioProcessorEditor::hostGetUserPresetsDirForIndex(int instrumentIndex)
    { return OrchSynthAudioProcessor::getUserPresetsDirectory(instrumentIndex); }

juce::String OrchSynthAudioProcessorEditor::hostPresetInstrumentAttr() const
    { return "instr"; }

juce::String OrchSynthAudioProcessorEditor::hostFormatFactoryPresetLabel(int presetIndex,
                                                                         const juce::String& displayName) const
{
    if (const auto* preset = proc.getFactoryPresetDefinition(resolveFactoryDisplayIndex(presetIndex)))
        return compactTierLabel(preset->metadata) + " | "
             + compactRoleLabel(preset->metadata.mixRole) + " | "
             + displayName;
    return displayName;
}

juce::String OrchSynthAudioProcessorEditor::hostFormatUserPresetLabel(const juce::File&,
                                                                      const juce::String& displayName) const
{
    return displayName;
}

juce::String OrchSynthAudioProcessorEditor::hostFactoryPresetSearchText(int presetIndex,
                                                                        const juce::String& displayName) const
{
    juce::String text = displayName;
    if (const auto* preset = proc.getFactoryPresetDefinition(resolveFactoryDisplayIndex(presetIndex)))
    {
        text << " " << presetTierLabel(preset->metadata);
        text << " " << juce::String(preset->metadata.mixRole.c_str());
        text << " " << juce::String(preset->metadata.familyLabel.c_str());
        text << " " << juce::String(preset->metadata.description.c_str());
        text << " " << compactOutputProfileLabel(preset->metadata.outputProfile);
        text << " " << juce::String(preset->metadata.outputProfile.c_str());
        text << browserSearchAliases(preset->metadata);
        for (const auto& tag : preset->metadata.tags)
            text << " " << juce::String(tag.c_str());
    }
    return text;
}

juce::String OrchSynthAudioProcessorEditor::hostUserPresetSearchText(const juce::File& presetFile,
                                                                     const juce::String& displayName) const
{
    juce::String text = displayName + " user " + utf8Text(mos::getInstrName(selectedInstrFromParam()));
    musique::preset::PresetManifest manifest;
    if (musique::preset::loadManifestFromFile(musique::preset::manifestFileForPresetFile(presetFile), manifest))
        text << " " << manifest.instrumentName << " " << manifest.synthType << " " << manifest.sourceModel;
    return text;
}

bool OrchSynthAudioProcessorEditor::hostShouldIncludeFactoryPreset(int presetIndex) const
{
    juce::ignoreUnused(presetIndex);
    return true;
}

bool OrchSynthAudioProcessorEditor::hostShouldIncludeUserPreset(const juce::File&) const
{
    return true;
}

// =============================================================================
// Constructor
// =============================================================================
OrchSynthAudioProcessorEditor::OrchSynthAudioProcessorEditor(
    OrchSynthAudioProcessor& processor)
    : CommonSynthEditor(processor,
                        processor.getAPVTS(),
                        processor.getKeyboardState(),
                        juce::Colour(0xff4E5AA7),
                        36, 84, 38.0f)
    , proc(processor)
{
    familySelectorLbl.setText("ORCHESTRAL FAMILY", juce::dontSendNotification);
    modelSelectorLbl.setText("INSTRUMENT", juce::dontSendNotification);
    familySelectorLbl.setVisible(false);
    familySelector.setVisible(false);

    for (int f = 0; f < mos::kNumFamilies; ++f)
        familySelector.addItem(mos::getFamilyName(f), f + 1);

    familySelector.onChange = [this]
    {
        const int fi = juce::jlimit(0, mos::kNumFamilies - 1,
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

    for (int f = 0; f < mos::kNumFamilies; ++f)
    {
        familyTabs[(size_t)f].configure(f, mos::getFamilyName(f), familyColour(f));
        familyTabs[(size_t)f].onClicked = [this](int idx)
        {
            familySelector.setSelectedId(idx + 1, juce::sendNotificationSync);
        };
        familyTabs[(size_t)f].setVisible(false);
        addChildComponent(familyTabs[(size_t)f]);
    }

    instrSelector.setVisible(false);
    addChildComponent(instrSelector);
    for (int i = 0; i < mos::kNumInstruments; ++i)
        instrSelector.addItem(utf8Text(mos::getInstrName(i)), i + 1);
    selInstrAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(), "selected_instr", instrSelector);
    instrSelector.onChange = [this] {
        rebuildInstrAttachments();
        syncSelectionUiFromInstr();
    };

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
                setupGrandDial(envDials[si], accent_, {});
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
        envLabels[si].setFont(juce::Font(juce::FontOptions{}.withHeight(11.2f).withStyle("Bold")));
        envLabels[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.88f));
        addAndMakeVisible(envLabels[si]);
    }
    envVisual.setAccent(accent_);
    envVisual.bindAdsr(&envDials[3], &envDials[4], &envDials[5], &envDials[6]);
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

    noteReleaseModeBtn.setClickingTogglesState(false);
    noteReleaseModeBtn.setTooltip("Choose whether notes stop when the keyboard key is released or stay held until Stop");
    noteReleaseModeBtn.onClick = [this]
    {
        proc.setStopNotesOnKeyRelease(!proc.shouldStopNotesOnKeyRelease());
        syncNoteReleaseModeUi();
    };
    addAndMakeVisible(noteReleaseModeBtn);
    noteReleaseModeBtn.setVisible(kShowAdvancedFrontControls);

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
            macroLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(11.2f).withStyle("Bold")));
            macroLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.88f));
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

    velocityCurveLabel.setText("Velocity", juce::dontSendNotification);
    velocityCurveLabel.setJustificationType(juce::Justification::centredLeft);
    velocityCurveLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    velocityCurveLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    addAndMakeVisible(velocityCurveLabel);

    velocityCurveSelector.addItem("LINEAR", 1);
    velocityCurveSelector.addItem("SOFT", 2);
    velocityCurveSelector.addItem("SOFTER", 3);
    velocityCurveSelector.addItem("HARD", 4);
    velocityCurveSelector.addItem("HARDER", 5);
    velocityCurveSelector.addItem("FIXED", 6);
    velocityCurveSelector.addItem("TOUCH", 7);
    velocityCurveAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(), "velocity_curve", velocityCurveSelector);
    addAndMakeVisible(velocityCurveSelector);

    delaySyncLabel.setText("SYNC", juce::dontSendNotification);
    delayDivisionLabel.setText("DIV", juce::dontSendNotification);
    delaySyncLabel.setJustificationType(juce::Justification::centredLeft);
    delayDivisionLabel.setJustificationType(juce::Justification::centredLeft);
    delaySyncLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    delayDivisionLabel.setColour(juce::Label::textColourId, synthcol::textDim);
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
    addAndMakeVisible(delaySyncLabel);
    addAndMakeVisible(delayDivisionLabel);
    addAndMakeVisible(delaySyncSelector);
    addAndMakeVisible(delayDivisionSelector);

    auto setupPerformanceSlider = [this](juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 16);
        slider.setColour(juce::Slider::trackColourId, accent_);
        slider.setColour(juce::Slider::thumbColourId, accent_);
        slider.setColour(juce::Slider::textBoxTextColourId, synthcol::text);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, synthcol::surfHi);
        slider.setColour(juce::Slider::textBoxOutlineColourId, synthcol::border);
        addAndMakeVisible(slider);
    };

    auto setupPerfLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        label.setColour(juce::Label::textColourId, synthcol::textDim);
        addAndMakeVisible(label);
    };

    setupPerfLabel(portamentoLabel, "Portamento");
    setupPerfLabel(legatoLabel, "Legato");
    setupPerfLabel(roundRobinLabel, "Round Robin");
    setupPerfLabel(reverbTypeLabel, "Reverb Type");
    setupPerformanceSlider(portamentoSlider);
    setupPerformanceSlider(legatoSlider);
    setupPerformanceSlider(roundRobinSlider);
    reverbTypeSelector.addItem("PLATE", 1);
    reverbTypeSelector.addItem("HALL", 2);
    addAndMakeVisible(reverbTypeSelector);

    portamentoSlider.setTooltip("Note glide time between repeated notes on the same instrument");
    legatoSlider.setTooltip("Amount of natural legato attack smoothing applied during portamento transitions");
    roundRobinSlider.setTooltip("Deterministic variation amount for repeated notes");
    reverbTypeSelector.setTooltip("Selects the global reverb flavour: Plate or Hall");
    applyValueFormatter(portamentoSlider, [] (double value) { return formatPortamentoDisplay(value); });
    applyValueFormatter(legatoSlider, [] (double value) { return juce::String(juce::roundToInt(value * 100.0)) + "%"; });
    applyValueFormatter(roundRobinSlider, [] (double value) { return juce::String(juce::roundToInt(value * 100.0)) + "%"; });
    portamentoAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "portamento_seconds", portamentoSlider);
    legatoAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "legato_amount", legatoSlider);
    roundRobinAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "round_robin_amount", roundRobinSlider);
    reverbTypeAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "reverb_type", reverbTypeSelector);

    auto setupStatusLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        label.setColour(juce::Label::textColourId, synthcol::textDim);
        addAndMakeVisible(label);
    };

    fxLockButton.setButtonText("FX LOCK");
    fxLockButton.setClickingTogglesState(true);
    fxLockButton.setTooltip("Keep the current master FX chain when switching instrument or preset");
    addAndMakeVisible(fxLockButton);
    fxLockAtt = std::make_unique<BtnAttach>(proc.getAPVTS(), "fx_lock", fxLockButton);

    setupStatusLabel(qualityLabel, "SAT QUALITY");
    qualitySelector.addItem("LIVE", 1);
    qualitySelector.addItem("STUDIO", 2);
    qualitySelector.setTooltip("Saturator quality only: Live is lighter, Studio uses higher-quality oversampling");
    qualityAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "quality_mode", qualitySelector);
    addAndMakeVisible(qualitySelector);

    setupStatusLabel(outputLabel, "STEM");
    outputSelector.addItem(compactOutputRouteLabel(1), 1);
    for (int outputIndex = 0; outputIndex < OrchSynthAudioProcessor::kNumAuxOutputs; ++outputIndex)
        outputSelector.addItem(compactOutputRouteLabel(outputIndex + 2), outputIndex + 2);
    outputSelector.setTooltip("Stem send routing. Main Mix always keeps the full production mix; STEM outputs carry the dry instrument stem.");
    addAndMakeVisible(outputSelector);

    modMatrixTitle.setText("MOD MATRIX", juce::dontSendNotification);
    modMatrixTitle.setJustificationType(juce::Justification::centredLeft);
    modMatrixTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f).withStyle("Bold")));
    modMatrixTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(modMatrixTitle);

    auto setupModHeader = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
        label.setColour(juce::Label::textColourId, synthcol::textDim);
        addAndMakeVisible(label);
    };
    setupModHeader(modMatrixSourceHdr, "Source");
    setupModHeader(modMatrixDestHdr, "Destination");
    setupModHeader(modMatrixAmountHdr, "Amount");
    modMatrixViewport.setName("mod-matrix-viewport");
    modMatrixRowsComponent.setName("mod-matrix-rows");
    modMatrixViewport.setViewedComponent(&modMatrixRowsComponent, false);
    modMatrixViewport.setScrollBarsShown(true, false, true, false);
    modMatrixViewport.setScrollBarThickness(10);
    addAndMakeVisible(modMatrixViewport);

    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        auto si = static_cast<std::size_t>(slotIndex);
        auto& sourceBox = modSourceBoxes[si];
        auto& destBox = modDestBoxes[si];
        auto& amountSlider = modAmountSliders[si];

        for (int itemIndex = 0; itemIndex < 5; ++itemIndex)
            sourceBox.addItem(kModMatrixSourceItems[itemIndex], itemIndex + 1);
        for (int itemIndex = 0; itemIndex < 5; ++itemIndex)
            destBox.addItem(kModMatrixDestItems[itemIndex], itemIndex + 1);
        modMatrixRowsComponent.addAndMakeVisible(sourceBox);
        modMatrixRowsComponent.addAndMakeVisible(destBox);

        amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
        amountSlider.setRange(-1.0, 1.0, 0.001);
        amountSlider.setColour(juce::Slider::trackColourId, accent_);
        amountSlider.setColour(juce::Slider::thumbColourId, accent_);
        amountSlider.setColour(juce::Slider::textBoxTextColourId, synthcol::text);
        amountSlider.setColour(juce::Slider::textBoxBackgroundColourId, synthcol::surfHi);
        amountSlider.setColour(juce::Slider::textBoxOutlineColourId, synthcol::border);
        modMatrixRowsComponent.addAndMakeVisible(amountSlider);

        modSourceAtt[si] = std::make_unique<ComboBoxAttach>(
            proc.getAPVTS(),
            OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, "source"),
            sourceBox);
        modDestAtt[si] = std::make_unique<ComboBoxAttach>(
            proc.getAPVTS(),
            OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, "dest"),
            destBox);
        modAmountAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(),
            OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, "amount"),
            amountSlider);
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
        fxLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(11.3f).withStyle("Bold")));
        fxLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.88f));
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
    fxDetailTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f).withStyle("Bold")));
    fxDetailTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(fxDetailTitle);

    fxUnavailableLbl.setText("Not available for this instrument", juce::dontSendNotification);
    fxUnavailableLbl.setJustificationType(juce::Justification::centred);
    fxUnavailableLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    fxUnavailableLbl.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.55f));
    addChildComponent(fxUnavailableLbl);

    rebuildInstrAttachments();
    syncSelectionUiFromInstr();
    syncFxAvailability();

    // ── Tooltip mode button ──
    tooltipModeBtn.setButtonText("TIP: SHORT");
    tooltipModeBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff2A2A32));
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xffBBBBCC));
    tooltipModeBtn.onClick = [this] { cycleTooltipMode(); };
    addAndMakeVisible(tooltipModeBtn);

    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);
    presetMetaLabel.setMinimumHorizontalScale(0.72f);
    presetMetaLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    presetMetaLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.4f)));
    addAndMakeVisible(presetMetaLabel);

    presetBrowserHintLabel.setJustificationType(juce::Justification::centredLeft);
    presetBrowserHintLabel.setMinimumHorizontalScale(0.78f);
    presetBrowserHintLabel.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.92f));
    presetBrowserHintLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f).withStyle("Bold")));
    addAndMakeVisible(presetBrowserHintLabel);

    // ── MIDI CC page label ──
    midiCCPageLabel.setText("CC: ---", juce::dontSendNotification);
    midiCCPageLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f).withStyle("Bold")));
    midiCCPageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffBBBBCC));
    midiCCPageLabel.setJustificationType(juce::Justification::centredLeft);
    midiCCPageLabel.setMinimumHorizontalScale(0.80f);
    addAndMakeVisible(midiCCPageLabel);

    outputGainLabel.setText("0.0 dB", juce::dontSendNotification);
    outputGainLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    outputGainLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    outputGainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(outputGainLabel);

    randButton.setButtonText("Rand");
    randButton.setTooltip("Randomize current orchestral preset parameters around the current sound");
    randButton.onClick = [this] { proc.randomizePreset(); };
    addAndMakeVisible(randButton);
    randButton.setVisible(kShowAdvancedFrontControls);

    stopNotesButton.setButtonText("Stop");
    stopNotesButton.setTooltip("Stop all currently sounding notes immediately");
    stopNotesButton.onClick = [this] { proc.requestPanicAllVoices(); };
    addAndMakeVisible(stopNotesButton);

    voiceCountLabel.setText("V 0", juce::dontSendNotification);
    voiceCountLabel.setJustificationType(juce::Justification::centred);
    voiceCountLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    voiceCountLabel.setColour(juce::Label::textColourId, synthcol::textSec);
    voiceCountLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1d22));
    voiceCountLabel.setColour(juce::Label::outlineColourId, accent_.withAlpha(0.28f));
    addAndMakeVisible(voiceCountLabel);

    backgroundImage_ = juce::ImageCache::getFromMemory(
        BinaryData::fond_rare_png, BinaryData::fond_rare_pngSize);
    {
        int logoSize = 0;
        if (auto* logoData = BinaryData::getNamedResource("logo_orch.png", logoSize))
            setHeaderLogo(juce::ImageCache::getFromMemory(logoData, logoSize));
    }
    initCommon();
    presetSearch.setTextToShowWhenEmpty("Family / role / name / tag...", synthcol::textDim);

    applyValueFormatter(envDials[3], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[4], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[5], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[6], [](double value) { return juce::String(value, 2); });
    applyValueFormatter(envDials[0], [this](double value) { return formatPercentFromNormalised(envDials[0], value); });
    applyValueFormatter(envDials[1], [](double value) { return formatSignedValue(value, " st", 1); });
    applyValueFormatter(envDials[2], [this](double value) { return formatPercentFromNormalised(envDials[2], value); });
    applyValueFormatter(envDials[7], [this](double value) {
        return std::abs(value) < 0.005 ? juce::String("Off") : formatPercentFromNormalised(envDials[7], value);
    });
    applyValueFormatter(envDials[8], [this](double value) { return formatPercentFromNormalised(envDials[8], value); });
    applyValueFormatter(envDials[9], [this](double value) { return formatPercentFromNormalised(envDials[9], value); });
    applyValueFormatter(envDials[10], [this](double value) { return formatPercentFromNormalised(envDials[10], value); });
    applyValueFormatter(envDials[11], [this](double value) { return formatPercentFromNormalised(envDials[11], value); });
    applyValueFormatter(envDials[12], [](double value) { return formatCutoffDisplay(value); });
    applyValueFormatter(envDials[13], [this](double value) { return formatPanDisplay(envDials[13], value); });
    applyValueFormatter(lfoRateDial, [](double value) { return juce::String(value, 2) + " Hz"; });
    applyValueFormatter(lfoDepthDial, [this](double value) { return formatPercentFromNormalised(lfoDepthDial, value); });
    for (int i = 0; i < kMacroTotal; ++i)
        applyValueFormatter(macroDials[(size_t)i], [this, i](double value) { return formatPercentFromNormalised(macroDials[(size_t)i], value); });
    for (int i = 0; i < kFxN; ++i)
        applyValueFormatter(fxDials[(size_t)i], [](double value) { return juce::String(value, 2); });
    for (int i = 0; i < kModSlots; ++i)
        modAmountSliders[(size_t)i].textFromValueFunction = [](double value) { return formatSignedValue(value, "", 2); };

    envDials[12].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 126, 18);

    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, accent_.withAlpha(0.48f));
    presetSearch.setColour(juce::TextEditor::highlightColourId, accent_.withAlpha(0.22f));

    updatePresetMetadataSummary();
    syncNoteReleaseModeUi();
    updateOutputGainUi();
    applyTooltips();

    startTimerHz(30);
    setResizable(true, true);
    setResizeLimits(960, 600, 2560, 1600);
    setSize(lay::W, lay::H);
}

void OrchSynthAudioProcessorEditor::syncNoteReleaseModeUi()
{
    const bool stopOnRelease = proc.shouldStopNotesOnKeyRelease();
    cachedStopNotesOnKeyRelease = stopOnRelease;
    noteReleaseModeBtn.setToggleState(!stopOnRelease, juce::dontSendNotification);
    noteReleaseModeBtn.setButtonText(stopOnRelease ? "REL: STOP" : "REL: HOLD");
}

// =============================================================================
// Timer
// =============================================================================
void OrchSynthAudioProcessorEditor::timerCallback()
{
    bool needsRepaint = false;
    syncPresetBox();

    if (selectedInstrFromParam() != cachedInstrIdx)
        rebuildInstrAttachments();

    updatePresetMetadataSummary();
    if (cachedStopNotesOnKeyRelease != proc.shouldStopNotesOnKeyRelease())
    {
        syncNoteReleaseModeUi();
        needsRepaint = true;
    }

    const int activeVoices = proc.getActiveVoiceCount();
    if (activeVoices != cachedVoiceCount)
    {
        cachedVoiceCount = activeVoices;
        voiceCountLabel.setText(compactVoiceCountLabel(activeVoices), juce::dontSendNotification);
        needsRepaint = true;
    }

    updateOutputGainUi();

    // ── MIDI CC page sync ──
    const int page = proc.getMidiCCPage();
    if (page != cachedMidiCCPage)
    {
        cachedMidiCCPage = page;
        midiCCPageLabel.setText(compactCcPageLabel(page), juce::dontSendNotification);
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void OrchSynthAudioProcessorEditor::updateOutputGainUi()
{
    if (auto* raw = proc.getAPVTS().getRawParameterValue("output_gain"))
    {
        const auto gainValue = raw->load();
        if (std::abs(gainValue - cachedOutputGainValue) > 1.0e-4f)
        {
            cachedOutputGainValue = gainValue;
            outputGainLabel.setText(formatSignedValue(gainValue, " dB", 1), juce::dontSendNotification);
        }
    }
}

// =============================================================================
// Paint
// =============================================================================
void OrchSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
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
    const int gainSize = layout.compact ? 38 : 42;
    const auto statusPrimaryRow = headerZones.statusPrimaryRow.reduced(0, 1);
    const auto statusSecondaryRow = headerZones.statusSecondaryRow.reduced(0, 1);

    paintHeader(g, layout.headerH);
    glazeOrchChrome(g, headerRect.reduced(1.5f, 1.5f), accent_, 13.0f, 1.0f);

    g.setColour(accent_.withAlpha(0.04f));
    g.fillRoundedRectangle(statusPrimaryRow.toFloat().withWidth(juce::jmin(128.0f, statusPrimaryRow.getWidth() * 0.36f)), 7.0f);

    paintCard(g, layout.contentX, layout.outerMargin + layout.headerH + 8,
              layout.contentW, layout.selectorH - 8, "Family / Model");
    paintCard(g, layout.col1X, layout.bodyY, layout.colW, layout.bodyH, "Source / Envelope");
    paintCard(g, layout.col2X, layout.bodyY, layout.colW, layout.bodyH, "Tone Shaping");
    paintCard(g, layout.col3X, layout.bodyY, layout.colW, layout.bodyH, "Performance / Routing / FX");
    glazeOrchChrome(g, selectorRect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.96f);
    glazeOrchChrome(g, col1Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);
    glazeOrchChrome(g, col2Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);
    glazeOrchChrome(g, col3Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.92f);

    const int gainX = headerZones.statusZone.getRight() - gainSize - 6;
    const int meterW = layout.compact ? 42 : 46;
    const int meterGap = 6;
    const int meterBlockW = meterW * 2 + meterGap + 10;
    const int meterLeft = juce::jmax(statusSecondaryRow.getX() + 98, gainX - meterBlockW - 10);
    const int meterY = statusSecondaryRow.getCentreY() - 4;
    const bool showHeaderMeters = !useCompactHeaderCopy(getWidth(), getHeight())
        && meterLeft >= statusSecondaryRow.getX() + 84
        && meterLeft + meterBlockW <= gainX - 8;
    if (showHeaderMeters)
    {
        paintMeterBar(g, { meterLeft, meterY, meterW, 8 },
                      proc.getMainMeterLevel(0), accent_);
        paintMeterBar(g, { meterLeft + meterW + meterGap, meterY, meterW, 8 },
                      proc.getMainMeterLevel(1), accent_.brighter(0.22f));
        g.setColour(synthcol::textDim);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.6f)));
        g.drawText("L", juce::Rectangle<int>(meterLeft - 10, meterY - 2, 10, 12), juce::Justification::centredLeft);
        g.drawText("R", juce::Rectangle<int>(meterLeft + meterW + meterGap - 10, meterY - 2, 10, 12), juce::Justification::centredLeft);
    }

    g.setColour(accent_.withAlpha(0.12f));
    g.drawLine(static_cast<float>(layout.contentX + 18), static_cast<float>(layout.kbY - 8),
               static_cast<float>(layout.contentX + layout.contentW - 18), static_cast<float>(layout.kbY - 8),
               1.0f);

    paintKeyboardDock(g, layout.contentX, layout.kbY, layout.contentW, layout.kbH);
    glazeOrchChrome(g, keyboardRect.reduced(2.0f, 2.0f), accent_, 11.0f, 1.04f);
}

// =============================================================================
// Resized
// =============================================================================
void OrchSynthAudioProcessorEditor::resized()
{
    const auto layout = computeLayoutMetrics(getWidth(), getHeight());
    const auto headerZones = computeHeaderZones(layout.headerH);

    const int ctrlH   = layout.compact ? 32 : 34;
    const int gainSize = layout.compact ? 38 : 42;
    const auto presetPrimaryRow = headerZones.presetPrimaryRow.reduced(0, 1);
    const auto presetSecondaryRow = headerZones.presetSecondaryRow.reduced(0, 1);
    const auto statusPrimaryRow = headerZones.statusPrimaryRow.reduced(0, 1);
    const auto statusSecondaryRow = headerZones.statusSecondaryRow.reduced(0, 1);
    const int topRowY = presetPrimaryRow.getY() + (presetPrimaryRow.getHeight() - ctrlH) / 2;

    int navW    = 26;
    int searchW = juce::jlimit(layout.compact ? 94 : 100,
                               layout.compact ? 124 : 140,
                               presetPrimaryRow.getWidth() / 5);
    int x = presetPrimaryRow.getX();
    const int presetW = juce::jmax(layout.compact ? 234 : 320,
                                   presetPrimaryRow.getRight() - x - searchW - navW * 2 - 16);
    presetSearch.setBounds(x, topRowY, searchW, ctrlH);   x += searchW + 8;
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH);     x += navW + 4;
    presetBox.setBounds(x, topRowY, presetW, ctrlH);      x += presetW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    const int actionY = presetSecondaryRow.getY() + juce::jmax(0, (presetSecondaryRow.getHeight() - 22) / 2);
    const int saveW = layout.compact ? 50 : 64;
    const int saveAsW = layout.compact ? 58 : 78;
    const int deleteW = layout.compact ? 54 : 70;
    const int importW = layout.compact ? 58 : 70;
    const int randW = layout.compact ? 60 : 68;
    const int stopW = layout.compact ? 52 : 68;
    const int btnGap = layout.compact ? 6 : 8;
    const int actionX = presetSecondaryRow.getX();
    const int actionBtnH = layout.compact ? 22 : 24;
    savePresetBtn.setBounds(actionX, actionY, saveW, actionBtnH);
    saveAsPresetBtn.setBounds(savePresetBtn.getRight() + btnGap, actionY, saveAsW, actionBtnH);
    deletePresetBtn.setBounds(saveAsPresetBtn.getRight() + btnGap, actionY, deleteW, actionBtnH);
    importPresetsBtn.setBounds(deletePresetBtn.getRight() + btnGap, actionY, importW, actionBtnH);
    if (kShowAdvancedFrontControls)
        randButton.setBounds(importPresetsBtn.getRight() + btnGap, actionY, randW, actionBtnH);
    else
        randButton.setBounds(0, 0, 0, 0);
    const int stopX = kShowAdvancedFrontControls ? randButton.getRight() + btnGap
                                                 : importPresetsBtn.getRight() + btnGap;
    stopNotesButton.setBounds(stopX, actionY, stopW, actionBtnH);
    const int metaX = stopNotesButton.getRight() + 10;
    const int metaW = juce::jmax(0, presetSecondaryRow.getRight() - metaX);
    const int browserHintW = metaW < 220 ? metaW : juce::jlimit(0, 248, metaW / 3);
    presetBrowserHintLabel.setBounds(metaX, actionY - 1, browserHintW, actionBtnH + 2);
    presetBrowserHintLabel.setVisible(browserHintW > 88);
    presetMetaLabel.setBounds(metaX + browserHintW + (browserHintW > 0 ? 8 : 0),
                              actionY - 1,
                              juce::jmax(0, metaW - browserHintW - (browserHintW > 0 ? 8 : 0)),
                              actionBtnH + 2);
    presetMetaLabel.setVisible(metaW - browserHintW > 132);

    const int statusBtnH = layout.compact ? 22 : 24;
    const int statusPrimaryY = statusPrimaryRow.getY() + juce::jmax(0, (statusPrimaryRow.getHeight() - statusBtnH) / 2);
    int statusX = statusPrimaryRow.getX();
    const int releaseW = layout.compact ? 72 : 78;
    const int tipW = layout.compact ? 62 : 68;
    const int voicesW = layout.compact ? 42 : 48;
    if (kShowAdvancedFrontControls)
    {
        noteReleaseModeBtn.setBounds(statusX, statusPrimaryY, releaseW, statusBtnH);
        statusX += releaseW + 5;
    }
    else
    {
        noteReleaseModeBtn.setBounds(0, 0, 0, 0);
    }
    tooltipModeBtn.setBounds(statusX, statusPrimaryY, tipW, statusBtnH);
    statusX += tipW + 5;
    voiceCountLabel.setBounds(statusX, statusPrimaryY, voicesW, statusBtnH);
    statusX += voicesW + 6;
    midiCCPageLabel.setBounds(statusX, statusPrimaryY,
                              juce::jmax(54, statusPrimaryRow.getRight() - statusX), statusBtnH);

    const int secondaryCentreY = statusSecondaryRow.getCentreY();
    const int secondaryCtrlH = layout.compact ? 22 : 24;
    const int secondaryCtrlY = secondaryCentreY - secondaryCtrlH / 2;
    const int gainX = headerZones.statusZone.getRight() - gainSize - 6;
    const int gainY = secondaryCentreY - gainSize / 2;
    gainDial.setBounds(gainX, gainY, gainSize, gainSize);

    const bool compactHeaderCopy = useCompactHeaderCopy(getWidth(), getHeight());
    fxLockButton.setButtonText(compactHeaderCopy ? "FX" : "FX LOCK");
    qualityLabel.setText(compactHeaderCopy ? "SAT" : "SAT QUALITY", juce::dontSendNotification);
    outputLabel.setText(compactHeaderCopy ? "OUT" : "STEM", juce::dontSendNotification);

    int secondaryX = statusSecondaryRow.getX();
    const int lockW = compactHeaderCopy ? 50 : 88;
    const int qualityLabelW = compactHeaderCopy ? 24 : 82;
    const int qualitySelectorW = compactHeaderCopy ? 60 : 82;
    const int outputLabelW = compactHeaderCopy ? 24 : 58;
    const int minOutputSelectorW = compactHeaderCopy ? 44 : 88;
    const int maxOutputSelectorW = compactHeaderCopy ? 50 : 110;
    const int minOutputGainW = layout.compact ? 72 : 84;
    const int secondaryGap = compactHeaderCopy ? 6 : 10;
    const int statusControlsRight = gainX - secondaryGap;

    fxLockButton.setBounds(secondaryX, secondaryCtrlY, lockW, secondaryCtrlH);
    secondaryX += lockW + secondaryGap;

    qualityLabel.setBounds(secondaryX, secondaryCtrlY, qualityLabelW, secondaryCtrlH);
    secondaryX += qualityLabelW + (compactHeaderCopy ? 3 : 4);
    qualitySelector.setBounds(secondaryX, secondaryCtrlY, qualitySelectorW, secondaryCtrlH);
    secondaryX += qualitySelectorW + secondaryGap;

    outputLabel.setBounds(secondaryX, secondaryCtrlY, outputLabelW, secondaryCtrlH);
    secondaryX += outputLabelW + (compactHeaderCopy ? 3 : 4);
    const int outputSelectorMaxW = juce::jmax(minOutputSelectorW,
                                              statusControlsRight - secondaryX);
    const int outputSelectorW = juce::jlimit(minOutputSelectorW, maxOutputSelectorW, outputSelectorMaxW);
    outputSelector.setBounds(secondaryX, secondaryCtrlY, outputSelectorW, secondaryCtrlH);
    secondaryX += outputSelectorW + secondaryGap;

    const int outputGainW = statusControlsRight - secondaryX;
    outputGainLabel.setVisible(!compactHeaderCopy && outputGainW >= minOutputGainW);
    outputGainLabel.setBounds(secondaryX, secondaryCentreY - 10,
                              juce::jmax(0, outputGainW), 20);

    refreshPresetFilterChoices();

    const int selPad = layout.compact ? 12 : 14;
    const int selectorInnerX = layout.contentX + selPad;
    const int selectorInnerW = layout.contentW - selPad * 2;
    const int selectorLabelY = layout.selectorY + (layout.compact ? 28 : 29);
    const int selectorLabelH = 12;
    const int selectorTopY = selectorLabelY + 15;
    const int selectorRowH = layout.compact ? 22 : 24;
    const int selectorGap = layout.compact ? 10 : 12;
    const int tabsZoneW = static_cast<int>(selectorInnerW * (layout.compact ? 0.62f : 0.66f));
    const int comboZoneW = selectorInnerW - tabsZoneW - selectorGap;
    const int tabGap = layout.compact ? 6 : 8;
    const int tabsY = selectorTopY;
    const int tabsH = selectorRowH;
    const int tabW = (tabsZoneW - tabGap * (mos::kNumFamilies - 1)) / mos::kNumFamilies;
    for (int familyIndex = 0; familyIndex < mos::kNumFamilies; ++familyIndex)
        familyTabs[(size_t)familyIndex].setBounds(selectorInnerX + familyIndex * (tabW + tabGap), tabsY, tabW, tabsH);

    familySelectorLbl.setVisible(true);
    familySelectorLbl.setBounds(selectorInnerX, selectorLabelY, tabsZoneW, selectorLabelH);
    familySelector.setVisible(false);
    familySelector.setBounds(0, 0, 0, 0);

    modelSelectorLbl.setVisible(true);
    modelSelectorLbl.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorLabelY, comboZoneW, selectorLabelH);
    modelSelector.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorTopY, comboZoneW, selectorRowH);

    const int cPad  = layout.compact ? 14 : 18;
    const int knobGapX = layout.compact ? 8 : (layout.roomy ? 13 : 11);
    const int knobGapY = layout.compact ? 9 : (layout.roomy ? 14 : 12);
    const int knobW = (layout.colW - cPad * 2 - knobGapX * 2) / 3;
    const int lblH  = layout.compact ? 12 : 14;
    const int graphTargetH = layout.compact ? 86 : (layout.roomy ? 178 : 126);
    const int knobH = juce::jlimit(layout.compact ? 50 : 58,
                                   layout.roomy ? 98 : 84,
                                   (layout.bodyH - graphTargetH - cPad * 2 - lblH * 3 - knobGapY * 3) / 3);

    int toneIdx[] = { 8, 9, 10, 2, 13, 11 };
    const int col2StartY = layout.bodyY + cPad + 28;
    for (int i = 0; i < 6; ++i)
    {
        int row = i / 3, col = i % 3;
        int xk = layout.col2X + cPad + col * (knobW + knobGapX);
        int yk = col2StartY + row * (knobH + lblH + knobGapY);
        auto si = (size_t)toneIdx[i];
        envLabels[si].setBounds(xk, yk,         knobW, lblH);
        envDials[si] .setBounds(xk, yk + lblH,  knobW, knobH);
    }

    int cutoffSize = juce::jlimit(layout.compact ? 66 : 72,
                                  layout.roomy ? 116 : 104,
                                  knobH + (layout.compact ? 14 : 18));
    int cutoffX = layout.col2X + (layout.colW - cutoffSize) / 2;
    int cutoffY = col2StartY + 2 * (knobH + lblH + knobGapY) + (layout.compact ? 6 : 10);
    envLabels[12].setBounds(cutoffX, cutoffY,          cutoffSize, lblH);
    envDials[12] .setBounds(cutoffX, cutoffY + lblH,   cutoffSize, cutoffSize);

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

    lfoVisual.setVisible(false);
    lfoVisual.setBounds(0, 0, 0, 0);
    lfoRateDial.setVisible(false);
    lfoDepthDial.setVisible(false);
    lfoWaveSelector.setVisible(false);

    const int col3StartY = layout.bodyY + cPad + 28;
    const int rightTabGap = layout.compact ? 6 : 8;
    const int rightTabH = layout.compact ? 23 : 26;
    const int rightTabW = (layout.colW - cPad * 2 - rightTabGap * (kRightPanelSections - 1)) / kRightPanelSections;
    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.setBounds(layout.col3X + cPad + sectionIndex * (rightTabW + rightTabGap),
                      col3StartY,
                      rightTabW,
                      rightTabH);
        tab.setSelected(sectionIndex == activeRightPanelSection);
    }
    const int sectionContentY = col3StartY + rightTabH + (layout.compact ? 12 : 14);
    const int macroGap = layout.compact ? 7 : 9;
    const int macroW = (layout.colW - cPad * 2 - macroGap * 3) / 4;
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
            int xk = layout.col3X + cPad + i * (macroW + macroGap);
            int yk = sectionContentY;
            macroLbls[si].setBounds(xk, yk,         macroW, lblH);
            macroDials[si].setBounds(xk, yk + lblH,  macroW, macroH);
        }
        else
        {
            macroLbls[si].setBounds(0, 0, 0, 0);
            macroDials[si].setBounds(0, 0, 0, 0);
        }
    }

    velocityCurveLabel.setVisible(activeRightPanelSection == 0);
    velocityCurveSelector.setVisible(activeRightPanelSection == 0);
    delaySyncLabel.setVisible(activeRightPanelSection == 0);
    delaySyncSelector.setVisible(activeRightPanelSection == 0);
    delayDivisionLabel.setVisible(activeRightPanelSection == 0);
    delayDivisionSelector.setVisible(activeRightPanelSection == 0);
    portamentoLabel.setVisible(activeRightPanelSection == 0);
    portamentoSlider.setVisible(activeRightPanelSection == 0);
    legatoLabel.setVisible(activeRightPanelSection == 0);
    legatoSlider.setVisible(activeRightPanelSection == 0);
    roundRobinLabel.setVisible(activeRightPanelSection == 0);
    roundRobinSlider.setVisible(activeRightPanelSection == 0);
    reverbTypeLabel.setVisible(activeRightPanelSection == 0);
    reverbTypeSelector.setVisible(activeRightPanelSection == 0);
    if (activeRightPanelSection == 0)
    {
        const int perfLabelY = sectionContentY + macroH + lblH + (layout.compact ? 12 : 14);
        const int perfGap = layout.compact ? 6 : 8;
        const int perfW = (layout.colW - cPad * 2 - perfGap * 2) / 3;
        velocityCurveLabel.setBounds(layout.col3X + cPad, perfLabelY, perfW, 14);
        velocityCurveSelector.setBounds(layout.col3X + cPad, perfLabelY + 16, perfW, layout.compact ? 24 : 26);
        delaySyncLabel.setBounds(velocityCurveSelector.getRight() + perfGap, perfLabelY, perfW, 14);
        delaySyncSelector.setBounds(velocityCurveSelector.getRight() + perfGap, perfLabelY + 16, perfW, layout.compact ? 24 : 26);
        delayDivisionLabel.setBounds(delaySyncSelector.getRight() + perfGap, perfLabelY, perfW, 14);
        delayDivisionSelector.setBounds(delaySyncSelector.getRight() + perfGap, perfLabelY + 16, perfW, layout.compact ? 24 : 26);

        const int advancedLabelY = velocityCurveSelector.getBottom() + (layout.compact ? 10 : 12);
        const int advancedControlH = layout.compact ? 24 : 26;
        portamentoLabel.setBounds(layout.col3X + cPad, advancedLabelY, perfW, 14);
        portamentoSlider.setBounds(layout.col3X + cPad, advancedLabelY + 16, perfW, advancedControlH);
        legatoLabel.setBounds(portamentoSlider.getRight() + perfGap, advancedLabelY, perfW, 14);
        legatoSlider.setBounds(portamentoSlider.getRight() + perfGap, advancedLabelY + 16, perfW, advancedControlH);
        roundRobinLabel.setBounds(legatoSlider.getRight() + perfGap, advancedLabelY, perfW, 14);
        roundRobinSlider.setBounds(legatoSlider.getRight() + perfGap, advancedLabelY + 16, perfW, advancedControlH);
        const int reverbLabelY = roundRobinSlider.getBottom() + (layout.compact ? 10 : 12);
        reverbTypeLabel.setBounds(layout.col3X + cPad, reverbLabelY, perfW, 14);
        reverbTypeSelector.setBounds(layout.col3X + cPad, reverbLabelY + 16, perfW, advancedControlH);

        const int lfoY = reverbTypeSelector.getBottom() + (layout.compact ? 12 : 14);
        const int lfoAvailableH = protectedKeyboardTop - lfoY - (layout.compact ? 8 : 10);
        const int minLfoH = layout.compact ? 48 : 88;
        const int maxLfoH = layout.roomy ? 160 : 126;
        const bool lfoFits = lfoAvailableH >= minLfoH;
        const int lfoH = lfoFits ? juce::jlimit(minLfoH, maxLfoH, lfoAvailableH) : 0;
        lfoVisual.setVisible(lfoFits);
        lfoVisual.setBounds(lfoFits ? layout.col3X + cPad : 0,
                            lfoFits ? lfoY : 0,
                            lfoFits ? layout.colW - cPad * 2 : 0,
                            lfoH);
    }
    else
    {
        velocityCurveLabel.setBounds(0, 0, 0, 0);
        velocityCurveSelector.setBounds(0, 0, 0, 0);
        delaySyncLabel.setBounds(0, 0, 0, 0);
        delaySyncSelector.setBounds(0, 0, 0, 0);
        delayDivisionLabel.setBounds(0, 0, 0, 0);
        delayDivisionSelector.setBounds(0, 0, 0, 0);
        portamentoLabel.setBounds(0, 0, 0, 0);
        portamentoSlider.setBounds(0, 0, 0, 0);
        legatoLabel.setBounds(0, 0, 0, 0);
        legatoSlider.setBounds(0, 0, 0, 0);
        roundRobinLabel.setBounds(0, 0, 0, 0);
        roundRobinSlider.setBounds(0, 0, 0, 0);
        reverbTypeLabel.setBounds(0, 0, 0, 0);
        reverbTypeSelector.setBounds(0, 0, 0, 0);
    }

    const bool showModMatrix = activeRightPanelSection == 1;
    modMatrixTitle.setVisible(showModMatrix);
    modMatrixSourceHdr.setVisible(showModMatrix);
    modMatrixDestHdr.setVisible(showModMatrix);
    modMatrixAmountHdr.setVisible(showModMatrix);
    modMatrixViewport.setVisible(showModMatrix);
    const int modBlockY = sectionContentY;
    const int modTitleH = 16;
    const int modHeaderGap = 6;
    const int modRowH = layout.compact ? 22 : 24;
    const int modRowGap = layout.compact ? 5 : 6;
    const int modViewportW = layout.colW - cPad * 2;
    const int modContentW = juce::jmax(160, modViewportW - modMatrixViewport.getScrollBarThickness() - 6);
    const int modSourceW = juce::jlimit(74, 102, modContentW / 4);
    const int modDestW = juce::jlimit(88, 118, modContentW / 3);
    const int modAmountW = juce::jmax(62, modContentW - modSourceW - modDestW - 18);
    modMatrixTitle.setBounds(layout.col3X + cPad, modBlockY, layout.colW - cPad * 2, modTitleH);
    const int modHeaderY = modBlockY + modTitleH + 2;
    modMatrixSourceHdr.setBounds(layout.col3X + cPad, modHeaderY, modSourceW, 14);
    modMatrixDestHdr.setBounds(modMatrixSourceHdr.getRight() + 8, modHeaderY, modDestW, 14);
    modMatrixAmountHdr.setBounds(modMatrixDestHdr.getRight() + 8, modHeaderY, modAmountW, 14);
    const int modRowsY = modHeaderY + 14 + modHeaderGap;
    const int modViewportH = juce::jmax(layout.compact ? 116 : 132, protectedKeyboardTop - modRowsY - 14);
    modMatrixViewport.setBounds(layout.col3X + cPad, modRowsY, modViewportW, modViewportH);
    for (int slotIndex = 0; slotIndex < kModSlots; ++slotIndex)
    {
        const int rowY = slotIndex * (modRowH + modRowGap);
        const auto si = static_cast<std::size_t>(slotIndex);
        modSourceBoxes[si].setBounds(0, rowY, modSourceW, modRowH);
        modDestBoxes[si].setBounds(modSourceBoxes[si].getRight() + 8, rowY, modDestW, modRowH);
        modAmountSliders[si].setBounds(modDestBoxes[si].getRight() + 8, rowY, modAmountW, modRowH);
    }
    modMatrixRowsComponent.setSize(modContentW,
                                   kModSlots * modRowH + juce::jmax(0, kModSlots - 1) * modRowGap);

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

    int visibleRackRow = 0;
    for (int t = 0; t < kFxTabs; ++t)
    {
        const bool available = activeRightPanelSection == 2 && isFxTabAvailable(t);
        fxRackItems[(size_t)t].setVisible(available);
        fxBypassBtns[(size_t)t].setVisible(available);
        if (!available)
        {
            fxRackItems[(size_t)t].setBounds(0, 0, 0, 0);
            fxBypassBtns[(size_t)t].setBounds(0, 0, 0, 0);
            continue;
        }

        const int rowY = rackStartY + visibleRackRow * (rackRowH + kRackRowGap);
        fxRackItems[(size_t)t].setBounds(layout.col3X + cPad, rowY, rackItemW, rackRowH);
        fxBypassBtns[(size_t)t].setBounds(layout.col3X + cPad + rackItemW + 6, rowY + (rackRowH - 18) / 2, kBypassW, 18);
        ++visibleRackRow;
    }

    const int detailX = layout.col3X + cPad + rackTotalW + kRackGap;
    const int detailW = layout.colW - cPad * 2 - rackTotalW - kRackGap;
    const int currentFxTab = (activeFxTab >= 0 && activeFxTab < kFxTabs) ? activeFxTab : firstAvailableFxTab();
    const bool showFxUnavailable = activeRightPanelSection == 2 && !isFxTabAvailable(currentFxTab);
    fxDetailTitle.setVisible(activeRightPanelSection == 2);
    fxUnavailableLbl.setVisible(showFxUnavailable);
    fxDetailTitle.setBounds(detailX, fxAreaY, detailW, 16);
    fxUnavailableLbl.setBounds(detailX, fxAreaY + 22, detailW, 20);

    for (int i = 0; i < kFxN; ++i)
    {
        fxDials[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setBounds(0, 0, 0, 0);
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
    for (int k = 0; k < kFxPerTab; ++k)
    {
        const int fi = currentFxTab >= 0 ? kFxTabMap[currentFxTab][k] : -1;
        if (fi < 0)
            continue;

        auto si = (size_t)fi;
        const int row = visibleIndex / detailCols;
        const int col = visibleIndex % detailCols;
        const int xk = detailX + col * (detailKnobW + detailGap);
        const int yk = detailStartY + row * detailRowStride;
        if (activeRightPanelSection == 2)
        {
            fxLbls[si].setBounds(xk, yk, detailKnobW, lblH);
            fxDials[si].setBounds(xk, yk + lblH, detailKnobW, detailKnobH);
            fxLbls[si].setVisible(true);
            fxDials[si].setVisible(true);
        }
        ++visibleIndex;
    }

    const int keyboardInsetLeft = 66;
    const int keyboardInsetTop = layout.compact ? 6 : 8;
    const int keyboardH = juce::jmax(36, layout.kbH - keyboardInsetTop * 2);
    const int keyboardCenterY = layout.kbY + keyboardInsetTop + keyboardH / 2;
    keyboard->setBounds(layout.contentX + keyboardInsetLeft, layout.kbY + keyboardInsetTop,
                        layout.contentW - keyboardInsetLeft - 4, keyboardH);
    octaveDownBtn.setBounds(layout.contentX + 6, keyboardCenterY - 14, 20, 26);
    octaveUpBtn.setBounds(layout.contentX + 30, keyboardCenterY - 14, 20, 26);

    auto applyLbl = [layout](juce::Label& l) {
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(layout.compact ? 11.0f : 12.0f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, synthcol::textSec);
    };
    for (auto& l : envLabels) applyLbl(l);
    for (auto& l : macroLbls) applyLbl(l);
    for (auto& l : fxLbls)    applyLbl(l);
    applyLbl(velocityCurveLabel);
}

// =============================================================================
// Switch effect tab
// =============================================================================
bool OrchSynthAudioProcessorEditor::isFxTabAvailable(int tabIndex) const
{
    return tabIndex >= 0 && tabIndex < kFxTabs
        && proc.isFxAvailableForCurrentInstr(kOrchFxSlots[(size_t)tabIndex]);
}

int OrchSynthAudioProcessorEditor::firstAvailableFxTab() const
{
    for (int t = 0; t < kFxTabs; ++t)
        if (isFxTabAvailable(t))
            return t;
    return 0;
}

void OrchSynthAudioProcessorEditor::syncFxAvailability()
{
    std::array<bool, kFxTabs> availability {};
    for (int t = 0; t < kFxTabs; ++t)
        availability[static_cast<std::size_t>(t)] = isFxTabAvailable(t);

    const bool availabilityChanged = !cachedFxAvailabilityValid || availability != cachedFxAvailability;
    cachedFxAvailability = availability;
    cachedFxAvailabilityValid = true;

    const int fallbackTab = firstAvailableFxTab();
    int resolvedTab = activeFxTab;
    if (resolvedTab < 0 || resolvedTab >= kFxTabs
        || !availability[static_cast<std::size_t>(resolvedTab)])
        resolvedTab = fallbackTab;

    const bool tabChanged = resolvedTab != activeFxTab;
    activeFxTab = resolvedTab;
    fxDetailTitle.setText(juce::String("FX Detail: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
    syncFxRackState();
    if (availabilityChanged || tabChanged)
    {
        resized();
        repaint();
    }
}

void OrchSynthAudioProcessorEditor::switchEffectTab(int tabIndex)
{
    if (!isFxTabAvailable(tabIndex))
        return;
    if (activeFxTab == tabIndex)
        return;

    activeFxTab = tabIndex;
    fxDetailTitle.setText(juce::String("FX Detail: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
    syncFxRackState();
    resized();
    repaint();
}

void OrchSynthAudioProcessorEditor::switchRightPanelSection(int sectionIndex)
{
    if (sectionIndex < 0 || sectionIndex >= kRightPanelSections || activeRightPanelSection == sectionIndex)
        return;

    activeRightPanelSection = sectionIndex;
    resized();
    repaint();
}

void OrchSynthAudioProcessorEditor::syncFxRackState()
{
    for (int t = 0; t < kFxTabs; ++t)
    {
        if (!isFxTabAvailable(t)) continue; // item est masqué dans resized()
        auto& rackItem = fxRackItems[(size_t)t];
        auto& bypass = fxBypassBtns[(size_t)t];
        rackItem.setSelected(t == activeFxTab);
        rackItem.setEnabledState(bypass.getToggleState());
        bypass.setEnabled(true);
        bypass.setTooltip(juce::String());
        rackItem.setTooltip(juce::String(kFxRackSummaries[t]));
    }
}

// =============================================================================
// Instr attachment management
// =============================================================================
void OrchSynthAudioProcessorEditor::rebuildInstrAttachments()
{
    auto instrIdx = selectedInstrFromParam();
    if (instrIdx == cachedInstrIdx) return;
    cachedInstrIdx = instrIdx;
    const auto family = mos::getFamily(instrIdx);
    const auto& profile = envProfileForInstrument(instrIdx);
    const auto& macroLabels = macroLabelsForFamily(family);

    for (auto& a : envAttach) a.reset();

    for (int i = 0; i < kEnvN; ++i)
    {
        auto si = (size_t)i;
        auto id = OrchSynthAudioProcessor::makeInstrParamId(
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
                setupGrandDial(envDials[si], accent_, {});
                break;
            case 13:
                setupDial(envDials[si], accent_);
                break;
            default:
                setupDial(envDials[si], accent_);
                break;
        }
    }
    synthui::applyLabelProfile(profile, envLabels);
    synthui::applyMacroLabelProfile(macroLabels, macroLbls);
    applyInstrumentControlAvailability(instrIdx);

    outputAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(),
        OrchSynthAudioProcessor::makeInstrParamId(cachedInstrIdx, "output"),
        outputSelector);

    applyInstrumentTheme(instrIdx);

    activeFamilyIndex = static_cast<int>(family);
    syncSelectionUiFromInstr();
    syncFxAvailability();
    refreshPresetFilterChoices();
    refreshPresetList();
    updatePresetMetadataSummary();
    repaint();
}

void OrchSynthAudioProcessorEditor::applyInstrumentTheme(int instrIndex)
{
    const auto catC = instrCatColour(instrIndex);
    const auto noteRange = mos::getInstrMidiNoteRange(instrIndex);
    const auto controlText = catC.brighter(0.18f);
    const auto panelBg = juce::Colour(0xff1A1B20).interpolatedWith(catC.withAlpha(0.14f), 0.12f);
    const auto pillBg = juce::Colour(0xff14161B).withAlpha(0.84f);
    const auto chromeHeader = juce::Colour(0xff35363B);
    const auto chromeBase = juce::Colour(0xff171A1F);
    const auto chromeCavity = juce::Colour(0xff0F1216);
    const auto chromeTitle = juce::Colour(0xff22252B);
    const auto chromeKeyboard = juce::Colour(0xff0E1115);
    setAccentTheme(catC);
    setKeyboardPlayableRange(noteRange.low, noteRange.high);
    setChromePalette(chromeHeader, chromeBase, chromeCavity, chromeTitle, chromeKeyboard);

    for (auto& dial : envDials)
        applyKnobPalette(dial, catC);

    for (auto& dial : macroDials)
        applyKnobPalette(dial, catC);

    for (auto& slider : modAmountSliders)
    {
        slider.setColour(juce::Slider::trackColourId, catC);
        slider.setColour(juce::Slider::thumbColourId, catC);
    }

    for (auto& dial : fxDials)
        applyKnobPalette(dial, catC);

    applyKnobPalette(lfoRateDial, catC);
    applyKnobPalette(lfoDepthDial, catC);
    applyKnobPalette(gainDial, catC);

    for (auto* combo : { &modelSelector, &lfoWaveSelector, &velocityCurveSelector, &delaySyncSelector,
                         &delayDivisionSelector, &reverbTypeSelector, &qualitySelector, &outputSelector })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, panelBg);
        combo->setColour(juce::ComboBox::outlineColourId, catC.withAlpha(0.35f));
    }

    for (auto* slider : { &portamentoSlider, &legatoSlider, &roundRobinSlider })
    {
        slider->setColour(juce::Slider::trackColourId, catC);
        slider->setColour(juce::Slider::thumbColourId, catC);
        slider->setColour(juce::Slider::textBoxOutlineColourId, catC.withAlpha(0.35f));
    }

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

    for (juce::TextButton* button : { static_cast<juce::TextButton*>(&noteReleaseModeBtn),
                                      static_cast<juce::TextButton*>(&randButton),
                                      static_cast<juce::TextButton*>(&stopNotesButton),
                                      &tooltipModeBtn })
    {
        button->setColour(juce::TextButton::buttonColourId, panelBg);
        button->setColour(juce::TextButton::buttonOnColourId, panelBg.brighter(0.05f));
        button->setColour(juce::TextButton::textColourOffId, controlText);
        button->setColour(juce::TextButton::textColourOnId, controlText.brighter(0.08f));
    }

    fxLockButton.setColour(juce::ToggleButton::tickColourId, catC);
    fxLockButton.setColour(juce::ToggleButton::textColourId, controlText);

    envVisual.setAccent(catC);
    lfoVisual.setAccent(catC);
    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, catC.withAlpha(0.50f));
    presetSearch.setColour(juce::TextEditor::highlightColourId, catC.withAlpha(0.20f));
    presetSearch.setColour(juce::TextEditor::outlineColourId, catC.withAlpha(0.22f));

    modMatrixTitle.setColour(juce::Label::textColourId, catC.brighter(0.25f));
    fxDetailTitle.setColour(juce::Label::textColourId, catC.brighter(0.30f));
    presetBrowserHintLabel.setColour(juce::Label::textColourId, controlText.withAlpha(0.92f));
    presetMetaLabel.setColour(juce::Label::textColourId, catC.brighter(0.12f));
    outputGainLabel.setColour(juce::Label::textColourId, controlText);
    velocityCurveLabel.setColour(juce::Label::textColourId, controlText);
    delaySyncLabel.setColour(juce::Label::textColourId, controlText);
    delayDivisionLabel.setColour(juce::Label::textColourId, controlText);
    portamentoLabel.setColour(juce::Label::textColourId, controlText);
    legatoLabel.setColour(juce::Label::textColourId, controlText);
    roundRobinLabel.setColour(juce::Label::textColourId, controlText);
    reverbTypeLabel.setColour(juce::Label::textColourId, controlText);
    qualityLabel.setColour(juce::Label::textColourId, controlText);
    outputLabel.setColour(juce::Label::textColourId, controlText);
    familySelectorLbl.setColour(juce::Label::textColourId, controlText);
    modelSelectorLbl.setColour(juce::Label::textColourId, controlText);
    modMatrixSourceHdr.setColour(juce::Label::textColourId, controlText);
    modMatrixDestHdr.setColour(juce::Label::textColourId, controlText);
    modMatrixAmountHdr.setColour(juce::Label::textColourId, controlText);
    fxUnavailableLbl.setColour(juce::Label::textColourId, controlText.withAlpha(0.58f));

    midiCCPageLabel.setColour(juce::Label::textColourId, controlText);
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, pillBg);
    midiCCPageLabel.setColour(juce::Label::outlineColourId, catC.withAlpha(0.32f));
    voiceCountLabel.setColour(juce::Label::textColourId, controlText);
    voiceCountLabel.setColour(juce::Label::backgroundColourId, pillBg);
    voiceCountLabel.setColour(juce::Label::outlineColourId, catC.withAlpha(0.28f));

    for (auto& tab : rightPanelTabs)
        tab.setAccent(catC);

    for (auto& rackItem : fxRackItems)
        rackItem.setAccent(catC);
}

void OrchSynthAudioProcessorEditor::applyInstrumentControlAvailability(int instrIdx)
{
    const bool lockMotion = isModalInstrumentForUi(instrIdx);
    const bool lockSpread = isModalInstrumentForUi(instrIdx);

    for (int controlIndex = 0; controlIndex < kEnvN; ++controlIndex)
    {
        const bool enabled = !((controlIndex == 7 && lockMotion)
                            || (controlIndex == 9 && lockSpread));
        auto& dial = envDials[static_cast<std::size_t>(controlIndex)];
        auto& label = envLabels[static_cast<std::size_t>(controlIndex)];
        dial.setEnabled(enabled);
        label.setEnabled(enabled);
        label.setAlpha(enabled ? 1.0f : 0.55f);
    }
}

void OrchSynthAudioProcessorEditor::rebuildFactoryPresetDisplayOrder()
{
    const auto rawNames = proc.getFactoryPresetNames();
    factoryPresetDisplayOrder.resize(static_cast<std::size_t>(rawNames.size()));
    std::iota(factoryPresetDisplayOrder.begin(), factoryPresetDisplayOrder.end(), 0);
    std::stable_sort(factoryPresetDisplayOrder.begin(), factoryPresetDisplayOrder.end(),
        [this] (int lhs, int rhs)
        {
            const auto* leftPreset = proc.getFactoryPresetDefinition(lhs);
            const auto* rightPreset = proc.getFactoryPresetDefinition(rhs);
            if (leftPreset == nullptr || rightPreset == nullptr)
                return lhs < rhs;
            const int leftRank = presetRoleDisplayRank(leftPreset->metadata);
            const int rightRank = presetRoleDisplayRank(rightPreset->metadata);
            if (leftRank != rightRank)
                return leftRank < rightRank;
            return lhs < rhs;
        });
}

int OrchSynthAudioProcessorEditor::resolveFactoryDisplayIndex(int displayIndex) const
{
    if (juce::isPositiveAndBelow(displayIndex, static_cast<int>(factoryPresetDisplayOrder.size())))
        return factoryPresetDisplayOrder[static_cast<std::size_t>(displayIndex)];
    return displayIndex;
}

int OrchSynthAudioProcessorEditor::resolveFactoryActualIndex(int actualIndex) const
{
    for (int displayIndex = 0; displayIndex < static_cast<int>(factoryPresetDisplayOrder.size()); ++displayIndex)
        if (factoryPresetDisplayOrder[static_cast<std::size_t>(displayIndex)] == actualIndex)
            return displayIndex;
    return actualIndex;
}

// =============================================================================
// Selection UI
// =============================================================================
void OrchSynthAudioProcessorEditor::rebuildModelSelectorForFamily(
    int familyIndex, int preferredInstr)
{
    familyIndex = juce::jlimit(0, mos::kNumFamilies - 1, familyIndex);
    modelSelector.clear(juce::dontSendNotification);

    const int first = mos::kFamilyStart[familyIndex];
    const int count = mos::kFamilySize[familyIndex];

    for (int i = 0; i < count; ++i)
        modelSelector.addItem(utf8Text(mos::getInstrName(first + i)), first + i + 1);

    int target = preferredInstr;
    if (target < first || target >= first + count) target = first;
    modelSelector.setSelectedId(target + 1, juce::dontSendNotification);
}

void OrchSynthAudioProcessorEditor::syncSelectionUiFromInstr()
{
    const int instrIndex  = selectedInstrFromParam();
    const int familyIndex = static_cast<int>(mos::getFamily(instrIndex));

    if (familySelector.getSelectedId() != familyIndex + 1)
        familySelector.setSelectedId(familyIndex + 1, juce::dontSendNotification);

    rebuildModelSelectorForFamily(familyIndex, instrIndex);

    for (int f = 0; f < mos::kNumFamilies; ++f)
    {
        auto& tab = familyTabs[(size_t)f];
        tab.setSelected(f == familyIndex);
        tab.setVisible(true);
    }
}

const mos::InstrumentPreset* OrchSynthAudioProcessorEditor::currentFactoryPresetDefinition() const noexcept
{
    return proc.getFactoryPresetDefinition(proc.getCurrentFactoryPresetIndex());
}

juce::String OrchSynthAudioProcessorEditor::currentPresetMetadataSummary() const
{
    if (proc.isCurrentPresetUser())
    {
        auto file = proc.getCurrentUserPresetFile();
        musique::preset::PresetManifest manifest;
        if (file.existsAsFile()
            && musique::preset::loadManifestFromFile(musique::preset::manifestFileForPresetFile(file), manifest))
        {
            return "User | " + manifest.instrumentName + " | " + manifest.sourceModel;
        }
        return "User preset";
    }

    if (const auto* preset = currentFactoryPresetDefinition())
    {
        juce::StringArray parts;
        parts.add(presetTierLabel(preset->metadata));
        parts.add(juce::String(preset->metadata.familyLabel.c_str()).toUpperCase());
        parts.add(compactRoleLabel(preset->metadata.mixRole));

        const auto outputProfile = compactOutputProfileLabel(preset->metadata.outputProfile);
        if (outputProfile.isNotEmpty())
            parts.add(outputProfile);

        return parts.joinIntoString(" | ");
    }

    return {};
}

void OrchSynthAudioProcessorEditor::updatePresetMetadataSummary()
{
    juce::String key;
    if (proc.isCurrentPresetUser())
        key = "U|" + proc.getCurrentUserPresetFile().getFullPathName();
    else
        key = "F|" + juce::String(selectedInstrFromParam()) + "|" + juce::String(proc.getCurrentFactoryPresetIndex());

    if (key == cachedPresetMetadataKey)
        return;

    cachedPresetMetadataKey = std::move(key);
    cachedPresetMetadataSummary = currentPresetMetadataSummary();
    presetMetaLabel.setText(cachedPresetMetadataSummary, juce::dontSendNotification);
}

void OrchSynthAudioProcessorEditor::refreshPresetFilterChoices()
{
    const int instrIndex = cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam();
    const bool compact = useCompactHeaderCopy(getWidth(), getHeight());
    const juce::String guide = browserGuideForInstrument(instrIndex, compact);

    presetBrowserHintLabel.setText(guide, juce::dontSendNotification);
    presetSearch.setTextToShowWhenEmpty(browserPlaceholderForInstrument(instrIndex, compact),
                                        synthcol::textDim);
}

// =============================================================================
// Tooltip mode
// =============================================================================
void OrchSynthAudioProcessorEditor::cycleTooltipMode()
{
    switch (tooltipMode)
    {
        case TooltipMode::Off:    tooltipMode = TooltipMode::Short;  break;
        case TooltipMode::Short:  tooltipMode = TooltipMode::Novice; break;
        case TooltipMode::Novice: tooltipMode = TooltipMode::Off;    break;
    }

    const char* label = "TIP: OFF";
    if (tooltipMode == TooltipMode::Short)  label = "TIP: SHORT";
    if (tooltipMode == TooltipMode::Novice) label = "TIP: NOVICE";
    tooltipModeBtn.setButtonText(label);

    applyTooltips();
}

void OrchSynthAudioProcessorEditor::applyTooltips()
{
    const bool off = (tooltipMode == TooltipMode::Off);
    const char* const* tips = (tooltipMode == TooltipMode::Novice)
                                  ? kTooltipsNovice : kTooltipsShort;
    const auto& profile = envProfileForInstrument(cachedInstrIdx >= 0 ? cachedInstrIdx : selectedInstrFromParam());
    const auto sharedTooltipMode = tooltipMode == TooltipMode::Short ? synthui::TooltipMode::Short
                                 : tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                                      : synthui::TooltipMode::Off;
    int idx = 0;

    synthui::applyTooltipProfile(profile, envDials, sharedTooltipMode);
    idx += kEnvN;

    // 2 LFO dials
    lfoRateDial .setTooltip(off ? "" : tips[idx++]);
    lfoDepthDial.setTooltip(off ? "" : tips[idx++]);

    // 4 macro dials
    for (int i = 0; i < kMacroTotal; ++i)
        macroDials[(size_t)i].setTooltip(off ? "" : tips[idx++]);

    // 30 FX dials
    for (int i = 0; i < kFxN; ++i)
        fxDials[(size_t)i].setTooltip(off ? "" : tips[idx++]);

    // 1 gain dial
    gainDial.setTooltip(off ? "" : tips[idx]);

    portamentoSlider.setTooltip(off ? "" : tooltipMode == TooltipMode::Novice
        ? "Time spent gliding from the previous note pitch to the next note"
        : "Glide time between repeated notes");
    legatoSlider.setTooltip(off ? "" : tooltipMode == TooltipMode::Novice
        ? "Controls how much the transition keeps the old note's energy and suppresses a fresh attack"
        : "Legato transition amount");
    roundRobinSlider.setTooltip(off ? "" : tooltipMode == TooltipMode::Novice
        ? "Adds deterministic note-to-note variation without changing the preset surface"
        : "Repeated-note variation amount");
    reverbTypeSelector.setTooltip(off ? "" : tooltipMode == TooltipMode::Novice
        ? "Switches the global reverb flavour between a tighter Plate and a more diffuse Hall"
        : "Plate or Hall reverb flavour");
}
