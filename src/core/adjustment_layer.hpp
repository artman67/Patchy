#pragma once

#include "core/ink_space.hpp"
#include "core/layer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace patchy {

inline constexpr const char* kLayerMetadataAdjustmentType = "patchy.adjustment.type";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInput = "patchy.adjustment.levels.black_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsWhiteInput = "patchy.adjustment.levels.white_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGammaPercent = "patchy.adjustment.levels.gamma_percent";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackOutput = "patchy.adjustment.levels.black_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsWhiteOutput = "patchy.adjustment.levels.white_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsChannel = "patchy.adjustment.levels.channel";
inline constexpr const char* kLayerMetadataAdjustmentLevelsRedBlackInput = "patchy.adjustment.levels.red.black_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsRedWhiteInput = "patchy.adjustment.levels.red.white_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsRedGammaPercent =
    "patchy.adjustment.levels.red.gamma_percent";
inline constexpr const char* kLayerMetadataAdjustmentLevelsRedBlackOutput =
    "patchy.adjustment.levels.red.black_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsRedWhiteOutput =
    "patchy.adjustment.levels.red.white_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGreenBlackInput =
    "patchy.adjustment.levels.green.black_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGreenWhiteInput =
    "patchy.adjustment.levels.green.white_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGreenGammaPercent =
    "patchy.adjustment.levels.green.gamma_percent";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGreenBlackOutput =
    "patchy.adjustment.levels.green.black_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsGreenWhiteOutput =
    "patchy.adjustment.levels.green.white_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlueBlackInput =
    "patchy.adjustment.levels.blue.black_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlueWhiteInput =
    "patchy.adjustment.levels.blue.white_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlueGammaPercent =
    "patchy.adjustment.levels.blue.gamma_percent";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlueBlackOutput =
    "patchy.adjustment.levels.blue.black_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlueWhiteOutput =
    "patchy.adjustment.levels.blue.white_output";
inline constexpr const char* kLayerMetadataAdjustmentCurvesShadowOutput = "patchy.adjustment.curves.shadow_output";
inline constexpr const char* kLayerMetadataAdjustmentCurvesMidtoneOutput = "patchy.adjustment.curves.midtone_output";
inline constexpr const char* kLayerMetadataAdjustmentCurvesHighlightOutput =
    "patchy.adjustment.curves.highlight_output";
inline constexpr const char* kLayerMetadataAdjustmentCurvesRgbPoints = "patchy.adjustment.curves.rgb.points";
// Adjustments read from a CMYK document (see InkSpace): the space's id, and the black
// ink's Levels record and curve, which have no RGB counterpart.
inline constexpr const char* kLayerMetadataAdjustmentInkSpace = "patchy.adjustment.ink_space";
inline constexpr const char* kLayerMetadataAdjustmentCurvesBlackPoints = "patchy.adjustment.curves.black.points";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInkBlackInput =
    "patchy.adjustment.levels.black_ink.black_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInkWhiteInput =
    "patchy.adjustment.levels.black_ink.white_input";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInkGammaPercent =
    "patchy.adjustment.levels.black_ink.gamma_percent";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInkBlackOutput =
    "patchy.adjustment.levels.black_ink.black_output";
inline constexpr const char* kLayerMetadataAdjustmentLevelsBlackInkWhiteOutput =
    "patchy.adjustment.levels.black_ink.white_output";
inline constexpr const char* kLayerMetadataAdjustmentCurvesRedPoints = "patchy.adjustment.curves.red.points";
inline constexpr const char* kLayerMetadataAdjustmentCurvesGreenPoints = "patchy.adjustment.curves.green.points";
inline constexpr const char* kLayerMetadataAdjustmentCurvesBluePoints = "patchy.adjustment.curves.blue.points";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationHueShift =
    "patchy.adjustment.hue_saturation.hue_shift";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationSaturationDelta =
    "patchy.adjustment.hue_saturation.saturation_delta";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationLightnessDelta =
    "patchy.adjustment.hue_saturation.lightness_delta";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationColorize =
    "patchy.adjustment.hue_saturation.colorize";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationColorizeHue =
    "patchy.adjustment.hue_saturation.colorize_hue";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationColorizeSaturation =
    "patchy.adjustment.hue_saturation.colorize_saturation";
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationColorizeLightness =
    "patchy.adjustment.hue_saturation.colorize_lightness";
// Per-hue-range bands. One key per band index 0..5 (reds, yellows, greens,
// cyans, blues, magentas) holding "r0;r1;r2;r3;hue;saturation;lightness".
inline constexpr const char* kLayerMetadataAdjustmentHueSaturationBandPrefix =
    "patchy.adjustment.hue_saturation.band.";
inline constexpr const char* kLayerMetadataAdjustmentColorBalanceCyanRed =
    "patchy.adjustment.color_balance.cyan_red";
inline constexpr const char* kLayerMetadataAdjustmentColorBalanceMagentaGreen =
    "patchy.adjustment.color_balance.magenta_green";
inline constexpr const char* kLayerMetadataAdjustmentColorBalanceYellowBlue =
    "patchy.adjustment.color_balance.yellow_blue";
inline constexpr const char* kLayerMetadataAdjustmentPosterizeLevels = "patchy.adjustment.posterize.levels";
inline constexpr const char* kLayerMetadataAdjustmentThresholdLevel = "patchy.adjustment.threshold.level";
inline constexpr const char* kLayerMetadataAdjustmentExposureValue = "patchy.adjustment.exposure.value";
inline constexpr const char* kLayerMetadataAdjustmentExposureOffset = "patchy.adjustment.exposure.offset";
inline constexpr const char* kLayerMetadataAdjustmentExposureGamma = "patchy.adjustment.exposure.gamma";
inline constexpr const char* kLayerMetadataAdjustmentVibranceVibrance = "patchy.adjustment.vibrance.vibrance";
inline constexpr const char* kLayerMetadataAdjustmentVibranceSaturation = "patchy.adjustment.vibrance.saturation";
// Selective Color: "relative" or "absolute", plus one key per color index 0..8
// (reds, yellows, greens, cyans, blues, magentas, whites, neutrals, blacks)
// holding "cyan;magenta;yellow;black".
inline constexpr const char* kLayerMetadataAdjustmentSelectiveColorMethod =
    "patchy.adjustment.selective_color.method";
inline constexpr const char* kLayerMetadataAdjustmentSelectiveColorPrefix =
    "patchy.adjustment.selective_color.color.";
// Black & White: the six color weights as "reds;yellows;greens;cyans;blues;magentas"
// (percent), the tint switch (0/1), and the tint hue (degrees) and saturation (percent).
inline constexpr const char* kLayerMetadataAdjustmentBlackWhiteWeights = "patchy.adjustment.black_white.weights";
inline constexpr const char* kLayerMetadataAdjustmentBlackWhiteTint = "patchy.adjustment.black_white.tint";
inline constexpr const char* kLayerMetadataAdjustmentBlackWhiteTintHue = "patchy.adjustment.black_white.tint_hue";
inline constexpr const char* kLayerMetadataAdjustmentBlackWhiteTintSaturation =
    "patchy.adjustment.black_white.tint_saturation";
// Channel Mixer: monochrome 0/1, plus one key per mix (red, green, blue outputs and
// the monochrome gray) holding "red;green;blue;constant" percentages.
inline constexpr const char* kLayerMetadataAdjustmentChannelMixerMonochrome =
    "patchy.adjustment.channel_mixer.monochrome";
inline constexpr const char* kLayerMetadataAdjustmentChannelMixerRed = "patchy.adjustment.channel_mixer.red";
inline constexpr const char* kLayerMetadataAdjustmentChannelMixerGreen = "patchy.adjustment.channel_mixer.green";
inline constexpr const char* kLayerMetadataAdjustmentChannelMixerBlue = "patchy.adjustment.channel_mixer.blue";
inline constexpr const char* kLayerMetadataAdjustmentChannelMixerGray = "patchy.adjustment.channel_mixer.gray";
// The filter color as 0xRRGGBB.
inline constexpr const char* kLayerMetadataAdjustmentPhotoFilterColor = "patchy.adjustment.photo_filter.color";
inline constexpr const char* kLayerMetadataAdjustmentPhotoFilterDensity = "patchy.adjustment.photo_filter.density";
inline constexpr const char* kLayerMetadataAdjustmentPhotoFilterPreserveLuminosity =
    "patchy.adjustment.photo_filter.preserve_luminosity";
inline constexpr const char* kLayerMetadataAdjustmentBrightnessContrastBrightness =
    "patchy.adjustment.brightness_contrast.brightness";
inline constexpr const char* kLayerMetadataAdjustmentBrightnessContrastContrast =
    "patchy.adjustment.brightness_contrast.contrast";
// Absent on pre-July-2026 documents, which were always legacy-mode; the reader
// therefore defaults to 1 (legacy) so old files keep their render.
inline constexpr const char* kLayerMetadataAdjustmentBrightnessContrastUseLegacy =
    "patchy.adjustment.brightness_contrast.use_legacy";
// Gradient Map. The gradient splits across keys so the free-text name needs no
// escaping; stop locations, midpoints and opacities are stored as the hex bits
// of their float values, so the metadata round trip is exact.
inline constexpr const char* kLayerMetadataAdjustmentGradientMapName = "patchy.adjustment.gradient_map.name";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapForm = "patchy.adjustment.gradient_map.form";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapSmoothness =
    "patchy.adjustment.gradient_map.smoothness";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapColorStops =
    "patchy.adjustment.gradient_map.color_stops";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapAlphaStops =
    "patchy.adjustment.gradient_map.alpha_stops";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapNoise = "patchy.adjustment.gradient_map.noise";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapReverse = "patchy.adjustment.gradient_map.reverse";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapDither = "patchy.adjustment.gradient_map.dither";
inline constexpr const char* kLayerMetadataAdjustmentGradientMapMethod = "patchy.adjustment.gradient_map.method";

enum class AdjustmentKind {
  Levels,
  Curves,
  HueSaturation,
  ColorBalance,
  Invert,
  Posterize,
  Threshold,
  BrightnessContrast,
  Exposure,
  GradientMap,
  Vibrance,
  SelectiveColor,
  BlackWhite,
  ChannelMixer,
  PhotoFilter
};

enum class LevelsChannel {
  Rgb,
  Red,
  Green,
  Blue
};

struct LevelsRecord {
  int black_input{0};
  int white_input{255};
  int gamma_percent{100};
  int black_output{0};
  int white_output{255};

  friend bool operator==(const LevelsRecord&, const LevelsRecord&) = default;
};

struct LevelsAdjustment {
  int black_input{0};
  int white_input{255};
  int gamma_percent{100};
  int black_output{0};
  int white_output{255};
  LevelsChannel channel{LevelsChannel::Rgb};
  LevelsRecord red{};
  LevelsRecord green{};
  LevelsRecord blue{};
  // The fifth record of a CMYK document's Levels (the black ink). Only an adjustment
  // with an ink space reads it; there red, green and blue hold cyan, magenta and yellow.
  LevelsRecord black_ink{};

  friend bool operator==(const LevelsAdjustment&, const LevelsAdjustment&) = default;
};

enum class CurvesChannel {
  Rgb,
  Red,
  Green,
  Blue
};

struct CurveControlPoint {
  int input{0};
  int output{0};

  friend bool operator==(const CurveControlPoint&, const CurveControlPoint&) = default;
};

using CurveControlPoints = std::vector<CurveControlPoint>;

struct CurvesAdjustment {
  CurveControlPoints rgb{{0, 0}, {255, 255}};
  CurveControlPoints red{{0, 0}, {255, 255}};
  CurveControlPoints green{{0, 0}, {255, 255}};
  CurveControlPoints blue{{0, 0}, {255, 255}};
  // The fifth curve of a CMYK document's Curves (the black ink); see LevelsAdjustment.
  CurveControlPoints black_ink{{0, 0}, {255, 255}};

  friend bool operator==(const CurvesAdjustment&, const CurvesAdjustment&) = default;
};

struct CurvesEyedropperSamples {
  std::optional<RgbColor> black;
  std::optional<RgbColor> gray;
  std::optional<RgbColor> white;
};

// One of Photoshop's six per-hue-range bands. The four stops are hue degrees in
// wheel order and may wrap past 360 (the reds default is 315/345/15/45); the
// effect ramps in between the outer and inner start, holds between the inner
// stops, and ramps out between the inner and outer end.
struct HueSaturationBand {
  int outer_start{0};
  int inner_start{0};
  int inner_end{0};
  int outer_end{0};
  int hue_shift{0};          // -180..180
  int saturation_delta{0};   // -100..100
  int lightness_delta{0};    // -100..100

  [[nodiscard]] bool has_effect() const {
    return hue_shift != 0 || saturation_delta != 0 || lightness_delta != 0;
  }
  friend bool operator==(const HueSaturationBand&, const HueSaturationBand&) = default;
};

// Photoshop's fresh-layer hextants, the same ranges `kPhotoshopHueSaturationDefaultTail` writes.
inline constexpr std::array<std::array<int, 4>, 6> kHueSaturationDefaultBandRanges{
    {{315, 345, 15, 45},
     {15, 45, 75, 105},
     {75, 105, 135, 165},
     {135, 165, 195, 225},
     {195, 225, 255, 285},
     {255, 285, 315, 345}}};

[[nodiscard]] std::array<HueSaturationBand, 6> default_hue_saturation_bands();

struct HueSaturationAdjustment {
  int hue_shift{0};
  int saturation_delta{0};
  int lightness_delta{0};
  // Photoshop's Colorize mode: recolors from per-pixel lightness using the
  // colorize triple below; the master sliders above are ignored while active.
  bool colorize{false};
  int colorize_hue{0};          // 0..360 (UI convention; PSD stores -180..180)
  int colorize_saturation{25};  // 0..100 (Photoshop's colorize default is 25)
  int colorize_lightness{0};    // -100..100
  // Per-hue-range bands, in Photoshop's order: reds, yellows, greens, cyans,
  // blues, magentas. Ignored while colorize is on.
  std::array<HueSaturationBand, 6> bands{default_hue_saturation_bands()};

  [[nodiscard]] bool any_band_has_effect() const {
    return std::any_of(bands.begin(), bands.end(), [](const auto& band) { return band.has_effect(); });
  }
};

struct ColorBalanceAdjustment {
  int cyan_red{0};
  int magenta_green{0};
  int yellow_blue{0};
};

// Levels 2..255: wider than the destructive dialog's 2..16 so Photoshop
// 'post' files with any legal value round-trip.
struct PosterizeAdjustment {
  int levels{4};
};

// Level 1..255 (Photoshop's 'thrs' range; the destructive filter keeps its
// historical 0..255 clamp).
struct ThresholdAdjustment {
  int level{128};
};

// Photoshop's Exposure adjustment ('expA'), stored at the precision of Photoshop's own
// fields: exposure in hundredths of a stop (-20.00..20.00), offset in ten-thousandths
// (-0.5000..0.5000), gamma correction in hundredths (0.01..9.99).
inline constexpr int kExposureValueRange = 2000;
inline constexpr int kExposureOffsetRange = 5000;
inline constexpr int kExposureGammaMin = 1;
inline constexpr int kExposureGammaMax = 999;
struct ExposureAdjustment {
  int exposure_hundredths{0};
  int offset_ten_thousandths{0};
  int gamma_hundredths{100};
};
[[nodiscard]] ExposureAdjustment clamp_exposure(ExposureAdjustment settings);
// One channel through Photoshop's Exposure: linearize with gamma 2.2, scale by
// 2^exposure, add the offset, apply 1/gamma, encode again. Within 1/255 of Photoshop's
// render of psd-tools' exposure_rgb.psd on all four of its setting triples.
[[nodiscard]] std::uint8_t exposure_channel_value(std::uint8_t value, ExposureAdjustment settings);

// Photoshop's Vibrance adjustment ('vibA'): both sliders -100..100, default 0.
// Saturation scales every color's saturation by the same factor (-100 is gray,
// +100 doubles it); Vibrance raises or lowers the less-saturated colors more and
// leaves fully saturated colors alone.
inline constexpr int kVibranceRange = 100;
struct VibranceAdjustment {
  int vibrance{0};
  int saturation{0};
};
[[nodiscard]] VibranceAdjustment clamp_vibrance(VibranceAdjustment settings);
// One pixel through Vibrance. Works on HSL saturation at fixed HSL lightness, so hue
// and lightness are kept and no channel ever clips. Vibrance maps saturation s to
// 1 - (1 - s)^(2^(vibrance/100)); the Saturation factor follows, capped at full
// saturation. Patent constraint (docs/legal-constraints.md, Vibrance): the
// amount depends on the pixel's own saturation through this one formula for every
// color; never add a hue- or skin-tone-dependent term or treat a color region
// differently before re-reading that note.
[[nodiscard]] RgbColor apply_vibrance(RgbColor color, VibranceAdjustment settings);

// Photoshop's Selective Color ('selc'): cyan, magenta, yellow and black corrections
// (-100..100 percent) for each of nine color families, in Photoshop's Colors-menu order.
enum class SelectiveColorRange {
  Reds,
  Yellows,
  Greens,
  Cyans,
  Blues,
  Magentas,
  Whites,
  Neutrals,
  Blacks
};
inline constexpr std::size_t kSelectiveColorRangeCount = 9;
struct SelectiveColorCorrection {
  int cyan{0};
  int magenta{0};
  int yellow{0};
  int black{0};

  [[nodiscard]] bool has_effect() const { return cyan != 0 || magenta != 0 || yellow != 0 || black != 0; }
  friend bool operator==(const SelectiveColorCorrection&, const SelectiveColorCorrection&) = default;
};
// Relative scales the ink a channel already carries (pure white cannot change);
// Absolute adds the percentage outright. Photoshop's default is Relative.
struct SelectiveColorAdjustment {
  std::array<SelectiveColorCorrection, kSelectiveColorRangeCount> corrections{};
  bool absolute{false};

  [[nodiscard]] bool has_effect() const {
    return std::any_of(corrections.begin(), corrections.end(), [](const auto& entry) { return entry.has_effect(); });
  }
  friend bool operator==(const SelectiveColorAdjustment&, const SelectiveColorAdjustment&) = default;
};
[[nodiscard]] SelectiveColorAdjustment clamp_selective_color(SelectiveColorAdjustment settings);
// How strongly `color` belongs to each family, as fractions that always sum to 1:
// the six hue families share the chroma (max - mid to the dominant primary, mid -
// min to the secondary opposite the weakest channel), and Whites, Neutrals and
// Blacks share the rest by where max and min sit around the midpoint. NOT
// calibrated against Photoshop; see docs/adjustments-calibration.md.
[[nodiscard]] std::array<double, kSelectiveColorRangeCount> selective_color_weights(RgbColor color);
[[nodiscard]] RgbColor apply_selective_color(RgbColor color, const SelectiveColorAdjustment& settings);

// Photoshop's Black & White ('blwh'): every pixel becomes one gray value mixed from six
// color weights (percent, -200..300), optionally tinted. The defaults are Photoshop's.
// NOT calibrated against Photoshop; see docs/adjustments-calibration.md.
enum class BlackWhiteColor {
  Reds,
  Yellows,
  Greens,
  Cyans,
  Blues,
  Magentas
};
inline constexpr std::size_t kBlackWhiteColorCount = 6;
inline constexpr int kBlackWhiteWeightMin = -200;
inline constexpr int kBlackWhiteWeightMax = 300;
inline constexpr std::array<int, kBlackWhiteColorCount> kBlackWhiteDefaultWeights{40, 60, 40, 60, 20, 80};
struct BlackWhiteAdjustment {
  std::array<int, kBlackWhiteColorCount> weights{kBlackWhiteDefaultWeights};
  bool tint{false};
  int tint_hue{35};         // degrees, 0..360
  int tint_saturation{25};  // percent, 0..100

  friend bool operator==(const BlackWhiteAdjustment&, const BlackWhiteAdjustment&) = default;
};
[[nodiscard]] BlackWhiteAdjustment clamp_black_white(BlackWhiteAdjustment settings);
// The gray value: `min + (mid - min) * secondary% + (max - mid) * primary%`, where the
// primary color is the largest channel's (Reds, Greens, Blues) and the secondary the
// pair of the two largest (Yellows, Cyans, Magentas). Grays keep their value.
[[nodiscard]] std::uint8_t black_white_gray(RgbColor color, const std::array<int, kBlackWhiteColorCount>& weights);
// The tint as a color: the fully bright color of `hue` whose channel spread is
// `saturation` percent of the full range (HSB with brightness 100%).
[[nodiscard]] RgbColor black_white_tint_color(int hue, int saturation);
// The tint read back from a color: its hexcone hue and its spread in percent, the two
// things the Color-mode tint depends on. A gray color keeps `hue` and reads saturation 0.
void black_white_tint_from_color(double red, double green, double blue, int& hue, int& saturation);
// The gray value, then (with Tint on) the tint color laid over it in Color blend mode,
// which keeps the gray's luminosity.
[[nodiscard]] RgbColor apply_black_white(RgbColor color, const BlackWhiteAdjustment& settings);

// Photoshop's Channel Mixer ('mixr'): each output channel is a blend of the source
// channels plus a constant, all in percent (-200..200). Monochrome sends one Gray mix
// to every channel.
inline constexpr int kChannelMixerRange = 200;
struct ChannelMixerChannel {
  int red{0};
  int green{0};
  int blue{0};
  int constant{0};

  // The dialog's Total readout: the source percentages, without the constant.
  [[nodiscard]] int total() const { return red + green + blue; }
  friend bool operator==(const ChannelMixerChannel&, const ChannelMixerChannel&) = default;
};
struct ChannelMixerAdjustment {
  // Red, green and blue outputs; the defaults are the identity.
  std::array<ChannelMixerChannel, 3> outputs{
      {ChannelMixerChannel{100, 0, 0, 0}, ChannelMixerChannel{0, 100, 0, 0}, ChannelMixerChannel{0, 0, 100, 0}}};
  // The Gray mix Photoshop shows when Monochrome is first ticked.
  ChannelMixerChannel gray{40, 40, 20, 0};
  bool monochrome{false};

  friend bool operator==(const ChannelMixerAdjustment&, const ChannelMixerAdjustment&) = default;
};
// Clamps every value to -200..200. A monochrome mixer also copies the Gray mix into
// the three outputs: Photoshop leaves each output on the gray mix when Monochrome is
// cleared, so that is the one canonical form.
[[nodiscard]] ChannelMixerAdjustment clamp_channel_mixer(ChannelMixerAdjustment settings);
[[nodiscard]] bool channel_mixer_has_effect(const ChannelMixerAdjustment& settings);
// out = (red% * R + green% * G + blue% * B) + constant% * 255 per output, rounded and
// clamped, on the encoded 8-bit values. NOT calibrated against Photoshop; see
// docs/adjustments-calibration.md.
[[nodiscard]] RgbColor apply_channel_mixer(RgbColor color, const ChannelMixerAdjustment& settings);

// Photoshop's Photo Filter adjustment ('phfl'): a filter color, Density 1..100 percent
// (default 25) and Preserve Luminosity (default on). Photoshop opens on Warming
// Filter (85). The PSD block stores only the color, never which preset chose it.
inline constexpr int kPhotoFilterDensityMin = 1;
inline constexpr int kPhotoFilterDensityMax = 100;
inline constexpr RgbColor kPhotoFilterDefaultColor{236, 138, 0};
struct PhotoFilterAdjustment {
  RgbColor color{kPhotoFilterDefaultColor};
  int density{25};
  bool preserve_luminosity{true};
};
[[nodiscard]] PhotoFilterAdjustment clamp_photo_filter(PhotoFilterAdjustment settings);
// Photoshop's twenty filter presets in its menu order (Warming Filter (85) first,
// Underwater last). The UI owns the translated names; see
// docs/adjustments-calibration.md for which colors are verified.
inline constexpr std::size_t kPhotoFilterPresetCount = 20;
[[nodiscard]] const std::array<RgbColor, kPhotoFilterPresetCount>& photo_filter_preset_colors() noexcept;
// The preset whose color is exactly `color`, or nullopt for a custom color.
[[nodiscard]] std::optional<std::size_t> photo_filter_preset_index(RgbColor color) noexcept;
// One pixel through Photo Filter: each channel moves toward its product with the filter
// color (a colored gel passes that fraction of each primary) by Density; Preserve
// Luminosity then restores the source's luminosity the way the Luminosity blend mode
// does. A white filter changes nothing.
[[nodiscard]] RgbColor apply_photo_filter(RgbColor color, PhotoFilterAdjustment settings);

// Both Photoshop algorithms are modeled. Modern mode (Photoshop's default,
// use_legacy false) takes brightness -150..150 and contrast -50..100; legacy
// mode takes -100..100 for both. Old Patchy documents and 'brit'-only PSDs
// load as legacy so their render is unchanged.
inline constexpr int kBrightnessContrastLegacyRange = 100;
inline constexpr int kModernBrightnessRange = 150;
inline constexpr int kModernContrastMin = -50;
inline constexpr int kModernContrastMax = 100;
struct BrightnessContrastAdjustment {
  int brightness{0};
  int contrast{0};
  bool use_legacy{false};
};
// Clamps both sliders into the active mode's Photoshop range.
[[nodiscard]] BrightnessContrastAdjustment clamp_brightness_contrast(BrightnessContrastAdjustment settings);

// Photoshop's default Gradient Map runs from the foreground color to the
// background color; the defaults are Photoshop's default black and white.
[[nodiscard]] GradientDefinition default_gradient_map_definition(RgbColor foreground = RgbColor{0, 0, 0},
                                                                 RgbColor background = RgbColor{255, 255, 255});

// Photoshop's Gradient Map: each pixel's luminance picks a color from the
// gradient (shadows from the left end, highlights from the right). Gradient
// opacity stops are kept for the file round trip but do not render (see
// build_gradient_map_lut). Photoshop 2022 and later default new Gradient Maps
// to the Perceptual method.
struct GradientMapAdjustment {
  GradientDefinition gradient{default_gradient_map_definition()};
  bool reverse{false};
  bool dither{false};
  GradientInterpolationMethod method{GradientInterpolationMethod::Perceptual};
  // Derived 256-entry color table indexed by luminance, undithered and
  // already reversed. adjustment_settings_from_layer fills it; code that
  // changes the fields above must call prepare_gradient_map_lut again or
  // reset it. When empty, lookups evaluate the gradient directly.
  std::shared_ptr<const std::array<RgbColor, 256>> lut{};

  friend bool operator==(const GradientMapAdjustment& lhs, const GradientMapAdjustment& rhs) {
    return lhs.gradient == rhs.gradient && lhs.reverse == rhs.reverse && lhs.dither == rhs.dither &&
           lhs.method == rhs.method;
  }
};

struct AdjustmentSettings {
  AdjustmentKind kind{AdjustmentKind::Levels};
  LevelsAdjustment levels{};
  CurvesAdjustment curves{};
  HueSaturationAdjustment hue_saturation{};
  ColorBalanceAdjustment color_balance{};
  PosterizeAdjustment posterize{};
  ThresholdAdjustment threshold{};
  BrightnessContrastAdjustment brightness_contrast{};
  ExposureAdjustment exposure{};
  GradientMapAdjustment gradient_map{};
  VibranceAdjustment vibrance{};
  SelectiveColorAdjustment selective_color{};
  BlackWhiteAdjustment black_white{};
  ChannelMixerAdjustment channel_mixer{};
  PhotoFilterAdjustment photo_filter{};
  // Set for an adjustment layer that came from a CMYK document whose profile could be
  // read: the channel-wise kinds (Levels, Curves, Invert, Posterize, Brightness/Contrast,
  // Exposure) then run on the four inks instead of on RGB. See core/ink_space.hpp.
  std::shared_ptr<const InkSpace> ink_space;
};
// True when `settings` runs in its ink space (it has one and its kind is channel-wise).
[[nodiscard]] bool adjustment_runs_in_ink_space(const AdjustmentSettings& settings) noexcept;

// Levels record math shared by the UI dialogs and the PSD lvls codec: the
// single source of truth for the clamp ranges (black_input 0..254,
// white_input black_input+1..255, gamma_percent 10..999, black_output 0..255,
// white_output black_output..255). NOTE: the per-channel transfer FORMULA is
// deliberately not shared: core's levels_channel rounds through a float
// (clamp_byte) while the UI preview's map_levels_value lrounds the double,
// and the two differ by 1/255 on real inputs (e.g. value 4, record
// {0,45,121%,0,255} -> 35 vs 34).
[[nodiscard]] LevelsRecord clamp_levels_record(LevelsRecord record);
[[nodiscard]] LevelsRecord levels_master_record(LevelsAdjustment settings);
void set_levels_master_record(LevelsAdjustment& settings, LevelsRecord record);
[[nodiscard]] LevelsRecord levels_record_for_channel(const LevelsAdjustment& settings, LevelsChannel channel);
void set_levels_record_for_channel(LevelsAdjustment& settings, LevelsChannel channel, LevelsRecord record);

// Curves always carry two to nineteen points per channel, sorted by input.
// Duplicate inputs are resolved in favor of the last supplied point. Inputs
// outside the first/last control point clamp to that endpoint's output.
[[nodiscard]] CurveControlPoints normalized_curve_control_points(CurveControlPoints points);
[[nodiscard]] const CurveControlPoints& curve_points_for_channel(const CurvesAdjustment& curves,
                                                                 CurvesChannel channel) noexcept;
void set_curve_points_for_channel(CurvesAdjustment& curves, CurvesChannel channel, CurveControlPoints points);
[[nodiscard]] CurvesAdjustment curves_adjustment_from_legacy_outputs(int shadow_output, int midtone_output,
                                                                     int highlight_output);
// Rebuilds the component curves from sampled black/neutral/white points. Like
// Photoshop's Curves eyedroppers, this replaces prior hand-drawn geometry
// instead of layering an ambiguous point onto it.
[[nodiscard]] CurvesAdjustment curves_adjustment_from_eyedropper_samples(
    const CurvesEyedropperSamples& samples);

// The single shared formulas between the destructive catalog filters
// (filter_engine.cpp delegates here) and the adjustment layers: rounding is
// lround-based to match the historical filter_clamp_byte semantics, and
// threshold operates on the (30r + 59g + 11b) / 100 integer luminance.
[[nodiscard]] std::uint8_t posterize_channel_value(std::uint8_t value, int levels);
[[nodiscard]] int threshold_luminance(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
// Photoshop Brightness/Contrast, both algorithms calibrated against PS 2026
// ramp captures (July 2026). DELIBERATELY different from the destructive
// patchy.filters.brightness_contrast formula (which is byte-pinned and keeps
// its historical linear (100+c)/100 slope for positive contrast): the
// adjustment layer round-trips as a native Photoshop record, so it must
// render Photoshop's math, like the Levels dual-formula precedent. The
// modern (use_legacy false) model is the full closed form recovered from
// 300 16-bit ramp captures; see docs/ps-compat.md "Modern Brightness/Contrast".
[[nodiscard]] std::uint8_t brightness_contrast_channel_value(std::uint8_t value, int brightness, int contrast,
                                                             bool use_legacy);

// The luminance a Gradient Map looks up, and the table it looks it up in.
[[nodiscard]] std::uint8_t gradient_map_luminance(RgbColor color) noexcept;
[[nodiscard]] std::array<RgbColor, 256> build_gradient_map_lut(const GradientMapAdjustment& settings);
void prepare_gradient_map_lut(GradientMapAdjustment& settings);

[[nodiscard]] bool layer_is_adjustment(const Layer& layer);
[[nodiscard]] std::string adjustment_kind_key(AdjustmentKind kind);
[[nodiscard]] std::string adjustment_display_name(AdjustmentKind kind);
[[nodiscard]] std::optional<AdjustmentKind> adjustment_kind_from_key(std::string_view key);
[[nodiscard]] std::optional<AdjustmentSettings> adjustment_settings_from_layer(const Layer& layer);
void configure_adjustment_layer(Layer& layer, const AdjustmentSettings& settings);
// Position-free form: Gradient Map dither is skipped. Compositing paths use
// the positioned form, whose (x, y) is the document coordinate that seeds the
// Gradient Map dither, so a dirty-rect repaint matches a full render.
[[nodiscard]] RgbColor apply_adjustment_to_color(RgbColor color, const AdjustmentSettings& settings);
[[nodiscard]] RgbColor apply_adjustment_to_color(RgbColor color, const AdjustmentSettings& settings,
                                                 std::int32_t x, std::int32_t y);
void apply_adjustment_to_pixels(PixelBuffer& pixels, const AdjustmentSettings& settings);
[[nodiscard]] bool adjustment_has_effect(const AdjustmentSettings& settings);

// Exact per-channel 256-entry lookup for channel-separable adjustments
// (Levels, Curves, Color Balance): lut.red[v] equals the per-pixel math's red
// output for any pixel whose red input is v, so the LUT path is bit-identical
// at a fraction of the cost. nullopt for Hue/Saturation, whose channels mix
// through HSL.
struct AdjustmentLut {
  std::array<std::uint8_t, 256> red{};
  std::array<std::uint8_t, 256> green{};
  std::array<std::uint8_t, 256> blue{};
};
[[nodiscard]] std::array<std::uint8_t, 256> build_curve_lut(const CurveControlPoints& points);
[[nodiscard]] AdjustmentLut build_curves_lut(const CurvesAdjustment& curves);
[[nodiscard]] std::optional<AdjustmentLut> build_adjustment_lut(const AdjustmentSettings& settings);

}  // namespace patchy
