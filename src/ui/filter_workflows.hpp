#pragma once

#include "core/adjustment_layer.hpp"
#include "core/layer.hpp"
#include "core/match_color.hpp"
#include "filters/filter_registry.hpp"
#include "filters/shadows_highlights.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/curves_editor.hpp"

#include <QColor>
#include <QRegion>
#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QWidget;

namespace patchy::ui {

class GradientLibrary;

struct HueSaturationSettings {
  int hue_shift{0};
  int saturation_delta{0};
  int lightness_delta{0};
  bool colorize{false};
  int colorize_hue{0};          // 0..360
  int colorize_saturation{25};  // 0..100
  int colorize_lightness{0};    // -100..100
  // Photoshop's per-hue-range bands (reds, yellows, greens, cyans, blues,
  // magentas), plus which one the dialog's three sliders are editing: 0 is
  // Master, 1..6 select bands[0..5].
  std::array<HueSaturationBand, 6> bands{default_hue_saturation_bands()};
  int edit_range{0};
};

[[nodiscard]] HueSaturationAdjustment to_hue_saturation_adjustment(const HueSaturationSettings& settings);
[[nodiscard]] HueSaturationSettings to_hue_saturation_settings(const HueSaturationAdjustment& adjustment);

// The dialog settings are the core model types; the record math
// (clamp_levels_record, levels_master_record, ...) lives in
// core/adjustment_layer.hpp. main_window.hpp re-declares this alias.
using LevelsSettings = LevelsAdjustment;

using CurvesSettings = CurvesAdjustment;

enum class CurvesCanvasMode {
  None,
  Targeted,
  BlackPoint,
  GrayPoint,
  WhitePoint
};

struct CurvesCanvasSample {
  QColor input_color{};
  CanvasReadGesture gesture{};
};

struct CurvesDialogHooks {
  std::function<void(CurvesCanvasMode mode,
                     std::function<void(const CurvesCanvasSample& sample)> sample_changed)>
      set_canvas_mode;
  std::function<void()> clear_canvas_mode;
  std::function<void(std::optional<CurvesClippingMode> mode, CurvesChannel channel)> clipping_changed;
};

struct ColorBalanceSettings {
  int cyan_red{0};
  int magenta_green{0};
  int yellow_blue{0};
};

using PosterizeSettings = PosterizeAdjustment;
using ThresholdSettings = ThresholdAdjustment;
using ExposureSettings = ExposureAdjustment;
using VibranceSettings = VibranceAdjustment;
using SelectiveColorSettings = SelectiveColorAdjustment;
using BlackWhiteSettings = BlackWhiteAdjustment;
using ChannelMixerSettings = ChannelMixerAdjustment;
using PhotoFilterSettings = PhotoFilterAdjustment;
using ShadowsHighlightsSettings = ::patchy::ShadowsHighlightsSettings;
using ColorLookupSettings = ColorLookupAdjustment;
using BrightnessContrastSettings = BrightnessContrastAdjustment;
using GradientMapSettings = GradientMapAdjustment;

struct FilterControlSpec {
  // The first six fields retain the legacy aggregate shape used by existing UI
  // tests and helpers. Catalog-generated specs also fill the typed fields below.
  QString label;
  QString object_name;
  int minimum{0};
  int maximum{100};
  int value{100};
  QString suffix;
  std::string parameter_key{};
  FilterParameterKind kind{FilterParameterKind::Integer};
  FilterParameterValue default_value{std::int64_t{100}};
  std::optional<double> typed_minimum{};
  std::optional<double> typed_maximum{};
  std::optional<double> step{};
  std::vector<FilterParameterOption> options{};
  FilterParameterPresentation presentation{
      FilterParameterPresentation::Standard};
};

struct FilterDialogSpec {
  QString identifier;
  QString display_name;
  std::vector<FilterControlSpec> controls;
  std::uint32_t schema_version{1};
};

struct SmartFilterBlendingSettings {
  BlendMode blend_mode{BlendMode::Normal};
  double opacity{1.0};
};

struct FilterPreviewSettings {
  bool preview_enabled{true};
  FilterInvocation invocation;
  // Filled only by the smart-filter settings dialog (its Blending section).
  std::optional<SmartFilterBlendingSettings> blending{};
};

// Optional in-dialog preview source for request_filter_settings. When
// provided, the dialog shows a bounded zoomable proxy preview with the
// gallery's draggable spatial overlays; the pixels are consumed once at
// dialog open to build the proxy and must stay alive until the call returns.
struct FilterDialogPreviewSource {
  const PixelBuffer* pixels{nullptr};
  Rect bounds{};
  QRegion selection;
  const FilterRegistry* registry{nullptr};
};

using FilterProgress = ::patchy::FilterProgress;
using FilterCancelled = ::patchy::FilterCancelled;

[[nodiscard]] QString filter_action_object_name(const QString& identifier);
[[nodiscard]] QString filter_display_name(const FilterDefinition& filter);
[[nodiscard]] QString filter_category_display_name(FilterCategory category);
[[nodiscard]] QString filter_progress_stage_text(FilterProgressStage stage);
[[nodiscard]] bool is_adjustment_only_filter(const FilterDefinition& filter);
[[nodiscard]] FilterDialogSpec filter_dialog_spec_for(const FilterDefinition& filter);
// When `blending` is non-null the dialog appends a Blending section (blend
// mode + opacity, the smart-filter per-entry settings): *blending seeds the
// controls, preview requests carry the live values in
// FilterPreviewSettings::blending, and an accepted dialog writes the chosen
// values back through the pointer.
[[nodiscard]] std::optional<FilterInvocation> request_filter_settings(
    QWidget* parent, const FilterDialogSpec& spec, const std::function<void(FilterPreviewSettings)>& preview_changed,
    FilterInvocation initial = {},
    const FilterDialogPreviewSource* preview_source = nullptr,
    SmartFilterBlendingSettings* blending = nullptr);
[[nodiscard]] std::optional<LevelsSettings> request_levels_settings(
    QWidget* parent, std::function<void(bool, const LevelsSettings&)> preview_changed = {},
    LevelsSettings initial = {}, const PixelBuffer* histogram_source = nullptr);
[[nodiscard]] std::optional<CurvesSettings> request_curves_settings(
    QWidget* parent, std::function<void(bool, const CurvesSettings&)> preview_changed = {},
    CurvesSettings initial = {}, CurvesHistograms histograms = {}, CurvesDialogHooks hooks = {});
[[nodiscard]] std::optional<HueSaturationSettings> request_hue_saturation_settings(
    QWidget* parent, std::function<void(bool, const HueSaturationSettings&)> preview_changed = {},
    HueSaturationSettings initial = {});
[[nodiscard]] std::optional<ColorBalanceSettings> request_color_balance_settings(
    QWidget* parent, std::function<void(bool, const ColorBalanceSettings&)> preview_changed = {},
    ColorBalanceSettings initial = {});
[[nodiscard]] std::optional<PosterizeSettings> request_posterize_settings(
    QWidget* parent, std::function<void(bool, const PosterizeSettings&)> preview_changed = {},
    PosterizeSettings initial = {});
[[nodiscard]] std::optional<ThresholdSettings> request_threshold_settings(
    QWidget* parent, std::function<void(bool, const ThresholdSettings&)> preview_changed = {},
    ThresholdSettings initial = {});
[[nodiscard]] std::optional<ExposureSettings> request_exposure_settings(
    QWidget* parent, std::function<void(bool, const ExposureSettings&)> preview_changed = {},
    ExposureSettings initial = {});
[[nodiscard]] std::optional<VibranceSettings> request_vibrance_settings(
    QWidget* parent, std::function<void(bool, const VibranceSettings&)> preview_changed = {},
    VibranceSettings initial = {});
[[nodiscard]] std::optional<SelectiveColorSettings> request_selective_color_settings(
    QWidget* parent, std::function<void(bool, const SelectiveColorSettings&)> preview_changed = {},
    SelectiveColorSettings initial = {});
[[nodiscard]] std::optional<BlackWhiteSettings> request_black_white_settings(
    QWidget* parent, std::function<void(bool, const BlackWhiteSettings&)> preview_changed = {},
    BlackWhiteSettings initial = {});
[[nodiscard]] std::optional<ChannelMixerSettings> request_channel_mixer_settings(
    QWidget* parent, std::function<void(bool, const ChannelMixerSettings&)> preview_changed = {},
    ChannelMixerSettings initial = {});
[[nodiscard]] std::optional<PhotoFilterSettings> request_photo_filter_settings(
    QWidget* parent, std::function<void(bool, const PhotoFilterSettings&)> preview_changed = {},
    PhotoFilterSettings initial = {});
// The translated name of Photoshop's Photo Filter preset at `index`
// (photo_filter_preset_colors() order).
[[nodiscard]] QString photo_filter_preset_name(std::size_t index);
// Opens with the saved defaults (Save Defaults) or Photoshop's factory values.
[[nodiscard]] std::optional<ShadowsHighlightsSettings> request_shadows_highlights_settings(
    QWidget* parent, std::function<void(bool, const ShadowsHighlightsSettings&)> preview_changed = {});
// Photoshop's Color Lookup dialog in its 3DLUT File mode: a file dropdown (None, the
// loaded LUT, Load 3D LUT...) and Dither.
[[nodiscard]] std::optional<ColorLookupSettings> request_color_lookup_settings(
    QWidget* parent, std::function<void(bool, const ColorLookupSettings&)> preview_changed = {},
    ColorLookupSettings initial = {});
[[nodiscard]] std::optional<BrightnessContrastSettings> request_brightness_contrast_settings(
    QWidget* parent, std::function<void(bool, const BrightnessContrastSettings&)> preview_changed = {},
    BrightnessContrastSettings initial = {});
// Defined in gradient_map_dialog.cpp. `library` feeds the Preset... quick
// picker (disabled when null); presets resolve their dynamic stops from
// `foreground` and `background`. The returned settings carry no lookup table.
[[nodiscard]] std::optional<GradientMapSettings> request_gradient_map_settings(
    QWidget* parent, std::function<void(bool, const GradientMapSettings&)> preview_changed = {},
    GradientMapSettings initial = {}, GradientLibrary* library = nullptr, RgbColor foreground = RgbColor{0, 0, 0},
    RgbColor background = RgbColor{255, 255, 255});
// When a blur-family filter grows the layer (see build_filter_preview_pixels), the
// returned buffer is larger than `original` and `result_bounds`, if provided,
// receives the new document-space bounds (origin shifted, size grown). For other
// filters the buffer keeps its size and `result_bounds` is set to `bounds`.
[[nodiscard]] PixelBuffer build_filter_preview_pixels(
    const PixelBuffer& original, const QRegion& selection, Rect bounds, const FilterRegistry& registry,
    const FilterPreviewSettings& settings, const FilterProgress* progress = nullptr, Rect* result_bounds = nullptr);
[[nodiscard]] PixelBuffer build_filter_preview_pixels(
    const PixelBuffer& original, const QRegion& selection, Rect bounds, const FilterRegistry& registry,
    const FilterRecipe& recipe, const FilterProgress* progress = nullptr, Rect* result_bounds = nullptr);
// True when any enabled, nonzero-opacity entry's catalog metadata declares
// fills_entire_canvas (today only Clouds).
[[nodiscard]] bool filter_recipe_fills_entire_canvas(const FilterRegistry& registry,
                                                     const FilterRecipe& recipe);
struct CanvasFilterSource {
  PixelBuffer pixels;  // original embedded at its offset, transparent elsewhere
  Rect bounds;         // unite of the layer bounds and the canvas rect
};
// Builds the document-wide source a canvas-filling filter renders from.
// Returns nullopt when embedding does not apply: the layer has no alpha
// channel, the canvas rect is empty, or the layer already covers the canvas.
[[nodiscard]] std::optional<CanvasFilterSource> make_canvas_filling_filter_source(
    const PixelBuffer& original, Rect bounds, Rect canvas_bounds);
[[nodiscard]] bool pixel_buffers_equal(const PixelBuffer& lhs, const PixelBuffer& rhs);
[[nodiscard]] bool editable_rgb8_layer(const Layer* layer);
void apply_levels_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection, LevelsSettings settings,
                            const FilterProgress* progress = nullptr);
void apply_curves_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection, CurvesSettings settings,
                            const FilterProgress* progress = nullptr);
void apply_hue_saturation_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                    HueSaturationSettings settings, const FilterProgress* progress = nullptr);
void apply_color_balance_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                   ColorBalanceSettings settings, const FilterProgress* progress = nullptr);
// The destructive Gradient Map: the adjustment layer's core math, with the
// dither seeded by document coordinates (bounds gives the buffer's origin).
void apply_gradient_map_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                  const GradientMapSettings& settings, const FilterProgress* progress = nullptr);
void apply_vibrance_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection, VibranceSettings settings,
                              const FilterProgress* progress = nullptr);
// The destructive Selective Color: the adjustment layer's core math, so the result
// equals a Selective Color layer over the same pixels.
void apply_selective_color_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                     const SelectiveColorSettings& settings,
                                     const FilterProgress* progress = nullptr);
// The destructive Black & White: the adjustment layer's core math, so the result
// equals a Black & White layer over the same pixels.
void apply_black_white_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                 const BlackWhiteSettings& settings, const FilterProgress* progress = nullptr);
// The destructive Channel Mixer: the adjustment layer's core math, so the result
// equals a Channel Mixer layer over the same pixels.
void apply_channel_mixer_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                   const ChannelMixerSettings& settings, const FilterProgress* progress = nullptr);
// The destructive Photo Filter: the adjustment layer's per-pixel function, so the
// result equals a Photo Filter layer over the same pixels.
void apply_photo_filter_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                  PhotoFilterSettings settings, const FilterProgress* progress = nullptr);
// Applies a resolved Match Color map (core/match_color.hpp) inside the selection.
void apply_match_color_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                 const MatchColorTransform& transform, const FilterProgress* progress = nullptr);
// Neighborhoods read the whole layer; only selected pixels change.
void apply_shadows_highlights_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                        const ShadowsHighlightsSettings& settings,
                                        const FilterProgress* progress = nullptr);
void apply_color_lookup_to_pixels(PixelBuffer& pixels, Rect bounds, const QRegion& selection,
                                  const ColorLookupSettings& settings, const FilterProgress* progress = nullptr);

}  // namespace patchy::ui
