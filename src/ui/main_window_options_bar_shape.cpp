// The Options-bar rows for the shape and pen tools: the Shape/Path/Pixels mode
// combo, the fill/stroke appearance controls and the active shape's W/H, the
// Line/Pen/Polygon/Custom Shape options, and the Pixels-mode Fill/Radius/Style
// fields.
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

void MainWindow::build_shape_mode_option(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // Mode sits FIRST for the shape tools so it never moves when the Pixels-mode
  // brush controls below appear (Seth, September 2026).
  // Shape | Path | Pixels for the vector-capable draw tools (Shape is the
  // Photoshop-parity default; Pixels is the legacy raster behavior). The
  // vector appearance/combine widgets below register for the same tools and
  // refresh_vector_tool_options_visibility() refines them per mode.
  add_option_label(QT_TR_NOOP("Mode:"), {CanvasTool::Line, CanvasTool::Rectangle, CanvasTool::Ellipse, CanvasTool::Pen,
                     CanvasTool::Polygon, CanvasTool::CustomShape});
  vector_mode_combo_ = new QComboBox(toolbar);
  vector_mode_combo_->setObjectName(QStringLiteral("vectorModeCombo"));
  vector_mode_combo_->addItems({tr("Shape"), tr("Path"), tr("Pixels")});
  vector_mode_combo_->setCurrentIndex(0);
  vector_mode_combo_->setFixedWidth(76);
  bind_tooltip(vector_mode_combo_, QT_TR_NOOP("What the shape tools create: a shape layer, work-path subpaths, or raster pixels"));
  QPointer<QComboBox> vector_mode_combo_pointer(vector_mode_combo_);
  register_retranslation([vector_mode_combo_pointer] {
    if (vector_mode_combo_pointer == nullptr || vector_mode_combo_pointer->count() < 3) {
      return;
    }
    QSignalBlocker blocker(vector_mode_combo_pointer);
    // MainWindow::tr (not QObject::tr): "Pixels"/"Subtract" exist in the
    // QObject context with unrelated meanings (color mode, blend mode).
    vector_mode_combo_pointer->setItemText(0, MainWindow::tr("Shape"));
    vector_mode_combo_pointer->setItemText(1, MainWindow::tr("Path"));
    vector_mode_combo_pointer->setItemText(2, MainWindow::tr("Pixels"));
  });
  add_option_widget(vector_mode_combo_, {CanvasTool::Line, CanvasTool::Rectangle, CanvasTool::Ellipse, CanvasTool::Pen,
                     CanvasTool::Polygon, CanvasTool::CustomShape});
  connect(vector_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    current_vector_tool_mode_ = index == 1   ? VectorToolMode::Path
                                : index == 2 ? VectorToolMode::Pixels
                                             : VectorToolMode::Shape;
    if (canvas_ != nullptr) {
      canvas_->set_vector_tool_mode(current_vector_tool_mode_);
      schedule_save_tool_settings();
    }
    refresh_options_bar();
  });
}

void MainWindow::build_shape_appearance_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // The appearance controls also register for the path-select tools: there
  // they show only while an editable shape layer is active and live-edit it
  // (refresh_vector_tool_options_visibility refines; Photoshop's behavior).
  const std::initializer_list<CanvasTool> vector_appearance_tools{
      CanvasTool::Line,    CanvasTool::Rectangle,  CanvasTool::Ellipse,
      CanvasTool::Pen,     CanvasTool::Polygon,    CanvasTool::CustomShape,
      CanvasTool::PathSelect, CanvasTool::DirectSelect};
  const std::initializer_list<CanvasTool> vector_shape_size_tools{
      CanvasTool::Move,     CanvasTool::Line,       CanvasTool::Rectangle,
      CanvasTool::Ellipse,  CanvasTool::Pen,        CanvasTool::Polygon,
      CanvasTool::CustomShape, CanvasTool::PathSelect, CanvasTool::DirectSelect};
  vector_shape_mode_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Fill:"), vector_appearance_tools));
  vector_fill_swatch_button_ = new QToolButton(toolbar);
  vector_fill_swatch_button_->setObjectName(QStringLiteral("vectorFillSwatchButton"));
  bind_tooltip(vector_fill_swatch_button_, QT_TR_NOOP("Shape fill: none, solid color, gradient, or pattern"));
  vector_fill_swatch_button_->setAutoRaise(true);
  vector_fill_swatch_button_->setProperty("optionsBarButton", true);
  add_option_widget(vector_fill_swatch_button_, vector_appearance_tools);
  vector_shape_mode_option_widgets_.push_back(vector_fill_swatch_button_);
  connect(vector_fill_swatch_button_, &QToolButton::clicked, this,
          [this] { show_vector_paint_menu(false); });
  auto* vector_stroke_check = new CheckGlyphBox(tr("Stroke"), toolbar);

  bind_widget_text(vector_stroke_check, QT_TR_NOOP("Stroke"));
  vector_stroke_check->setObjectName(QStringLiteral("vectorStrokeCheck"));
  vector_stroke_check->setChecked(current_vector_stroke_enabled_);
  bind_tooltip(vector_stroke_check, QT_TR_NOOP("Stroke the shape outline"));
  add_option_widget(vector_stroke_check, vector_appearance_tools);
  vector_shape_mode_option_widgets_.push_back(vector_stroke_check);
  connect(vector_stroke_check, &QCheckBox::toggled, this, [this](bool checked) {
    current_vector_stroke_enabled_ = checked;
    refresh_vector_stroke_controls();
    schedule_save_tool_settings();
    apply_options_bar_appearance_to_active_shape({"stroke.enabled"});
  });

  vector_stroke_swatch_button_ = new QToolButton(toolbar);
  vector_stroke_swatch_button_->setObjectName(QStringLiteral("vectorStrokeSwatchButton"));
  bind_tooltip(vector_stroke_swatch_button_, QT_TR_NOOP("Shape stroke: solid color, gradient, or pattern"));
  vector_stroke_swatch_button_->setAutoRaise(true);
  vector_stroke_swatch_button_->setProperty("optionsBarButton", true);
  add_option_widget(vector_stroke_swatch_button_, vector_appearance_tools);
  vector_shape_mode_option_widgets_.push_back(vector_stroke_swatch_button_);
  connect(vector_stroke_swatch_button_, &QToolButton::clicked, this,
          [this] { show_vector_paint_menu(true); });

  // A thickness has no percent basis: the document extent means nothing to it.
  const auto thickness_context = [this]() -> UnitConversionContext {
    auto context = document_unit_context(true);
    context.percent_reference_pixels = 0.0;
    return context;
  };
  auto* vector_stroke_width_label = add_option_label(QT_TR_NOOP("Stroke width:"), vector_appearance_tools);
  vector_stroke_width_label->setObjectName(QStringLiteral("vectorStrokeWidthLabel"));
  vector_shape_mode_option_widgets_.push_back(vector_stroke_width_label);
  auto* vector_stroke_width = new UnitSpinBox(SpinUnit::Pixels, toolbar);
  vector_stroke_width->setObjectName(QStringLiteral("vectorStrokeWidthSpin"));
  vector_stroke_width->setRange(0.1, 1000.0);
  vector_stroke_width->setDecimals(1);
  vector_stroke_width->setValue(current_vector_stroke_width_);
  vector_stroke_width->set_context_provider(thickness_context);
  bind_tooltip(vector_stroke_width, QT_TR_NOOP("Stroke width"));
  configure_toolbar_spinbox(vector_stroke_width, 64);
  add_option_widget(vector_stroke_width, vector_appearance_tools);
  vector_shape_mode_option_widgets_.push_back(vector_stroke_width);
  register_ruler_unit_field(vector_stroke_width);  // print users think in mm strokes
  connect(vector_stroke_width, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    current_vector_stroke_width_ = value;
    schedule_save_tool_settings();
    schedule_vector_appearance_apply();
  });

  install_appearance_edit_intent(vector_stroke_width, [this, vector_stroke_width] {
    current_vector_stroke_width_ = vector_stroke_width->value();
    schedule_save_tool_settings();
    schedule_vector_appearance_apply();
  });
  connect(vector_stroke_width, &QDoubleSpinBox::editingFinished, this,
          [this] { finish_pending_shape_appearance_edit(); });

  // W / H of the ACTIVE shape layer (Photoshop's options-bar readouts): they
  // mirror the selected shape's bounds and resize it live (top-left anchored,
  // axis-aligned scale, so live shapes stay live); disabled without one.
  const auto make_shape_size_spin = [this, toolbar, &vector_shape_size_tools,
                                     add_option_widget](const char* name, const char* tooltip) {
    auto* spin = new UnitSpinBox(SpinUnit::Pixels, toolbar);
    spin->setObjectName(QLatin1String(name));
    spin->setRange(0.0, 60000.0);
    spin->setDecimals(1);
    spin->setSpecialValueText(QStringLiteral(" "));  // 0 = no shape to show
    spin->setKeyboardTracking(false);
    spin->setEnabled(false);
    bind_tooltip(spin, tooltip);
    configure_toolbar_spinbox(spin, 84);
    add_option_widget(spin, vector_shape_size_tools);
    vector_shape_size_option_widgets_.push_back(spin);
    return spin;
  };
  vector_shape_size_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("W:"), vector_shape_size_tools));
  vector_shape_width_spin_ =
      make_shape_size_spin("vectorShapeWidthSpin", QT_TR_NOOP("Width of the active shape"));
  vector_shape_link_size_button_ = new QPushButton(toolbar);
  vector_shape_link_size_button_->setObjectName(QStringLiteral("vectorShapeLinkSizeButton"));
  vector_shape_link_size_button_->setCheckable(true);
  vector_shape_link_size_button_->setChecked(false);
  vector_shape_link_size_button_->setIcon(simple_icon(QStringLiteral("link"), QColor(220, 226, 235)));
  bind_tooltip(vector_shape_link_size_button_, QT_TR_NOOP("Keep the shape's width and height in proportion"));
  vector_shape_link_size_button_->setFixedWidth(28);
  vector_shape_link_size_button_->setEnabled(false);
  add_option_widget(vector_shape_link_size_button_, vector_shape_size_tools);
  vector_shape_size_option_widgets_.push_back(vector_shape_link_size_button_);
  connect(vector_shape_link_size_button_, &QPushButton::toggled, this, [this](bool checked) {
    if (properties_shape_link_size_button_ != nullptr) {
      const QSignalBlocker blocker(properties_shape_link_size_button_);
      properties_shape_link_size_button_->setChecked(checked);
    }
  });
  vector_shape_size_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("H:"), vector_shape_size_tools));
  vector_shape_height_spin_ =
      make_shape_size_spin("vectorShapeHeightSpin", QT_TR_NOOP("Height of the active shape"));
  // The readouts follow the ruler unit; value() stays document pixels.
  vector_shape_width_spin_->set_context_provider(document_unit_context_provider(true));
  vector_shape_height_spin_->set_context_provider(document_unit_context_provider(false));
  register_ruler_unit_field(vector_shape_width_spin_);
  register_ruler_unit_field(vector_shape_height_spin_);
  connect(vector_shape_width_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    handle_vector_shape_size_value_changed(true, value);
  });
  connect(vector_shape_height_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    handle_vector_shape_size_value_changed(false, value);
  });

  // The Appearance button and Line Weight in the later shape rows reuse these.
  bar.vector_appearance_tools = vector_appearance_tools;
  bar.thickness_context = thickness_context;
}

void MainWindow::build_shape_path_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  const auto& thickness_context = bar.thickness_context;

  vector_vector_mode_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Weight:"), {CanvasTool::Line}));
  // Fractional so a 0.5 mm hairline survives a ruler unit of mm; the shape
  // model's line_weight is a double already.
  auto* vector_line_weight = new UnitSpinBox(SpinUnit::Pixels, toolbar);
  vector_line_weight->setObjectName(QStringLiteral("vectorLineWeightSpin"));
  vector_line_weight->setRange(1.0, 1000.0);
  vector_line_weight->setDecimals(1);
  vector_line_weight->setValue(current_vector_line_weight_);
  vector_line_weight->set_context_provider(thickness_context);
  bind_tooltip(vector_line_weight, QT_TR_NOOP("Line thickness"));
  vector_line_weight->setProperty(kToolbarSpinboxSliderCurvedProperty, true);
  configure_toolbar_spinbox(vector_line_weight, 58);
  add_option_widget(vector_line_weight, {CanvasTool::Line});
  vector_vector_mode_option_widgets_.push_back(vector_line_weight);
  register_ruler_unit_field(vector_line_weight);
  connect(vector_line_weight, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    current_vector_line_weight_ = value;
    schedule_save_tool_settings();
  });

  vector_vector_mode_option_widgets_.push_back(add_option_label(
      QT_TR_NOOP("Combine:"), {CanvasTool::Line, CanvasTool::Rectangle, CanvasTool::Ellipse, CanvasTool::Pen,
                       CanvasTool::Polygon, CanvasTool::CustomShape,
                       CanvasTool::PathSelect, CanvasTool::DirectSelect}));
  auto* vector_combine_combo = new QComboBox(toolbar);
  vector_combine_combo->setObjectName(QStringLiteral("vectorCombineCombo"));
  vector_combine_combo->addItems(
      {tr("New Layer"), tr("Add"), tr("Subtract"), tr("Intersect"), tr("Exclude")});
  vector_combine_combo->setCurrentIndex(0);
  vector_combine_combo->setFixedWidth(96);
  bind_tooltip(vector_combine_combo, QT_TR_NOOP("How the next shape combines with the active shape layer or work path"));
  QPointer<QComboBox> vector_combine_combo_pointer(vector_combine_combo);
  register_retranslation([vector_combine_combo_pointer] {
    if (vector_combine_combo_pointer == nullptr || vector_combine_combo_pointer->count() < 5) {
      return;
    }
    QSignalBlocker blocker(vector_combine_combo_pointer);
    vector_combine_combo_pointer->setItemText(0, MainWindow::tr("New Layer"));
    vector_combine_combo_pointer->setItemText(1, MainWindow::tr("Add"));
    vector_combine_combo_pointer->setItemText(2, MainWindow::tr("Subtract"));
    vector_combine_combo_pointer->setItemText(3, MainWindow::tr("Intersect"));
    vector_combine_combo_pointer->setItemText(4, MainWindow::tr("Exclude"));
  });
  add_option_widget(vector_combine_combo,
                    {CanvasTool::Line, CanvasTool::Rectangle, CanvasTool::Ellipse, CanvasTool::Pen,
                     CanvasTool::Polygon, CanvasTool::CustomShape,
                     CanvasTool::PathSelect, CanvasTool::DirectSelect});
  vector_vector_mode_option_widgets_.push_back(vector_combine_combo);
  connect(vector_combine_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
    current_vector_combine_index_ = index;
    // With a path-select selection, the combo edits the selected shapes'
    // combine operation in place (indices 1-4; "New Layer" is creation-only).
    if (canvas_ != nullptr && index >= 1 &&
        (canvas_->tool() == CanvasTool::PathSelect ||
         canvas_->tool() == CanvasTool::DirectSelect) &&
        canvas_->path_edit_has_selection()) {
      canvas_->set_selected_subpaths_combine_op(index == 1   ? patchy::PathCombineOp::Add
                                                : index == 2 ? patchy::PathCombineOp::Subtract
                                                : index == 3 ? patchy::PathCombineOp::Intersect
                                                             : patchy::PathCombineOp::Xor);
    }
  });
  update_vector_swatch_icons();

  // Photoshop's Pen "Auto Add/Delete": on, a click on the target path edits
  // its anchors (add on a segment, delete on a point); off, every click draws.
  auto* pen_auto_add_delete = new CheckGlyphBox(tr("Auto Add/Delete"), toolbar);
  bind_widget_text(pen_auto_add_delete, QT_TR_NOOP("Auto Add/Delete"));
  pen_auto_add_delete->setObjectName(QStringLiteral("penAutoAddDeleteCheck"));
  pen_auto_add_delete->setChecked(current_pen_auto_add_delete_);
  bind_tooltip(pen_auto_add_delete, QT_TR_NOOP("Clicking a segment of the path adds a point and clicking a point deletes it"));
  add_option_widget(pen_auto_add_delete, {CanvasTool::Pen});
  connect(pen_auto_add_delete, &QCheckBox::toggled, this, [this](bool checked) {
    current_pen_auto_add_delete_ = checked;
    if (canvas_ != nullptr) {
      canvas_->set_pen_auto_add_delete(checked);
    }
    schedule_save_tool_settings();
  });

  vector_vector_mode_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Sides:"), {CanvasTool::Polygon}));
  auto* polygon_sides = new QSpinBox(toolbar);
  polygon_sides->setObjectName(QStringLiteral("polygonSidesSpin"));
  polygon_sides->setRange(3, 100);
  polygon_sides->setValue(5);
  configure_toolbar_spinbox(polygon_sides, 52);
  add_option_widget(polygon_sides, {CanvasTool::Polygon});
  vector_vector_mode_option_widgets_.push_back(polygon_sides);
  connect(polygon_sides, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_polygon_sides(value);
      schedule_save_tool_settings();
    }
  });

  vector_vector_mode_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Star inset:"), {CanvasTool::Polygon}));
  auto* polygon_star_inset = new QSpinBox(toolbar);
  polygon_star_inset->setObjectName(QStringLiteral("polygonStarInsetSpin"));
  polygon_star_inset->setRange(0, 99);
  polygon_star_inset->setValue(0);
  polygon_star_inset->setSuffix(percent_suffix());
  bind_tooltip(polygon_star_inset, QT_TR_NOOP("0 makes a plain polygon; higher values pull in star points"));
  configure_toolbar_spinbox(polygon_star_inset, 56);
  add_option_widget(polygon_star_inset, {CanvasTool::Polygon});
  vector_vector_mode_option_widgets_.push_back(polygon_star_inset);
  connect(polygon_star_inset, &QSpinBox::valueChanged, this, [this](int value) {
    if (canvas_ != nullptr) {
      canvas_->set_polygon_star_inset(value);
      schedule_save_tool_settings();
    }
  });

  vector_vector_mode_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Shape:"), {CanvasTool::CustomShape}));
  custom_shape_combo_ = new QComboBox(toolbar);
  custom_shape_combo_->setObjectName(QStringLiteral("customShapeCombo"));
  custom_shape_combo_->setIconSize(QSize(24, 24));
  custom_shape_combo_->setFixedWidth(150);
  register_retranslation([this] { refresh_custom_shape_combo(); });
  add_option_widget(custom_shape_combo_, {CanvasTool::CustomShape});
  vector_vector_mode_option_widgets_.push_back(custom_shape_combo_);
  connect(custom_shape_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
    apply_custom_shape_selection();
    schedule_save_tool_settings();
  });

  auto* line_arrow_start = new CheckGlyphBox(tr("Arrow start"), toolbar);

  bind_widget_text(line_arrow_start, QT_TR_NOOP("Arrow start"));
  line_arrow_start->setObjectName(QStringLiteral("lineArrowStartCheck"));
  bind_tooltip(line_arrow_start, QT_TR_NOOP("Add an arrowhead at the line start"));
  add_option_widget(line_arrow_start, {CanvasTool::Line});
  vector_vector_mode_option_widgets_.push_back(line_arrow_start);
  connect(line_arrow_start, &QCheckBox::toggled, this, [this](bool checked) {
    current_line_arrow_start_ = checked;
    schedule_save_tool_settings();
  });
  auto* line_arrow_end = new CheckGlyphBox(tr("Arrow end"), toolbar);
  bind_widget_text(line_arrow_end, QT_TR_NOOP("Arrow end"));
  line_arrow_end->setObjectName(QStringLiteral("lineArrowEndCheck"));
  bind_tooltip(line_arrow_end, QT_TR_NOOP("Add an arrowhead at the line end"));
  add_option_widget(line_arrow_end, {CanvasTool::Line});
  vector_vector_mode_option_widgets_.push_back(line_arrow_end);
  connect(line_arrow_end, &QCheckBox::toggled, this, [this](bool checked) {
    current_line_arrow_end_ = checked;
    schedule_save_tool_settings();
  });
}

void MainWindow::build_shape_pixel_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;
  const auto& document_axis_context = bar.document_axis_context;
  const auto& vector_appearance_tools = bar.vector_appearance_tools;

  auto* fill_shapes = new CheckGlyphBox(tr("Fill"), toolbar);
  fill_shapes->setObjectName(QStringLiteral("shapeFillCheck"));
  add_option_widget(fill_shapes, {CanvasTool::Rectangle, CanvasTool::Ellipse});
  vector_pixel_only_option_widgets_.push_back(fill_shapes);
  connect(fill_shapes, &QCheckBox::toggled, this, [this](bool checked) {
    current_fill_shapes_ = checked;
    if (canvas_ != nullptr) {
      canvas_->set_fill_shapes(checked);
    }
  });

  add_option_label(QT_TR_NOOP("Radius:"), {CanvasTool::Rectangle});
  auto* shape_corner_radius = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  shape_corner_radius->setObjectName(QStringLiteral("shapeCornerRadiusSpin"));
  shape_corner_radius->set_context_provider(document_unit_context_provider(true));
  shape_corner_radius->setRange(0, 512);
  shape_corner_radius->setValue(canvas_defaults->shape_corner_radius());
  bind_tooltip(shape_corner_radius, QT_TR_NOOP("Rounded-corner radius for the rectangle tool (0 = sharp corners)"));
  configure_toolbar_spinbox(shape_corner_radius, 64);
  add_option_widget(shape_corner_radius, {CanvasTool::Rectangle});
  connect(shape_corner_radius, &QSpinBox::valueChanged, this, [this](int value) {
    current_shape_corner_radius_ = value;
    apply_selected_shape_corner_radius(value);
    if (canvas_ != nullptr) {
      canvas_->set_shape_corner_radius(value);
      schedule_save_tool_settings();
    }
  });

  install_appearance_edit_intent(shape_corner_radius, [this, shape_corner_radius] {
    current_shape_corner_radius_ = shape_corner_radius->value();
    apply_selected_shape_corner_radius(current_shape_corner_radius_);
    if (canvas_ != nullptr) canvas_->set_shape_corner_radius(current_shape_corner_radius_);
    schedule_save_tool_settings();
  });
  connect(shape_corner_radius, &QSpinBox::editingFinished, this,
          [this] { finish_pending_shape_appearance_edit(); });

  // Style / Width / Height for the shape draw tools, mirroring the marquee's
  // Normal / Fixed Ratio / Fixed Size options (session-only, like the marquee's).
  // Pixels-mode only: the vector modes show the active shape's W / H instead,
  // and a tap opens the Create dialog (docs/vector-tools.md).
  vector_pixel_only_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Style:"), {CanvasTool::Rectangle, CanvasTool::Ellipse}));
  auto* shape_style_combo = new QComboBox(toolbar);
  shape_style_combo->setObjectName(QStringLiteral("shapeStyleCombo"));
  shape_style_combo->addItems({tr("Normal"), tr("Fixed Ratio"), tr("Fixed Size")});
  shape_style_combo->setCurrentText(tr("Normal"));
  shape_style_combo->setFixedWidth(92);
  QPointer<QComboBox> shape_style_combo_pointer(shape_style_combo);
  register_retranslation([shape_style_combo_pointer] {
    if (shape_style_combo_pointer == nullptr || shape_style_combo_pointer->count() < 3) {
      return;
    }
    QSignalBlocker blocker(shape_style_combo_pointer);
    shape_style_combo_pointer->setItemText(0, QObject::tr("Normal"));
    shape_style_combo_pointer->setItemText(1, QObject::tr("Fixed Ratio"));
    shape_style_combo_pointer->setItemText(2, QObject::tr("Fixed Size"));
  });
  add_option_widget(shape_style_combo, {CanvasTool::Rectangle, CanvasTool::Ellipse});
  vector_pixel_only_option_widgets_.push_back(shape_style_combo);
  vector_pixel_only_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Width:"), {CanvasTool::Rectangle, CanvasTool::Ellipse}));
  auto* shape_fixed_width = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  shape_fixed_width->set_context_provider(document_axis_context(true));
  shape_fixed_width->setObjectName(QStringLiteral("shapeFixedWidthSpin"));
  shape_fixed_width->setRange(1, 30000);
  shape_fixed_width->setValue(has_active_document() ? document().width() : 1024);
  configure_toolbar_spinbox(shape_fixed_width, 78);
  add_option_widget(shape_fixed_width, {CanvasTool::Rectangle, CanvasTool::Ellipse});
  vector_pixel_only_option_widgets_.push_back(shape_fixed_width);
  vector_pixel_only_option_widgets_.push_back(
      add_option_label(QT_TR_NOOP("Height:"), {CanvasTool::Rectangle, CanvasTool::Ellipse}));
  auto* shape_fixed_height = new UnitIntSpinBox(SpinUnit::Pixels, toolbar);
  shape_fixed_height->set_context_provider(document_axis_context(false));
  shape_fixed_height->setObjectName(QStringLiteral("shapeFixedHeightSpin"));
  shape_fixed_height->setRange(1, 30000);
  shape_fixed_height->setValue(has_active_document() ? document().height() : 768);
  configure_toolbar_spinbox(shape_fixed_height, 78);
  add_option_widget(shape_fixed_height, {CanvasTool::Rectangle, CanvasTool::Ellipse});
  vector_pixel_only_option_widgets_.push_back(shape_fixed_height);
  const auto apply_shape_style_settings = [this, shape_style_combo, shape_fixed_width, shape_fixed_height] {
    // Normal ignores the size fields, so they grey out (Fixed Ratio reads
    // them as the ratio, Fixed Size as the size).
    shape_fixed_width->setEnabled(shape_style_combo->currentIndex() != 0);
    shape_fixed_height->setEnabled(shape_style_combo->currentIndex() != 0);
    switch (shape_style_combo->currentIndex()) {
      case 1:
        current_shape_style_ = CanvasWidget::MarqueeStyle::FixedRatio;
        break;
      case 2:
        current_shape_style_ = CanvasWidget::MarqueeStyle::FixedSize;
        break;
      default:
        current_shape_style_ = CanvasWidget::MarqueeStyle::Normal;
        break;
    }
    current_shape_width_ = shape_fixed_width->value();
    current_shape_height_ = shape_fixed_height->value();
    if (canvas_ != nullptr) {
      canvas_->set_shape_style(current_shape_style_);
      canvas_->set_shape_fixed_size(current_shape_width_, current_shape_height_);
    }
  };
  connect(shape_style_combo, &QComboBox::currentIndexChanged, this, [apply_shape_style_settings](int) {
    apply_shape_style_settings();
  });
  connect(shape_fixed_width, &QSpinBox::valueChanged, this, [apply_shape_style_settings](int) {
    apply_shape_style_settings();
  });
  connect(shape_fixed_height, &QSpinBox::valueChanged, this, [apply_shape_style_settings](int) {
    apply_shape_style_settings();
  });
  apply_shape_style_settings();

  // Extra settings follow every shape-specific control at the end of the row.
  vector_appearance_button_ = new QPushButton(tr("Appearance..."), toolbar);
  vector_appearance_button_->setObjectName(QStringLiteral("vectorAppearanceButton"));
  bind_widget_text(vector_appearance_button_, QT_TR_NOOP("Appearance..."));
  bind_tooltip(vector_appearance_button_, QT_TR_NOOP("Edit the active shape layer's fill, stroke, opacity, and edge"));
  vector_appearance_button_->setProperty("optionsBarButton", true);
  vector_appearance_button_->setMinimumHeight(24);
  vector_appearance_button_->setMaximumHeight(26);
  add_option_widget(vector_appearance_button_, vector_appearance_tools);
  vector_shape_mode_option_widgets_.push_back(vector_appearance_button_);
  connect(vector_appearance_button_, &QPushButton::clicked, this, [this] { edit_active_shape_appearance(); });

  // build_options_bar() exports it for bind_action_translations().
  bar.fill_shapes = fill_shapes;
}

}  // namespace patchy::ui
