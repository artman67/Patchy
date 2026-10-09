// The Options-bar rows for the retouching tools: Pattern Stamp, Clone/Healing/
// Patch with their Sample All Layers, the toning tools (Dodge, Burn, Sponge,
// Blur, Sharpen), and Color Replacement.
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

void MainWindow::build_pattern_stamp_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  auto* pattern_label = add_option_label(QT_TR_NOOP("Pattern:"), {CanvasTool::PatternStamp});
  bind_widget_text(pattern_label, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pattern:"));
  pattern_stamp_pattern_combo_ = new QComboBox(toolbar);
  pattern_stamp_pattern_combo_->setObjectName(QStringLiteral("patternStampPatternCombo"));
  pattern_stamp_pattern_combo_->setIconSize(QSize(18, 18));
  pattern_stamp_pattern_combo_->setMinimumWidth(150);
  register_retranslation([this] { refresh_pattern_stamp_pattern_combo(); });
  add_option_widget(pattern_stamp_pattern_combo_, {CanvasTool::PatternStamp});
  connect(pattern_stamp_pattern_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (pattern_stamp_pattern_combo_ == nullptr || index < 0) {
      return;
    }
    current_pattern_stamp_pattern_id_ = pattern_stamp_pattern_combo_->itemData(index).toString();
    apply_pattern_stamp_settings_to_canvas(canvas_);
    schedule_save_tool_settings();
  });

  auto* manage_patterns = new QPushButton(toolbar);
  manage_patterns->setObjectName(QStringLiteral("patternStampManageButton"));
  manage_patterns->setText(tr("Manage..."));
  bind_widget_text(manage_patterns, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Manage..."));
  bind_tooltip(manage_patterns, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Import or manage patterns"));
  add_option_widget(manage_patterns, {CanvasTool::PatternStamp});
  connect(manage_patterns, &QPushButton::clicked, this, [this] {
    const auto selected_storage_id =
        request_pattern_manager(this, pattern_library(), current_pattern_stamp_pattern_id_);
    if (selected_storage_id.isEmpty()) {
      return;
    }
    if (const auto* entry = pattern_library().find_entry(selected_storage_id); entry != nullptr) {
      current_pattern_stamp_pattern_id_ = entry->id;
      refresh_pattern_stamp_pattern_combo();
      apply_pattern_stamp_settings_to_canvas(canvas_);
      save_tool_settings();
    }
  });

  pattern_stamp_aligned_check_ = new CheckGlyphBox(tr("Aligned"), toolbar);

  bind_widget_text(pattern_stamp_aligned_check_, QT_TR_NOOP("Aligned"));
  pattern_stamp_aligned_check_->setObjectName(QStringLiteral("patternStampAlignedCheck"));
  pattern_stamp_aligned_check_->setChecked(current_pattern_stamp_aligned_);
  bind_tooltip(pattern_stamp_aligned_check_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Keep pattern alignment continuous across strokes"));
  add_option_widget(pattern_stamp_aligned_check_, {CanvasTool::PatternStamp});
  connect(pattern_stamp_aligned_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_pattern_stamp_aligned_ = checked;
    apply_pattern_stamp_settings_to_canvas(canvas_);
    save_tool_settings();
  });
}

void MainWindow::build_retouch_tool_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  clone_aligned_check_ = new CheckGlyphBox(tr("Aligned"), toolbar);
  clone_aligned_check_->setObjectName(QStringLiteral("cloneAlignedCheck"));
  clone_aligned_check_->setChecked(canvas_defaults->clone_aligned());
  bind_tooltip(clone_aligned_check_, QT_TR_NOOP("Keep sample source offset aligned across strokes"));
  add_option_widget(clone_aligned_check_, {CanvasTool::Clone, CanvasTool::Healing});
  connect(clone_aligned_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_clone_aligned(checked);
      save_tool_settings();
    }
  });

  add_option_label(QT_TR_NOOP("Diffusion:"), {CanvasTool::Healing});
  auto* healing_diffusion = new QSpinBox(toolbar);
  healing_diffusion->setObjectName(QStringLiteral("healingDiffusionSpin"));
  healing_diffusion->setRange(1, 7);
  healing_diffusion->setValue(current_healing_diffusion_);
  bind_tooltip(healing_diffusion, QT_TR_NOOP("Lower values preserve fine texture; higher values adapt more quickly"));
  configure_toolbar_spinbox(healing_diffusion, 42);
  add_option_widget(healing_diffusion, {CanvasTool::Healing});
  connect(healing_diffusion, &QSpinBox::valueChanged, this, [this](int value) {
    current_healing_diffusion_ = value;
    if (canvas_ != nullptr) {
      canvas_->set_healing_diffusion(value);
      save_tool_settings();
    }
  });

  add_option_label(QT_TR_NOOP("Patch:"), {CanvasTool::PatchTool});
  patch_mode_combo_ = new QComboBox(toolbar);
  patch_mode_combo_->setObjectName(QStringLiteral("patchModeCombo"));
  patch_mode_combo_->addItem(tr("Source"), static_cast<int>(CanvasWidget::PatchToolMode::Source));
  patch_mode_combo_->addItem(tr("Destination"), static_cast<int>(CanvasWidget::PatchToolMode::Destination));
  patch_mode_combo_->setCurrentIndex(std::max(
      0, patch_mode_combo_->findData(static_cast<int>(canvas_defaults->patch_tool_mode()))));
  patch_mode_combo_->setFixedWidth(104);
  bind_tooltip(patch_mode_combo_, QT_TR_NOOP("Source heals the dragged-from region; Destination copies it onto the drop point"));
  {
    QPointer<QComboBox> patch_mode_combo(patch_mode_combo_);
    register_retranslation([patch_mode_combo] {
      if (patch_mode_combo == nullptr || patch_mode_combo->count() < 2) {
        return;
      }
      QSignalBlocker blocker(patch_mode_combo);
      patch_mode_combo->setItemText(0, QObject::tr("Source"));
      patch_mode_combo->setItemText(1, QObject::tr("Destination"));
    });
  }
  add_option_widget(patch_mode_combo_, {CanvasTool::PatchTool});
  connect(patch_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index < 0 || patch_mode_combo_ == nullptr) {
      return;
    }
    if (canvas_ != nullptr) {
      canvas_->set_patch_tool_mode(
          static_cast<CanvasWidget::PatchToolMode>(patch_mode_combo_->itemData(index).toInt()));
      save_tool_settings();
    }
  });

  patch_transparent_check_ = new CheckGlyphBox(tr("Transparent"), toolbar);
  patch_transparent_check_->setObjectName(QStringLiteral("patchTransparentCheck"));
  patch_transparent_check_->setChecked(canvas_defaults->patch_tool_transparent());
  bind_tooltip(patch_transparent_check_, QT_TR_NOOP("Keep the region and add only the sampled texture instead of replacing it; clearest when the "
         "source has distinct marks over a plain background"));
  add_option_widget(patch_transparent_check_, {CanvasTool::PatchTool});
  connect(patch_transparent_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_patch_tool_transparent(checked);
      save_tool_settings();
    }
  });

  // Patch tool: Remove Object heals the drawn selection automatically, the
  // same command as Edit > Remove Object and the Enter key.
  patch_remove_object_button_ = new QPushButton(tr("Remove Object"), toolbar);
  patch_remove_object_button_->setObjectName(QStringLiteral("patchRemoveObjectButton"));
  bind_widget_text(patch_remove_object_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Remove Object"));
  bind_tooltip(patch_remove_object_button_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                                 "Fill the selection from its surroundings with the content-aware search (Enter)"));
  add_option_widget(patch_remove_object_button_, {CanvasTool::PatchTool});
  connect(patch_remove_object_button_, &QPushButton::clicked, this, [this] {
    if (remove_object_action_ != nullptr) {
      remove_object_action_->trigger();
    }
  });

  retouch_sample_all_layers_check_ = new CheckGlyphBox(tr("Sample All Layers"), toolbar);
  retouch_sample_all_layers_check_->setObjectName(QStringLiteral("retouchSampleAllLayersCheck"));
  retouch_sample_all_layers_check_->setChecked(canvas_defaults->retouch_sample_all_layers());
  bind_tooltip(retouch_sample_all_layers_check_, QT_TR_NOOP("Sample the merged document instead of the active layer"));
  add_option_widget(retouch_sample_all_layers_check_,
                    {CanvasTool::Clone, CanvasTool::Healing, CanvasTool::SpotHealing, CanvasTool::PatchTool});
  connect(retouch_sample_all_layers_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_retouch_sample_all_layers(checked);
      save_tool_settings();
    }
  });
}

void MainWindow::build_toning_tool_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  add_option_label(QT_TR_NOOP("Strength:"), {CanvasTool::Dodge, CanvasTool::Burn, CanvasTool::Sponge,
                                      CanvasTool::BlurBrush, CanvasTool::SharpenBrush});
  local_adjustment_strength_spin_ = new QSpinBox(toolbar);
  local_adjustment_strength_spin_->setObjectName(QStringLiteral("localAdjustmentStrengthSpin"));
  local_adjustment_strength_spin_->setRange(1, 100);
  local_adjustment_strength_spin_->setValue(current_local_adjustment_strength_);
  local_adjustment_strength_spin_->setSuffix(percent_suffix());
  bind_tooltip(local_adjustment_strength_spin_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Maximum adjustment applied during one stroke"));
  configure_toolbar_spinbox(local_adjustment_strength_spin_, 52);
  add_option_widget(local_adjustment_strength_spin_,
                    {CanvasTool::Dodge, CanvasTool::Burn, CanvasTool::Sponge,
                     CanvasTool::BlurBrush, CanvasTool::SharpenBrush});
  connect(local_adjustment_strength_spin_, &QSpinBox::valueChanged, this, [this](int value) {
    current_local_adjustment_strength_ = value;
    if (canvas_ != nullptr) {
      canvas_->set_local_adjustment_strength(value);
    }
    schedule_save_tool_settings();
    refresh_document_info();
  });

  add_option_label(QT_TR_NOOP("Range:"), {CanvasTool::Dodge, CanvasTool::Burn});
  local_tone_range_combo_ = new QComboBox(toolbar);
  local_tone_range_combo_->setObjectName(QStringLiteral("localToneRangeCombo"));
  local_tone_range_combo_->addItem(tr("Shadows"), static_cast<int>(CanvasWidget::LocalToneRange::Shadows));
  local_tone_range_combo_->addItem(tr("Midtones"), static_cast<int>(CanvasWidget::LocalToneRange::Midtones));
  local_tone_range_combo_->addItem(tr("Highlights"), static_cast<int>(CanvasWidget::LocalToneRange::Highlights));
  local_tone_range_combo_->setCurrentIndex(
      std::max(0, local_tone_range_combo_->findData(static_cast<int>(current_local_tone_range_))));
  local_tone_range_combo_->setFixedWidth(92);
  add_option_widget(local_tone_range_combo_, {CanvasTool::Dodge, CanvasTool::Burn});
  connect(local_tone_range_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index < 0 || local_tone_range_combo_ == nullptr) {
      return;
    }
    current_local_tone_range_ =
        static_cast<CanvasWidget::LocalToneRange>(local_tone_range_combo_->itemData(index).toInt());
    if (canvas_ != nullptr) {
      canvas_->set_local_tone_range(current_local_tone_range_);
    }
    save_tool_settings();
    refresh_document_info();
  });
  QPointer<QComboBox> local_tone_range_combo(local_tone_range_combo_);
  register_retranslation([local_tone_range_combo] {
    if (local_tone_range_combo == nullptr || local_tone_range_combo->count() < 3) {
      return;
    }
    const QSignalBlocker blocker(local_tone_range_combo);
    local_tone_range_combo->setItemText(
        0, QCoreApplication::translate(kMainWindowTranslationContext, "Shadows"));
    local_tone_range_combo->setItemText(
        1, QCoreApplication::translate(kMainWindowTranslationContext, "Midtones"));
    local_tone_range_combo->setItemText(
        2, QCoreApplication::translate(kMainWindowTranslationContext, "Highlights"));
  });

  local_protect_tones_check_ = new CheckGlyphBox(tr("Protect Tones"), toolbar);
  local_protect_tones_check_->setObjectName(QStringLiteral("localProtectTonesCheck"));
  local_protect_tones_check_->setChecked(current_local_protect_tones_);
  bind_tooltip(local_protect_tones_check_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Preserve local color differences while lightening or darkening"));
  bind_widget_text(local_protect_tones_check_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Protect Tones"));
  add_option_widget(local_protect_tones_check_, {CanvasTool::Dodge, CanvasTool::Burn});
  connect(local_protect_tones_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_local_protect_tones_ = checked;
    if (canvas_ != nullptr) {
      canvas_->set_local_protect_tones(checked);
    }
    save_tool_settings();
  });

  add_option_label(QT_TR_NOOP("Mode:"), {CanvasTool::Sponge});
  sponge_mode_combo_ = new QComboBox(toolbar);
  sponge_mode_combo_->setObjectName(QStringLiteral("spongeModeCombo"));
  sponge_mode_combo_->addItem(tr("Saturate"), static_cast<int>(CanvasWidget::SpongeMode::Saturate));
  sponge_mode_combo_->addItem(tr("Desaturate"), static_cast<int>(CanvasWidget::SpongeMode::Desaturate));
  sponge_mode_combo_->setCurrentIndex(
      std::max(0, sponge_mode_combo_->findData(static_cast<int>(current_sponge_mode_))));
  sponge_mode_combo_->setFixedWidth(94);
  add_option_widget(sponge_mode_combo_, {CanvasTool::Sponge});
  connect(sponge_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index < 0 || sponge_mode_combo_ == nullptr) {
      return;
    }
    current_sponge_mode_ =
        static_cast<CanvasWidget::SpongeMode>(sponge_mode_combo_->itemData(index).toInt());
    if (canvas_ != nullptr) {
      canvas_->set_sponge_mode(current_sponge_mode_);
    }
    save_tool_settings();
    refresh_document_info();
  });
  QPointer<QComboBox> sponge_mode_combo(sponge_mode_combo_);
  register_retranslation([sponge_mode_combo] {
    if (sponge_mode_combo == nullptr || sponge_mode_combo->count() < 2) {
      return;
    }
    const QSignalBlocker blocker(sponge_mode_combo);
    sponge_mode_combo->setItemText(
        0, QCoreApplication::translate(kMainWindowTranslationContext, "Saturate"));
    sponge_mode_combo->setItemText(
        1, QCoreApplication::translate(kMainWindowTranslationContext, "Desaturate"));
  });

  sponge_vibrance_check_ = new CheckGlyphBox(tr("Vibrance"), toolbar);
  sponge_vibrance_check_->setObjectName(QStringLiteral("spongeVibranceCheck"));
  sponge_vibrance_check_->setChecked(current_sponge_vibrance_);
  bind_tooltip(sponge_vibrance_check_, QT_TR_NOOP("Reduce the adjustment on colors that are already strongly saturated"));
  bind_tooltip(sponge_vibrance_check_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Reduce the adjustment on colors that are already strongly saturated"));
  bind_widget_text(sponge_vibrance_check_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Vibrance"));
  add_option_widget(sponge_vibrance_check_, {CanvasTool::Sponge});
  connect(sponge_vibrance_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_sponge_vibrance_ = checked;
    if (canvas_ != nullptr) {
      canvas_->set_sponge_vibrance(checked);
    }
    save_tool_settings();
  });
}

void MainWindow::build_color_replacement_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // Color Replacement (docs/color-replacement.md): Photoshop's Mode, Sampling,
  // Limits, Tolerance, and Anti-alias, beside the shared Size/Soft footprint.
  const auto store_color_replacement = [this] {
    if (canvas_ != nullptr) {
      canvas_->set_color_replacement_settings(current_color_replacement_);
    }
    save_tool_settings();
    refresh_document_info();
  };
  add_option_label(QT_TR_NOOP("Mode:"), {CanvasTool::ColorReplacement});
  color_replacement_mode_combo_ = new QComboBox(toolbar);
  color_replacement_mode_combo_->setObjectName(QStringLiteral("colorReplacementModeCombo"));
  color_replacement_mode_combo_->addItem(tr("Hue"), static_cast<int>(ColorReplacementMode::Hue));
  color_replacement_mode_combo_->addItem(tr("Saturation"), static_cast<int>(ColorReplacementMode::Saturation));
  color_replacement_mode_combo_->addItem(tr("Color"), static_cast<int>(ColorReplacementMode::Color));
  color_replacement_mode_combo_->addItem(tr("Luminosity"), static_cast<int>(ColorReplacementMode::Luminosity));
  color_replacement_mode_combo_->setCurrentIndex(std::max(
      0, color_replacement_mode_combo_->findData(static_cast<int>(current_color_replacement_.mode))));
  color_replacement_mode_combo_->setFixedWidth(96);
  add_option_widget(color_replacement_mode_combo_, {CanvasTool::ColorReplacement});
  connect(color_replacement_mode_combo_, &QComboBox::currentIndexChanged, this,
          [this, store_color_replacement](int index) {
            if (index < 0 || color_replacement_mode_combo_ == nullptr) {
              return;
            }
            current_color_replacement_.mode =
                static_cast<ColorReplacementMode>(color_replacement_mode_combo_->itemData(index).toInt());
            store_color_replacement();
          });

  add_option_label(QT_TR_NOOP("Sampling:"), {CanvasTool::ColorReplacement});
  color_replacement_sampling_combo_ = new QComboBox(toolbar);
  color_replacement_sampling_combo_->setObjectName(QStringLiteral("colorReplacementSamplingCombo"));
  color_replacement_sampling_combo_->addItem(tr("Continuous"),
                                             static_cast<int>(ColorReplacementSampling::Continuous));
  color_replacement_sampling_combo_->addItem(tr("Once"), static_cast<int>(ColorReplacementSampling::Once));
  color_replacement_sampling_combo_->addItem(tr("Background Swatch"),
                                             static_cast<int>(ColorReplacementSampling::BackgroundSwatch));
  color_replacement_sampling_combo_->setCurrentIndex(std::max(
      0, color_replacement_sampling_combo_->findData(static_cast<int>(current_color_replacement_.sampling))));
  bind_tooltip(color_replacement_sampling_combo_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Which color the brush replaces: the color under each dab, "
                                 "the color where the stroke starts, or the background color"));
  add_option_widget(color_replacement_sampling_combo_, {CanvasTool::ColorReplacement});
  connect(color_replacement_sampling_combo_, &QComboBox::currentIndexChanged, this,
          [this, store_color_replacement](int index) {
            if (index < 0 || color_replacement_sampling_combo_ == nullptr) {
              return;
            }
            current_color_replacement_.sampling =
                static_cast<ColorReplacementSampling>(color_replacement_sampling_combo_->itemData(index).toInt());
            store_color_replacement();
          });

  add_option_label(QT_TR_NOOP("Limits:"), {CanvasTool::ColorReplacement});
  color_replacement_limits_combo_ = new QComboBox(toolbar);
  color_replacement_limits_combo_->setObjectName(QStringLiteral("colorReplacementLimitsCombo"));
  color_replacement_limits_combo_->addItem(tr("Discontiguous"),
                                           static_cast<int>(ColorReplacementLimits::Discontiguous));
  color_replacement_limits_combo_->addItem(tr("Contiguous"), static_cast<int>(ColorReplacementLimits::Contiguous));
  color_replacement_limits_combo_->addItem(tr("Find Edges"), static_cast<int>(ColorReplacementLimits::FindEdges));
  color_replacement_limits_combo_->setCurrentIndex(std::max(
      0, color_replacement_limits_combo_->findData(static_cast<int>(current_color_replacement_.limits))));
  bind_tooltip(color_replacement_limits_combo_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Discontiguous replaces every match under the brush; "
                                 "Contiguous only matches connected to its center; Find Edges also stops at sharp edges"));
  add_option_widget(color_replacement_limits_combo_, {CanvasTool::ColorReplacement});
  connect(color_replacement_limits_combo_, &QComboBox::currentIndexChanged, this,
          [this, store_color_replacement](int index) {
            if (index < 0 || color_replacement_limits_combo_ == nullptr) {
              return;
            }
            current_color_replacement_.limits =
                static_cast<ColorReplacementLimits>(color_replacement_limits_combo_->itemData(index).toInt());
            store_color_replacement();
          });
  QPointer<QComboBox> color_replacement_mode_combo(color_replacement_mode_combo_);
  QPointer<QComboBox> color_replacement_sampling_combo(color_replacement_sampling_combo_);
  QPointer<QComboBox> color_replacement_limits_combo(color_replacement_limits_combo_);
  register_retranslation([color_replacement_mode_combo, color_replacement_sampling_combo,
                          color_replacement_limits_combo] {
    const auto retitle = [](QComboBox* combo, std::initializer_list<const char*> sources) {
      if (combo == nullptr || combo->count() != static_cast<int>(sources.size())) {
        return;
      }
      const QSignalBlocker blocker(combo);
      int index = 0;
      for (const auto* source : sources) {
        combo->setItemText(index++, QCoreApplication::translate(kMainWindowTranslationContext, source));
      }
    };
    retitle(color_replacement_mode_combo, {"Hue", "Saturation", "Color", "Luminosity"});
    retitle(color_replacement_sampling_combo, {"Continuous", "Once", "Background Swatch"});
    retitle(color_replacement_limits_combo, {"Discontiguous", "Contiguous", "Find Edges"});
  });

  add_option_label(QT_TR_NOOP("Tol:"), {CanvasTool::ColorReplacement});
  color_replacement_tolerance_spin_ = new QSpinBox(toolbar);
  color_replacement_tolerance_spin_->setObjectName(QStringLiteral("colorReplacementToleranceSpin"));
  color_replacement_tolerance_spin_->setRange(1, 100);
  color_replacement_tolerance_spin_->setValue(current_color_replacement_.tolerance);
  color_replacement_tolerance_spin_->setSuffix(percent_suffix());
  configure_toolbar_spinbox(color_replacement_tolerance_spin_, 52);
  bind_tooltip(color_replacement_tolerance_spin_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "How far a pixel's color may differ from the sampled color "
                                 "and still be replaced"));
  add_option_widget(color_replacement_tolerance_spin_, {CanvasTool::ColorReplacement});
  connect(color_replacement_tolerance_spin_, &QSpinBox::valueChanged, this, [this](int value) {
    current_color_replacement_.tolerance = value;
    if (canvas_ != nullptr) {
      canvas_->set_color_replacement_settings(current_color_replacement_);
    }
    schedule_save_tool_settings();
    refresh_document_info();
  });

  color_replacement_anti_alias_check_ = new CheckGlyphBox(tr("Anti-alias"), toolbar);
  color_replacement_anti_alias_check_->setObjectName(QStringLiteral("colorReplacementAntiAliasCheck"));
  color_replacement_anti_alias_check_->setChecked(current_color_replacement_.anti_alias);
  bind_widget_text(color_replacement_anti_alias_check_, QT_TR_NOOP("Anti-alias"));
  bind_tooltip(color_replacement_anti_alias_check_,
               QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Fade the replacement out near the tolerance limit for smooth "
                                 "edges"));
  add_option_widget(color_replacement_anti_alias_check_, {CanvasTool::ColorReplacement});
  connect(color_replacement_anti_alias_check_, &QCheckBox::toggled, this,
          [this, store_color_replacement](bool checked) {
            current_color_replacement_.anti_alias = checked;
            store_color_replacement();
          });
}

}  // namespace patchy::ui
