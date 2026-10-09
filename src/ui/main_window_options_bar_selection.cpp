// The Options-bar rows for the selection tools and Crop: the combine-mode
// buttons, Feather/Anti-alias and the marquee Style fields, the Crop row, and
// the Quick Select, Magnetic Lasso and Magic Wand rows with the bracket-key
// brush-size actions they share with the painting tools.
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

void MainWindow::build_selection_tool_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_separator = bar.add_option_separator;
  const auto& add_option_action = bar.add_option_action;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  const auto& document_axis_context = bar.document_axis_context;

  auto* selection_new = add_option_action(
      simple_icon(QStringLiteral("N")), QT_TR_NOOP("New Selection"),
      {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso, CanvasTool::MagneticLasso,
       CanvasTool::MagicWand, CanvasTool::QuickSelect, CanvasTool::PatchTool});
  selection_new->setObjectName(QStringLiteral("selectionNewModeAction"));
  auto* selection_add = add_option_action(
      simple_icon(QStringLiteral("+")), QT_TR_NOOP("Add to Selection"),
      {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso, CanvasTool::MagneticLasso,
       CanvasTool::MagicWand, CanvasTool::QuickSelect, CanvasTool::PatchTool});
  selection_add->setObjectName(QStringLiteral("selectionAddModeAction"));
  auto* selection_subtract = add_option_action(
      simple_icon(QStringLiteral("-")), QT_TR_NOOP("Subtract from Selection"),
      {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso, CanvasTool::MagneticLasso,
       CanvasTool::MagicWand, CanvasTool::QuickSelect, CanvasTool::PatchTool});
  selection_subtract->setObjectName(QStringLiteral("selectionSubtractModeAction"));
  auto* selection_intersect = add_option_action(simple_icon(QStringLiteral("Ix")), QT_TR_NOOP("Intersect Selection"),
                                                {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso,
                                                 CanvasTool::MagneticLasso, CanvasTool::MagicWand,
                                                 CanvasTool::PatchTool});
  selection_intersect->setObjectName(QStringLiteral("selectionIntersectModeAction"));
  selection_new_mode_action_ = selection_new;
  selection_add_mode_action_ = selection_add;
  selection_subtract_mode_action_ = selection_subtract;
  selection_intersect_mode_action_ = selection_intersect;
  auto* selection_mode_group = new QActionGroup(this);
  selection_mode_group->setExclusive(true);
  const auto configure_selection_mode_action = [selection_mode_group](QAction* action) {
    action->setCheckable(true);
    selection_mode_group->addAction(action);
  };
  configure_selection_mode_action(selection_new);
  configure_selection_mode_action(selection_add);
  configure_selection_mode_action(selection_subtract);
  configure_selection_mode_action(selection_intersect);
  selection_new->setChecked(true);
  const auto set_selection_mode = [this](CanvasWidget::SelectionMode mode) {
    // Each selection tool keeps its own combine mode; store it for the active
    // tool (so new documents inherit it) and apply it to the live canvas.
    if (const auto index = CanvasWidget::selection_tool_index(current_tool_); index >= 0) {
      selection_modes_[static_cast<std::size_t>(index)] = mode;
    }
    if (canvas_ != nullptr) {
      canvas_->set_selection_mode(mode);
    }
    refresh_options_bar();
  };
  connect(selection_new, &QAction::triggered, this,
          [set_selection_mode] { set_selection_mode(CanvasWidget::SelectionMode::Replace); });
  connect(selection_add, &QAction::triggered, this,
          [set_selection_mode] { set_selection_mode(CanvasWidget::SelectionMode::Add); });
  connect(selection_subtract, &QAction::triggered, this,
          [set_selection_mode] { set_selection_mode(CanvasWidget::SelectionMode::Subtract); });
  connect(selection_intersect, &QAction::triggered, this,
          [set_selection_mode] { set_selection_mode(CanvasWidget::SelectionMode::Intersect); });
  add_option_separator({CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso,
                        CanvasTool::MagneticLasso, CanvasTool::MagicWand, CanvasTool::QuickSelect});

  auto* feather_group = new QWidget(toolbar);
  feather_group->setObjectName(QStringLiteral("selectionFeatherGroup"));
  auto* feather_layout = new QHBoxLayout(feather_group);
  feather_layout->setContentsMargins(0, 0, 0, 0);
  feather_layout->setSpacing(0);
  auto* feather_label = new QLabel(tr("Feather:"), feather_group);
  bind_widget_text(feather_label, QT_TR_NOOP("Feather:"));
  feather_label->setAlignment(Qt::AlignCenter);
  feather_layout->addWidget(feather_label);
  auto* feather = new UnitIntSpinBox(SpinUnit::Pixels, feather_group);
  feather->setObjectName(QStringLiteral("selectionFeatherSpin"));
  feather->set_context_provider(document_unit_context_provider(true));  // "2 mm" converts at the document PPI
  feather->setRange(0, kMaxSelectionFeatherRadius);
  feather->setValue(current_selection_feather_radius_);
  feather->setProperty(kToolbarSpinboxSliderCurvedProperty, true);
  configure_toolbar_spinbox(feather, 64);
  feather_layout->addWidget(feather);
  add_option_widget(feather_group, {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso,
                                    CanvasTool::MagneticLasso, CanvasTool::MagicWand, CanvasTool::QuickSelect});
  auto* anti_alias = new CheckGlyphBox(tr("Anti-alias"), toolbar);
  bind_widget_text(anti_alias, QT_TR_NOOP("Anti-alias"));
  anti_alias->setObjectName(QStringLiteral("selectionAntiAliasCheck"));
  anti_alias->setChecked(current_selection_antialias_);
  add_option_widget(anti_alias,
                    {CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso,
                     CanvasTool::MagneticLasso, CanvasTool::MagicWand});
  const auto apply_selection_edge_settings = [this, feather, anti_alias] {
    current_selection_feather_radius_ = feather->value();
    current_selection_antialias_ = anti_alias->isChecked();
    // Each selection tool keeps its own Feather and Anti-alias.
    if (const auto index = CanvasWidget::selection_tool_index(current_tool_); index >= 0) {
      selection_feather_by_tool_[static_cast<std::size_t>(index)] = current_selection_feather_radius_;
      selection_antialias_by_tool_[static_cast<std::size_t>(index)] = current_selection_antialias_;
    }
    if (canvas_ != nullptr) {
      canvas_->set_selection_feather_radius(current_selection_feather_radius_);
      canvas_->set_selection_antialias(current_selection_antialias_);
    }
    refresh_document_info();
  };
  connect(feather, &QSpinBox::valueChanged, this, [this, apply_selection_edge_settings](int) {
    apply_selection_edge_settings();
    schedule_save_tool_settings();
  });
  connect(anti_alias, &QCheckBox::toggled, this, [this, apply_selection_edge_settings](bool) {
    apply_selection_edge_settings();
    save_tool_settings();
  });
  add_option_label(QT_TR_NOOP("Radius:"), {CanvasTool::Marquee});
  auto* marquee_corner_radius = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  marquee_corner_radius->setObjectName(QStringLiteral("selectionCornerRadiusSpin"));
  marquee_corner_radius->set_context_provider(document_unit_context_provider(true));
  marquee_corner_radius->setRange(0, 512);
  marquee_corner_radius->setValue(current_marquee_corner_radius_);
  bind_tooltip(marquee_corner_radius, QT_TR_NOOP("Rounded-corner radius for the rectangular marquee (0 = sharp corners)"));
  configure_toolbar_spinbox(marquee_corner_radius, 64);
  add_option_widget(marquee_corner_radius, {CanvasTool::Marquee});
  connect(marquee_corner_radius, &QSpinBox::valueChanged, this, [this](int value) {
    current_marquee_corner_radius_ = value;
    if (canvas_ != nullptr) {
      canvas_->set_marquee_corner_radius(value);
    }
  });
  add_option_label(QT_TR_NOOP("Style:"), {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  auto* style_combo = new QComboBox(toolbar);
  style_combo->setObjectName(QStringLiteral("selectionStyleCombo"));
  style_combo->addItems({tr("Normal"), tr("Fixed Ratio"), tr("Fixed Size")});
  style_combo->setCurrentText(tr("Normal"));
  style_combo->setFixedWidth(92);
  QPointer<QComboBox> selection_style_combo(style_combo);
  register_retranslation([selection_style_combo] {
    if (selection_style_combo == nullptr || selection_style_combo->count() < 3) {
      return;
    }
    QSignalBlocker blocker(selection_style_combo);
    selection_style_combo->setItemText(0, QObject::tr("Normal"));
    selection_style_combo->setItemText(1, QObject::tr("Fixed Ratio"));
    selection_style_combo->setItemText(2, QObject::tr("Fixed Size"));
  });
  add_option_widget(style_combo, {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  add_option_label(QT_TR_NOOP("Width:"), {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  auto* fixed_width = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  fixed_width->set_context_provider(document_axis_context(true));
  fixed_width->setObjectName(QStringLiteral("selectionFixedWidthSpin"));
  fixed_width->setRange(1, 30000);
  fixed_width->setValue(has_active_document() ? document().width() : 1024);
  configure_toolbar_spinbox(fixed_width, 78);
  add_option_widget(fixed_width, {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  add_option_label(QT_TR_NOOP("Height:"), {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  auto* fixed_height = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  fixed_height->set_context_provider(document_axis_context(false));
  fixed_height->setObjectName(QStringLiteral("selectionFixedHeightSpin"));
  fixed_height->setRange(1, 30000);
  fixed_height->setValue(has_active_document() ? document().height() : 768);
  configure_toolbar_spinbox(fixed_height, 78);
  add_option_widget(fixed_height, {CanvasTool::Marquee, CanvasTool::EllipticalMarquee});
  const auto apply_marquee_settings = [this, style_combo, fixed_width, fixed_height] {
    // Normal ignores the size fields, so they grey out (Fixed Ratio reads
    // them as the ratio, Fixed Size as the size).
    fixed_width->setEnabled(style_combo->currentIndex() != 0);
    fixed_height->setEnabled(style_combo->currentIndex() != 0);
    switch (style_combo->currentIndex()) {
      case 1:
        current_marquee_style_ = CanvasWidget::MarqueeStyle::FixedRatio;
        break;
      case 2:
        current_marquee_style_ = CanvasWidget::MarqueeStyle::FixedSize;
        break;
      default:
        current_marquee_style_ = CanvasWidget::MarqueeStyle::Normal;
        break;
    }
    current_marquee_width_ = fixed_width->value();
    current_marquee_height_ = fixed_height->value();
    if (canvas_ != nullptr) {
      canvas_->set_marquee_style(current_marquee_style_);
      canvas_->set_marquee_fixed_size(current_marquee_width_, current_marquee_height_);
    }
  };
  connect(style_combo, &QComboBox::currentIndexChanged, this, [apply_marquee_settings](int) {
    apply_marquee_settings();
  });
  connect(fixed_width, &QSpinBox::valueChanged, this, [apply_marquee_settings](int) {
    apply_marquee_settings();
  });
  connect(fixed_height, &QSpinBox::valueChanged, this, [apply_marquee_settings](int) {
    apply_marquee_settings();
  });
  apply_marquee_settings();
  add_option_separator({CanvasTool::Marquee, CanvasTool::EllipticalMarquee, CanvasTool::Lasso,
                        CanvasTool::MagneticLasso, CanvasTool::MagicWand});
}

void MainWindow::build_crop_tool_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_content = bar.options_content;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  const auto& document_axis_context = bar.document_axis_context;

  // Crop: Style picks what the fields mean, like the marquee's Style combo.
  // Ratio shows the preset combo and the unitless W : H pair; Size shows unit
  // Width / Height fields that mirror the box and resize it about its center.
  add_option_label(QT_TR_NOOP("Style:"), {CanvasTool::Crop});
  crop_style_combo_ = new QComboBox(toolbar);
  crop_style_combo_->setObjectName(QStringLiteral("cropStyleCombo"));
  crop_style_combo_->addItems({tr("Ratio"), tr("Size")});
  crop_style_combo_->setFixedWidth(72);
  QPointer<QComboBox> crop_style_combo(crop_style_combo_);
  register_retranslation([crop_style_combo] {
    if (crop_style_combo == nullptr || crop_style_combo->count() < 2) {
      return;
    }
    QSignalBlocker blocker(crop_style_combo);
    crop_style_combo->setItemText(0, QObject::tr("Ratio"));
    crop_style_combo->setItemText(1, QObject::tr("Size"));
  });
  bind_tooltip(crop_style_combo_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Ratio constrains the crop box; Size shows its exact width and height and sets them"));
  add_option_widget(crop_style_combo_, {CanvasTool::Crop});
  crop_ratio_option_widgets_.push_back(add_option_label(QT_TR_NOOP("Ratio:"), {CanvasTool::Crop}));
  crop_ratio_preset_combo_ = new QComboBox(toolbar);
  crop_ratio_preset_combo_->setObjectName(QStringLiteral("cropRatioPresetCombo"));
  crop_ratio_preset_combo_->setMinimumWidth(118);
  // Index 0 "None" and the last index "Custom" are fixed anchors: the preset
  // handler and sync_crop_ratio_preset_combo key on them. "Original Ratio"
  // (index 1) reads the active document at selection time.
  crop_ratio_preset_combo_->addItem(tr("None"));
  crop_ratio_preset_combo_->addItem(tr("Original Ratio"));
  crop_ratio_preset_combo_->addItem(tr("1 : 1 (Square)"), QSizeF(1.0, 1.0));
  crop_ratio_preset_combo_->addItem(QStringLiteral("4 : 5 (8 : 10)"), QSizeF(4.0, 5.0));
  crop_ratio_preset_combo_->addItem(QStringLiteral("5 : 7"), QSizeF(5.0, 7.0));
  crop_ratio_preset_combo_->addItem(QStringLiteral("2 : 3 (4 : 6)"), QSizeF(2.0, 3.0));
  crop_ratio_preset_combo_->addItem(QStringLiteral("16 : 9"), QSizeF(16.0, 9.0));
  crop_ratio_preset_combo_->addItem(tr("Custom"));
  QPointer<QComboBox> crop_preset_combo(crop_ratio_preset_combo_);
  register_retranslation([crop_preset_combo] {
    if (crop_preset_combo == nullptr || crop_preset_combo->count() < 8) {
      return;
    }
    QSignalBlocker blocker(crop_preset_combo);
    crop_preset_combo->setItemText(0, QObject::tr("None"));
    crop_preset_combo->setItemText(1, QObject::tr("Original Ratio"));
    crop_preset_combo->setItemText(2, QObject::tr("1 : 1 (Square)"));
    crop_preset_combo->setItemText(crop_preset_combo->count() - 1, QObject::tr("Custom"));
  });
  bind_tooltip(crop_ratio_preset_combo_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Aspect ratio preset for the crop box"));
  crop_ratio_option_widgets_.push_back(add_option_widget(crop_ratio_preset_combo_, {CanvasTool::Crop}));
  crop_ratio_w_spin_ = new QDoubleSpinBox(toolbar);
  crop_ratio_w_spin_->setObjectName(QStringLiteral("cropRatioWidthSpin"));
  crop_ratio_w_spin_->setRange(0.0, 10000.0);
  crop_ratio_w_spin_->setDecimals(2);
  crop_ratio_w_spin_->setValue(0.0);
  bind_tooltip(crop_ratio_w_spin_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Aspect ratio width (0 = unconstrained)"));
  configure_toolbar_spinbox(crop_ratio_w_spin_, 64);
  crop_ratio_option_widgets_.push_back(add_option_widget(crop_ratio_w_spin_, {CanvasTool::Crop}));
  auto* crop_ratio_separator = new QLabel(QStringLiteral(":"), options_content);
  crop_ratio_separator->setProperty("optionLabel", true);
  crop_ratio_separator->setAlignment(Qt::AlignVCenter);
  crop_ratio_option_widgets_.push_back(add_option_widget(crop_ratio_separator, {CanvasTool::Crop}));
  crop_ratio_h_spin_ = new QDoubleSpinBox(toolbar);
  crop_ratio_h_spin_->setObjectName(QStringLiteral("cropRatioHeightSpin"));
  crop_ratio_h_spin_->setRange(0.0, 10000.0);
  crop_ratio_h_spin_->setDecimals(2);
  crop_ratio_h_spin_->setValue(0.0);
  bind_tooltip(crop_ratio_h_spin_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Aspect ratio height (0 = unconstrained)"));
  configure_toolbar_spinbox(crop_ratio_h_spin_, 64);
  crop_ratio_option_widgets_.push_back(add_option_widget(crop_ratio_h_spin_, {CanvasTool::Crop}));
  crop_ratio_clear_button_ = new QPushButton(tr("Clear"), toolbar);
  crop_ratio_clear_button_->setObjectName(QStringLiteral("cropRatioClearButton"));
  bind_widget_text(crop_ratio_clear_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Clear"));
  bind_tooltip(crop_ratio_clear_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Clear the aspect ratio constraint"));
  crop_ratio_option_widgets_.push_back(add_option_widget(crop_ratio_clear_button_, {CanvasTool::Crop}));
  // Size mode: the same unit fields as the marquee's Fixed Size pair (pixel
  // native, a typed unit token converts at the document PPI, the same quick
  // values in the popup). Values commit on Enter or focus-out, never per
  // keystroke, so a half-typed width never reshapes the box.
  crop_size_option_widgets_.push_back(add_option_label(QT_TR_NOOP("Width:"), {CanvasTool::Crop}));
  crop_width_spin_ = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  crop_width_spin_->set_context_provider(document_axis_context(true));
  crop_width_spin_->setObjectName(QStringLiteral("cropWidthSpin"));
  crop_width_spin_->setRange(1, 30000);
  crop_width_spin_->setKeyboardTracking(false);
  bind_tooltip(crop_width_spin_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Width of the crop box"));
  configure_toolbar_spinbox(crop_width_spin_, 78);
  crop_size_option_widgets_.push_back(add_option_widget(crop_width_spin_, {CanvasTool::Crop}));
  crop_link_size_button_ = new QPushButton(toolbar);
  crop_link_size_button_->setObjectName(QStringLiteral("cropLinkSizeButton"));
  crop_link_size_button_->setCheckable(true);
  crop_link_size_button_->setChecked(false);
  crop_link_size_button_->setIcon(simple_icon(QStringLiteral("link"), QColor(220, 226, 235)));
  bind_tooltip(crop_link_size_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Keep the crop box's width and height in proportion"));
  crop_link_size_button_->setFixedWidth(28);
  crop_size_option_widgets_.push_back(add_option_widget(crop_link_size_button_, {CanvasTool::Crop}));
  crop_size_option_widgets_.push_back(add_option_label(QT_TR_NOOP("Height:"), {CanvasTool::Crop}));
  crop_height_spin_ = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  crop_height_spin_->set_context_provider(document_axis_context(false));
  crop_height_spin_->setObjectName(QStringLiteral("cropHeightSpin"));
  crop_height_spin_->setRange(1, 30000);
  crop_height_spin_->setKeyboardTracking(false);
  bind_tooltip(crop_height_spin_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Height of the crop box"));
  configure_toolbar_spinbox(crop_height_spin_, 78);
  crop_size_option_widgets_.push_back(add_option_widget(crop_height_spin_, {CanvasTool::Crop}));
  // Canvas Size's two layer options, as Photoshop's "Delete Cropped Pixels"
  // (on by default there too) plus Patchy's own off-canvas layer delete.
  crop_delete_pixels_check_ = new CheckGlyphBox(tr("Delete Cropped Pixels"), toolbar);
  crop_delete_pixels_check_->setObjectName(QStringLiteral("cropDeletePixelsCheck"));
  crop_delete_pixels_check_->setChecked(current_crop_delete_pixels_);
  bind_tooltip(crop_delete_pixels_check_,
               QT_TR_NOOP("Crop every layer to the new canvas; off, layers keep their pixels beyond it"));
  add_option_widget(crop_delete_pixels_check_, {CanvasTool::Crop});
  connect(crop_delete_pixels_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_crop_delete_pixels_ = checked;
    save_tool_settings();
  });
  crop_delete_layers_check_ = new CheckGlyphBox(tr("Delete Off-Canvas Layers"), toolbar);
  crop_delete_layers_check_->setObjectName(QStringLiteral("cropDeleteLayersCheck"));
  crop_delete_layers_check_->setChecked(current_crop_delete_layers_);
  bind_tooltip(crop_delete_layers_check_,
               QT_TR_NOOP("Delete layers that end up fully outside the crop box"));
  add_option_widget(crop_delete_layers_check_, {CanvasTool::Crop});
  connect(crop_delete_layers_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_crop_delete_layers_ = checked;
    save_tool_settings();
  });
  crop_apply_button_ = new QPushButton(toolbar);
  crop_apply_button_->setObjectName(QStringLiteral("cropApplyButton"));
  crop_apply_button_->setIcon(simple_icon(QStringLiteral("ok"), QColor(160, 220, 165)));
  bind_tooltip(crop_apply_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Apply crop (Enter)"));
  crop_apply_button_->setFixedWidth(30);
  crop_apply_button_->setIconSize(QSize(20, 20));
  crop_apply_button_->setProperty("optionsSessionButton", true);
  add_option_widget(crop_apply_button_, {CanvasTool::Crop});
  crop_cancel_button_ = new QPushButton(toolbar);
  crop_cancel_button_->setObjectName(QStringLiteral("cropCancelButton"));
  crop_cancel_button_->setIcon(simple_icon(QStringLiteral("clear"), QColor(255, 150, 150)));
  bind_tooltip(crop_cancel_button_, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Reset the crop box to the canvas (Esc)"));
  crop_cancel_button_->setFixedWidth(30);
  crop_cancel_button_->setIconSize(QSize(20, 20));
  crop_cancel_button_->setProperty("optionsSessionButton", true);
  add_option_widget(crop_cancel_button_, {CanvasTool::Crop});
  const auto apply_crop_ratio = [this] {
    if (crop_ratio_w_spin_ == nullptr || crop_ratio_h_spin_ == nullptr) {
      return;
    }
    current_crop_ratio_w_ = crop_ratio_w_spin_->value();
    current_crop_ratio_h_ = crop_ratio_h_spin_->value();
    if (canvas_ != nullptr) {
      canvas_->set_crop_ratio(effective_crop_ratio_width(), effective_crop_ratio_height());
    }
    sync_crop_ratio_preset_combo();
    schedule_save_tool_settings();
  };
  connect(crop_style_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    apply_crop_style(index);
  });
  connect(crop_width_spin_, &QSpinBox::valueChanged, this,
          [this](int value) { handle_crop_size_value_changed(true, value); });
  connect(crop_height_spin_, &QSpinBox::valueChanged, this,
          [this](int value) { handle_crop_size_value_changed(false, value); });
  connect(crop_ratio_w_spin_, &QDoubleSpinBox::valueChanged, this,
          [apply_crop_ratio](double) { apply_crop_ratio(); });
  connect(crop_ratio_h_spin_, &QDoubleSpinBox::valueChanged, this,
          [apply_crop_ratio](double) { apply_crop_ratio(); });
  connect(crop_ratio_preset_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (crop_ratio_preset_combo_ == nullptr || crop_ratio_w_spin_ == nullptr ||
        crop_ratio_h_spin_ == nullptr || index < 0) {
      return;
    }
    if (index == crop_ratio_preset_combo_->count() - 1) {
      return;  // Custom: whatever the fields hold stays.
    }
    auto ratio = QSizeF(0.0, 0.0);
    if (index == 1) {
      if (!has_active_document() || document().width() <= 0 || document().height() <= 0) {
        return;
      }
      const auto divisor = std::gcd(document().width(), document().height());
      ratio = QSizeF(static_cast<double>(document().width() / divisor),
                     static_cast<double>(document().height() / divisor));
    } else if (index > 1) {
      ratio = crop_ratio_preset_combo_->itemData(index).toSizeF();
    }
    crop_ratio_w_spin_->setValue(ratio.width());
    crop_ratio_h_spin_->setValue(ratio.height());
  });
  connect(crop_ratio_clear_button_, &QPushButton::clicked, this, [this] {
    if (crop_ratio_w_spin_ != nullptr) {
      crop_ratio_w_spin_->setValue(0.0);
    }
    if (crop_ratio_h_spin_ != nullptr) {
      crop_ratio_h_spin_->setValue(0.0);
    }
  });
  connect(crop_apply_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->commit_crop_session();
    }
  });
  connect(crop_cancel_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->reset_crop_session_to_canvas();
    }
  });
}

void MainWindow::build_quick_select_lasso_wand_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  // The bracket keys below also resize the brush build_brush_size_options made.
  auto* brush_size = brush_controls_.size;

  add_option_label(QT_TR_NOOP("Size:"), {CanvasTool::QuickSelect});
  auto* quick_select_size = new QSpinBox(toolbar);
  quick_select_size->setObjectName(QStringLiteral("quickSelectSizeSpin"));
  quick_select_size->setRange(1, 512);
  quick_select_size->setValue(canvas_defaults->quick_select_size());
  configure_toolbar_spinbox(quick_select_size, 46);
  add_option_widget(quick_select_size, {CanvasTool::QuickSelect});
  auto* quick_select_size_slider = new QSlider(Qt::Horizontal, toolbar);
  quick_select_size_slider->setObjectName(QStringLiteral("quickSelectSizeSlider"));
  bind_curved_slider(*quick_select_size_slider, *quick_select_size);
  quick_select_size_slider->setFixedWidth(150);
  bind_tooltip(quick_select_size_slider, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Quick Select brush size: press [ or ]"));
  add_option_widget(quick_select_size_slider, {CanvasTool::QuickSelect});
  connect(quick_select_size, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_quick_select_size(value);
      canvas_->refresh_tool_cursor();
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });

  quick_select_sample_all_layers_check_ = new CheckGlyphBox(tr("Sample All Layers"), toolbar);
  quick_select_sample_all_layers_check_->setObjectName(QStringLiteral("quickSelectSampleAllLayersCheck"));
  quick_select_sample_all_layers_check_->setChecked(canvas_defaults->quick_select_sample_all_layers());
  bind_tooltip(quick_select_sample_all_layers_check_, QT_TR_NOOP("Sample the merged document instead of the active layer"));
  add_option_widget(quick_select_sample_all_layers_check_, {CanvasTool::QuickSelect});
  connect(quick_select_sample_all_layers_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_quick_select_sample_all_layers(checked);
      save_tool_settings();
      refresh_document_info();
    }
  });

  quick_select_enhance_edge_check_ = new CheckGlyphBox(tr("Enhance Edge"), toolbar);
  quick_select_enhance_edge_check_->setObjectName(QStringLiteral("quickSelectEnhanceEdgeCheck"));
  quick_select_enhance_edge_check_->setChecked(canvas_defaults->quick_select_enhance_edge());
  bind_tooltip(quick_select_enhance_edge_check_, QT_TR_NOOP("Smooth the selection boundary after each stroke"));
  add_option_widget(quick_select_enhance_edge_check_, {CanvasTool::QuickSelect});
  connect(quick_select_enhance_edge_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_quick_select_enhance_edge(checked);
      save_tool_settings();
      refresh_document_info();
    }
  });

  add_option_label(QT_TR_NOOP("Width:"), {CanvasTool::MagneticLasso});
  auto* magnetic_width = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  magnetic_width->setObjectName(QStringLiteral("magneticLassoWidthSpin"));
  magnetic_width->set_context_provider(document_unit_context_provider(true));
  magnetic_width->setRange(1, 256);
  magnetic_width->setValue(canvas_defaults->magnetic_lasso_width());
  bind_tooltip(magnetic_width, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Edge search width in document pixels: press [ or ]"));
  configure_toolbar_spinbox(magnetic_width, 64);
  add_option_widget(magnetic_width, {CanvasTool::MagneticLasso});
  connect(magnetic_width, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_magnetic_lasso_width(value);
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });
  add_option_label(QT_TR_NOOP("Contrast:"), {CanvasTool::MagneticLasso});
  auto* magnetic_contrast = new QSpinBox(toolbar);
  magnetic_contrast->setObjectName(QStringLiteral("magneticLassoContrastSpin"));
  magnetic_contrast->setRange(1, 100);
  magnetic_contrast->setSuffix(percent_suffix());
  magnetic_contrast->setValue(canvas_defaults->magnetic_lasso_edge_contrast());
  bind_tooltip(magnetic_contrast, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Minimum edge contrast the trace snaps to"));
  configure_toolbar_spinbox(magnetic_contrast, 56);
  add_option_widget(magnetic_contrast, {CanvasTool::MagneticLasso});
  connect(magnetic_contrast, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_magnetic_lasso_edge_contrast(value);
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });
  add_option_label(QT_TR_NOOP("Frequency:"), {CanvasTool::MagneticLasso});
  auto* magnetic_frequency = new QSpinBox(toolbar);
  magnetic_frequency->setObjectName(QStringLiteral("magneticLassoFrequencySpin"));
  magnetic_frequency->setRange(0, 100);
  magnetic_frequency->setValue(canvas_defaults->magnetic_lasso_frequency());
  bind_tooltip(magnetic_frequency, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "How often anchor points are placed while tracing"));
  configure_toolbar_spinbox(magnetic_frequency, 46);
  add_option_widget(magnetic_frequency, {CanvasTool::MagneticLasso});
  connect(magnetic_frequency, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_magnetic_lasso_frequency(value);
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });

  auto* brush_smaller_action = new QAction(tr("Brush Smaller"), this);
  auto* brush_larger_action = new QAction(tr("Brush Larger"), this);
  auto* brush_much_smaller_action = new QAction(tr("Brush Much Smaller"), this);
  auto* brush_much_larger_action = new QAction(tr("Brush Much Larger"), this);
  brush_smaller_action->setObjectName(QStringLiteral("brushSmallerAction"));
  brush_larger_action->setObjectName(QStringLiteral("brushLargerAction"));
  brush_much_smaller_action->setObjectName(QStringLiteral("brushMuchSmallerAction"));
  brush_much_larger_action->setObjectName(QStringLiteral("brushMuchLargerAction"));
  register_hotkey(brush_smaller_action, "brush.smaller", QKeySequence(Qt::Key_BracketLeft), QStringLiteral("brush"));
  register_hotkey(brush_larger_action, "brush.larger", QKeySequence(Qt::Key_BracketRight), QStringLiteral("brush"));
  register_hotkey(brush_much_smaller_action, "brush.much_smaller", QKeySequence(Qt::SHIFT | Qt::Key_BracketLeft), QStringLiteral("brush"));
  register_hotkey(brush_much_larger_action, "brush.much_larger", QKeySequence(Qt::SHIFT | Qt::Key_BracketRight), QStringLiteral("brush"));
  addAction(brush_smaller_action);
  addAction(brush_larger_action);
  addAction(brush_much_smaller_action);
  addAction(brush_much_larger_action);
  // The bracket keys resize whichever brush the active tool uses (Quick Select
  // has its own; for the Magnetic Lasso they adjust the edge search width).
  const auto adjust_brush_size = [this, brush_size, quick_select_size, magnetic_width](int direction, bool coarse) {
    const bool quick_select = current_tool_ == CanvasTool::QuickSelect;
    const bool magnetic = current_tool_ == CanvasTool::MagneticLasso;
    auto* spin = quick_select ? quick_select_size : magnetic ? magnetic_width : brush_size;
    const int cap = quick_select ? 512 : magnetic ? 256 : kMaxBrushSize;
    const int value = spin->value();
    const int step = proportional_brush_step(value, direction, coarse);
    spin->setValue(std::clamp(value + direction * step, 1, cap));
  };
  connect(brush_smaller_action, &QAction::triggered, brush_size,
          [adjust_brush_size] { adjust_brush_size(-1, false); });
  connect(brush_larger_action, &QAction::triggered, brush_size,
          [adjust_brush_size] { adjust_brush_size(1, false); });
  connect(brush_much_smaller_action, &QAction::triggered, brush_size,
          [adjust_brush_size] { adjust_brush_size(-1, true); });
  connect(brush_much_larger_action, &QAction::triggered, brush_size,
          [adjust_brush_size] { adjust_brush_size(1, true); });
  for (auto* action : {brush_smaller_action, brush_larger_action, brush_much_smaller_action,
                       brush_much_larger_action}) {
    register_document_action(action);
  }

  add_option_label(QT_TR_NOOP("Tol:"), {CanvasTool::MagicWand});
  auto* wand_tolerance = new QSpinBox(toolbar);
  wand_tolerance->setObjectName(QStringLiteral("wandToleranceSpin"));
  wand_tolerance->setRange(0, 255);
  wand_tolerance->setValue(canvas_defaults->wand_tolerance());
  configure_toolbar_spinbox(wand_tolerance, 46);
  add_option_widget(wand_tolerance, {CanvasTool::MagicWand});
  connect(wand_tolerance, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_wand_tolerance(value);
      schedule_save_tool_settings();
      refresh_document_info();
    }
  });

  wand_contiguous_check_ = new CheckGlyphBox(tr("Contiguous"), toolbar);
  wand_contiguous_check_->setObjectName(QStringLiteral("wandContiguousCheck"));
  wand_contiguous_check_->setChecked(canvas_defaults->wand_contiguous());
  bind_tooltip(wand_contiguous_check_, QT_TR_NOOP("Limit Magic Wand selection to connected pixels"));
  add_option_widget(wand_contiguous_check_, {CanvasTool::MagicWand});
  connect(wand_contiguous_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_wand_contiguous(checked);
      save_tool_settings();
      refresh_document_info();
    }
  });

  wand_sample_all_layers_check_ = new CheckGlyphBox(tr("Sample All Layers"), toolbar);
  wand_sample_all_layers_check_->setObjectName(QStringLiteral("wandSampleAllLayersCheck"));
  wand_sample_all_layers_check_->setChecked(canvas_defaults->wand_sample_all_layers());
  bind_tooltip(wand_sample_all_layers_check_, QT_TR_NOOP("Sample the merged document instead of the active layer"));
  add_option_widget(wand_sample_all_layers_check_, {CanvasTool::MagicWand});
  connect(wand_sample_all_layers_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_wand_sample_all_layers(checked);
      save_tool_settings();
      refresh_document_info();
    }
  });

  // build_options_bar() exports the bracket actions for bind_action_translations().
  bar.brush_smaller_action = brush_smaller_action;
  bar.brush_larger_action = brush_larger_action;
  bar.brush_much_smaller_action = brush_much_smaller_action;
  bar.brush_much_larger_action = brush_much_larger_action;
}

}  // namespace patchy::ui
