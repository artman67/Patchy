// The Options-bar rows for moving and transforming layers: the Move tool row
// (Auto-Select, Show Transform Controls, Snap, the Align mirrors), and the
// session rows that replace the tool row while one runs (Free Transform, Warp,
// Puppet Warp, their shared mode/apply/cancel trio, and Perspective Warp).
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

void MainWindow::build_move_tool_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  auto* options_content = bar.options_content;
  auto* options_flow = bar.options_flow;
  const auto& add_option_separator = bar.add_option_separator;
  const auto& add_option_widget = bar.add_option_widget;

  move_auto_select_check_ = new CheckGlyphBox(tr("Auto-Select"), toolbar);
  move_auto_select_check_->setObjectName(QStringLiteral("moveAutoSelectCheck"));
  bind_tooltip(move_auto_select_check_, QT_TR_NOOP("Select layers by clicking artwork or dragging a rectangle from empty space"));
  move_auto_select_check_->setChecked(canvas_defaults->auto_select_layer());
  add_option_widget(move_auto_select_check_, {CanvasTool::Move});
  connect(move_auto_select_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_auto_select_layer(checked);
    }
    // Remembered across runs (GitHub issue 73). Written here, by the user's
    // click, rather than by save_tool_settings: tests set the canvas flag
    // directly and must not leak it into later windows.
    app_settings().setValue(QStringLiteral("tools/moveAutoSelect"), checked);
  });
  move_show_transform_controls_check_ = new CheckGlyphBox(tr("Show Transform Controls"), toolbar);
  move_show_transform_controls_check_->setObjectName(QStringLiteral("moveShowTransformControlsCheck"));
  bind_tooltip(move_show_transform_controls_check_, QT_TR_NOOP("Show transform controls when selecting a layer with Move"));
  move_show_transform_controls_check_->setChecked(canvas_defaults->show_transform_controls());
  add_option_widget(move_show_transform_controls_check_, {CanvasTool::Move});
  connect(move_show_transform_controls_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (canvas_ != nullptr) {
      canvas_->set_show_transform_controls(checked);
    }
  });
  // Snap mirrors View > Snap: one persisted setting (view/snapEnabled), two
  // surfaces. The action owns the state; the checkbox follows its toggled signal.
  move_snap_check_ = new CheckGlyphBox(tr("Snap"), toolbar);
  move_snap_check_->setObjectName(QStringLiteral("moveSnapCheck"));
  bind_tooltip(move_snap_check_,
               QT_TR_NOOP("Snap moved layers to other layers, guides, the grid, and the canvas (View > Snap). "
                          "Choose the targets under View > Snap To."));
  move_snap_check_->setChecked(view_snap_action_ != nullptr ? view_snap_action_->isChecked() : view_snap_enabled_);
  add_option_widget(move_snap_check_, {CanvasTool::Move});
  connect(move_snap_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (view_snap_action_ != nullptr) {
      view_snap_action_->setChecked(checked);
    }
  });
  if (view_snap_action_ != nullptr) {
    connect(view_snap_action_, &QAction::toggled, this, [this](bool checked) {
      if (move_snap_check_ != nullptr) {
        QSignalBlocker blocker(move_snap_check_);
        move_snap_check_->setChecked(checked);
      }
    });
  }
  // Align buttons MIRROR the Layer > Arrange > Align QActions (docs/alignment.md)
  // rather than wrapping them as default actions: refresh_options_bar hides
  // and re-enables a button's default action with the tool row, which would
  // override the menu commands' own enabled state whenever Move is not active.
  add_option_separator({CanvasTool::Move});
  const auto add_option_action_mirror = [this, options_content, options_flow](QAction* action,
                                                                             const QString& object_name,
                                                                             std::initializer_list<CanvasTool> tools) {
    auto* button = new QToolButton(options_content);
    button->setObjectName(object_name);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("optionsBarButton", true);
    // refresh_options_bar leaves the enabled state to the mirrored action.
    button->setProperty("optionsBarMirrorsAction", true);
    const auto sync = [button, action] {
      button->setIcon(action->icon());
      button->setToolTip(action->toolTip());
      button->setEnabled(action->isEnabled());
    };
    sync();
    connect(action, &QAction::changed, button, sync);
    connect(button, &QToolButton::clicked, action, [action] { action->trigger(); });
    options_flow->addWidget(button);
    register_option_action(button, tools);
    return button;
  };
  {
    const char* const align_button_names[] = {"moveAlignLeftButton", "moveAlignHCenterButton",
                                              "moveAlignRightButton", "moveAlignTopButton",
                                              "moveAlignVCenterButton", "moveAlignBottomButton"};
    for (std::size_t i = 0; i < layer_align_actions_.size(); ++i) {
      if (layer_align_actions_[i] != nullptr) {
        add_option_action_mirror(layer_align_actions_[i], QLatin1String(align_button_names[i]),
                                 {CanvasTool::Move});
      }
    }
  }
  move_align_more_button_ = new QToolButton(toolbar);
  move_align_more_button_->setObjectName(QStringLiteral("moveAlignMoreButton"));
  // The brushSmoothingOptionsButton pattern: compact "..." text, InstantPopup,
  // height pinned by the optionsBarMenuButton QSS rule so the 26 px row holds.
  move_align_more_button_->setText(QStringLiteral("..."));
  move_align_more_button_->setProperty("optionsBarMenuButton", true);
  move_align_more_button_->setFocusPolicy(Qt::NoFocus);
  bind_tooltip(move_align_more_button_, QT_TR_NOOP("Distribute layers and choose what to align to"));
  move_align_more_button_->setPopupMode(QToolButton::InstantPopup);
  {
    auto* more_menu = new QMenu(move_align_more_button_);
    more_menu->setObjectName(QStringLiteral("moveAlignMoreMenu"));
    for (std::size_t i = 0; i < layer_distribute_actions_.size(); ++i) {
      if (i == static_cast<std::size_t>(DistributeMode::HorizontalSpacing)) {
        more_menu->addSeparator();
      }
      if (layer_distribute_actions_[i] != nullptr) {
        more_menu->addAction(layer_distribute_actions_[i]);
      }
    }
    more_menu->addSeparator();
    if (layer_align_to_selection_action_ != nullptr) {
      more_menu->addAction(layer_align_to_selection_action_);
    }
    if (layer_align_to_canvas_action_ != nullptr) {
      more_menu->addAction(layer_align_to_canvas_action_);
    }
    move_align_more_button_->setMenu(more_menu);
  }
  add_option_widget(move_align_more_button_, {CanvasTool::Move});
}

void MainWindow::build_free_transform_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;
  const auto& document_ppi = bar.document_ppi;
  const auto& document_axis_context = bar.document_axis_context;
  const auto add_transform_option_widget = [this, options_flow](QWidget* widget) {
    options_flow->addWidget(widget);
    transform_option_actions_.push_back(widget);
    return widget;
  };

  {
    auto* pivot_label = new QLabel(QCoreApplication::translate(kMainWindowTranslationContext, "Pivot:"), toolbar);
    pivot_label->setObjectName(QStringLiteral("freeTransformPivotLabel"));
    pivot_label->setProperty("optionLabel", true);
    pivot_label->setAlignment(Qt::AlignVCenter);
    bind_widget_text(pivot_label, QT_TR_NOOP("Pivot:"));
    add_transform_option_widget(pivot_label);
  }
  transform_reference_combo_ = new QComboBox(toolbar);
  transform_reference_combo_->setObjectName(QStringLiteral("freeTransformReferenceCombo"));
  bind_tooltip(transform_reference_combo_, QT_TR_NOOP("Reference point"));
  transform_reference_combo_->setMinimumWidth(96);
  add_transform_option_widget(transform_reference_combo_);
  register_retranslation([this] {
    if (transform_reference_combo_ == nullptr) {
      return;
    }
    const auto current = transform_reference_combo_->currentData();
    QSignalBlocker blocker(transform_reference_combo_);
    transform_reference_combo_->clear();
    transform_reference_combo_->addItem(tr("Top Left"), static_cast<int>(CanvasAnchor::TopLeft));
    transform_reference_combo_->addItem(tr("Top"), static_cast<int>(CanvasAnchor::Top));
    transform_reference_combo_->addItem(tr("Top Right"), static_cast<int>(CanvasAnchor::TopRight));
    transform_reference_combo_->addItem(tr("Left"), static_cast<int>(CanvasAnchor::Left));
    transform_reference_combo_->addItem(tr("Center"), static_cast<int>(CanvasAnchor::Center));
    transform_reference_combo_->addItem(tr("Right"), static_cast<int>(CanvasAnchor::Right));
    transform_reference_combo_->addItem(tr("Bottom Left"), static_cast<int>(CanvasAnchor::BottomLeft));
    transform_reference_combo_->addItem(tr("Bottom"), static_cast<int>(CanvasAnchor::Bottom));
    transform_reference_combo_->addItem(tr("Bottom Right"), static_cast<int>(CanvasAnchor::BottomRight));
    const auto index = transform_reference_combo_->findData(current.isValid() ? current : QVariant(static_cast<int>(CanvasAnchor::Center)));
    transform_reference_combo_->setCurrentIndex(index >= 0 ? index : transform_reference_combo_->findData(static_cast<int>(CanvasAnchor::Center)));
  });
  connect(transform_reference_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (updating_transform_controls_ || canvas_ == nullptr || transform_reference_combo_ == nullptr || index < 0) {
      return;
    }
    canvas_->set_transform_reference_point(
        static_cast<CanvasAnchor>(transform_reference_combo_->itemData(index).toInt()));
    sync_transform_controls_from_canvas();
  });

  const auto make_transform_label = [toolbar, add_transform_option_widget](const char* source) {
    auto* label = new QLabel(QCoreApplication::translate(kMainWindowTranslationContext, source), toolbar);
    label->setProperty("optionLabel", true);
    label->setAlignment(Qt::AlignVCenter);
    bind_widget_text(label, source);
    add_transform_option_widget(label);
    return label;
  };
  // Unit-entry fields (Photoshop behavior): the display stays in the native unit,
  // but "2 in", "50%" or "200 px" typed into any of them converts on entry. The
  // context providers supply the document PPI and what 100% means per field: X/Y
  // percent is relative to the document extent, W/H pixels are relative to the
  // session's original extent.
  const auto make_transform_spin = [toolbar, add_transform_option_widget](const QString& object_name,
                                                                          double minimum, double maximum,
                                                                          int decimals, SpinUnit native) {
    auto* spin = new UnitSpinBox(native, toolbar);
    spin->setObjectName(object_name);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setKeyboardTracking(false);
    spin->setMinimumWidth(82);
    configure_dialog_spinbox(spin, 82);
    add_transform_option_widget(spin);
    return spin;
  };
  const auto transform_extent_context = [this, document_ppi](bool horizontal) {
    return [this, document_ppi, horizontal]() -> UnitConversionContext {
      UnitConversionContext context;
      context.ppi = document_ppi();
      const auto state = canvas_ != nullptr ? canvas_->transform_controls_state()
                                            : std::optional<CanvasWidget::TransformControlsState>{};
      if (state.has_value()) {
        context.percent_reference_pixels = horizontal ? state->original_size.width() : state->original_size.height();
      }
      return context;
    };
  };

  make_transform_label(QT_TR_NOOP("X:"));
  transform_x_spin_ = make_transform_spin(QStringLiteral("freeTransformXSpin"), -30000.0, 30000.0, 2,
                                          SpinUnit::Pixels);
  transform_x_spin_->set_context_provider(document_axis_context(true));
  register_ruler_unit_field(transform_x_spin_);  // starts in the ruler unit (Photoshop)
  bind_tooltip(transform_x_spin_, QT_TR_NOOP("Reference X position"));
  make_transform_label(QT_TR_NOOP("Y:"));
  transform_y_spin_ = make_transform_spin(QStringLiteral("freeTransformYSpin"), -30000.0, 30000.0, 2,
                                          SpinUnit::Pixels);
  transform_y_spin_->set_context_provider(document_axis_context(false));
  register_ruler_unit_field(transform_y_spin_);
  bind_tooltip(transform_y_spin_, QT_TR_NOOP("Reference Y position"));
  make_transform_label(QT_TR_NOOP("W:"));
  transform_scale_x_spin_ = make_transform_spin(QStringLiteral("freeTransformScaleXSpin"), -10000.0, 10000.0, 2,
                                                 SpinUnit::Percent);
  transform_scale_x_spin_->set_context_provider(transform_extent_context(true));
  transform_scale_x_spin_->set_display_unit_switchable(true);
  bind_tooltip(transform_scale_x_spin_, QT_TR_NOOP("Horizontal scale"));
  transform_link_scale_button_ = new QPushButton(toolbar);
  transform_link_scale_button_->setObjectName(QStringLiteral("freeTransformLinkScaleButton"));
  transform_link_scale_button_->setCheckable(true);
  transform_link_scale_button_->setChecked(true);
  transform_link_scale_button_->setIcon(simple_icon(QStringLiteral("link"), QColor(220, 226, 235)));
  bind_tooltip(transform_link_scale_button_, QT_TR_NOOP("Link horizontal and vertical scale"));
  transform_link_scale_button_->setFixedWidth(28);
  add_transform_option_widget(transform_link_scale_button_);
  make_transform_label(QT_TR_NOOP("H:"));
  transform_scale_y_spin_ = make_transform_spin(QStringLiteral("freeTransformScaleYSpin"), -10000.0, 10000.0, 2,
                                                 SpinUnit::Percent);
  transform_scale_y_spin_->set_context_provider(transform_extent_context(false));
  transform_scale_y_spin_->set_display_unit_switchable(true);
  // The linked W/H pair reads as one control, so a unit switch on either side
  // carries the other along (X and Y stay independent, like Photoshop).
  connect(transform_scale_x_spin_, &UnitSpinBox::display_unit_changed, transform_scale_y_spin_,
          &UnitSpinBox::set_display_unit);
  connect(transform_scale_y_spin_, &UnitSpinBox::display_unit_changed, transform_scale_x_spin_,
          &UnitSpinBox::set_display_unit);
  bind_tooltip(transform_scale_y_spin_, QT_TR_NOOP("Vertical scale"));
  make_transform_label(QT_TR_NOOP("Angle:"));
  transform_rotation_spin_ = make_transform_spin(QStringLiteral("freeTransformRotationSpin"), -3600.0, 3600.0, 2,
                                                 SpinUnit::Degrees);
  bind_tooltip(transform_rotation_spin_, QT_TR_NOOP("Rotation angle"));
  register_retranslation([this] {
    for (auto* spin : {transform_x_spin_, transform_y_spin_, transform_scale_x_spin_, transform_scale_y_spin_,
                       transform_rotation_spin_}) {
      if (spin != nullptr) {
        spin->refresh_suffix();
      }
    }
  });
  transform_interpolation_combo_ = new QComboBox(toolbar);
  transform_interpolation_combo_->setObjectName(QStringLiteral("freeTransformInterpolationCombo"));
  bind_tooltip(transform_interpolation_combo_, QT_TR_NOOP("Interpolation"));
  transform_interpolation_combo_->setMinimumWidth(132);
  add_transform_option_widget(transform_interpolation_combo_);
  register_retranslation([this] {
    if (transform_interpolation_combo_ == nullptr) {
      return;
    }
    const auto current = transform_interpolation_combo_->currentData();
    QSignalBlocker blocker(transform_interpolation_combo_);
    transform_interpolation_combo_->clear();
    transform_interpolation_combo_->addItem(tr("Nearest Neighbor"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::NearestNeighbor));
    transform_interpolation_combo_->addItem(tr("Bilinear"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::Bilinear));
    transform_interpolation_combo_->addItem(tr("Bicubic"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::Bicubic));
    // Photoshop's Free Transform set, in its order; the kernels are Image Size's.
    transform_interpolation_combo_->addItem(tr("Bicubic Smoother"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::BicubicSmoother));
    transform_interpolation_combo_->addItem(tr("Bicubic Sharper"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::BicubicSharper));
    transform_interpolation_combo_->addItem(tr("Bicubic Automatic"),
                                            static_cast<int>(CanvasWidget::TransformInterpolation::Automatic));
    const auto fallback = static_cast<int>(CanvasWidget::TransformInterpolation::Bicubic);
    const auto index = transform_interpolation_combo_->findData(current.isValid() ? current : QVariant(fallback));
    transform_interpolation_combo_->setCurrentIndex(index >= 0 ? index : transform_interpolation_combo_->findData(fallback));
  });
  const auto apply_transform_from_spin = [this] { apply_transform_controls_from_ui(); };
  connect(transform_x_spin_, &QDoubleSpinBox::valueChanged, this, apply_transform_from_spin);
  connect(transform_y_spin_, &QDoubleSpinBox::valueChanged, this, apply_transform_from_spin);
  connect(transform_scale_x_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (!updating_transform_controls_ && transform_link_scale_button_ != nullptr && transform_link_scale_button_->isChecked() &&
        transform_scale_y_spin_ != nullptr) {
      QSignalBlocker blocker(transform_scale_y_spin_);
      transform_scale_y_spin_->setValue(value);
    }
    apply_transform_controls_from_ui();
  });
  connect(transform_scale_y_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (!updating_transform_controls_ && transform_link_scale_button_ != nullptr && transform_link_scale_button_->isChecked() &&
        transform_scale_x_spin_ != nullptr) {
      QSignalBlocker blocker(transform_scale_x_spin_);
      transform_scale_x_spin_->setValue(value);
    }
    apply_transform_controls_from_ui();
  });
  connect(transform_link_scale_button_, &QPushButton::toggled, this, [this](bool checked) {
    if (checked && transform_scale_x_spin_ != nullptr && transform_scale_y_spin_ != nullptr) {
      QSignalBlocker blocker(transform_scale_y_spin_);
      transform_scale_y_spin_->setValue(transform_scale_x_spin_->value());
      apply_transform_controls_from_ui();
    }
  });
  connect(transform_rotation_spin_, &QDoubleSpinBox::valueChanged, this, apply_transform_from_spin);
  connect(transform_interpolation_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (updating_transform_controls_ || canvas_ == nullptr || transform_interpolation_combo_ == nullptr || index < 0) {
      return;
    }
    canvas_->set_transform_interpolation(
        static_cast<CanvasWidget::TransformInterpolation>(transform_interpolation_combo_->itemData(index).toInt()));
  });
}

void MainWindow::build_warp_transform_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;

  // Warp Transform options: visible only while the warp cage is active.
  const auto add_warp_option_widget = [this, options_flow](QWidget* widget) {
    options_flow->addWidget(widget);
    warp_option_actions_.push_back(widget);
    return widget;
  };
  {
    auto* label = new QLabel(QObject::tr("Warp:"), toolbar);
    label->setProperty("optionLabel", true);
    label->setAlignment(Qt::AlignVCenter);
    bind_widget_text(label, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Warp:"));
    add_warp_option_widget(label);
  }
  warp_style_combo_ = new QComboBox(toolbar);
  warp_style_combo_->setObjectName(QStringLiteral("warpStyleCombo"));
  bind_tooltip(warp_style_combo_, QT_TR_NOOP("Warp style"));
  warp_style_combo_->setMinimumWidth(110);
  add_warp_option_widget(warp_style_combo_);
  register_retranslation([this] {
    if (warp_style_combo_ == nullptr) {
      return;
    }
    const auto current = warp_style_combo_->currentData();
    QSignalBlocker blocker(warp_style_combo_);
    warp_style_combo_->clear();
    warp_style_combo_->addItem(tr("Custom"), QStringLiteral("warpCustom"));
    warp_style_combo_->addItem(tr("Arc"), QStringLiteral("warpArc"));
    warp_style_combo_->addItem(tr("Arc Lower"), QStringLiteral("warpArcLower"));
    warp_style_combo_->addItem(tr("Arc Upper"), QStringLiteral("warpArcUpper"));
    warp_style_combo_->addItem(tr("Arch"), QStringLiteral("warpArch"));
    warp_style_combo_->addItem(tr("Bulge"), QStringLiteral("warpBulge"));
    warp_style_combo_->addItem(tr("Shell Lower"), QStringLiteral("warpShellLower"));
    warp_style_combo_->addItem(tr("Shell Upper"), QStringLiteral("warpShellUpper"));
    warp_style_combo_->addItem(tr("Flag"), QStringLiteral("warpFlag"));
    warp_style_combo_->addItem(tr("Wave"), QStringLiteral("warpWave"));
    warp_style_combo_->addItem(tr("Fish"), QStringLiteral("warpFish"));
    warp_style_combo_->addItem(tr("Rise"), QStringLiteral("warpRise"));
    warp_style_combo_->addItem(tr("Fisheye"), QStringLiteral("warpFisheye"));
    warp_style_combo_->addItem(tr("Inflate"), QStringLiteral("warpInflate"));
    warp_style_combo_->addItem(tr("Squeeze"), QStringLiteral("warpSqueeze"));
    warp_style_combo_->addItem(tr("Twist"), QStringLiteral("warpTwist"));
    const auto index = warp_style_combo_->findData(current.isValid() ? current : QVariant(QStringLiteral("warpCustom")));
    warp_style_combo_->setCurrentIndex(std::max(0, index));
  });
  {
    auto* label = new QLabel(QObject::tr("Bend:"), toolbar);
    label->setProperty("optionLabel", true);
    label->setAlignment(Qt::AlignVCenter);
    bind_widget_text(label, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Bend:"));
    add_warp_option_widget(label);
  }
  warp_bend_spin_ = new QDoubleSpinBox(toolbar);
  warp_bend_spin_->setObjectName(QStringLiteral("warpBendSpin"));
  warp_bend_spin_->setRange(-100.0, 100.0);
  warp_bend_spin_->setDecimals(0);
  warp_bend_spin_->setKeyboardTracking(false);
  warp_bend_spin_->setSuffix(percent_suffix());
  warp_bend_spin_->setValue(50.0);
  bind_tooltip(warp_bend_spin_, QT_TR_NOOP("Warp bend"));
  configure_dialog_spinbox(warp_bend_spin_, 74);
  add_warp_option_widget(warp_bend_spin_);
  const auto apply_warp_style_from_ui = [this] {
    if (updating_transform_controls_ || canvas_ == nullptr || warp_style_combo_ == nullptr ||
        warp_bend_spin_ == nullptr) {
      return;
    }
    canvas_->apply_warp_style_preset(warp_style_combo_->currentData().toString(), warp_bend_spin_->value());
  };
  connect(warp_style_combo_, &QComboBox::currentIndexChanged, this,
          [apply_warp_style_from_ui](int index) {
            if (index >= 0) {
              apply_warp_style_from_ui();
            }
          });
  connect(warp_bend_spin_, &QDoubleSpinBox::valueChanged, this,
          [apply_warp_style_from_ui](double) { apply_warp_style_from_ui(); });
}

void MainWindow::build_puppet_warp_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;

  // Puppet Warp options: visible only while a Puppet Warp session runs. Pin
  // rotation and depth are set here and never from the canvas (no pop-up and no
  // rotation ring around a pin; see docs/puppet-warp.md).
  const auto add_puppet_option_widget = [this, options_flow](QWidget* widget) {
    options_flow->addWidget(widget);
    puppet_warp_option_actions_.push_back(widget);
    return widget;
  };
  const auto make_puppet_label = [toolbar, add_puppet_option_widget](const char* source) {
    auto* label = new QLabel(QCoreApplication::translate(kMainWindowTranslationContext, source), toolbar);
    label->setProperty("optionLabel", true);
    label->setAlignment(Qt::AlignVCenter);
    bind_widget_text(label, source);
    add_puppet_option_widget(label);
    return label;
  };
  const auto apply_puppet_options_from_ui = [this] {
    if (updating_transform_controls_ || canvas_ == nullptr) {
      return;
    }
    canvas_->set_puppet_warp_options(puppet_warp_options_from_ui());
  };
  // Its own "Mode:" entry: the shared one is the blend-mode label in some languages.
  auto* puppet_mode_label = new QLabel(toolbar);
  puppet_mode_label->setProperty("optionLabel", true);
  puppet_mode_label->setAlignment(Qt::AlignVCenter);
  add_puppet_option_widget(puppet_mode_label);
  register_retranslation([puppet_mode_label] { puppet_mode_label->setText(tr("Mode:", "Puppet Warp")); });
  puppet_warp_mode_combo_ = new QComboBox(toolbar);
  puppet_warp_mode_combo_->setObjectName(QStringLiteral("puppetWarpModeCombo"));
  bind_tooltip(puppet_warp_mode_combo_, QT_TR_NOOP("How rigidly the mesh holds its shape"));
  puppet_warp_mode_combo_->setMinimumWidth(86);
  add_puppet_option_widget(puppet_warp_mode_combo_);
  make_puppet_label(QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Density:"));
  puppet_warp_density_combo_ = new QComboBox(toolbar);
  puppet_warp_density_combo_->setObjectName(QStringLiteral("puppetWarpDensityCombo"));
  bind_tooltip(puppet_warp_density_combo_, QT_TR_NOOP("Mesh spacing"));
  puppet_warp_density_combo_->setMinimumWidth(104);
  add_puppet_option_widget(puppet_warp_density_combo_);
  register_retranslation([this] {
    if (puppet_warp_mode_combo_ == nullptr || puppet_warp_density_combo_ == nullptr) {
      return;
    }
    const auto mode = puppet_warp_mode_combo_->currentData();
    const auto density = puppet_warp_density_combo_->currentData();
    QSignalBlocker mode_blocker(puppet_warp_mode_combo_);
    QSignalBlocker density_blocker(puppet_warp_density_combo_);
    puppet_warp_mode_combo_->clear();
    puppet_warp_mode_combo_->addItem(tr("Rigid", "Puppet Warp mode"), static_cast<int>(PuppetWarpMode::Rigid));
    puppet_warp_mode_combo_->addItem(tr("Normal", "Puppet Warp mode"), static_cast<int>(PuppetWarpMode::Normal));
    puppet_warp_mode_combo_->addItem(tr("Distort", "Puppet Warp mode"), static_cast<int>(PuppetWarpMode::Distort));
    puppet_warp_density_combo_->clear();
    puppet_warp_density_combo_->addItem(tr("Fewer Points", "Puppet Warp density"),
                                        static_cast<int>(PuppetWarpDensity::FewerPoints));
    puppet_warp_density_combo_->addItem(tr("Normal", "Puppet Warp density"),
                                        static_cast<int>(PuppetWarpDensity::Normal));
    puppet_warp_density_combo_->addItem(tr("More Points", "Puppet Warp density"),
                                        static_cast<int>(PuppetWarpDensity::MorePoints));
    const auto normal_mode = QVariant(static_cast<int>(PuppetWarpMode::Normal));
    const auto normal_density = QVariant(static_cast<int>(PuppetWarpDensity::Normal));
    puppet_warp_mode_combo_->setCurrentIndex(
        std::max(0, puppet_warp_mode_combo_->findData(mode.isValid() ? mode : normal_mode)));
    puppet_warp_density_combo_->setCurrentIndex(
        std::max(0, puppet_warp_density_combo_->findData(density.isValid() ? density : normal_density)));
  });
  make_puppet_label(QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Expansion:"));
  puppet_warp_expansion_spin_ = new UnitSpinBox(SpinUnit::Pixels, toolbar);
  puppet_warp_expansion_spin_->setObjectName(QStringLiteral("puppetWarpExpansionSpin"));
  puppet_warp_expansion_spin_->setRange(-100.0, 100.0);
  puppet_warp_expansion_spin_->setDecimals(0);
  puppet_warp_expansion_spin_->setKeyboardTracking(false);
  puppet_warp_expansion_spin_->setValue(2.0);
  bind_tooltip(puppet_warp_expansion_spin_, QT_TR_NOOP("Grow or shrink the mesh beyond the layer's edge"));
  configure_dialog_spinbox(puppet_warp_expansion_spin_, 70);
  add_puppet_option_widget(puppet_warp_expansion_spin_);
  puppet_warp_show_mesh_check_ = new CheckGlyphBox(tr("Show Mesh"), toolbar);
  puppet_warp_show_mesh_check_->setObjectName(QStringLiteral("puppetWarpShowMeshCheck"));
  bind_widget_text(puppet_warp_show_mesh_check_, QT_TR_NOOP("Show Mesh"));
  puppet_warp_show_mesh_check_->setChecked(true);
  add_puppet_option_widget(puppet_warp_show_mesh_check_);
  make_puppet_label(QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pin Depth:"));
  const auto make_puppet_button = [toolbar, add_puppet_option_widget](const QString& object_name, const QString& text,
                                                                      const char* tooltip) {
    auto* button = new QPushButton(text, toolbar);
    button->setObjectName(object_name);
    bind_tooltip(button, tooltip);
    button->setFixedWidth(30);
    button->setProperty("optionsSessionButton", true);
    add_puppet_option_widget(button);
    return button;
  };
  puppet_warp_pin_forward_button_ =
      make_puppet_button(QStringLiteral("puppetWarpPinForwardButton"), QStringLiteral("+"),
                         QT_TR_NOOP("Bring the selected pins forward where the warp overlaps"));
  puppet_warp_pin_backward_button_ =
      make_puppet_button(QStringLiteral("puppetWarpPinBackwardButton"), QStringLiteral("-"),
                         QT_TR_NOOP("Send the selected pins backward where the warp overlaps"));
  make_puppet_label(QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Rotate:"));
  puppet_warp_rotate_combo_ = new QComboBox(toolbar);
  puppet_warp_rotate_combo_->setObjectName(QStringLiteral("puppetWarpRotateCombo"));
  bind_tooltip(puppet_warp_rotate_combo_, QT_TR_NOOP("Rotation of the selected pins"));
  puppet_warp_rotate_combo_->setMinimumWidth(70);
  add_puppet_option_widget(puppet_warp_rotate_combo_);
  register_retranslation([this] {
    if (puppet_warp_rotate_combo_ == nullptr) {
      return;
    }
    const auto current = puppet_warp_rotate_combo_->currentData();
    QSignalBlocker blocker(puppet_warp_rotate_combo_);
    puppet_warp_rotate_combo_->clear();
    puppet_warp_rotate_combo_->addItem(tr("Auto", "Puppet Warp pin rotation"), false);
    puppet_warp_rotate_combo_->addItem(tr("Fixed", "Puppet Warp pin rotation"), true);
    puppet_warp_rotate_combo_->setCurrentIndex(
        std::max(0, puppet_warp_rotate_combo_->findData(current.isValid() ? current : QVariant(false))));
  });
  puppet_warp_rotate_angle_spin_ = new UnitSpinBox(SpinUnit::Degrees, toolbar);
  puppet_warp_rotate_angle_spin_->setObjectName(QStringLiteral("puppetWarpRotateAngleSpin"));
  puppet_warp_rotate_angle_spin_->setRange(-180.0, 180.0);
  puppet_warp_rotate_angle_spin_->setDecimals(0);
  puppet_warp_rotate_angle_spin_->setKeyboardTracking(false);
  bind_tooltip(puppet_warp_rotate_angle_spin_, QT_TR_NOOP("Fixed rotation angle of the selected pins"));
  configure_dialog_spinbox(puppet_warp_rotate_angle_spin_, 70);
  add_puppet_option_widget(puppet_warp_rotate_angle_spin_);
  puppet_warp_remove_all_button_ = new QPushButton(toolbar);
  puppet_warp_remove_all_button_->setObjectName(QStringLiteral("puppetWarpRemoveAllPinsButton"));
  puppet_warp_remove_all_button_->setIcon(simple_icon(QStringLiteral("rotate")));
  bind_tooltip(puppet_warp_remove_all_button_, QT_TR_NOOP("Remove all pins"));
  puppet_warp_remove_all_button_->setFixedWidth(30);
  puppet_warp_remove_all_button_->setIconSize(QSize(20, 20));
  puppet_warp_remove_all_button_->setProperty("optionsSessionButton", true);
  add_puppet_option_widget(puppet_warp_remove_all_button_);
  connect(puppet_warp_mode_combo_, &QComboBox::currentIndexChanged, this,
          [apply_puppet_options_from_ui](int) { apply_puppet_options_from_ui(); });
  connect(puppet_warp_density_combo_, &QComboBox::currentIndexChanged, this,
          [apply_puppet_options_from_ui](int) { apply_puppet_options_from_ui(); });
  connect(puppet_warp_expansion_spin_, &QDoubleSpinBox::valueChanged, this,
          [apply_puppet_options_from_ui](double) { apply_puppet_options_from_ui(); });
  connect(puppet_warp_show_mesh_check_, &QCheckBox::toggled, this,
          [apply_puppet_options_from_ui](bool) { apply_puppet_options_from_ui(); });
  connect(puppet_warp_pin_forward_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->shift_selected_puppet_pins_depth(1);
    }
  });
  connect(puppet_warp_pin_backward_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->shift_selected_puppet_pins_depth(-1);
    }
  });
  const auto apply_puppet_rotation_from_ui = [this] {
    if (updating_transform_controls_ || canvas_ == nullptr || puppet_warp_rotate_combo_ == nullptr ||
        puppet_warp_rotate_angle_spin_ == nullptr) {
      return;
    }
    canvas_->set_selected_puppet_pins_rotation(puppet_warp_rotate_combo_->currentData().toBool(),
                                               puppet_warp_rotate_angle_spin_->value());
  };
  connect(puppet_warp_rotate_combo_, &QComboBox::currentIndexChanged, this,
          [apply_puppet_rotation_from_ui](int index) {
            if (index >= 0) {
              apply_puppet_rotation_from_ui();
            }
          });
  connect(puppet_warp_rotate_angle_spin_, &QDoubleSpinBox::valueChanged, this,
          [apply_puppet_rotation_from_ui](double) { apply_puppet_rotation_from_ui(); });
  connect(puppet_warp_remove_all_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->remove_all_puppet_pins();
    }
  });
}

void MainWindow::build_transform_session_buttons(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;

  // Shared session trio, laid out after both control sets so it closes the row in
  // either mode (Photoshop's options-bar order: mode toggle, then cancel/commit).
  // Apply/cancel dispatch on whichever session is active.
  const auto add_session_option_widget = [this, options_flow](QWidget* widget) {
    options_flow->addWidget(widget);
    transform_session_actions_.push_back(widget);
    return widget;
  };
  transform_warp_mode_button_ = new QPushButton(toolbar);
  transform_warp_mode_button_->setObjectName(QStringLiteral("transformWarpModeButton"));
  transform_warp_mode_button_->setCheckable(true);
  transform_warp_mode_button_->setIcon(simple_icon(QStringLiteral("warp")));
  bind_tooltip(transform_warp_mode_button_, QT_TR_NOOP("Switch between free transform and warp"));
  transform_warp_mode_button_->setFixedWidth(30);
  // Session buttons render their icons at 20px (the QPushButton default of 16px
  // reads tiny on the bar); optionsSessionButton relaxes the QSS side padding so
  // the larger icon is not clipped.
  transform_warp_mode_button_->setIconSize(QSize(20, 20));
  transform_warp_mode_button_->setProperty("optionsSessionButton", true);
  add_session_option_widget(transform_warp_mode_button_);
  transform_apply_button_ = new QPushButton(toolbar);
  transform_apply_button_->setObjectName(QStringLiteral("freeTransformApplyButton"));
  transform_apply_button_->setIcon(simple_icon(QStringLiteral("ok"), QColor(160, 220, 165)));
  bind_tooltip(transform_apply_button_, QT_TR_NOOP("Apply transform"));
  transform_apply_button_->setFixedWidth(30);
  transform_apply_button_->setIconSize(QSize(20, 20));
  transform_apply_button_->setProperty("optionsSessionButton", true);
  add_session_option_widget(transform_apply_button_);
  transform_cancel_button_ = new QPushButton(toolbar);
  transform_cancel_button_->setObjectName(QStringLiteral("freeTransformCancelButton"));
  transform_cancel_button_->setIcon(simple_icon(QStringLiteral("clear"), QColor(255, 150, 150)));
  bind_tooltip(transform_cancel_button_, QT_TR_NOOP("Cancel transform"));
  transform_cancel_button_->setFixedWidth(30);
  transform_cancel_button_->setIconSize(QSize(20, 20));
  transform_cancel_button_->setProperty("optionsSessionButton", true);
  add_session_option_widget(transform_cancel_button_);
  connect(transform_warp_mode_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ == nullptr) {
      return;
    }
    if (canvas_->warp_transform_active()) {
      canvas_->switch_warp_to_free_transform();
    } else if (canvas_->free_transform_active()) {
      canvas_->begin_warp_transform();  // refusal reasons land in the status bar
    }
    // Re-sync the checked state (a refused switch leaves the mode unchanged).
    refresh_options_bar();
  });
  connect(transform_apply_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ == nullptr) {
      return;
    }
    canvas_->commit_modal_session();
  });
  connect(transform_cancel_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ == nullptr) {
      return;
    }
    canvas_->cancel_modal_session();
  });
}

void MainWindow::build_perspective_warp_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;

  // Perspective Warp session row (Edit > Perspective Warp): the Layout/Warp mode
  // pair, the one-shot straighten trio (Warp mode, selected quad), Remove All
  // Quads, and the session's own apply/cancel pair. Visibility and enabled
  // states follow the canvas session in refresh_options_bar. Qt::NoFocus keeps
  // the keyboard on the canvas, so Enter and Esc still end the session.
  const auto add_perspective_warp_widget = [this, options_flow](QWidget* widget) {
    widget->setFocusPolicy(Qt::NoFocus);
    options_flow->addWidget(widget);
    perspective_warp_option_actions_.push_back(widget);
    return widget;
  };
  const auto make_perspective_mode_button = [toolbar](const char* object_name, const char* text,
                                                      const char* tooltip) {
    auto* button = new QPushButton(toolbar);
    button->setObjectName(QString::fromLatin1(object_name));
    button->setCheckable(true);
    bind_widget_text(button, text);
    bind_tooltip(button, tooltip);
    return button;
  };
  perspective_warp_layout_button_ = make_perspective_mode_button(
      "perspectiveWarpLayoutButton", QT_TR_NOOP("Layout"), QT_TR_NOOP("Draw quads over the planes of the image"));
  perspective_warp_warp_button_ = make_perspective_mode_button(
      "perspectiveWarpWarpButton", QT_TR_NOOP("Warp"), QT_TR_NOOP("Drag the quad corners to change the perspective"));
  add_perspective_warp_widget(perspective_warp_layout_button_);
  add_perspective_warp_widget(perspective_warp_warp_button_);
  const auto set_perspective_mode = [this](CanvasWidget::PerspectiveWarpMode mode) {
    if (canvas_ != nullptr) {
      canvas_->set_perspective_warp_mode(mode);  // a refusal lands in the status bar
    }
    refresh_options_bar();  // re-sync the checked pair
  };
  connect(perspective_warp_layout_button_, &QPushButton::clicked, this,
          [set_perspective_mode] { set_perspective_mode(CanvasWidget::PerspectiveWarpMode::Layout); });
  connect(perspective_warp_warp_button_, &QPushButton::clicked, this,
          [set_perspective_mode] { set_perspective_mode(CanvasWidget::PerspectiveWarpMode::Warp); });
  // Straighten glyphs: bold vertical bars, bold horizontal bars, or both.
  const auto straighten_icon = [](bool vertical, bool horizontal) {
    return themed_glyph_icon(
        vertical && horizontal ? QStringLiteral("perspective-straighten-both")
                               : (vertical ? QStringLiteral("perspective-straighten-vertical")
                                           : QStringLiteral("perspective-level-horizontal")),
        32.0, &ThemePalette::icon_ink, [vertical, horizontal](QPainter& painter, const QColor& ink) {
          painter.setRenderHint(QPainter::Antialiasing, true);
          painter.setPen(QPen(ink, 1.5));
          painter.setBrush(Qt::NoBrush);
          painter.drawPolygon(QPolygonF{QPointF(8.0, 9.0), QPointF(24.0, 6.0), QPointF(26.0, 26.0), QPointF(6.0, 24.0)});
          painter.setPen(QPen(ink, 3.0, Qt::SolidLine, Qt::FlatCap));
          if (vertical) {
            painter.drawLine(QPointF(10.0, 4.0), QPointF(10.0, 28.0));
            painter.drawLine(QPointF(22.0, 4.0), QPointF(22.0, 28.0));
          }
          if (horizontal) {
            painter.drawLine(QPointF(4.0, 10.0), QPointF(28.0, 10.0));
            painter.drawLine(QPointF(4.0, 22.0), QPointF(28.0, 22.0));
          }
        });
  };
  const auto add_straighten_button = [this, toolbar, add_perspective_warp_widget, straighten_icon](
                                         const char* object_name, const char* tooltip, bool vertical,
                                         bool horizontal) {
    auto* button = new QPushButton(toolbar);
    button->setObjectName(QString::fromLatin1(object_name));
    button->setIcon(straighten_icon(vertical, horizontal));
    bind_tooltip(button, tooltip);
    button->setFixedWidth(30);
    button->setIconSize(QSize(20, 20));
    button->setProperty("optionsSessionButton", true);
    add_perspective_warp_widget(button);
    perspective_warp_straighten_buttons_.push_back(button);
    connect(button, &QPushButton::clicked, this, [this, vertical, horizontal] {
      if (canvas_ != nullptr) {
        canvas_->straighten_perspective_warp_quad(vertical, horizontal);
      }
    });
  };
  add_straighten_button("perspectiveWarpStraightenVerticalButton",
                        QT_TR_NOOP("Make the selected quad's near-vertical sides vertical"), true, false);
  add_straighten_button("perspectiveWarpLevelHorizontalButton",
                        QT_TR_NOOP("Make the selected quad's near-horizontal sides horizontal"), false, true);
  add_straighten_button("perspectiveWarpStraightenBothButton",
                        QT_TR_NOOP("Make the selected quad's sides vertical and horizontal"), true, true);
  perspective_warp_remove_all_button_ = new QPushButton(toolbar);
  perspective_warp_remove_all_button_->setObjectName(QStringLiteral("perspectiveWarpRemoveAllButton"));
  bind_widget_text(perspective_warp_remove_all_button_, QT_TR_NOOP("Remove All Quads"));
  bind_tooltip(perspective_warp_remove_all_button_, QT_TR_NOOP("Remove every quad and start the layout again"));
  add_perspective_warp_widget(perspective_warp_remove_all_button_);
  connect(perspective_warp_remove_all_button_, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->remove_all_perspective_warp_quads();
    }
  });
  auto* perspective_apply = new QPushButton(toolbar);
  perspective_apply->setObjectName(QStringLiteral("perspectiveWarpApplyButton"));
  perspective_apply->setIcon(simple_icon(QStringLiteral("ok"), QColor(160, 220, 165)));
  bind_tooltip(perspective_apply, QT_TR_NOOP("Apply Perspective Warp"));
  perspective_apply->setFixedWidth(30);
  perspective_apply->setIconSize(QSize(20, 20));
  perspective_apply->setProperty("optionsSessionButton", true);
  add_perspective_warp_widget(perspective_apply);
  auto* perspective_cancel = new QPushButton(toolbar);
  perspective_cancel->setObjectName(QStringLiteral("perspectiveWarpCancelButton"));
  perspective_cancel->setIcon(simple_icon(QStringLiteral("clear"), QColor(255, 150, 150)));
  bind_tooltip(perspective_cancel, QT_TR_NOOP("Cancel Perspective Warp"));
  perspective_cancel->setFixedWidth(30);
  perspective_cancel->setIconSize(QSize(20, 20));
  perspective_cancel->setProperty("optionsSessionButton", true);
  add_perspective_warp_widget(perspective_cancel);
  connect(perspective_apply, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->finish_perspective_warp();
    }
  });
  connect(perspective_cancel, &QPushButton::clicked, this, [this] {
    if (canvas_ != nullptr) {
      canvas_->cancel_perspective_warp();
    }
  });
}

}  // namespace patchy::ui
