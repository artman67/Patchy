// The Options-bar rows for the painting tools: the brush preset, Size/Opacity/
// Soft, Flow and Airbrush, Smoothing, the Mixer Brush cluster (with its Useful
// Combinations table), Paint Symmetry, the tip picker and Dynamics button, plus
// the Gradient and Fill tool rows. The mixer-combination and paint-symmetry
// members those rows drive live here too.
// Row builders called by build_options_bar (main_window_actions_options_bar.cpp);
// pure moves out of its single body, so behavior and construction order are unchanged.

#include "ui/main_window.hpp"
#include "ui/appearance_edits.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/main_window_options_bar_internal.hpp"

#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "core/vector_shape.hpp"
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
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/current_brush.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/modifier_names.hpp"
#include "ui/raw_develop_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/font_picker.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/localization.hpp"
#include "ui/measurement_units.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/pattern_library.hpp"
#include "ui/pattern_manager_dialog.hpp"
#include "ui/photo_pattern_presets.hpp"
#include "ui/style_library.hpp"
#include "ui/print_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/scanner_import.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/tile_preview_window.hpp"
#include "ui/warp_text_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/update_checker.hpp"
#include "ui/zoom_status_bar.hpp"
#include "ui/tool_traits.hpp"
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
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
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
#include <numeric>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
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

// Photoshop's Useful Mixer Brush Combinations as Wet/Load/Mix triples,
// VERIFIED against Photoshop 2026 on 2026-08-14 by selecting each preset in
// the real UI and reading currentToolOptions back over COM
// (local-test-fixtures/mixer-calibration/preset-readings.txt). Photoshop sets
// Mix to 0 on the dry rows even though the control is greyed there. Names are
// generic descriptive English words, tr()'d for display.
struct MixerCombination {
  const char* name;
  int wet;
  int load;
  int mix;
};

const std::array<MixerCombination, 12>& mixer_useful_combinations() {
  static constexpr std::array<MixerCombination, 12> kCombinations = {{
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Dry"), 0, 50, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Dry, Light Load"), 0, 5, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Dry, Heavy Load"), 0, 100, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Moist"), 10, 5, 50},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Moist, Light Mix"), 10, 5, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Moist, Heavy Mix"), 10, 5, 100},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Wet"), 50, 50, 50},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Wet, Light Mix"), 50, 50, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Wet, Heavy Mix"), 50, 50, 100},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Very Wet"), 100, 50, 50},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Very Wet, Light Mix"), 100, 50, 0},
      {QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Very Wet, Heavy Mix"), 100, 50, 100},
  }};
  return kCombinations;
}

// The closed combo is deliberately compact (the Options bar must keep the
// mixer row on one line), so the popup list needs its own width or every
// entry truncates to "Dry...oad".
void widen_combo_popup_to_items(QComboBox* combo) {
  const QFontMetrics metrics(combo->font());
  int width = 0;
  for (int i = 0; i < combo->count(); ++i) {
    width = std::max(width, metrics.horizontalAdvance(combo->itemText(i)));
  }
  combo->view()->setMinimumWidth(width + 36);
}

}  // namespace

void MainWindow::retranslate_mixer_combination_combo() {
  if (mixer_combination_combo_ == nullptr) {
    return;
  }
  const QSignalBlocker blocker(mixer_combination_combo_);
  mixer_combination_combo_->setItemText(0, tr("Custom"));
  const auto& combinations = mixer_useful_combinations();
  for (std::size_t i = 0; i < combinations.size(); ++i) {
    mixer_combination_combo_->setItemText(static_cast<int>(i) + 1, tr(combinations[i].name));
  }
  widen_combo_popup_to_items(mixer_combination_combo_);
}

void MainWindow::sync_mixer_combination_combo() {
  if (mixer_combination_combo_ == nullptr) {
    return;
  }
  const auto* wet_spin = findChild<QSpinBox*>(QStringLiteral("mixerWetSpin"));
  const auto* load_spin = findChild<QSpinBox*>(QStringLiteral("mixerLoadSpin"));
  const auto* mix_spin = findChild<QSpinBox*>(QStringLiteral("mixerMixSpin"));
  if (wet_spin == nullptr || load_spin == nullptr || mix_spin == nullptr) {
    return;
  }
  int selection = 0;  // Custom
  const auto& combinations = mixer_useful_combinations();
  for (std::size_t i = 0; i < combinations.size(); ++i) {
    const auto& combination = combinations[i];
    if (combination.wet != wet_spin->value() || combination.load != load_spin->value()) {
      continue;
    }
    if (combination.mix >= 0 && combination.mix != mix_spin->value()) {
      continue;
    }
    selection = static_cast<int>(i) + 1;
    break;
  }
  const QSignalBlocker blocker(mixer_combination_combo_);
  mixer_combination_combo_->setCurrentIndex(selection);
}

void MainWindow::set_paint_symmetry(patchy::PaintSymmetryMode mode, int segments) {
  current_paint_symmetry_mode_ = mode;
  current_paint_symmetry_segments_ = patchy::clamp_paint_symmetry_segments(
      patchy::paint_symmetry_uses_segments(mode) ? mode : patchy::PaintSymmetryMode::Radial, segments);
  apply_paint_symmetry_to_canvas(canvas_);
  sync_paint_symmetry_controls();
}

void MainWindow::apply_paint_symmetry_to_canvas(CanvasWidget* canvas) {
  if (canvas == nullptr) {
    return;
  }
  canvas->set_paint_symmetry(current_paint_symmetry_mode_, current_paint_symmetry_segments_);
  canvas->set_paint_symmetry_visible(current_paint_symmetry_visible_);
}

void MainWindow::sync_paint_symmetry_controls() {
  const auto mode_index = static_cast<std::size_t>(current_paint_symmetry_mode_);
  for (std::size_t index = 0; index < paint_symmetry_mode_actions_.size(); ++index) {
    if (auto* action = paint_symmetry_mode_actions_[index]; action != nullptr) {
      const QSignalBlocker blocker(action);
      action->setChecked(index == mode_index);
    }
  }
  const auto active = current_paint_symmetry_mode_ != patchy::PaintSymmetryMode::Off;
  if (paint_symmetry_transform_action_ != nullptr) {
    paint_symmetry_transform_action_->setEnabled(active);
  }
  if (paint_symmetry_reset_action_ != nullptr) {
    paint_symmetry_reset_action_->setEnabled(active);
  }
  if (paint_symmetry_hide_action_ != nullptr) {
    const QSignalBlocker blocker(paint_symmetry_hide_action_);
    paint_symmetry_hide_action_->setChecked(!current_paint_symmetry_visible_);
  }
  if (paint_symmetry_button_ != nullptr &&
      paint_symmetry_button_->property("symmetryActive").toBool() != active) {
    // The accent outline marks a live symmetry, like the Dynamics button's.
    paint_symmetry_button_->setProperty("symmetryActive", active);
    paint_symmetry_button_->style()->unpolish(paint_symmetry_button_);
    paint_symmetry_button_->style()->polish(paint_symmetry_button_);
  }
}

void MainWindow::request_paint_symmetry_segments(patchy::PaintSymmetryMode mode) {
  // Photoshop asks for the segment count each time Radial or Mandala is chosen.
  QInputDialog input(this);
  input.setObjectName(QStringLiteral("paintSymmetrySegmentsDialog"));
  input.setWindowTitle(mode == patchy::PaintSymmetryMode::Mandala ? tr("Mandala Symmetry")
                                                                 : tr("Radial Symmetry"));
  input.setLabelText(tr("Segment count:"));
  input.setInputMode(QInputDialog::IntInput);
  input.setIntRange(patchy::kPaintSymmetryMinSegments,
                    mode == patchy::PaintSymmetryMode::Mandala ? patchy::kPaintSymmetryMaxMandalaSegments
                                                               : patchy::kPaintSymmetryMaxRadialSegments);
  input.setIntValue(patchy::clamp_paint_symmetry_segments(mode, current_paint_symmetry_segments_));
  if (exec_dialog(input) == QDialog::Accepted) {
    set_paint_symmetry(mode, input.intValue());
  } else {
    sync_paint_symmetry_controls();  // a cancelled prompt keeps the previous check
  }
}

void MainWindow::build_brush_preset_option(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  add_option_label(QT_TR_NOOP("Preset:"), tools_with(kToolBrushPresets));
  brush_preset_combo_ = new QComboBox(toolbar);
  brush_preset_combo_->setObjectName(QStringLiteral("brushPresetCombo"));
  // 112 (was 132): reclaims room for the labeled Smoothing controls on the
  // one-line Brush row (ui_brush_tip_picker_keeps_options_bar_height).
  brush_preset_combo_->setMinimumWidth(112);
  brush_preset_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  brush_preset_combo_->setMinimumContentsLength(8);
  for (const auto& preset : builtin_brush_presets()) {
    brush_preset_combo_->addItem(brush_preset_display_name(preset), preset.id);
  }
  {
    const auto preset_index = brush_preset_combo_->findData(default_startup_brush_preset_id());
    if (preset_index >= 0) {
      brush_preset_combo_->setCurrentIndex(preset_index);
    }
  }
  brush_preset_combo_->setProperty("lastBrushPresetId", brush_preset_combo_->currentData());
  add_option_widget(brush_preset_combo_, tools_with(kToolBrushPresets));
}

void MainWindow::build_brush_size_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // The raster brush controls double as the shape tools' Pixels-mode options;
  // refresh_vector_tool_options_visibility hides them in the vector modes.
  vector_pixel_only_option_widgets_.push_back(add_option_label(QT_TR_NOOP("Size:"), tools_with(kToolBrushSize)));
  auto* brush_size = new QSpinBox(toolbar);
  brush_size->setObjectName(QStringLiteral("brushSizeSpin"));
  brush_size->setRange(1, kMaxBrushSize);
  brush_size->setValue(canvas_defaults->brush_size());
  brush_size->setProperty(kToolbarSpinboxSliderCurvedProperty, true);
  configure_toolbar_spinbox(brush_size, 58);
  add_option_widget(brush_size, tools_with(kToolBrushSize));
  auto* brush_size_slider = new QSlider(Qt::Horizontal, toolbar);
  brush_size_slider->setObjectName(QStringLiteral("brushSizeSlider"));
  bind_curved_slider(*brush_size_slider, *brush_size);
  // 130 (was 150): the Brush row must keep one Options-bar line at ordinary
  // window widths now that it also carries the Smoothing spin and gear
  // (ui_brush_tip_picker_keeps_options_bar_height).
  brush_size_slider->setFixedWidth(124);
  register_retranslation([brush_size_slider] {
    brush_size_slider->setToolTip(resolve_modifier_names(MainWindow::tr("Brush size: press [ or ], or %ALT%+Right-drag on the canvas")));
  });
  add_option_widget(brush_size_slider, tools_with(kToolBrushSizeSliders));
  vector_pixel_only_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Opacity:"), tools_with(kToolBrushOpacity)));
  auto* brush_opacity = new QSpinBox(toolbar);
  brush_opacity->setObjectName(QStringLiteral("brushOpacitySpin"));
  brush_opacity->setRange(1, 100);
  brush_opacity->setValue(canvas_defaults->brush_opacity());
  brush_opacity->setSuffix(percent_suffix());
  configure_toolbar_spinbox(brush_opacity, 52);
  add_option_widget(brush_opacity, tools_with(kToolBrushOpacity));
  auto* brush_opacity_slider = new QSlider(Qt::Horizontal, toolbar);
  brush_opacity_slider->setObjectName(QStringLiteral("brushOpacitySlider"));
  brush_opacity_slider->setRange(1, 100);
  brush_opacity_slider->setValue(canvas_defaults->brush_opacity());
  brush_opacity_slider->setFixedWidth(100);  // was 120; see the size-slider note

  bind_tooltip(brush_opacity_slider, QT_TR_NOOP("Brush opacity: press number keys (5 = 50%, 0 = 100%)"));
  add_option_widget(brush_opacity_slider, tools_with(kToolBrushOpacitySlider));
  vector_pixel_only_option_widgets_.push_back(add_option_label(QT_TR_NOOP("Soft:"), tools_with(kToolBrushSize)));
  auto* brush_softness = new QSpinBox(toolbar);
  brush_softness->setObjectName(QStringLiteral("brushSoftnessSpin"));
  brush_softness->setRange(0, 100);
  brush_softness->setValue(canvas_defaults->brush_softness());
  brush_softness->setSuffix(percent_suffix());
  configure_toolbar_spinbox(brush_softness, 52);
  add_option_widget(brush_softness, tools_with(kToolBrushSize));
  auto* brush_softness_slider = new QSlider(Qt::Horizontal, toolbar);
  brush_softness_slider->setObjectName(QStringLiteral("brushSoftnessSlider"));
  brush_softness_slider->setRange(0, 100);
  brush_softness_slider->setValue(canvas_defaults->brush_softness());
  brush_softness_slider->setFixedWidth(96);  // was 110; see the size-slider note
  register_retranslation([brush_softness_slider] {
    brush_softness_slider->setToolTip(resolve_modifier_names(MainWindow::tr("Brush edge softness: %ALT%+Right-drag up or down on the canvas")));
  });
  add_option_widget(brush_softness_slider, tools_with(kToolBrushSizeSliders));
  for (auto* raster_only :
       std::initializer_list<QWidget*>{brush_size, brush_size_slider, brush_opacity,
                                       brush_opacity_slider, brush_softness, brush_softness_slider}) {
    vector_pixel_only_option_widgets_.push_back(raster_only);
  }
  brush_controls_.size = brush_size;
  brush_controls_.size_slider = brush_size_slider;
  brush_controls_.opacity = brush_opacity;
  brush_controls_.opacity_slider = brush_opacity_slider;
  brush_controls_.softness = brush_softness;
  brush_controls_.softness_slider = brush_softness_slider;
  connect(brush_size, &QSpinBox::valueChanged, this, [this](int value) {
    BrushEdit edit;
    edit.size = value;
    edit_brush_option(edit);
  });
  connect(brush_opacity, &QSpinBox::valueChanged, brush_opacity_slider, &QSlider::setValue);
  connect(brush_opacity_slider, &QSlider::valueChanged, brush_opacity, &QSpinBox::setValue);
  connect(brush_opacity, &QSpinBox::valueChanged, this, [this](int value) {
    BrushEdit edit;
    edit.opacity = value;
    edit_brush_option(edit);
  });

  auto* brush_flow_label = add_option_label(QT_TR_NOOP("Flow:"), tools_with(kToolFlow));
  bind_widget_text(brush_flow_label, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Flow:"));
  auto* brush_flow = new QSpinBox(toolbar);
  brush_flow->setObjectName(QStringLiteral("brushFlowSpin"));
  brush_flow->setRange(1, 100);
  brush_flow->setValue(canvas_defaults->brush_flow());
  brush_flow->setSuffix(percent_suffix());
  bind_tooltip(brush_flow, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Brush flow: Shift+number keys (number keys with Airbrush)"));
  configure_toolbar_spinbox(brush_flow, 60);
  add_option_widget(brush_flow, tools_with(kToolFlow));
  auto* brush_airbrush = new CheckGlyphBox(tr("Airbrush"), toolbar);
  brush_airbrush->setObjectName(QStringLiteral("brushAirbrushCheck"));
  bind_widget_text(brush_airbrush, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Airbrush"));
  brush_airbrush->setChecked(canvas_defaults->brush_build_up());
  bind_tooltip(brush_airbrush, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Build paint while the pointer is held still"));
  options_flow->addWidget(brush_airbrush);
  register_option_action(brush_airbrush, tools_honoring(BrushSection::BuildUp));
  brush_controls_.flow = brush_flow;
  brush_controls_.airbrush = brush_airbrush;
  connect(brush_flow, &QSpinBox::valueChanged, this, [this](int value) {
    BrushEdit edit;
    edit.flow = value;
    edit_brush_option(edit);
  });
  connect(brush_airbrush, &QCheckBox::toggled, this, [this](bool checked) {
    BrushEdit edit;
    edit.airbrush = checked;
    edit_brush_option(edit);
  });
}

void MainWindow::build_brush_smoothing_and_mixer_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // Stroke Smoothing (the kToolSmoothing tools): the percent spin plus a
  // gear button whose menu holds the four Photoshop smoothing toggles.
  const auto smoothing_tools = tools_with(kToolSmoothing);
  // Created here, APPENDED to the bar after the mixer cluster below so every
  // row reads Photoshop-style (..., Flow, Sample All Layers, Smooth).
  auto* brush_smoothing = new QSpinBox(toolbar);
  brush_smoothing->setObjectName(QStringLiteral("brushSmoothingSpin"));
  brush_smoothing->setRange(0, 100);
  brush_smoothing->setValue(current_brush_smoothing_);
  brush_smoothing->setSuffix(percent_suffix());
  bind_tooltip(brush_smoothing, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Stroke smoothing - 0% paints the raw pointer path"));
  configure_toolbar_spinbox(brush_smoothing, 48);
  connect(brush_smoothing, &QSpinBox::valueChanged, this, [this](int value) {
    current_brush_smoothing_ = value;
    if (canvas_ != nullptr) {
      canvas_->set_brush_smoothing(value);
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });
  brush_smoothing_options_button_ = new QToolButton(toolbar);
  brush_smoothing_options_button_->setObjectName(QStringLiteral("brushSmoothingOptionsButton"));
  // No gear glyph exists in the icon set; the compact "..." text follows the
  // other small options-bar buttons. Its height comes from the
  // QToolButton#brushSmoothingOptionsButton rule in main_window_theme.cpp (the
  // brushDynamicsButton pattern); the global QToolButton QSS min-height would
  // otherwise grow the Options bar row
  // (ui_brush_tip_picker_keeps_options_bar_height).
  brush_smoothing_options_button_->setText(QStringLiteral("..."));
  bind_tooltip(brush_smoothing_options_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Smoothing options"));
  brush_smoothing_options_button_->setPopupMode(QToolButton::InstantPopup);
  auto* smoothing_menu = new QMenu(brush_smoothing_options_button_);
  const auto add_smoothing_option = [this, smoothing_menu](const char* source, bool checked,
                                                           auto setter) {
    auto* action = smoothing_menu->addAction(tr(source));
    bind_action_text(action, source);
    action->setCheckable(true);
    action->setChecked(checked);
    connect(action, &QAction::toggled, this, [this, setter](bool on) {
      setter(*this, on);
      schedule_save_tool_settings();
      refresh_document_info();
    });
    return action;
  };
  brush_smoothing_pulled_string_action_ = add_smoothing_option(
      QT_TR_NOOP("Pulled String Mode"), current_brush_smoothing_pulled_string_,
      [](MainWindow& window, bool on) {
        window.current_brush_smoothing_pulled_string_ = on;
        if (window.canvas_ != nullptr) {
          window.canvas_->set_brush_smoothing_pulled_string(on);
        }
      });
  brush_smoothing_catch_up_action_ = add_smoothing_option(
      QT_TR_NOOP("Stroke Catch-up"), current_brush_smoothing_catch_up_,
      [](MainWindow& window, bool on) {
        window.current_brush_smoothing_catch_up_ = on;
        if (window.canvas_ != nullptr) {
          window.canvas_->set_brush_smoothing_catch_up(on);
        }
      });
  brush_smoothing_catch_up_end_action_ = add_smoothing_option(
      QT_TR_NOOP("Catch-up on Stroke End"), current_brush_smoothing_catch_up_end_,
      [](MainWindow& window, bool on) {
        window.current_brush_smoothing_catch_up_end_ = on;
        if (window.canvas_ != nullptr) {
          window.canvas_->set_brush_smoothing_catch_up_end(on);
        }
      });
  brush_smoothing_zoom_adjust_action_ = add_smoothing_option(
      QT_TR_NOOP("Adjust for Zoom"), current_brush_smoothing_zoom_adjust_,
      [](MainWindow& window, bool on) {
        window.current_brush_smoothing_zoom_adjust_ = on;
        if (window.canvas_ != nullptr) {
          window.canvas_->set_brush_smoothing_zoom_adjust(on);
        }
      });
  brush_smoothing_options_button_->setMenu(smoothing_menu);

  // Photoshop's "Useful Mixer Brush Combinations" dropdown: each entry is one
  // Wet/Load/Mix triple from the verified kCombinations table above. Selection
  // is derived from the current spin values and never persisted separately.
  mixer_combination_combo_ = new QComboBox(toolbar);
  mixer_combination_combo_->setObjectName(QStringLiteral("mixerCombinationCombo"));
  mixer_combination_combo_->addItem(tr("Custom"));
  for (const auto& combination : mixer_useful_combinations()) {
    mixer_combination_combo_->addItem(tr(combination.name));
  }
  // Compact closed width (the popup still shows full names): the Options bar
  // must hold the whole mixer row in one line or the bar height changes on
  // tool switch (ui_brush_tip_picker_keeps_options_bar_height).
  mixer_combination_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  mixer_combination_combo_->setMinimumContentsLength(7);
  widen_combo_popup_to_items(mixer_combination_combo_);
  bind_tooltip(mixer_combination_combo_, QT_TR_NOOP("Useful mixer brush combinations"));
  add_option_widget(mixer_combination_combo_, {CanvasTool::MixerBrush});

  const auto add_mixer_percentage = [this, toolbar, add_option_label, add_option_widget](
                                        const char* label_source, const char* object_name,
                                        int minimum, int value, auto setter) {
    auto* label = add_option_label(label_source, {CanvasTool::MixerBrush});
    bind_widget_text(label, label_source);
    auto* spin = new QSpinBox(toolbar);
    spin->setObjectName(QString::fromLatin1(object_name));
    spin->setRange(minimum, 100);
    spin->setValue(value);
    spin->setSuffix(percent_suffix());
    configure_toolbar_spinbox(spin, 60);
    add_option_widget(spin, {CanvasTool::MixerBrush});
    connect(spin, &QSpinBox::valueChanged, this, [this, setter](int new_value) {
      setter(*this, new_value);
      schedule_save_tool_settings();
      refresh_document_info();
    });
    return spin;
  };
  auto* mixer_wet_spin =
      add_mixer_percentage(QT_TR_NOOP("Wet:"), "mixerWetSpin", 0, current_mixer_wet_,
                           [](MainWindow& window, int value) {
                             window.current_mixer_wet_ = value;
                             if (window.canvas_ != nullptr) {
                               window.canvas_->set_mixer_wet(value);
                             }
                           });
  auto* mixer_load_spin =
      add_mixer_percentage(QT_TR_NOOP("Load:"), "mixerLoadSpin", 1, current_mixer_load_,
                           [](MainWindow& window, int value) {
                             window.current_mixer_load_ = value;
                             if (window.canvas_ != nullptr) {
                               window.canvas_->set_mixer_load(value);
                             }
                           });
  auto* mixer_mix_spin =
      add_mixer_percentage(QT_TR_NOOP("Mix:"), "mixerMixSpin", 0, current_mixer_mix_,
                           [](MainWindow& window, int value) {
                             window.current_mixer_mix_ = value;
                             if (window.canvas_ != nullptr) {
                               window.canvas_->set_mixer_mix(value);
                             }
                           });
  // Wet 0 is a dry brush with no pickup, so Mix has nothing to blend;
  // Photoshop greys the control out the same way.
  mixer_mix_spin->setEnabled(current_mixer_wet_ > 0);
  connect(mixer_wet_spin, &QSpinBox::valueChanged, mixer_mix_spin,
          [mixer_mix_spin](int wet) { mixer_mix_spin->setEnabled(wet > 0); });

  const auto apply_mixer_combination = [mixer_wet_spin, mixer_load_spin, mixer_mix_spin](
                                           int combo_index) {
    if (combo_index <= 0) {
      return;  // Custom
    }
    const auto& combinations = mixer_useful_combinations();
    const auto table_index = static_cast<std::size_t>(combo_index - 1);
    if (table_index >= combinations.size()) {
      return;
    }
    const auto& combination = combinations[table_index];
    mixer_wet_spin->setValue(combination.wet);
    mixer_load_spin->setValue(combination.load);
    if (combination.mix >= 0) {
      mixer_mix_spin->setValue(combination.mix);
    }
  };
  // currentIndexChanged (not activated) so programmatic selection also applies;
  // sync_mixer_combination_combo() sets the index under a QSignalBlocker, so
  // derive-and-set never loops back through this handler.
  connect(mixer_combination_combo_, &QComboBox::currentIndexChanged, this, apply_mixer_combination);
  const auto resync_combination = [this] { sync_mixer_combination_combo(); };
  connect(mixer_wet_spin, &QSpinBox::valueChanged, this, resync_combination);
  connect(mixer_load_spin, &QSpinBox::valueChanged, this, resync_combination);
  connect(mixer_mix_spin, &QSpinBox::valueChanged, this, resync_combination);
  sync_mixer_combination_combo();

  add_mixer_percentage(QT_TR_NOOP("Flow:"), "mixerFlowSpin", 1, current_mixer_flow_,
                       [](MainWindow& window, int value) {
                         window.current_mixer_flow_ = value;
                         if (window.canvas_ != nullptr) {
                           window.canvas_->set_mixer_flow(value);
                         }
                       });

  mixer_sample_all_layers_check_ = new CheckGlyphBox(tr("Sample All Layers"), toolbar);
  mixer_sample_all_layers_check_->setObjectName(QStringLiteral("mixerSampleAllLayersCheck"));
  mixer_sample_all_layers_check_->setChecked(canvas_defaults->mixer_sample_all_layers());
  bind_tooltip(mixer_sample_all_layers_check_, QT_TR_NOOP("Sample the merged document instead of the active layer"));
  add_option_widget(mixer_sample_all_layers_check_, {CanvasTool::MixerBrush});
  connect(mixer_sample_all_layers_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_mixer_sample_all_layers(checked);
      save_tool_settings();
    }
  });

  // Smoothing lands after the mixer cluster so every row that shows it reads
  // Photoshop-style: ..., Flow, (Sample All Layers,) Smooth, gear.
  auto* brush_smoothing_label = add_option_label(QT_TR_NOOP("Smooth:"), smoothing_tools);
  bind_widget_text(brush_smoothing_label, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Smooth:"));
  add_option_widget(brush_smoothing, smoothing_tools);
  add_option_widget(brush_smoothing_options_button_, smoothing_tools);
}

void MainWindow::build_brush_symmetry_and_tip_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  const auto smoothing_tools = tools_with(kToolSmoothing);
  // build_brush_size_options made the softness pair; its connections follow
  // the symmetry button, in their historical order.
  auto* brush_softness = brush_controls_.softness;
  auto* brush_softness_slider = brush_controls_.softness_slider;

  // Paint Symmetry: Photoshop's butterfly button closes the Brush, Mixer
  // Brush, and Eraser rows. Its menu picks the symmetry, moves or rotates its
  // axes on the canvas, and hides the guide (docs/paint-symmetry.md).
  paint_symmetry_button_ = new QToolButton(toolbar);
  paint_symmetry_button_->setObjectName(QStringLiteral("paintSymmetryButton"));
  paint_symmetry_button_->setIcon(paint_symmetry_icon());
  paint_symmetry_button_->setIconSize(QSize(16, 16));
  paint_symmetry_button_->setProperty("optionsBarMenuButton", true);
  paint_symmetry_button_->setFocusPolicy(Qt::NoFocus);
  bind_tooltip(paint_symmetry_button_, QT_TR_NOOP("Paint symmetry"));
  paint_symmetry_button_->setPopupMode(QToolButton::InstantPopup);
  {
    auto* menu = new QMenu(paint_symmetry_button_);
    menu->setObjectName(QStringLiteral("paintSymmetryMenu"));
    auto* group = new QActionGroup(menu);
    group->setExclusive(true);
    struct ModeEntry {
      patchy::PaintSymmetryMode mode;
      const char* source;
    };
    const ModeEntry entries[] = {
        {patchy::PaintSymmetryMode::Off, QT_TR_NOOP("Symmetry Off")},
        {patchy::PaintSymmetryMode::Vertical, QT_TR_NOOP("Vertical")},
        {patchy::PaintSymmetryMode::Horizontal, QT_TR_NOOP("Horizontal")},
        {patchy::PaintSymmetryMode::DualAxis, QT_TR_NOOP("Dual Axis")},
        {patchy::PaintSymmetryMode::Diagonal, QT_TR_NOOP("Diagonal")},
        {patchy::PaintSymmetryMode::Radial, QT_TR_NOOP("Radial...")},
        {patchy::PaintSymmetryMode::Mandala, QT_TR_NOOP("Mandala...")},
    };
    for (const auto& entry : entries) {
      auto* action = menu->addAction(tr(entry.source));
      bind_action_text(action, entry.source);
      action->setCheckable(true);
      group->addAction(action);
      paint_symmetry_mode_actions_[static_cast<std::size_t>(entry.mode)] = action;
      const auto mode = entry.mode;
      connect(action, &QAction::triggered, this, [this, mode] {
        if (patchy::paint_symmetry_uses_segments(mode)) {
          request_paint_symmetry_segments(mode);
        } else {
          set_paint_symmetry(mode, current_paint_symmetry_segments_);
        }
      });
      if (entry.mode == patchy::PaintSymmetryMode::Off) {
        menu->addSeparator();
      }
    }
    menu->addSeparator();
    paint_symmetry_transform_action_ = menu->addAction(tr("Transform Symmetry"));
    bind_action_text(paint_symmetry_transform_action_, QT_TR_NOOP("Transform Symmetry"));
    connect(paint_symmetry_transform_action_, &QAction::triggered, this, [this] {
      if (canvas_ == nullptr) {
        return;
      }
      canvas_->begin_paint_symmetry_transform();
      canvas_->setFocus(Qt::OtherFocusReason);
      statusBar()->showMessage(tr("Drag the center to move the symmetry or drag elsewhere to rotate it (Shift snaps). Enter applies, Esc cancels."));
    });
    paint_symmetry_reset_action_ = menu->addAction(tr("Reset Symmetry"));
    bind_action_text(paint_symmetry_reset_action_, QT_TR_NOOP("Reset Symmetry"));
    connect(paint_symmetry_reset_action_, &QAction::triggered, this, [this] {
      if (canvas_ != nullptr) {
        canvas_->reset_paint_symmetry_placement();
      }
    });
    paint_symmetry_hide_action_ = menu->addAction(tr("Hide Symmetry"));
    bind_action_text(paint_symmetry_hide_action_, QT_TR_NOOP("Hide Symmetry"));
    paint_symmetry_hide_action_->setCheckable(true);
    connect(paint_symmetry_hide_action_, &QAction::toggled, this, [this](bool hidden) {
      current_paint_symmetry_visible_ = !hidden;
      apply_paint_symmetry_to_canvas(canvas_);
    });
    paint_symmetry_button_->setMenu(menu);
  }
  add_option_widget(paint_symmetry_button_, smoothing_tools);
  sync_paint_symmetry_controls();

  connect(brush_softness, &QSpinBox::valueChanged, brush_softness_slider, &QSlider::setValue);
  connect(brush_softness_slider, &QSlider::valueChanged, brush_softness, &QSpinBox::setValue);
  connect(brush_softness, &QSpinBox::valueChanged, this, [this](int value) {
    BrushEdit edit;
    edit.softness = value;
    edit_brush_option(edit);
  });
  connect(brush_preset_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (brush_preset_combo_ == nullptr || canvas_ == nullptr || index < 0) {
      return;
    }
    const auto preset_id = brush_preset_combo_->itemData(index).toString();
    if (preset_id.startsWith("__")) {
      const QSignalBlocker block(brush_preset_combo_);
      brush_preset_combo_->setCurrentIndex(brush_preset_combo_->findData(brush_preset_combo_->property("lastBrushPresetId")));
    } else {
      brush_preset_combo_->setProperty("lastBrushPresetId", preset_id);
    }
    if (preset_id == "__saveBrush") save_current_automation_brush();
    else if (preset_id == "__manageBrushes") manage_automation_brush_presets();
    else if (!preset_id.isEmpty()) pick_brush_preset(preset_id);
  });

  add_option_label(QT_TR_NOOP("Tip:"), tools_with(kToolBrushTip));
  (void)brush_automation_library();
  refresh_automation_brush_presets();
  register_retranslation([this] { refresh_automation_brush_presets(); });
  brush_tip_picker_ = new BrushTipPicker(brush_tip_library(), toolbar);
  brush_tip_picker_->set_current_tip_id(current_brush().brush().tip_id);
  add_option_widget(brush_tip_picker_, tools_with(kToolBrushTip));
  connect(brush_tip_picker_, &BrushTipPicker::tip_selected, this,
          [this](const QString& id) { set_active_brush_tip(id, true); });
  connect(brush_tip_picker_, &BrushTipPicker::import_requested, this,
          [this] { import_brush_tips_from_abr(); });
  connect(brush_tip_picker_, &BrushTipPicker::define_requested, this,
          [this] { define_brush_tip_from_selection(); });
  connect(brush_tip_picker_, &BrushTipPicker::manage_requested, this, [this] { open_brush_tip_manager(); });
  connect(&brush_tip_library(), &BrushTipLibrary::changed, this, [this] {
    // A removed tip must not stay active. A library edit of a tip still in use (its stored
    // dynamics, spacing, or a rewritten mask) never resets the working brush's own settings;
    // only the tip pixels and spacing are re-resolved.
    const auto& brush = current_brush().brush();
    if (brush.snapshot_tip == nullptr && !is_builtin_brush_tip_id(brush.tip_id) &&
        brush_tip_library().tip(brush.tip_id) == nullptr) {
      set_active_brush_tip(builtin_round_brush_tip_id(), false, false);
      return;
    }
    push_current_brush_to_canvas(canvas_, CurrentBrush::Tip);
  });
  QPointer<BrushTipPicker> tip_picker(brush_tip_picker_);
  register_retranslation([tip_picker] {
    if (tip_picker != nullptr) {
      tip_picker->refresh();
    }
  });

  brush_dynamics_button_ = new BrushDynamicsButton(toolbar);
  // Shown for the tools whose strokes apply dynamics (brush_settings_sections).
  options_flow->addWidget(brush_dynamics_button_);
  register_option_action(brush_dynamics_button_, tools_honoring(BrushSection::ShapeDynamics));
  connect(brush_dynamics_button_, &BrushDynamicsButton::show_settings_requested, this,
          [this] { show_brush_settings_panel(); });
  {
    const auto& brush = current_brush().brush();
    brush_dynamics_button_->set_working_brush(working_brush_tip_key(brush), brush.dynamics,
                                              brush.angle, brush.roundness);
  }
  QPointer<BrushDynamicsButton> dynamics_button(brush_dynamics_button_);
  register_retranslation([dynamics_button] {
    if (dynamics_button != nullptr) {
      dynamics_button->retranslate();
    }
  });
}

void MainWindow::build_gradient_tool_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  add_option_label(QT_TR_NOOP("Method:"), {CanvasTool::Gradient});
  gradient_method_combo_ = new QComboBox(toolbar);
  gradient_method_combo_->setObjectName(QStringLiteral("gradientMethodCombo"));
  gradient_method_combo_->addItem(tr("Linear"), static_cast<int>(GradientMethod::Linear));
  gradient_method_combo_->addItem(tr("Radial"), static_cast<int>(GradientMethod::Radial));
  gradient_method_combo_->setFixedWidth(86);
  add_option_widget(gradient_method_combo_, {CanvasTool::Gradient});
  QPointer<QComboBox> gradient_method_combo(gradient_method_combo_);
  register_retranslation([gradient_method_combo] {
    if (gradient_method_combo == nullptr || gradient_method_combo->count() < 2) {
      return;
    }
    const QSignalBlocker blocker(gradient_method_combo);
    gradient_method_combo->setItemText(0, QCoreApplication::translate(kMainWindowTranslationContext, "Linear"));
    gradient_method_combo->setItemText(1, QCoreApplication::translate(kMainWindowTranslationContext, "Radial"));
  });

  add_option_label(QT_TR_NOOP("Opacity:"), {CanvasTool::Gradient});
  gradient_opacity_spin_ = new QSpinBox(toolbar);
  gradient_opacity_spin_->setObjectName(QStringLiteral("gradientOpacitySpin"));
  gradient_opacity_spin_->setRange(0, 100);
  gradient_opacity_spin_->setValue(canvas_defaults->gradient_opacity());
  gradient_opacity_spin_->setSuffix(percent_suffix());
  configure_toolbar_spinbox(gradient_opacity_spin_, 52);
  add_option_widget(gradient_opacity_spin_, {CanvasTool::Gradient});
  gradient_opacity_slider_ = new QSlider(Qt::Horizontal, toolbar);
  gradient_opacity_slider_->setObjectName(QStringLiteral("gradientOpacitySlider"));
  gradient_opacity_slider_->setRange(0, 100);
  gradient_opacity_slider_->setValue(canvas_defaults->gradient_opacity());
  gradient_opacity_slider_->setFixedWidth(110);
  bind_tooltip(gradient_opacity_slider_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Gradient opacity"));
  add_option_widget(gradient_opacity_slider_, {CanvasTool::Gradient});
  gradient_reverse_check_ = new CheckGlyphBox(tr("Reverse"), toolbar);
  gradient_reverse_check_->setObjectName(QStringLiteral("gradientReverseCheck"));
  gradient_reverse_check_->setChecked(canvas_defaults->gradient_reverse());
  add_option_widget(gradient_reverse_check_, {CanvasTool::Gradient});
  gradient_preview_button_ = new QPushButton(toolbar);
  gradient_preview_button_->setObjectName(QStringLiteral("gradientPreviewButton"));
  bind_tooltip(gradient_preview_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Gradient preview"));
  add_option_widget(gradient_preview_button_, {CanvasTool::Gradient});
  gradient_presets_button_ = new QPushButton(toolbar);
  gradient_presets_button_->setObjectName(QStringLiteral("gradientPresetsButton"));
  gradient_presets_button_->setText(tr("Presets"));
  bind_widget_text(gradient_presets_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Presets"));
  bind_tooltip(gradient_presets_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Choose a gradient preset"));
  add_option_widget(gradient_presets_button_, {CanvasTool::Gradient});
  gradient_edit_stops_button_ = new QPushButton(tr("Edit Stops..."), toolbar);
  gradient_edit_stops_button_->setObjectName(QStringLiteral("gradientEditStopsButton"));
  add_option_widget(gradient_edit_stops_button_, {CanvasTool::Gradient});
  refresh_gradient_controls_from_canvas();
  connect(gradient_method_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (canvas_ == nullptr || gradient_method_combo_ == nullptr || index < 0) {
      return;
    }
    canvas_->set_gradient_method(static_cast<GradientMethod>(gradient_method_combo_->itemData(index).toInt()));
    save_tool_settings();
    refresh_document_info();
  });
  connect(gradient_opacity_spin_, &QSpinBox::valueChanged, gradient_opacity_slider_, &QSlider::setValue);
  connect(gradient_opacity_slider_, &QSlider::valueChanged, gradient_opacity_spin_, &QSpinBox::setValue);
  connect(gradient_opacity_spin_, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_gradient_opacity(value);
      refresh_gradient_controls_from_canvas();
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });
  connect(gradient_reverse_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_gradient_reverse(checked);
      refresh_gradient_controls_from_canvas();
      save_tool_settings();
      refresh_document_info();
    }
  });
  connect(gradient_preview_button_, &QPushButton::clicked, this, [this] { edit_gradient_stops(); });
  connect(gradient_presets_button_, &QPushButton::clicked, this, [this] { choose_gradient_preset(); });
  connect(gradient_edit_stops_button_, &QPushButton::clicked, this, [this] { edit_gradient_stops(); });
}

void MainWindow::build_fill_tool_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // Fill tool / Fill hotkey settings (independent of the brush; default 100% opacity, 0 softness).
  add_option_label(QT_TR_NOOP("Opacity:"), {CanvasTool::Fill});
  auto* fill_opacity = new QSpinBox(toolbar);
  fill_opacity->setObjectName(QStringLiteral("fillOpacitySpin"));
  fill_opacity->setRange(1, 100);
  fill_opacity->setValue(canvas_defaults->fill_opacity());
  fill_opacity->setSuffix(percent_suffix());
  configure_toolbar_spinbox(fill_opacity, 52);
  add_option_widget(fill_opacity, {CanvasTool::Fill});
  auto* fill_opacity_slider = new QSlider(Qt::Horizontal, toolbar);
  fill_opacity_slider->setObjectName(QStringLiteral("fillOpacitySlider"));
  fill_opacity_slider->setRange(1, 100);
  fill_opacity_slider->setValue(canvas_defaults->fill_opacity());
  fill_opacity_slider->setFixedWidth(120);
  bind_tooltip(fill_opacity_slider, QT_TR_NOOP("Fill opacity for the Fill tool and Fill shortcut"));
  add_option_widget(fill_opacity_slider, {CanvasTool::Fill});
  add_option_label(QT_TR_NOOP("Soft:"), {CanvasTool::Fill});
  auto* fill_softness = new QSpinBox(toolbar);
  fill_softness->setObjectName(QStringLiteral("fillSoftnessSpin"));
  fill_softness->setRange(0, 100);
  fill_softness->setValue(canvas_defaults->fill_softness());
  fill_softness->setSuffix(percent_suffix());
  configure_toolbar_spinbox(fill_softness, 52);
  add_option_widget(fill_softness, {CanvasTool::Fill});
  auto* fill_softness_slider = new QSlider(Qt::Horizontal, toolbar);
  fill_softness_slider->setObjectName(QStringLiteral("fillSoftnessSlider"));
  fill_softness_slider->setRange(0, 100);
  fill_softness_slider->setValue(canvas_defaults->fill_softness());
  fill_softness_slider->setFixedWidth(110);
  bind_tooltip(fill_softness_slider, QT_TR_NOOP("Soft edge feather for the Fill tool and Fill shortcut"));
  add_option_widget(fill_softness_slider, {CanvasTool::Fill});
  connect(fill_opacity, &QSpinBox::valueChanged, fill_opacity_slider, &QSlider::setValue);
  connect(fill_opacity_slider, &QSlider::valueChanged, fill_opacity, &QSpinBox::setValue);
  connect(fill_opacity, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_fill_opacity(value);
      schedule_save_tool_settings();
    }
  });
  connect(fill_softness, &QSpinBox::valueChanged, fill_softness_slider, &QSlider::setValue);
  connect(fill_softness_slider, &QSlider::valueChanged, fill_softness, &QSpinBox::setValue);
  connect(fill_softness, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_fill_softness(value);
      schedule_save_tool_settings();
    }
  });
  // Tolerance and Contiguous belong to the Fill tool's flood only (the Fill command fills the
  // whole selection). Same metric and range as the Magic Wand's Tol.
  add_option_label(QT_TR_NOOP("Tol:"), {CanvasTool::Fill});
  auto* fill_tolerance = new QSpinBox(toolbar);
  fill_tolerance->setObjectName(QStringLiteral("fillToleranceSpin"));
  fill_tolerance->setRange(0, 255);
  fill_tolerance->setValue(canvas_defaults->fill_tolerance());
  configure_toolbar_spinbox(fill_tolerance, 46);
  bind_tooltip(fill_tolerance,
               QT_TR_NOOP("How far a pixel's color may differ from the clicked color and still be filled"));
  add_option_widget(fill_tolerance, {CanvasTool::Fill});
  connect(fill_tolerance, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_fill_tolerance(value);
      schedule_save_tool_settings();
    }
  });
  fill_contiguous_check_ = new CheckGlyphBox(tr("Contiguous"), toolbar);
  fill_contiguous_check_->setObjectName(QStringLiteral("fillContiguousCheck"));
  fill_contiguous_check_->setChecked(canvas_defaults->fill_contiguous());
  bind_tooltip(fill_contiguous_check_, QT_TR_NOOP("Limit the fill to pixels connected to the click"));
  add_option_widget(fill_contiguous_check_, {CanvasTool::Fill});
  connect(fill_contiguous_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_fill_contiguous(checked);
      save_tool_settings();
    }
  });
}

}  // namespace patchy::ui
