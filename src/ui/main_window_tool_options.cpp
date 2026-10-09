// MainWindow's tool and options-bar state, split out of main_window.cpp: the
// brush-tip/pattern/gradient/style preset-library accessors, tool activation,
// the Pattern Stamp pattern combo, the transform and Puppet Warp session
// controls, the Crop style and size fields, register_option_action/
// refresh_options_bar, the selection-mode buttons, and
// sync_brush_controls_from_canvas. Tool-settings load/save live in
// main_window_tool_settings.cpp, the per-layer controls in
// main_window_layer_controls.cpp, and the color and gradient controls in
// main_window_colors.cpp.
// Pure function moves from main_window.cpp; behavior must stay identical.

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

// Reads a default-library seeding stamp (brushes/patterns/gradients/styles).
// On wasm the stamps would persist in localStorage while the seeded library
// files live in MEMFS and vanish on reload, so honoring a stored stamp there
// would skip seeding into an empty store; every wasm session reads as
// never-seeded and re-creates the defaults instead. (Deleted defaults
// therefore return on reload; preset persistence itself is a later step.)
int stored_default_asset_version(const QSettings& settings, const QString& key) {
#ifdef Q_OS_WASM
  Q_UNUSED(settings);
  Q_UNUSED(key);
  return 0;
#else
  return settings.value(key, 0).toInt();
#endif
}

}  // namespace

BrushTipLibrary& MainWindow::brush_tip_library() {
  if (brush_tip_library_ == nullptr) {
    brush_tip_library_ = new BrushTipLibrary({}, this);
    brush_tip_library_->set_pattern_library(&pattern_library());  // ABR-embedded textures
    // Seed the built-in bitmap tips once. The version gate (not an emptiness check) means a
    // user who deletes some or all of them is respected — they never come back on their own;
    // the manager's "Restore Defaults" button brings them back on demand. On upgrade only tips
    // NEWER than the stored version are seeded, so a bump never resurrects deleted defaults.
    auto settings = brush_library_settings();
    constexpr int kDefaultTipsVersion = 4;  // v4 (July 2026): texture, dual, color, wet-edge tips
    const auto stored_version =
        stored_default_asset_version(settings, QStringLiteral("brushes/defaultTipsVersion"));
    if (stored_version < kDefaultTipsVersion) {
      brush_tip_library_->restore_default_tips(stored_version);
      if (stored_version < 2) {
        // The v2 dynamics migration. Never re-run once applied: it cannot tell "user reset
        // dynamics after v2" from "never migrated", so a later re-run would stomp the reset.
        brush_tip_library_->apply_default_tip_dynamics();
      }
      settings.setValue(QStringLiteral("brushes/defaultTipsVersion"), kDefaultTipsVersion);
    }
  }
  return *brush_tip_library_;
}

PatternLibrary& MainWindow::pattern_library() {
  if (pattern_library_ == nullptr) {
    pattern_library_ = new PatternLibrary({}, this);
    connect(pattern_library_, &PatternLibrary::changed, this, [this] {
      refresh_pattern_stamp_pattern_combo();
      apply_pattern_stamp_settings_to_canvas(canvas_);
      push_current_brush_to_canvas(canvas_, CurrentBrush::Dynamics);  // a Brush Texture pattern
      schedule_save_tool_settings();
    });
    // Seed code-generated defaults once. A user deletion stays deleted across
    // launches; the Pattern Manager's explicit restore command brings it back.
    auto settings = app_settings();
    const auto stored_version =
        stored_default_asset_version(settings, QStringLiteral("patterns/defaultPatternsVersion"));
    if (stored_version < kDefaultPatternsVersion) {
      pattern_library_->restore_default_patterns(stored_version);
      if (pattern_library_->has_all_default_patterns_introduced_after(stored_version)) {
        settings.setValue(QStringLiteral("patterns/defaultPatternsVersion"),
                          kDefaultPatternsVersion);
      }
    }
  }
  return *pattern_library_;
}

GradientLibrary& MainWindow::gradient_library() {
  if (gradient_library_ == nullptr) {
    gradient_library_ = new GradientLibrary({}, this);
    auto settings = app_settings();
    const auto stored_version =
        stored_default_asset_version(settings, QStringLiteral("gradients/defaultGradientsVersion"));
    if (stored_version < kDefaultGradientsVersion) {
      gradient_library_->restore_default_gradients(stored_version);
      if (gradient_library_->has_all_default_gradients_introduced_after(stored_version))
        settings.setValue(QStringLiteral("gradients/defaultGradientsVersion"), kDefaultGradientsVersion);
    }
  }
  return *gradient_library_;
}

StyleLibrary& MainWindow::style_library() {
  if (style_library_ == nullptr) {
    style_library_ = new StyleLibrary({}, this);
    // Seed code-generated defaults once. A user deletion stays deleted across
    // launches; the Style Manager's explicit restore command brings it back.
    auto settings = app_settings();
    const auto stored_version =
        stored_default_asset_version(settings, QStringLiteral("styles/defaultStylesVersion"));
    if (stored_version < kDefaultStylesVersion) {
      style_library_->restore_default_styles(stored_version);
      if (style_library_->has_all_default_styles_introduced_after(stored_version)) {
        settings.setValue(QStringLiteral("styles/defaultStylesVersion"),
                          kDefaultStylesVersion);
      }
    }
  }
  return *style_library_;
}

void MainWindow::activate_tool(CanvasTool tool) {
  if (tool_action_group_ == nullptr) {
    return;
  }
  for (auto* action : tool_action_group_->actions()) {
    if (static_cast<CanvasTool>(action->data().toInt()) == tool) {
      action->trigger();
      return;
    }
  }
}

void MainWindow::apply_pattern_stamp_settings_to_canvas(CanvasWidget* canvas) {
  if (canvas == nullptr) {
    return;
  }
  canvas->set_pattern_stamp_aligned(current_pattern_stamp_aligned_);
  canvas->set_pattern_stamp_pattern(pattern_library().resource(current_pattern_stamp_pattern_id_));
}

void MainWindow::refresh_pattern_stamp_pattern_combo() {
  if (pattern_stamp_pattern_combo_ == nullptr) {
    return;
  }
  const QSignalBlocker blocker(pattern_stamp_pattern_combo_);
  pattern_stamp_pattern_combo_->clear();
  for (const auto& entry : pattern_library().entries()) {
    pattern_stamp_pattern_combo_->addItem(QIcon(entry.thumbnail),
                                          pattern_library_entry_display_name(entry), entry.id);
    const auto index = pattern_stamp_pattern_combo_->count() - 1;
    auto detail = QStringLiteral("%1 x %2").arg(entry.size.width()).arg(entry.size.height());
    if (!entry.folder.isEmpty()) {
      detail = entry.folder + QStringLiteral(" - ") + detail;
    }
    pattern_stamp_pattern_combo_->setItemData(index, detail, Qt::ToolTipRole);
  }
  auto index = pattern_stamp_pattern_combo_->findData(current_pattern_stamp_pattern_id_);
  if (index < 0 && pattern_stamp_pattern_combo_->count() > 0) {
    index = 0;
    current_pattern_stamp_pattern_id_ = pattern_stamp_pattern_combo_->itemData(0).toString();
  }
  pattern_stamp_pattern_combo_->setCurrentIndex(index);
}

void MainWindow::apply_transform_controls_from_ui() {
  if (updating_transform_controls_ || canvas_ == nullptr || transform_x_spin_ == nullptr ||
      transform_y_spin_ == nullptr || transform_scale_x_spin_ == nullptr || transform_scale_y_spin_ == nullptr ||
      transform_rotation_spin_ == nullptr) {
    return;
  }
  canvas_->set_transform_controls_state(QPointF(transform_x_spin_->value(), transform_y_spin_->value()),
                                        transform_scale_x_spin_->value(), transform_scale_y_spin_->value(),
                                        transform_rotation_spin_->value());
}

void MainWindow::sync_transform_controls_from_canvas() {
  if (updating_transform_controls_) {
    return;
  }
  const auto state = canvas_ != nullptr ? canvas_->transform_controls_state() : std::optional<CanvasWidget::TransformControlsState>{};
  updating_transform_controls_ = true;
  const auto clear_guard = qScopeGuard([this] { updating_transform_controls_ = false; });

  const bool has_state = state.has_value();
  const auto set_widget_enabled = [has_state](QWidget* widget) {
    if (widget != nullptr) {
      widget->setEnabled(has_state);
    }
  };
  for (auto* widget : {static_cast<QWidget*>(transform_reference_combo_), static_cast<QWidget*>(transform_x_spin_),
                       static_cast<QWidget*>(transform_y_spin_), static_cast<QWidget*>(transform_scale_x_spin_),
                       static_cast<QWidget*>(transform_scale_y_spin_), static_cast<QWidget*>(transform_link_scale_button_),
                       static_cast<QWidget*>(transform_rotation_spin_),
                       static_cast<QWidget*>(transform_interpolation_combo_)}) {
    set_widget_enabled(widget);
  }
  const bool warp_active = canvas_ != nullptr && canvas_->warp_transform_active();
  const bool puppet_active = canvas_ != nullptr && canvas_->puppet_warp_active();
  const bool session_active = warp_active || puppet_active || (has_state && state->active);
  // Warp works on one layer; a folder/multi-selection transform session cannot
  // switch modes (same rule as refresh_options_bar's warp-toggle state). Puppet
  // Warp has no mode toggle.
  const bool multi_target_transform = canvas_ != nullptr && canvas_->free_transform_is_multi_target();
  for (auto* button : {transform_warp_mode_button_, transform_apply_button_, transform_cancel_button_}) {
    if (button != nullptr) {
      button->setEnabled(session_active && !(button == transform_warp_mode_button_ &&
                                             (multi_target_transform || puppet_active)));
    }
  }
  if (!state.has_value()) {
    return;
  }

  if (transform_reference_combo_ != nullptr) {
    QSignalBlocker blocker(transform_reference_combo_);
    const auto index = transform_reference_combo_->findData(static_cast<int>(state->reference_point));
    if (index >= 0) {
      transform_reference_combo_->setCurrentIndex(index);
    }
  }
  if (transform_x_spin_ != nullptr) {
    QSignalBlocker blocker(transform_x_spin_);
    transform_x_spin_->setValue(state->reference_position.x());
  }
  if (transform_y_spin_ != nullptr) {
    QSignalBlocker blocker(transform_y_spin_);
    transform_y_spin_->setValue(state->reference_position.y());
  }
  if (transform_scale_x_spin_ != nullptr) {
    QSignalBlocker blocker(transform_scale_x_spin_);
    transform_scale_x_spin_->setValue(state->scale_x_percent);
  }
  if (transform_scale_y_spin_ != nullptr) {
    QSignalBlocker blocker(transform_scale_y_spin_);
    transform_scale_y_spin_->setValue(state->scale_y_percent);
  }
  if (transform_rotation_spin_ != nullptr) {
    QSignalBlocker blocker(transform_rotation_spin_);
    transform_rotation_spin_->setValue(state->rotation_degrees);
  }
  if (transform_interpolation_combo_ != nullptr) {
    QSignalBlocker blocker(transform_interpolation_combo_);
    const auto index = transform_interpolation_combo_->findData(static_cast<int>(state->interpolation));
    if (index >= 0) {
      transform_interpolation_combo_->setCurrentIndex(index);
    }
  }
}

CanvasWidget::PuppetWarpOptions MainWindow::puppet_warp_options_from_ui() const {
  CanvasWidget::PuppetWarpOptions options;
  if (puppet_warp_mode_combo_ != nullptr && puppet_warp_mode_combo_->currentIndex() >= 0) {
    options.mode = static_cast<PuppetWarpMode>(puppet_warp_mode_combo_->currentData().toInt());
  }
  if (puppet_warp_density_combo_ != nullptr && puppet_warp_density_combo_->currentIndex() >= 0) {
    options.density = static_cast<PuppetWarpDensity>(puppet_warp_density_combo_->currentData().toInt());
  }
  if (puppet_warp_expansion_spin_ != nullptr) {
    options.expansion = static_cast<int>(std::lround(puppet_warp_expansion_spin_->value()));
  }
  if (puppet_warp_show_mesh_check_ != nullptr) {
    options.show_mesh = puppet_warp_show_mesh_check_->isChecked();
  }
  return options;
}

void MainWindow::sync_puppet_warp_controls_from_canvas() {
  if (canvas_ == nullptr || !canvas_->puppet_warp_active() || updating_transform_controls_) {
    return;
  }
  updating_transform_controls_ = true;
  const auto clear_guard = qScopeGuard([this] { updating_transform_controls_ = false; });
  // Pin controls follow the first selected pin; with none selected they gray out.
  const auto pin = canvas_->puppet_pin_controls();
  for (auto* widget : {static_cast<QWidget*>(puppet_warp_pin_forward_button_),
                       static_cast<QWidget*>(puppet_warp_pin_backward_button_),
                       static_cast<QWidget*>(puppet_warp_rotate_combo_)}) {
    if (widget != nullptr) {
      widget->setEnabled(pin.has_selection);
    }
  }
  if (puppet_warp_rotate_combo_ != nullptr) {
    const auto index = puppet_warp_rotate_combo_->findData(pin.fixed_rotation);
    if (index >= 0) {
      puppet_warp_rotate_combo_->setCurrentIndex(index);
    }
  }
  if (puppet_warp_rotate_angle_spin_ != nullptr) {
    // Auto shows the solution's own rotation read-only; Fixed makes it editable.
    puppet_warp_rotate_angle_spin_->setEnabled(pin.has_selection && pin.fixed_rotation);
    puppet_warp_rotate_angle_spin_->setValue(pin.has_selection ? std::round(pin.angle_degrees) : 0.0);
  }
  if (puppet_warp_remove_all_button_ != nullptr) {
    puppet_warp_remove_all_button_->setEnabled(canvas_->puppet_pin_count() > 0);
  }
}

double MainWindow::effective_crop_ratio_width() const noexcept {
  return current_crop_style_ == 0 ? current_crop_ratio_w_ : 0.0;
}

double MainWindow::effective_crop_ratio_height() const noexcept {
  return current_crop_style_ == 0 ? current_crop_ratio_h_ : 0.0;
}

void MainWindow::apply_crop_style(int style) {
  current_crop_style_ = std::clamp(style, 0, 1);
  if (canvas_ != nullptr) {
    // Size mode lifts the constraint (a custom box keeps its shape, the
    // automatic frame grows back to the canvas); Ratio mode restores the
    // remembered ratio, which re-fits the box like a fresh entry.
    canvas_->set_crop_ratio(effective_crop_ratio_width(), effective_crop_ratio_height());
  }
  schedule_save_tool_settings();
  refresh_options_bar();
}

void MainWindow::handle_crop_size_value_changed(bool horizontal, int value) {
  if (canvas_ == nullptr || value < 1) {
    return;
  }
  const auto rect = canvas_->crop_session_rect();
  if (!rect.has_value() || rect->isEmpty()) {
    return;
  }
  auto size = rect->size();
  const bool linked = crop_link_size_button_ != nullptr && crop_link_size_button_->isChecked();
  if (horizontal) {
    size.setWidth(value);
    if (linked) {
      size.setHeight(std::max(1, static_cast<int>(std::lround(static_cast<double>(value) * rect->height() /
                                                              rect->width()))));
    }
  } else {
    size.setHeight(value);
    if (linked) {
      size.setWidth(std::max(1, static_cast<int>(std::lround(static_cast<double>(value) * rect->width() /
                                                             rect->height()))));
    }
  }
  canvas_->set_crop_session_size(size);  // notifies, which re-syncs both fields
}

bool MainWindow::crop_option_widget_visible(QWidget* widget) const {
  const auto in = [widget](const std::vector<QWidget*>& widgets) {
    return std::find(widgets.begin(), widgets.end(), widget) != widgets.end();
  };
  if (in(crop_ratio_option_widgets_)) {
    return current_crop_style_ == 0;
  }
  if (in(crop_size_option_widgets_)) {
    return current_crop_style_ == 1;
  }
  return true;
}

void MainWindow::sync_crop_ratio_preset_combo() {
  if (crop_ratio_preset_combo_ == nullptr || canvas_ == nullptr ||
      crop_ratio_preset_combo_->count() < 3) {
    return;
  }
  const auto ratio_w = canvas_->crop_ratio_width();
  const auto ratio_h = canvas_->crop_ratio_height();
  const QSignalBlocker blocker(crop_ratio_preset_combo_);
  const auto custom_index = crop_ratio_preset_combo_->count() - 1;
  if (ratio_w <= 0.0 || ratio_h <= 0.0) {
    crop_ratio_preset_combo_->setCurrentIndex(0);
    return;
  }
  if (has_active_document() && document().width() > 0 && document().height() > 0) {
    const auto divisor = std::gcd(document().width(), document().height());
    if (ratio_w == static_cast<double>(document().width() / divisor) &&
        ratio_h == static_cast<double>(document().height() / divisor)) {
      crop_ratio_preset_combo_->setCurrentIndex(1);
      return;
    }
  }
  for (int index = 2; index < custom_index; ++index) {
    const auto preset = crop_ratio_preset_combo_->itemData(index).toSizeF();
    if (ratio_w == preset.width() && ratio_h == preset.height()) {
      crop_ratio_preset_combo_->setCurrentIndex(index);
      return;
    }
  }
  crop_ratio_preset_combo_->setCurrentIndex(custom_index);
}

void MainWindow::register_option_action(QWidget* widget, std::vector<CanvasTool> tools) {
  if (widget == nullptr) {
    return;
  }
  option_actions_.emplace_back(widget, std::move(tools));
}

void MainWindow::refresh_options_bar() {
  // Runs on every passive transform-box change (each Move-tool press), so it
  // reports under PATCHY_UI_PROFILE=1 like the other per-interaction refreshes.
  const UiProfileScope profile_scope("refresh_options_bar");
  sync_view_rotation_controls();
  const bool has_document = has_active_document();
  const bool edit_allowed = has_document && !preview_dialog_edit_locked();
  const auto transform_state =
      canvas_ != nullptr ? canvas_->transform_controls_state() : std::optional<CanvasWidget::TransformControlsState>{};
  const bool free_transform_session = edit_allowed && transform_state.has_value() && transform_state->active;
  const bool warp_session = edit_allowed && canvas_ != nullptr && canvas_->warp_transform_active();
  const bool puppet_session = edit_allowed && canvas_ != nullptr && canvas_->puppet_warp_active();
  const bool transform_session_active = free_transform_session || warp_session || puppet_session;
  const bool perspective_session = edit_allowed && canvas_ != nullptr && canvas_->perspective_warp_active();
  // Every widget gets ONE setVisible with its final state, hides before shows.
  // Showing a child of a visible parent activates the parent layouts
  // synchronously (Qt), so a show-then-hide pass (per-tool show, then the
  // per-mode hide of the shape tools' raster-only controls) wrapped the bar
  // onto a second row and pushed the canvas down for one painted frame.
  const auto mode_rules = vector_option_mode_rules();
  const auto final_visibility = [&](QWidget* widget, const std::vector<CanvasTool>& tools) {
    // A transform/warp session owns the options bar (Photoshop behavior): the
    // tool's own controls are unusable while one runs (the canvas consumes every
    // click), so they hide instead of stacking next to the session controls and
    // wrapping the bar onto a second row (which shifted the canvas down).
    const auto tool_matches = tools.empty() || std::find(tools.begin(), tools.end(), current_tool_) != tools.end();
    return tool_matches && !transform_session_active && !perspective_session &&
           vector_option_widget_visible(mode_rules, widget) &&
           crop_option_widget_visible(widget);
  };
  for (const auto& [widget, tools] : option_actions_) {
    if (widget != nullptr && !final_visibility(widget, tools) && !widget->isHidden()) {
      widget->setVisible(false);
      if (auto* button = qobject_cast<QToolButton*>(widget);
          button != nullptr && button->defaultAction() != nullptr) {
        button->defaultAction()->setVisible(false);
      }
    }
  }
  for (const auto& [widget, tools] : option_actions_) {
    if (widget == nullptr) {
      continue;
    }
    const auto visible = final_visibility(widget, tools);
    if (visible != !widget->isHidden()) {
      widget->setVisible(visible);
    }
    auto enabled = edit_allowed;
    if (widget->objectName() == QStringLiteral("mixerMixSpin")) {
      enabled = enabled && current_mixer_wet_ > 0;
    }
    // Style Normal ignores the size fields, so they stay greyed across refreshes.
    if (widget->objectName() == QStringLiteral("shapeFixedWidthSpin") ||
        widget->objectName() == QStringLiteral("shapeFixedHeightSpin")) {
      enabled = enabled && current_shape_style_ != CanvasWidget::MarqueeStyle::Normal;
    }
    if (widget->objectName() == QStringLiteral("selectionFixedWidthSpin") ||
        widget->objectName() == QStringLiteral("selectionFixedHeightSpin")) {
      enabled = enabled && current_marquee_style_ != CanvasWidget::MarqueeStyle::Normal;
    }
    // The Zoom tool stays usable while a preview dialog locks editing, so its
    // options row does too (build_options_bar tags those widgets).
    const bool allowed_while_locked = widget->property("optionsBarAllowedWhileLocked").toBool();
    if (allowed_while_locked) {
      enabled = has_document;
    }
    if (widget == brush_dynamics_button_ && brush_dynamics_button_ != nullptr) {
      // Enabled once a model is loaded (bitmap tip or the Round session); only the brief
      // pre-initialization state has neither.
      enabled = enabled && brush_dynamics_button_->has_active_tip();
    }
    // Buttons that mirror a menu QAction (the Move tool's Align row) take their
    // enabled state from that action's own refresh, never from the tool row.
    if (!widget->property("optionsBarMirrorsAction").toBool()) {
      widget->setEnabled(enabled);
    }
    // Buttons backed by a default action mirror that action's state, so keep the
    // action in sync too (otherwise it can override the widget flags we just set).
    if (auto* button = qobject_cast<QToolButton*>(widget);
        button != nullptr && button->defaultAction() != nullptr) {
      button->defaultAction()->setVisible(visible);
      button->defaultAction()->setEnabled(allowed_while_locked ? has_document : edit_allowed);
    }
  }

  // The numeric transform controls show only while a session is actually active
  // (Photoshop): the Move tool's passive box swaps them in the instant a handle
  // drag or Ctrl+T starts the session.
  const bool show_transform_options = free_transform_session;
  for (auto* widget : transform_option_actions_) {
    if (widget != nullptr) {
      widget->setVisible(show_transform_options);
      widget->setEnabled(show_transform_options);
    }
  }
  const bool show_warp_options = warp_session;
  for (auto* widget : warp_option_actions_) {
    if (widget != nullptr) {
      widget->setVisible(show_warp_options);
      widget->setEnabled(show_warp_options);
    }
  }
  for (auto* widget : puppet_warp_option_actions_) {
    if (widget != nullptr) {
      widget->setVisible(puppet_session);
      widget->setEnabled(puppet_session);
    }
  }
  for (auto* widget : transform_session_actions_) {
    if (widget != nullptr) {
      // Puppet Warp keeps apply/cancel but has no free-transform/warp toggle.
      const bool visible = transform_session_active && !(widget == transform_warp_mode_button_ && puppet_session);
      widget->setVisible(visible);
      widget->setEnabled(visible);
    }
  }
  // Perspective Warp owns the row the same way; the straighten trio shows in
  // Warp mode and needs a selected quad.
  const bool perspective_warp_mode =
      perspective_session && canvas_->perspective_warp_mode() == CanvasWidget::PerspectiveWarpMode::Warp;
  for (auto* widget : perspective_warp_option_actions_) {
    if (widget == nullptr) {
      continue;
    }
    const bool straighten = std::find(perspective_warp_straighten_buttons_.begin(),
                                      perspective_warp_straighten_buttons_.end(),
                                      widget) != perspective_warp_straighten_buttons_.end();
    widget->setVisible(perspective_session && (!straighten || perspective_warp_mode));
    widget->setEnabled(perspective_session &&
                       (!straighten || canvas_->perspective_warp_selected_quad() >= 0) &&
                       (widget != perspective_warp_remove_all_button_ || canvas_->perspective_warp_quad_count() > 0));
  }
  if (perspective_warp_layout_button_ != nullptr && perspective_warp_warp_button_ != nullptr) {
    perspective_warp_layout_button_->setChecked(perspective_session && !perspective_warp_mode);
    perspective_warp_warp_button_->setChecked(perspective_warp_mode);
  }
  if (transform_warp_mode_button_ != nullptr) {
    // setChecked never emits clicked, so no blocker is needed; this also restores
    // the visual state after a refused switch (text layer, undecodable source).
    transform_warp_mode_button_->setChecked(warp_session);
    // Warp works on one layer; a folder/multi-selection transform session
    // cannot switch modes, so gray the toggle instead of letting it refuse.
    if (transform_session_active && canvas_ != nullptr && canvas_->free_transform_is_multi_target()) {
      transform_warp_mode_button_->setEnabled(false);
    }
  }
  // The text session's apply/cancel pair rides next to the text controls while an
  // inline editor is open.  The finished-property check matters: commit teardown
  // has a window between marking the editor finished and reparenting it away in
  // which re-entrant refreshes (layer-list updates) still find the child.
  auto* inline_text_editor =
      canvas_ != nullptr ? canvas_->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor")) : nullptr;
  const bool text_session_active = !transform_session_active && !perspective_session &&
                                   inline_text_editor != nullptr &&
                                   !inline_text_editor->property(kTextEditorFinishedProperty).toBool();
  for (auto* button : {text_apply_button_, text_cancel_button_}) {
    if (button != nullptr) {
      button->setVisible(text_session_active);
      button->setEnabled(text_session_active);
    }
  }
  // The non-modal Character dialog grays out (and shows its click-in-text hint) whenever
  // no live editor session exists; every session boundary funnels through this refresh.
  sync_text_character_dialog_from_editor();
  // With no session the font, size, face and smoothing controls mirror the active text layer.
  sync_text_options_from_active_layer();
  if (show_warp_options && warp_style_combo_ != nullptr && warp_bend_spin_ != nullptr) {
    // Mirror the canvas state (a handle drag flips the style back to Custom).
    QSignalBlocker combo_blocker(warp_style_combo_);
    QSignalBlocker spin_blocker(warp_bend_spin_);
    const auto index = warp_style_combo_->findData(canvas_->warp_style_preset());
    if (index >= 0) {
      warp_style_combo_->setCurrentIndex(index);
    }
    if (canvas_->warp_style_preset() != QStringLiteral("warpCustom")) {
      warp_bend_spin_->setValue(canvas_->warp_style_preset_value());
    }
  }
  if (puppet_session) {
    sync_puppet_warp_controls_from_canvas();
  }
  refresh_vector_tool_options_visibility();
  refresh_vector_stroke_controls();
  if (options_flow_container_ != nullptr) {
    // Visibility changes alter how many controls there are, so recompute the
    // wrapped height and let the toolbar grow or shrink accordingly.
    options_flow_container_->layout()->invalidate();
    options_flow_container_->updateGeometry();
  }
  sync_transform_controls_from_canvas();

  refresh_option_control_values(edit_allowed);
  // Show the active tool's stored combine mode. The temporary Shift/Alt override
  // is applied live from the canvas's key event filter (see
  // set_selection_mode_changed_callback), so it is not folded in here where a
  // stale global modifier state could mask an explicit choice.
  update_selection_mode_buttons(canvas_ != nullptr ? canvas_->selection_mode()
                                                   : CanvasWidget::SelectionMode::Replace);
  sync_text_alignment_buttons_from_editor();
  if (brush_settings_panel_ != nullptr) {
    // Grey out the Brush Settings sections the active tool's strokes ignore.
    brush_settings_panel_->set_tool_sections(brush_sections_for_tool(current_tool_));
  }
}

void MainWindow::refresh_option_control_values(bool edit_allowed) {
  if (move_auto_select_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(move_auto_select_check_);
    move_auto_select_check_->setChecked(canvas_->auto_select_layer());
  }
  if (move_show_transform_controls_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(move_show_transform_controls_check_);
    move_show_transform_controls_check_->setChecked(canvas_->show_transform_controls());
  }
  if (clone_aligned_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(clone_aligned_check_);
    clone_aligned_check_->setChecked(canvas_->clone_aligned());
  }
  if (retouch_sample_all_layers_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(retouch_sample_all_layers_check_);
    retouch_sample_all_layers_check_->setChecked(canvas_->retouch_sample_all_layers());
  }
  if (mixer_sample_all_layers_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(mixer_sample_all_layers_check_);
    mixer_sample_all_layers_check_->setChecked(canvas_->mixer_sample_all_layers());
  }
  if (crop_ratio_w_spin_ != nullptr && crop_ratio_h_spin_ != nullptr && canvas_ != nullptr) {
    const QSignalBlocker w_blocker(crop_ratio_w_spin_);
    const QSignalBlocker h_blocker(crop_ratio_h_spin_);
    crop_ratio_w_spin_->setValue(current_crop_ratio_w_);
    crop_ratio_h_spin_->setValue(current_crop_ratio_h_);
  }
  sync_crop_ratio_preset_combo();
  if (crop_width_spin_ != nullptr && crop_height_spin_ != nullptr) {
    // Size mode mirrors the pending box; without a session the fields rest.
    const auto crop_rect = canvas_ != nullptr ? canvas_->crop_session_rect() : std::optional<QRect>{};
    const QSignalBlocker w_blocker(crop_width_spin_);
    const QSignalBlocker h_blocker(crop_height_spin_);
    if (crop_rect.has_value()) {
      crop_width_spin_->setValue(crop_rect->width());
      crop_height_spin_->setValue(crop_rect->height());
    }
    for (QWidget* widget : {static_cast<QWidget*>(crop_width_spin_), static_cast<QWidget*>(crop_height_spin_),
                            static_cast<QWidget*>(crop_link_size_button_)}) {
      if (widget != nullptr) {
        widget->setEnabled(edit_allowed && crop_rect.has_value());
      }
    }
  }
  // The canvas frame the tool starts with has nothing to apply or reset.
  const auto crop_has_changes = canvas_ != nullptr && canvas_->crop_session_has_changes();
  for (auto* button : {crop_apply_button_, crop_cancel_button_}) {
    if (button != nullptr) {
      button->setEnabled(edit_allowed && crop_has_changes);
    }
  }
  if (patch_mode_combo_ != nullptr && canvas_ != nullptr) {
    const QSignalBlocker blocker(patch_mode_combo_);
    patch_mode_combo_->setCurrentIndex(
        std::max(0, patch_mode_combo_->findData(static_cast<int>(canvas_->patch_tool_mode()))));
  }
  if (patch_transparent_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(patch_transparent_check_);
    patch_transparent_check_->setChecked(canvas_->patch_tool_transparent());
  }
  if (pattern_stamp_pattern_combo_ != nullptr) {
    const QSignalBlocker blocker(pattern_stamp_pattern_combo_);
    pattern_stamp_pattern_combo_->setCurrentIndex(
        pattern_stamp_pattern_combo_->findData(current_pattern_stamp_pattern_id_));
  }
  if (pattern_stamp_aligned_check_ != nullptr) {
    const QSignalBlocker blocker(pattern_stamp_aligned_check_);
    pattern_stamp_aligned_check_->setChecked(current_pattern_stamp_aligned_);
  }
  if (auto* healing_diffusion = findChild<QSpinBox*>(QStringLiteral("healingDiffusionSpin"));
      healing_diffusion != nullptr) {
    QSignalBlocker blocker(healing_diffusion);
    healing_diffusion->setValue(current_healing_diffusion_);
  }
  if (local_adjustment_strength_spin_ != nullptr) {
    QSignalBlocker blocker(local_adjustment_strength_spin_);
    local_adjustment_strength_spin_->setValue(current_local_adjustment_strength_);
  }
  if (local_tone_range_combo_ != nullptr) {
    const auto index = local_tone_range_combo_->findData(static_cast<int>(current_local_tone_range_));
    QSignalBlocker blocker(local_tone_range_combo_);
    local_tone_range_combo_->setCurrentIndex(std::max(0, index));
  }
  if (local_protect_tones_check_ != nullptr) {
    QSignalBlocker blocker(local_protect_tones_check_);
    local_protect_tones_check_->setChecked(current_local_protect_tones_);
  }
  if (sponge_mode_combo_ != nullptr) {
    const auto index = sponge_mode_combo_->findData(static_cast<int>(current_sponge_mode_));
    QSignalBlocker blocker(sponge_mode_combo_);
    sponge_mode_combo_->setCurrentIndex(std::max(0, index));
  }
  if (sponge_vibrance_check_ != nullptr) {
    QSignalBlocker blocker(sponge_vibrance_check_);
    sponge_vibrance_check_->setChecked(current_sponge_vibrance_);
  }
  for (const auto& [combo, value] :
       {std::pair{color_replacement_mode_combo_, static_cast<int>(current_color_replacement_.mode)},
        std::pair{color_replacement_sampling_combo_, static_cast<int>(current_color_replacement_.sampling)},
        std::pair{color_replacement_limits_combo_, static_cast<int>(current_color_replacement_.limits)}}) {
    if (combo != nullptr) {
      QSignalBlocker blocker(combo);
      combo->setCurrentIndex(std::max(0, combo->findData(value)));
    }
  }
  if (color_replacement_tolerance_spin_ != nullptr) {
    QSignalBlocker blocker(color_replacement_tolerance_spin_);
    color_replacement_tolerance_spin_->setValue(current_color_replacement_.tolerance);
  }
  if (color_replacement_anti_alias_check_ != nullptr) {
    QSignalBlocker blocker(color_replacement_anti_alias_check_);
    color_replacement_anti_alias_check_->setChecked(current_color_replacement_.anti_alias);
  }
  if (wand_contiguous_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(wand_contiguous_check_);
    wand_contiguous_check_->setChecked(canvas_->wand_contiguous());
  }
  if (fill_contiguous_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(fill_contiguous_check_);
    fill_contiguous_check_->setChecked(canvas_->fill_contiguous());
  }
  if (zoom_scrubby_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(zoom_scrubby_check_);
    zoom_scrubby_check_->setChecked(canvas_->zoom_scrubby());
  }
  if (zoom_in_mode_action_ != nullptr && zoom_out_mode_action_ != nullptr && canvas_ != nullptr) {
    zoom_in_mode_action_->setChecked(!canvas_->zoom_tool_zooms_out());
    zoom_out_mode_action_->setChecked(canvas_->zoom_tool_zooms_out());
  }
  if (wand_sample_all_layers_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(wand_sample_all_layers_check_);
    wand_sample_all_layers_check_->setChecked(canvas_->wand_sample_all_layers());
  }
  if (quick_select_sample_all_layers_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(quick_select_sample_all_layers_check_);
    quick_select_sample_all_layers_check_->setChecked(canvas_->quick_select_sample_all_layers());
  }
  if (quick_select_enhance_edge_check_ != nullptr && canvas_ != nullptr) {
    QSignalBlocker blocker(quick_select_enhance_edge_check_);
    quick_select_enhance_edge_check_->setChecked(canvas_->quick_select_enhance_edge());
  }
  if (canvas_ != nullptr) {
    if (auto* spin = findChild<QSpinBox*>(QStringLiteral("quickSelectSizeSpin")); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(canvas_->quick_select_size());
    }
    if (auto* slider = findChild<QSlider*>(QStringLiteral("quickSelectSizeSlider")); slider != nullptr) {
      QSignalBlocker blocker(slider);
      set_slider_to_value(*slider, canvas_->quick_select_size());
    }
    if (auto* spin = findChild<QSpinBox*>(QStringLiteral("magneticLassoWidthSpin")); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(canvas_->magnetic_lasso_width());
    }
    if (auto* spin = findChild<QSpinBox*>(QStringLiteral("magneticLassoContrastSpin")); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(canvas_->magnetic_lasso_edge_contrast());
    }
    if (auto* spin = findChild<QSpinBox*>(QStringLiteral("magneticLassoFrequencySpin")); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(canvas_->magnetic_lasso_frequency());
    }
  }
  refresh_gradient_controls_from_canvas();
  if (canvas_ != nullptr) {
    for (const auto& [name, value] : {
             std::pair{"fillOpacitySpin", canvas_->fill_opacity()},
             std::pair{"fillSoftnessSpin", canvas_->fill_softness()},
             std::pair{"polygonSidesSpin", canvas_->polygon_sides()},
             std::pair{"polygonStarInsetSpin", canvas_->polygon_star_inset()}}) {
      if (auto* spin = findChild<QSpinBox*>(QString::fromLatin1(name)); spin != nullptr) {
        const QSignalBlocker blocker(spin);
        spin->setValue(value);
      }
    }
    for (const auto& [name, value] : {
             std::pair{"fillOpacitySlider", canvas_->fill_opacity()},
             std::pair{"fillSoftnessSlider", canvas_->fill_softness()}}) {
      if (auto* slider = findChild<QSlider*>(QString::fromLatin1(name)); slider != nullptr) {
        const QSignalBlocker blocker(slider);
        slider->setValue(value);
      }
    }
  }
}

void MainWindow::update_selection_mode_buttons(CanvasWidget::SelectionMode mode) {
  const auto set_checked = [](QAction* action, bool checked) {
    if (action == nullptr) {
      return;
    }
    QSignalBlocker blocker(action);
    action->setChecked(checked);
  };
  set_checked(selection_new_mode_action_, mode == CanvasWidget::SelectionMode::Replace);
  set_checked(selection_add_mode_action_, mode == CanvasWidget::SelectionMode::Add);
  set_checked(selection_subtract_mode_action_, mode == CanvasWidget::SelectionMode::Subtract);
  set_checked(selection_intersect_mode_action_, mode == CanvasWidget::SelectionMode::Intersect);
}

void MainWindow::apply_selection_modes_to_canvas(CanvasWidget* canvas) {
  if (canvas == nullptr) {
    return;
  }
  for (std::size_t value = 0; value < kCanvasToolCount; ++value) {
    const auto tool = static_cast<CanvasTool>(value);
    if (const auto index = CanvasWidget::selection_tool_index(tool); index >= 0) {
      canvas->set_selection_mode_for_tool(tool, selection_modes_[static_cast<std::size_t>(index)]);
    }
  }
}

void MainWindow::apply_selection_edge_settings_for_tool(CanvasTool tool) {
  const auto index = CanvasWidget::selection_tool_index(tool);
  if (index < 0) {
    return;
  }
  current_selection_feather_radius_ = selection_feather_by_tool_[static_cast<std::size_t>(index)];
  current_selection_antialias_ = selection_antialias_by_tool_[static_cast<std::size_t>(index)];
  if (canvas_ != nullptr) {
    canvas_->set_selection_feather_radius(current_selection_feather_radius_);
    canvas_->set_selection_antialias(current_selection_antialias_);
  }
  if (auto* feather = findChild<QSpinBox*>(QStringLiteral("selectionFeatherSpin")); feather != nullptr) {
    const QSignalBlocker blocker(feather);
    feather->setValue(current_selection_feather_radius_);
  }
  if (auto* anti_alias = findChild<QCheckBox*>(QStringLiteral("selectionAntiAliasCheck")); anti_alias != nullptr) {
    const QSignalBlocker blocker(anti_alias);
    anti_alias->setChecked(current_selection_antialias_);
  }
}

MainWindow::PreviewDialogEditLock::PreviewDialogEditLock(MainWindow& window) noexcept : window_(&window) {
  window_->begin_preview_dialog_edit_lock();
}

MainWindow::PreviewDialogEditLock::PreviewDialogEditLock(PreviewDialogEditLock&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)) {}

MainWindow::PreviewDialogEditLock::~PreviewDialogEditLock() {
  release();
}

void MainWindow::PreviewDialogEditLock::release() noexcept {
  if (window_ == nullptr) {
    return;
  }
  window_->end_preview_dialog_edit_lock();
  window_ = nullptr;
}

void MainWindow::sync_brush_controls_from_canvas() {
  if (canvas_ == nullptr) {
    return;
  }
  // Equal values and a slider under the pointer are left alone, so an edit from the control
  // itself never fights the user's typing or drag.
  const auto set_spin = [](QSpinBox* spin, int value) {
    if (spin != nullptr && spin->value() != value) {
      const QSignalBlocker blocker(spin);
      spin->setValue(value);
    }
  };
  const auto set_slider = [](QSlider* slider, int value) {
    if (slider != nullptr && !slider->isSliderDown() && slider->value() != value) {
      const QSignalBlocker blocker(slider);
      slider->setValue(value);
    }
  };
  set_spin(brush_controls_.size, canvas_->brush_size());
  if (brush_controls_.size_slider != nullptr && !brush_controls_.size_slider->isSliderDown()) {
    const QSignalBlocker blocker(brush_controls_.size_slider);
    set_slider_to_value(*brush_controls_.size_slider, canvas_->brush_size());
  }
  set_spin(brush_controls_.opacity, canvas_->brush_opacity());
  set_slider(brush_controls_.opacity_slider, canvas_->brush_opacity());
  set_spin(brush_controls_.flow, canvas_->brush_flow());
  if (brush_controls_.airbrush != nullptr) {
    const QSignalBlocker blocker(brush_controls_.airbrush);
    brush_controls_.airbrush->setChecked(canvas_->brush_build_up());
  }
  set_spin(brush_controls_.softness, canvas_->brush_softness());
  set_slider(brush_controls_.softness_slider, canvas_->brush_softness());
  const auto sync_mixer_spin = [this](const char* object_name, int value) {
    if (auto* spin = findChild<QSpinBox*>(QString::fromLatin1(object_name)); spin != nullptr) {
      QSignalBlocker blocker(spin);
      spin->setValue(value);
    }
  };
  sync_mixer_spin("mixerWetSpin", current_mixer_wet_);
  sync_mixer_spin("mixerLoadSpin", current_mixer_load_);
  sync_mixer_spin("mixerMixSpin", current_mixer_mix_);
  sync_mixer_spin("mixerFlowSpin", current_mixer_flow_);
  // Blocked setValue skips the live wet->mix-enable and combo-derive
  // connections; re-derive both here.
  if (auto* mixer_mix = findChild<QSpinBox*>(QStringLiteral("mixerMixSpin")); mixer_mix != nullptr) {
    mixer_mix->setEnabled(current_mixer_wet_ > 0);
  }
  sync_mixer_combination_combo();
  if (auto* brush_smoothing = findChild<QSpinBox*>(QStringLiteral("brushSmoothingSpin"));
      brush_smoothing != nullptr) {
    QSignalBlocker blocker(brush_smoothing);
    brush_smoothing->setValue(canvas_->brush_smoothing());
  }
  const auto sync_smoothing_action = [](QAction* action, bool checked) {
    if (action != nullptr) {
      QSignalBlocker blocker(action);
      action->setChecked(checked);
    }
  };
  sync_smoothing_action(brush_smoothing_pulled_string_action_, canvas_->brush_smoothing_pulled_string());
  sync_smoothing_action(brush_smoothing_catch_up_action_, canvas_->brush_smoothing_catch_up());
  sync_smoothing_action(brush_smoothing_catch_up_end_action_, canvas_->brush_smoothing_catch_up_end());
  sync_smoothing_action(brush_smoothing_zoom_adjust_action_, canvas_->brush_smoothing_zoom_adjust());
  sync_brush_panels();
}

}  // namespace patchy::ui
