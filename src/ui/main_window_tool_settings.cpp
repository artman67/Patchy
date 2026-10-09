// MainWindow's tool-settings persistence, split out of
// main_window_tool_options.cpp: load_tool_settings (one step per tool family),
// the debounced and immediate save_tool_settings, and
// stash_active_brush_settings, plus the selection-edge key table and the
// gradient-stop (de)serializers only they use. Every settings key is a
// permanent identifier.
// Pure function moves; behavior must stay identical.

#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "core/warp_mesh.hpp"
#include "core/layer_render_utils.hpp"
#include "core/layer_tree.hpp"
#include "core/palette_presets.hpp"
#include "core/pattern_presets.hpp"
#include "core/pixel_tools.hpp"
#include "formats/palette_io.hpp"
#include "filters/builtin_filters.hpp"
#include "formats/aseprite_document_io.hpp"
#include "formats/bmp_document_io.hpp"
#include "formats/heif_document_io.hpp"
#include "formats/raw_document_io.hpp"
#include "plugins/legacy_photoshop_adapter.hpp"
#include "psd/psd_document_io.hpp"
#include "psd/psd_filter_effects.hpp"
#include "psd/psd_smart_objects.hpp"
#include "ui/action_icons.hpp"
#include "ui/app_settings.hpp"
#include "render/compositor.hpp"
#include "ui/blend_mode_ui.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_settings_panel.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/current_brush.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/raw_develop_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "ui/gradient_preset_popup.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/font_picker.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_dock_panel.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/localization.hpp"
#include "ui/measurement_units.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/pattern_library.hpp"
#include "ui/photo_pattern_presets.hpp"
#include "ui/shape_appearance_dialog.hpp"
#include "ui/style_library.hpp"
#include "ui/print_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/scanner_import.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/start_panel.hpp"
#include "ui/tile_preview_window.hpp"
#include "ui/warp_text_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/update_checker.hpp"
#include "ui/zoom_status_bar.hpp"
#include "ui/theme_qss.hpp"
#include "ui/theme_palette.hpp"
#include "support/string_utils.hpp"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QDateTime>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QColorSpace>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLayout>
#include <QResizeEvent>
#include <QIcon>
#include <QImageReader>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QKeySequence>
#include <QListWidget>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygon>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QRegion>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QScopeGuard>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QMutex>
#include <QRawFont>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTextOption>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTransform>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <tchar.h>
#include <tpcshrd.h>
#endif

// Icon resources live in the static patchy_ui library; force registration before first use.
int qInitResources_icons();

namespace patchy::ui {

namespace {

QByteArray serialize_gradient_stops(const std::vector<GradientStop>& stops) {
  QJsonArray array;
  for (const auto& stop : normalized_gradient_stops(stops)) {
    QJsonObject object;
    object.insert(QStringLiteral("location"), std::clamp(stop.location, 0.0F, 1.0F));
    object.insert(QStringLiteral("r"), static_cast<int>(stop.color.r));
    object.insert(QStringLiteral("g"), static_cast<int>(stop.color.g));
    object.insert(QStringLiteral("b"), static_cast<int>(stop.color.b));
    object.insert(QStringLiteral("a"), static_cast<int>(stop.color.a));
    array.push_back(object);
  }
  return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

std::optional<std::vector<GradientStop>> deserialize_gradient_stops(const QByteArray& json) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !document.isArray()) {
    return std::nullopt;
  }
  std::vector<GradientStop> stops;
  for (const auto& value : document.array()) {
    if (!value.isObject()) {
      return std::nullopt;
    }
    const auto object = value.toObject();
    const auto location = object.value(QStringLiteral("location"));
    const auto r = object.value(QStringLiteral("r"));
    const auto g = object.value(QStringLiteral("g"));
    const auto b = object.value(QStringLiteral("b"));
    const auto a = object.value(QStringLiteral("a"));
    if (!location.isDouble() || !r.isDouble() || !g.isDouble() || !b.isDouble() || !a.isDouble()) {
      return std::nullopt;
    }
    stops.push_back(GradientStop{
        std::clamp(static_cast<float>(location.toDouble()), 0.0F, 1.0F),
        EditColor{static_cast<std::uint8_t>(std::clamp(r.toInt(), 0, 255)),
                  static_cast<std::uint8_t>(std::clamp(g.toInt(), 0, 255)),
                  static_cast<std::uint8_t>(std::clamp(b.toInt(), 0, 255)),
                  static_cast<std::uint8_t>(std::clamp(a.toInt(), 0, 255))}});
  }
  if (stops.size() < 2U) {
    return std::nullopt;
  }
  return normalized_gradient_stops(stops);
}

// Settings keys for each selection tool's Feather and Anti-alias. Permanent identifiers.
// Quick Select has no Anti-alias control; the Patch tool has neither and keeps the defaults.
struct SelectionEdgeSettingKeys {
  CanvasTool tool;
  const char* feather;
  const char* anti_alias;
};

constexpr std::array<SelectionEdgeSettingKeys, 6> kSelectionEdgeSettingKeys{{
    {CanvasTool::Marquee, "tools/marqueeFeather", "tools/marqueeAntiAlias"},
    {CanvasTool::EllipticalMarquee, "tools/ellipticalMarqueeFeather", "tools/ellipticalMarqueeAntiAlias"},
    {CanvasTool::Lasso, "tools/lassoFeather", "tools/lassoAntiAlias"},
    {CanvasTool::MagneticLasso, "tools/magneticLassoFeather", "tools/magneticLassoAntiAlias"},
    {CanvasTool::MagicWand, "tools/wandFeather", "tools/wandAntiAlias"},
    {CanvasTool::QuickSelect, "tools/quickSelectFeather", nullptr},
}};

}  // namespace

void MainWindow::load_tool_settings() {
  if (canvas_ == nullptr) {
    return;
  }
  auto settings = app_settings();
  // One step per tool family, in the historical order.
  load_brush_tool_settings(settings);
  load_selection_and_transform_tool_settings(settings);
  load_crop_and_retouch_tool_settings(settings);
  load_shape_tool_settings(settings);
  load_fill_zoom_gradient_and_type_settings(settings);
}

void MainWindow::load_brush_tool_settings(QSettings& settings) {
  // The working brush (with its base) is remembered under tools/workingBrush. A remembered
  // tip that no longer exists, or no remembered brush, starts from the Round startup preset.
  // The eraser starts from that preset too; only its size is kept across restarts.
  stored_eraser_brush_settings_ = BrushToolSettings{};
  preset_pen_override_.reset();
  apply_pen_input_settings(canvas_);
  const auto* preset = find_brush_preset(default_startup_brush_preset_id());
  if (preset != nullptr) {
    stored_eraser_brush_settings_ =
        BrushToolSettings{preset->size, preset->opacity, preset->flow, preset->softness,
                          preset->build_up};
  }
  if (!restore_working_brush(settings) && preset != nullptr) {
    current_brush().pick(working_brush_from_preset(*preset), BrushBase::Kind::Preset, preset->id);
  }
  stored_eraser_brush_settings_.size =
      settings.value(QStringLiteral("tools/eraserSize"), stored_eraser_brush_settings_.size).toInt();
  apply_active_brush_settings_to_canvas();
  current_mixer_wet_ =
      std::clamp(settings.value(QStringLiteral("tools/mixerWet"), current_mixer_wet_).toInt(), 0, 100);
  current_mixer_load_ =
      std::clamp(settings.value(QStringLiteral("tools/mixerLoad"), current_mixer_load_).toInt(), 1, 100);
  current_mixer_mix_ =
      std::clamp(settings.value(QStringLiteral("tools/mixerMix"), current_mixer_mix_).toInt(), 0, 100);
  current_mixer_flow_ =
      std::clamp(settings.value(QStringLiteral("tools/mixerFlow"), current_mixer_flow_).toInt(), 1, 100);
  canvas_->set_mixer_wet(current_mixer_wet_);
  canvas_->set_mixer_load(current_mixer_load_);
  canvas_->set_mixer_mix(current_mixer_mix_);
  canvas_->set_mixer_flow(current_mixer_flow_);
  canvas_->set_mixer_sample_all_layers(
      settings.value(QStringLiteral("tools/mixerSampleAllLayers"), canvas_->mixer_sample_all_layers())
          .toBool());
  current_brush_smoothing_ = std::clamp(
      settings.value(QStringLiteral("tools/brushSmoothing"), current_brush_smoothing_).toInt(), 0, 100);
  current_brush_smoothing_pulled_string_ =
      settings.value(QStringLiteral("tools/brushSmoothingPulledString"), current_brush_smoothing_pulled_string_)
          .toBool();
  current_brush_smoothing_catch_up_ =
      settings.value(QStringLiteral("tools/brushSmoothingCatchUp"), current_brush_smoothing_catch_up_).toBool();
  current_brush_smoothing_catch_up_end_ =
      settings.value(QStringLiteral("tools/brushSmoothingCatchUpEnd"), current_brush_smoothing_catch_up_end_)
          .toBool();
  current_brush_smoothing_zoom_adjust_ =
      settings.value(QStringLiteral("tools/brushSmoothingZoomAdjust"), current_brush_smoothing_zoom_adjust_)
          .toBool();
  canvas_->set_brush_smoothing(current_brush_smoothing_);
  canvas_->set_brush_smoothing_pulled_string(current_brush_smoothing_pulled_string_);
  canvas_->set_brush_smoothing_catch_up(current_brush_smoothing_catch_up_);
  canvas_->set_brush_smoothing_catch_up_end(current_brush_smoothing_catch_up_end_);
  canvas_->set_brush_smoothing_zoom_adjust(current_brush_smoothing_zoom_adjust_);
}

void MainWindow::load_selection_and_transform_tool_settings(QSettings& settings) {
  canvas_->set_wand_tolerance(settings.value(QStringLiteral("tools/wandTolerance"), canvas_->wand_tolerance()).toInt());
  canvas_->set_wand_contiguous(settings.value(QStringLiteral("tools/wandContiguous"), canvas_->wand_contiguous()).toBool());
  canvas_->set_wand_sample_all_layers(
      settings.value(QStringLiteral("tools/wandSampleAllLayers"), canvas_->wand_sample_all_layers()).toBool());
  for (const auto& keys : kSelectionEdgeSettingKeys) {
    const auto index = static_cast<std::size_t>(CanvasWidget::selection_tool_index(keys.tool));
    selection_feather_by_tool_[index] =
        std::clamp(settings.value(QLatin1StringView(keys.feather), selection_feather_by_tool_[index]).toInt(), 0,
                   kMaxSelectionFeatherRadius);
    if (keys.anti_alias != nullptr) {
      selection_antialias_by_tool_[index] =
          settings.value(QLatin1StringView(keys.anti_alias), selection_antialias_by_tool_[index]).toBool();
    }
  }
  apply_selection_edge_settings_for_tool(current_tool_);
  canvas_->set_quick_select_size(
      settings.value(QStringLiteral("tools/quickSelectSize"), canvas_->quick_select_size()).toInt());
  canvas_->set_quick_select_sample_all_layers(
      settings.value(QStringLiteral("tools/quickSelectSampleAllLayers"), canvas_->quick_select_sample_all_layers())
          .toBool());
  canvas_->set_quick_select_enhance_edge(
      settings.value(QStringLiteral("tools/quickSelectEnhanceEdge"), canvas_->quick_select_enhance_edge()).toBool());
  canvas_->set_magnetic_lasso_width(
      settings.value(QStringLiteral("tools/magneticLassoWidth"), canvas_->magnetic_lasso_width()).toInt());
  canvas_->set_magnetic_lasso_edge_contrast(
      settings.value(QStringLiteral("tools/magneticLassoEdgeContrast"), canvas_->magnetic_lasso_edge_contrast())
          .toInt());
  canvas_->set_magnetic_lasso_frequency(
      settings.value(QStringLiteral("tools/magneticLassoFrequency"), canvas_->magnetic_lasso_frequency()).toInt());
  canvas_->set_show_transform_controls(
      settings.value(QStringLiteral("tools/showTransformControls"), true).toBool());
  // Auto-Select is remembered across runs (GitHub issue 73).
  canvas_->set_auto_select_layer(settings.value(QStringLiteral("tools/moveAutoSelect"), true).toBool());
  const auto transform_interpolation =
      settings.value(QStringLiteral("tools/transformInterpolation"),
                     static_cast<int>(CanvasWidget::TransformInterpolation::Bicubic))
          .toInt();
  switch (static_cast<CanvasWidget::TransformInterpolation>(transform_interpolation)) {
    case CanvasWidget::TransformInterpolation::NearestNeighbor:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::NearestNeighbor);
      break;
    case CanvasWidget::TransformInterpolation::Bilinear:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::Bilinear);
      break;
    case CanvasWidget::TransformInterpolation::BicubicSmoother:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::BicubicSmoother);
      break;
    case CanvasWidget::TransformInterpolation::BicubicSharper:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::BicubicSharper);
      break;
    case CanvasWidget::TransformInterpolation::Automatic:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::Automatic);
      break;
    case CanvasWidget::TransformInterpolation::Bicubic:
    default:
      canvas_->set_transform_interpolation(CanvasWidget::TransformInterpolation::Bicubic);
      break;
  }
}

void MainWindow::load_crop_and_retouch_tool_settings(QSettings& settings) {
  canvas_->set_clone_aligned(settings.value(QStringLiteral("tools/cloneAligned"), canvas_->clone_aligned()).toBool());
  canvas_->set_retouch_sample_all_layers(
      settings.value(QStringLiteral("tools/retouchSampleAllLayers"), canvas_->retouch_sample_all_layers()).toBool());
  current_crop_ratio_w_ =
      std::clamp(settings.value(QStringLiteral("tools/cropRatioWidth"), 0.0).toDouble(), 0.0, 10000.0);
  current_crop_ratio_h_ =
      std::clamp(settings.value(QStringLiteral("tools/cropRatioHeight"), 0.0).toDouble(), 0.0, 10000.0);
  current_crop_style_ = std::clamp(settings.value(QStringLiteral("tools/cropStyle"), 0).toInt(), 0, 1);
  if (crop_style_combo_ != nullptr) {
    const QSignalBlocker blocker(crop_style_combo_);
    crop_style_combo_->setCurrentIndex(current_crop_style_);
  }
  current_crop_delete_pixels_ = settings.value(QStringLiteral("tools/cropDeletePixels"), true).toBool();
  current_crop_delete_layers_ = settings.value(QStringLiteral("tools/cropDeleteLayers"), false).toBool();
  for (const auto& [check, value] : {std::pair{crop_delete_pixels_check_, current_crop_delete_pixels_},
                                     std::pair{crop_delete_layers_check_, current_crop_delete_layers_}}) {
    if (check != nullptr) {
      const QSignalBlocker blocker(check);
      check->setChecked(value);
    }
  }
  canvas_->set_crop_ratio(effective_crop_ratio_width(), effective_crop_ratio_height());
  // Patch mode and Transparent are deliberately session-only: every startup
  // begins at Source with Transparent off, because a persisted Destination or
  // Transparent reads as "the tool is broken" a session later. The retired
  // keys tools/patchMode and tools/patchTransparent must never be reused.
  current_pattern_stamp_pattern_id_ =
      settings.value(QStringLiteral("tools/patternStampPatternId")).toString();
  const auto& patterns = pattern_library().entries();
  if (pattern_library().find_entry_by_pattern_id(current_pattern_stamp_pattern_id_) == nullptr) {
    current_pattern_stamp_pattern_id_ = patterns.empty() ? QString() : patterns.front().id;
  }
  current_pattern_stamp_aligned_ =
      settings.value(QStringLiteral("tools/patternStampAligned"), true).toBool();
  apply_pattern_stamp_settings_to_canvas(canvas_);
  current_healing_diffusion_ =
      std::clamp(settings.value(QStringLiteral("tools/healingDiffusion"), current_healing_diffusion_).toInt(), 1, 7);
  canvas_->set_healing_diffusion(current_healing_diffusion_);
  current_local_adjustment_strength_ =
      std::clamp(settings.value(QStringLiteral("tools/localAdjustmentStrength"),
                                current_local_adjustment_strength_).toInt(),
                 1, 100);
  const auto local_tone_range =
      settings.value(QStringLiteral("tools/localToneRange"), QStringLiteral("midtones")).toString();
  current_local_tone_range_ = local_tone_range == QStringLiteral("shadows")
                                  ? CanvasWidget::LocalToneRange::Shadows
                              : local_tone_range == QStringLiteral("highlights")
                                  ? CanvasWidget::LocalToneRange::Highlights
                                  : CanvasWidget::LocalToneRange::Midtones;
  current_local_protect_tones_ =
      settings.value(QStringLiteral("tools/localProtectTones"), current_local_protect_tones_).toBool();
  const auto sponge_mode =
      settings.value(QStringLiteral("tools/spongeMode"), QStringLiteral("desaturate")).toString();
  current_sponge_mode_ = sponge_mode == QStringLiteral("saturate")
                             ? CanvasWidget::SpongeMode::Saturate
                             : CanvasWidget::SpongeMode::Desaturate;
  current_sponge_vibrance_ =
      settings.value(QStringLiteral("tools/spongeVibrance"), current_sponge_vibrance_).toBool();
  canvas_->set_local_adjustment_strength(current_local_adjustment_strength_);
  canvas_->set_local_tone_range(current_local_tone_range_);
  canvas_->set_local_protect_tones(current_local_protect_tones_);
  canvas_->set_sponge_mode(current_sponge_mode_);
  canvas_->set_sponge_vibrance(current_sponge_vibrance_);
  // Color Replacement keys store tokens, never enum values (docs/color-replacement.md).
  const auto color_replacement_mode =
      settings.value(QStringLiteral("tools/colorReplacementMode"), QStringLiteral("color")).toString();
  current_color_replacement_.mode = color_replacement_mode == QStringLiteral("hue")
                                        ? ColorReplacementMode::Hue
                                    : color_replacement_mode == QStringLiteral("saturation")
                                        ? ColorReplacementMode::Saturation
                                    : color_replacement_mode == QStringLiteral("luminosity")
                                        ? ColorReplacementMode::Luminosity
                                        : ColorReplacementMode::Color;
  const auto color_replacement_sampling =
      settings.value(QStringLiteral("tools/colorReplacementSampling"), QStringLiteral("continuous")).toString();
  current_color_replacement_.sampling = color_replacement_sampling == QStringLiteral("once")
                                            ? ColorReplacementSampling::Once
                                        : color_replacement_sampling == QStringLiteral("backgroundSwatch")
                                            ? ColorReplacementSampling::BackgroundSwatch
                                            : ColorReplacementSampling::Continuous;
  const auto color_replacement_limits =
      settings.value(QStringLiteral("tools/colorReplacementLimits"), QStringLiteral("contiguous")).toString();
  current_color_replacement_.limits = color_replacement_limits == QStringLiteral("discontiguous")
                                          ? ColorReplacementLimits::Discontiguous
                                      : color_replacement_limits == QStringLiteral("findEdges")
                                          ? ColorReplacementLimits::FindEdges
                                          : ColorReplacementLimits::Contiguous;
  current_color_replacement_.tolerance =
      std::clamp(settings.value(QStringLiteral("tools/colorReplacementTolerance"), 30).toInt(), 1, 100);
  current_color_replacement_.anti_alias =
      settings.value(QStringLiteral("tools/colorReplacementAntiAlias"), true).toBool();
  canvas_->set_color_replacement_settings(current_color_replacement_);
}

void MainWindow::load_shape_tool_settings(QSettings& settings) {
  canvas_->set_shape_corner_radius(
      settings.value(QStringLiteral("tools/shapeCornerRadius"), canvas_->shape_corner_radius()).toInt());
  // The spin resync below is signal-blocked, so mirror the loaded value by hand.
  current_shape_corner_radius_ = canvas_->shape_corner_radius();
  if (auto* shape_corner_radius = findChild<QSpinBox*>(QStringLiteral("shapeCornerRadiusSpin"));
      shape_corner_radius != nullptr) {
    QSignalBlocker blocker(shape_corner_radius);
    shape_corner_radius->setValue(canvas_->shape_corner_radius());
  }
  const auto vector_mode_name =
      settings.value(QStringLiteral("tools/vectorToolMode"), QStringLiteral("shape")).toString();
  current_vector_tool_mode_ = vector_mode_name == QStringLiteral("path")     ? VectorToolMode::Path
                              : vector_mode_name == QStringLiteral("pixels") ? VectorToolMode::Pixels
                                                                             : VectorToolMode::Shape;
  canvas_->set_vector_tool_mode(current_vector_tool_mode_);
  current_pen_auto_add_delete_ =
      settings.value(QStringLiteral("tools/penAutoAddDelete"), true).toBool();
  canvas_->set_pen_auto_add_delete(current_pen_auto_add_delete_);
  if (auto* pen_auto_add_delete = findChild<QCheckBox*>(QStringLiteral("penAutoAddDeleteCheck"));
      pen_auto_add_delete != nullptr) {
    QSignalBlocker blocker(pen_auto_add_delete);
    pen_auto_add_delete->setChecked(current_pen_auto_add_delete_);
  }
  if (vector_mode_combo_ != nullptr) {
    QSignalBlocker blocker(vector_mode_combo_);
    vector_mode_combo_->setCurrentIndex(current_vector_tool_mode_ == VectorToolMode::Path     ? 1
                                        : current_vector_tool_mode_ == VectorToolMode::Pixels ? 2
                                                                                              : 0);
  }
  // Options-bar paint mirrors: solid colors keep the historical keys; the
  // paint kind and pattern/gradient references are their own append-only keys.
  // Gradient placement and pattern placement params deliberately reset to
  // defaults each launch (the appearance dialog owns per-layer tuning).
  const auto load_vector_paint = [this, &settings](patchy::VectorFill& paint,
                                                   QString& gradient_id, const char* color_key,
                                                   const char* kind_key, const char* pattern_key,
                                                   const char* gradient_key) {
    const QColor default_color(paint.color.red, paint.color.green, paint.color.blue);
    if (const auto color = QColor(
            settings.value(QLatin1String(color_key), default_color.name()).toString());
        color.isValid()) {
      paint.color = RgbColor{static_cast<std::uint8_t>(color.red()),
                             static_cast<std::uint8_t>(color.green()),
                             static_cast<std::uint8_t>(color.blue())};
    }
    const auto kind_name = settings.value(QLatin1String(kind_key), QStringLiteral("solid")).toString();
    paint.kind = kind_name == QStringLiteral("none")       ? patchy::VectorFillKind::None
                 : kind_name == QStringLiteral("gradient") ? patchy::VectorFillKind::Gradient
                 : kind_name == QStringLiteral("pattern")  ? patchy::VectorFillKind::Pattern
                                                           : patchy::VectorFillKind::Solid;
    if (paint.kind == patchy::VectorFillKind::Pattern) {
      const auto pattern_id = settings.value(QLatin1String(pattern_key)).toString();
      if (const auto* entry = pattern_library().find_entry_by_pattern_id(pattern_id);
          entry != nullptr) {
        paint.pattern_id = pattern_id.toStdString();
        paint.pattern_name = entry->name.toStdString();
      } else {
        paint.kind = patchy::VectorFillKind::Solid;  // unresolvable reference
      }
    }
    if (paint.kind == patchy::VectorFillKind::Gradient) {
      gradient_id = settings.value(QLatin1String(gradient_key)).toString();
      const auto* entry = gradient_library().find_entry(gradient_id);
      if (entry == nullptr && !gradient_library().entries().empty()) {
        gradient_id = gradient_library().entries().front().storage_id;
        entry = gradient_library().find_entry(gradient_id);
      }
      const auto foreground = canvas_ != nullptr ? canvas_->primary_color() : QColor(Qt::black);
      const auto background = canvas_ != nullptr ? canvas_->secondary_color() : QColor(Qt::white);
      const auto fg = RgbColor{static_cast<std::uint8_t>(foreground.red()),
                               static_cast<std::uint8_t>(foreground.green()),
                               static_cast<std::uint8_t>(foreground.blue())};
      const auto bg = RgbColor{static_cast<std::uint8_t>(background.red()),
                               static_cast<std::uint8_t>(background.green()),
                               static_cast<std::uint8_t>(background.blue())};
      if (entry != nullptr) {
        static_cast<GradientDefinition&>(paint.gradient) =
            resolve_gradient_definition(entry->definition, fg, bg);
      } else {
        paint.gradient.color_stops = {GradientColorStop{0.0F, fg, 0.5F},
                                      GradientColorStop{1.0F, bg, 0.5F}};
        paint.gradient.alpha_stops = {GradientAlphaStop{0.0F, 1.0F, 0.5F},
                                      GradientAlphaStop{1.0F, 1.0F, 0.5F}};
      }
      paint.gradient.type = LayerStyleGradientType::Linear;
      paint.gradient.angle_degrees = 90.0F;
      paint.gradient.scale = 1.0F;
      paint.gradient.reverse = false;
    }
  };
  load_vector_paint(current_vector_fill_, current_vector_fill_gradient_id_,
                    "tools/vectorFillColor", "tools/vectorFillKind", "tools/vectorFillPatternId",
                    "tools/vectorFillGradientId");
  load_vector_paint(current_vector_stroke_paint_, current_vector_stroke_gradient_id_,
                    "tools/vectorStrokeColor", "tools/vectorStrokePaintKind",
                    "tools/vectorStrokePatternId", "tools/vectorStrokeGradientId");
  current_vector_stroke_enabled_ =
      settings.value(QStringLiteral("tools/vectorStrokeEnabled"), current_vector_stroke_enabled_).toBool();
  current_vector_stroke_width_ = std::clamp(
      settings.value(QStringLiteral("tools/vectorStrokeWidth"), current_vector_stroke_width_).toDouble(),
      0.1, 1000.0);
  current_vector_line_weight_ = std::clamp(
      settings.value(QStringLiteral("tools/vectorLineWeight"), current_vector_line_weight_).toDouble(), 1.0,
      1000.0);
  if (auto* stroke_check = findChild<QCheckBox*>(QStringLiteral("vectorStrokeCheck"));
      stroke_check != nullptr) {
    QSignalBlocker blocker(stroke_check);
    stroke_check->setChecked(current_vector_stroke_enabled_);
  }
  if (auto* stroke_width = findChild<QDoubleSpinBox*>(QStringLiteral("vectorStrokeWidthSpin"));
      stroke_width != nullptr) {
    QSignalBlocker blocker(stroke_width);
    stroke_width->setValue(current_vector_stroke_width_);
  }
  if (auto* line_weight = findChild<QDoubleSpinBox*>(QStringLiteral("vectorLineWeightSpin"));
      line_weight != nullptr) {
    QSignalBlocker blocker(line_weight);
    line_weight->setValue(current_vector_line_weight_);
  }
  current_line_arrow_start_ =
      settings.value(QStringLiteral("tools/lineArrowStart"), current_line_arrow_start_).toBool();
  current_line_arrow_end_ =
      settings.value(QStringLiteral("tools/lineArrowEnd"), current_line_arrow_end_).toBool();
  if (auto* arrow_start = findChild<QCheckBox*>(QStringLiteral("lineArrowStartCheck"));
      arrow_start != nullptr) {
    QSignalBlocker blocker(arrow_start);
    arrow_start->setChecked(current_line_arrow_start_);
  }
  if (auto* arrow_end = findChild<QCheckBox*>(QStringLiteral("lineArrowEndCheck"));
      arrow_end != nullptr) {
    QSignalBlocker blocker(arrow_end);
    arrow_end->setChecked(current_line_arrow_end_);
  }
  if (auto* sides = findChild<QSpinBox*>(QStringLiteral("polygonSidesSpin")); sides != nullptr) {
    QSignalBlocker blocker(sides);
    sides->setValue(
        std::clamp(settings.value(QStringLiteral("tools/polygonSides"), sides->value()).toInt(), 3, 100));
    canvas_->set_polygon_sides(sides->value());
  }
  if (auto* inset = findChild<QSpinBox*>(QStringLiteral("polygonStarInsetSpin")); inset != nullptr) {
    QSignalBlocker blocker(inset);
    inset->setValue(std::clamp(
        settings.value(QStringLiteral("tools/polygonStarInset"), inset->value()).toInt(), 0, 99));
    canvas_->set_polygon_star_inset(inset->value());
  }
  if (custom_shape_combo_ != nullptr) {
    const auto stored_shape = settings.value(QStringLiteral("tools/customShapeId")).toString();
    if (!stored_shape.isEmpty()) {
      if (const auto index = custom_shape_combo_->findData(stored_shape); index >= 0) {
        QSignalBlocker blocker(custom_shape_combo_);
        custom_shape_combo_->setCurrentIndex(index);
      }
    }
    apply_custom_shape_selection();
  }
  update_vector_swatch_icons();
}

void MainWindow::load_fill_zoom_gradient_and_type_settings(QSettings& settings) {
  canvas_->set_fill_opacity(settings.value(QStringLiteral("tools/fillOpacity"), canvas_->fill_opacity()).toInt());
  canvas_->set_fill_softness(settings.value(QStringLiteral("tools/fillSoftness"), canvas_->fill_softness()).toInt());
  canvas_->set_fill_tolerance(
      settings.value(QStringLiteral("tools/fillTolerance"), canvas_->fill_tolerance()).toInt());
  canvas_->set_fill_contiguous(
      settings.value(QStringLiteral("tools/fillContiguous"), canvas_->fill_contiguous()).toBool());
  canvas_->set_zoom_scrubby(settings.value(QStringLiteral("tools/zoomScrubby"), canvas_->zoom_scrubby()).toBool());
  canvas_->set_zoom_tool_zooms_out(
      settings.value(QStringLiteral("tools/zoomToolZoomsOut"), canvas_->zoom_tool_zooms_out()).toBool());
  const auto sync_fill_widget = [this](const QString& spin_name, const QString& slider_name, int value) {
    if (auto* spin = findChild<QSpinBox*>(spin_name); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(value);
    }
    if (auto* slider = findChild<QSlider*>(slider_name); slider != nullptr) {
      QSignalBlocker blocker(slider);
      slider->setValue(value);
    }
  };
  sync_fill_widget(QStringLiteral("fillOpacitySpin"), QStringLiteral("fillOpacitySlider"), canvas_->fill_opacity());
  sync_fill_widget(QStringLiteral("fillSoftnessSpin"), QStringLiteral("fillSoftnessSlider"), canvas_->fill_softness());
  if (auto* spin = findChild<QSpinBox*>(QStringLiteral("fillToleranceSpin")); spin != nullptr) {
    QSignalBlocker blocker(spin);
    spin->setValue(canvas_->fill_tolerance());
  }
  const auto gradient_method = settings.value(QStringLiteral("tools/gradientMethod"),
                                              static_cast<int>(canvas_->gradient_method()))
                                   .toInt();
  canvas_->set_gradient_method(gradient_method == static_cast<int>(GradientMethod::Radial) ? GradientMethod::Radial
                                                                                           : GradientMethod::Linear);
  canvas_->set_gradient_reverse(settings.value(QStringLiteral("tools/gradientReverse"), canvas_->gradient_reverse()).toBool());
  canvas_->set_gradient_opacity(settings.value(QStringLiteral("tools/gradientOpacity"), canvas_->gradient_opacity()).toInt());
  if (settings.value(QStringLiteral("tools/gradientUseCustomStops"), false).toBool()) {
    const auto stops = deserialize_gradient_stops(settings.value(QStringLiteral("tools/gradientStops")).toByteArray());
    canvas_->set_gradient_stops(stops);
  } else {
    canvas_->set_gradient_stops(std::nullopt);
  }
  if (text_smoothing_combo_ != nullptr) {
    set_text_smoothing_combo_value(
        text_smoothing_combo_,
        settings.value(QStringLiteral("tools/textSmoothing"), kDefaultTextAntiAlias).toInt());
  }
}

void MainWindow::schedule_save_tool_settings() {
  if (tool_settings_save_timer_ != nullptr) {
    tool_settings_save_timer_->start();
  } else {
    save_tool_settings();
  }
}

void MainWindow::save_tool_settings() const {
  if (canvas_ == nullptr) {
    return;
  }
  // An explicit save captures the current state, so drop any debounced one
  // still pending; otherwise it would fire later and rewrite the same values.
  if (tool_settings_save_timer_ != nullptr) {
    tool_settings_save_timer_->stop();
  }
  auto settings = app_settings();
  // The working brush and its base persist as one JSON value (see load_tool_settings()); the
  // eraser keeps only its size.
  save_working_brush(settings);
  const auto eraser_size =
      eraser_brush_settings_active_ ? canvas_->brush_size() : stored_eraser_brush_settings_.size;
  settings.setValue(QStringLiteral("tools/eraserSize"), eraser_size);
  settings.setValue(QStringLiteral("tools/mixerWet"), current_mixer_wet_);
  settings.setValue(QStringLiteral("tools/mixerLoad"), current_mixer_load_);
  settings.setValue(QStringLiteral("tools/mixerMix"), current_mixer_mix_);
  settings.setValue(QStringLiteral("tools/mixerFlow"), current_mixer_flow_);
  settings.setValue(QStringLiteral("tools/mixerSampleAllLayers"), canvas_->mixer_sample_all_layers());
  settings.setValue(QStringLiteral("tools/brushSmoothing"), current_brush_smoothing_);
  settings.setValue(QStringLiteral("tools/brushSmoothingPulledString"), current_brush_smoothing_pulled_string_);
  settings.setValue(QStringLiteral("tools/brushSmoothingCatchUp"), current_brush_smoothing_catch_up_);
  settings.setValue(QStringLiteral("tools/brushSmoothingCatchUpEnd"), current_brush_smoothing_catch_up_end_);
  settings.setValue(QStringLiteral("tools/brushSmoothingZoomAdjust"), current_brush_smoothing_zoom_adjust_);
  settings.setValue(QStringLiteral("tools/wandTolerance"), canvas_->wand_tolerance());
  settings.setValue(QStringLiteral("tools/wandContiguous"), canvas_->wand_contiguous());
  settings.setValue(QStringLiteral("tools/wandSampleAllLayers"), canvas_->wand_sample_all_layers());
  for (const auto& keys : kSelectionEdgeSettingKeys) {
    const auto index = static_cast<std::size_t>(CanvasWidget::selection_tool_index(keys.tool));
    settings.setValue(QLatin1StringView(keys.feather), selection_feather_by_tool_[index]);
    if (keys.anti_alias != nullptr) {
      settings.setValue(QLatin1StringView(keys.anti_alias), selection_antialias_by_tool_[index]);
    }
  }
  settings.setValue(QStringLiteral("tools/quickSelectSize"), canvas_->quick_select_size());
  settings.setValue(QStringLiteral("tools/quickSelectSampleAllLayers"), canvas_->quick_select_sample_all_layers());
  settings.setValue(QStringLiteral("tools/quickSelectEnhanceEdge"), canvas_->quick_select_enhance_edge());
  settings.setValue(QStringLiteral("tools/magneticLassoWidth"), canvas_->magnetic_lasso_width());
  settings.setValue(QStringLiteral("tools/magneticLassoEdgeContrast"), canvas_->magnetic_lasso_edge_contrast());
  settings.setValue(QStringLiteral("tools/magneticLassoFrequency"), canvas_->magnetic_lasso_frequency());
  settings.setValue(QStringLiteral("tools/showTransformControls"), canvas_->show_transform_controls());
  // tools/moveAutoSelect is written by the options-bar checkbox itself, not here:
  // tests flip the canvas flag directly and must not leak it into later windows.
  settings.setValue(QStringLiteral("tools/transformInterpolation"), static_cast<int>(canvas_->transform_interpolation()));
  settings.setValue(QStringLiteral("tools/cloneAligned"), canvas_->clone_aligned());
  settings.setValue(QStringLiteral("tools/retouchSampleAllLayers"), canvas_->retouch_sample_all_layers());
  // The remembered ratio, not the canvas's: Size mode runs the canvas unconstrained.
  settings.setValue(QStringLiteral("tools/cropRatioWidth"), current_crop_ratio_w_);
  settings.setValue(QStringLiteral("tools/cropRatioHeight"), current_crop_ratio_h_);
  settings.setValue(QStringLiteral("tools/cropStyle"), current_crop_style_);
  settings.setValue(QStringLiteral("tools/cropDeletePixels"), current_crop_delete_pixels_);
  settings.setValue(QStringLiteral("tools/cropDeleteLayers"), current_crop_delete_layers_);
  settings.setValue(QStringLiteral("tools/patternStampPatternId"), current_pattern_stamp_pattern_id_);
  settings.setValue(QStringLiteral("tools/patternStampAligned"), current_pattern_stamp_aligned_);
  settings.setValue(QStringLiteral("tools/healingDiffusion"), current_healing_diffusion_);
  settings.setValue(QStringLiteral("tools/localAdjustmentStrength"), current_local_adjustment_strength_);
  QString local_tone_range = QStringLiteral("midtones");
  switch (current_local_tone_range_) {
    case CanvasWidget::LocalToneRange::Shadows:
      local_tone_range = QStringLiteral("shadows");
      break;
    case CanvasWidget::LocalToneRange::Midtones:
      break;
    case CanvasWidget::LocalToneRange::Highlights:
      local_tone_range = QStringLiteral("highlights");
      break;
  }
  settings.setValue(QStringLiteral("tools/localToneRange"), local_tone_range);
  settings.setValue(QStringLiteral("tools/localProtectTones"), current_local_protect_tones_);
  settings.setValue(QStringLiteral("tools/spongeMode"),
                    current_sponge_mode_ == CanvasWidget::SpongeMode::Saturate
                        ? QStringLiteral("saturate")
                        : QStringLiteral("desaturate"));
  settings.setValue(QStringLiteral("tools/spongeVibrance"), current_sponge_vibrance_);
  const auto& color_replacement = current_color_replacement_;
  settings.setValue(QStringLiteral("tools/colorReplacementMode"),
                    color_replacement.mode == ColorReplacementMode::Hue          ? QStringLiteral("hue")
                    : color_replacement.mode == ColorReplacementMode::Saturation ? QStringLiteral("saturation")
                    : color_replacement.mode == ColorReplacementMode::Luminosity ? QStringLiteral("luminosity")
                                                                                 : QStringLiteral("color"));
  settings.setValue(QStringLiteral("tools/colorReplacementSampling"),
                    color_replacement.sampling == ColorReplacementSampling::Once ? QStringLiteral("once")
                    : color_replacement.sampling == ColorReplacementSampling::BackgroundSwatch
                        ? QStringLiteral("backgroundSwatch")
                        : QStringLiteral("continuous"));
  settings.setValue(QStringLiteral("tools/colorReplacementLimits"),
                    color_replacement.limits == ColorReplacementLimits::Discontiguous ? QStringLiteral("discontiguous")
                    : color_replacement.limits == ColorReplacementLimits::FindEdges   ? QStringLiteral("findEdges")
                                                                                      : QStringLiteral("contiguous"));
  settings.setValue(QStringLiteral("tools/colorReplacementTolerance"), color_replacement.tolerance);
  settings.setValue(QStringLiteral("tools/colorReplacementAntiAlias"), color_replacement.anti_alias);
  settings.setValue(QStringLiteral("tools/shapeCornerRadius"), canvas_->shape_corner_radius());
  settings.setValue(QStringLiteral("tools/vectorToolMode"),
                    current_vector_tool_mode_ == VectorToolMode::Path     ? QStringLiteral("path")
                    : current_vector_tool_mode_ == VectorToolMode::Pixels ? QStringLiteral("pixels")
                                                                          : QStringLiteral("shape"));
  settings.setValue(QStringLiteral("tools/penAutoAddDelete"), current_pen_auto_add_delete_);
  const auto save_vector_paint = [&settings](const patchy::VectorFill& paint,
                                             const QString& gradient_id, const char* color_key,
                                             const char* kind_key, const char* pattern_key,
                                             const char* gradient_key) {
    settings.setValue(QLatin1String(color_key),
                      QColor(paint.color.red, paint.color.green, paint.color.blue)
                          .name(QColor::HexRgb));
    settings.setValue(QLatin1String(kind_key),
                      paint.kind == patchy::VectorFillKind::None       ? QStringLiteral("none")
                      : paint.kind == patchy::VectorFillKind::Gradient ? QStringLiteral("gradient")
                      : paint.kind == patchy::VectorFillKind::Pattern  ? QStringLiteral("pattern")
                                                                       : QStringLiteral("solid"));
    settings.setValue(QLatin1String(pattern_key), QString::fromStdString(paint.pattern_id));
    settings.setValue(QLatin1String(gradient_key), gradient_id);
  };
  save_vector_paint(current_vector_fill_, current_vector_fill_gradient_id_,
                    "tools/vectorFillColor", "tools/vectorFillKind", "tools/vectorFillPatternId",
                    "tools/vectorFillGradientId");
  save_vector_paint(current_vector_stroke_paint_, current_vector_stroke_gradient_id_,
                    "tools/vectorStrokeColor", "tools/vectorStrokePaintKind",
                    "tools/vectorStrokePatternId", "tools/vectorStrokeGradientId");
  settings.setValue(QStringLiteral("tools/vectorStrokeEnabled"), current_vector_stroke_enabled_);
  settings.setValue(QStringLiteral("tools/vectorStrokeWidth"), current_vector_stroke_width_);
  settings.setValue(QStringLiteral("tools/vectorLineWeight"), current_vector_line_weight_);
  settings.setValue(QStringLiteral("tools/lineArrowStart"), current_line_arrow_start_);
  settings.setValue(QStringLiteral("tools/lineArrowEnd"), current_line_arrow_end_);
  if (auto* sides = findChild<QSpinBox*>(QStringLiteral("polygonSidesSpin")); sides != nullptr) {
    settings.setValue(QStringLiteral("tools/polygonSides"), sides->value());
  }
  if (auto* inset = findChild<QSpinBox*>(QStringLiteral("polygonStarInsetSpin")); inset != nullptr) {
    settings.setValue(QStringLiteral("tools/polygonStarInset"), inset->value());
  }
  if (custom_shape_combo_ != nullptr) {
    settings.setValue(QStringLiteral("tools/customShapeId"),
                      custom_shape_combo_->currentData().toString());
  }
  settings.setValue(QStringLiteral("tools/fillOpacity"), canvas_->fill_opacity());
  settings.setValue(QStringLiteral("tools/fillSoftness"), canvas_->fill_softness());
  settings.setValue(QStringLiteral("tools/fillTolerance"), canvas_->fill_tolerance());
  settings.setValue(QStringLiteral("tools/fillContiguous"), canvas_->fill_contiguous());
  settings.setValue(QStringLiteral("tools/zoomScrubby"), canvas_->zoom_scrubby());
  settings.setValue(QStringLiteral("tools/zoomToolZoomsOut"), canvas_->zoom_tool_zooms_out());
  settings.setValue(QStringLiteral("tools/gradientMethod"), static_cast<int>(canvas_->gradient_method()));
  settings.setValue(QStringLiteral("tools/gradientReverse"), canvas_->gradient_reverse());
  settings.setValue(QStringLiteral("tools/gradientOpacity"), canvas_->gradient_opacity());
  settings.setValue(QStringLiteral("tools/gradientUseCustomStops"), canvas_->gradient_stops().has_value());
  if (canvas_->gradient_stops().has_value()) {
    settings.setValue(QStringLiteral("tools/gradientStops"), serialize_gradient_stops(*canvas_->gradient_stops()));
  } else {
    settings.remove(QStringLiteral("tools/gradientStops"));
  }
  if (text_smoothing_combo_ != nullptr) {
    settings.setValue(QStringLiteral("tools/textSmoothing"), text_smoothing_combo_value(text_smoothing_combo_));
  }
}

void MainWindow::stash_active_brush_settings() {
  if (canvas_ == nullptr) {
    return;
  }
  if (preset_pen_override_) {
    preset_pen_override_ = canvas_->pen_input_settings();
  }
  current_fill_opacity_ = canvas_->fill_opacity();
  current_fill_softness_ = canvas_->fill_softness();
  current_fill_tolerance_ = canvas_->fill_tolerance();
  current_fill_contiguous_ = canvas_->fill_contiguous();
  current_zoom_scrubby_ = canvas_->zoom_scrubby();
  current_zoom_tool_zooms_out_ = canvas_->zoom_tool_zooms_out();
  current_quick_select_size_ = canvas_->quick_select_size();
  current_quick_select_sample_all_layers_ = canvas_->quick_select_sample_all_layers();
  current_quick_select_enhance_edge_ = canvas_->quick_select_enhance_edge();
  current_transform_interpolation_ = canvas_->transform_interpolation();
  current_polygon_sides_ = canvas_->polygon_sides();
  current_polygon_star_inset_ = canvas_->polygon_star_inset();
  fold_canvas_brush_values();
}

}  // namespace patchy::ui
