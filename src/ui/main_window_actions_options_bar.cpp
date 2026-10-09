// MainWindow::build_options_bar(): the Options-bar phase of create_actions()
// (the top toolbar, its FlowLayout/OptionsFlowContainer hosts, the shared row
// helpers, and the per-tool row builders, called in construction order). The
// rows live in main_window_options_bar_<theme>.cpp (transform, selection, paint,
// retouch, shape, type); the Zoom and Rotate View rows stay here. Shared row
// state travels through MainWindow::OptionsBarBuildContext
// (main_window_options_bar_internal.hpp).
// Pure function move; behavior must stay identical, and the construction
// order is load-bearing (see create_actions() for the phase order).

#include "ui/main_window.hpp"
#include "ui/appearance_edits.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/main_window_actions_internal.hpp"
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

// A left-to-right layout that wraps its items onto additional rows when the
// available width is too small, skipping hidden widgets so the active tool's
// options pack tightly. Used by the Options bar so controls fold to a second
// line instead of being clipped.
class FlowLayout final : public QLayout {
public:
  explicit FlowLayout(QWidget* parent, int horizontal_spacing = 6, int vertical_spacing = 4)
      : QLayout(parent), horizontal_spacing_(horizontal_spacing), vertical_spacing_(vertical_spacing) {
    setContentsMargins(0, 0, 0, 0);
  }
  ~FlowLayout() override {
    while (QLayoutItem* item = takeAt(0)) {
      delete item;
    }
  }

  void addItem(QLayoutItem* item) override { items_.append(item); }
  int count() const override { return static_cast<int>(items_.size()); }
  QLayoutItem* itemAt(int index) const override { return items_.value(index); }
  QLayoutItem* takeAt(int index) override {
    return (index >= 0 && index < items_.size()) ? items_.takeAt(index) : nullptr;
  }
  Qt::Orientations expandingDirections() const override { return {}; }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override { return do_layout(QRect(0, 0, width, 0), true); }
  void setGeometry(const QRect& rect) override {
    QLayout::setGeometry(rect);
    do_layout(rect, false);
  }
  QSize sizeHint() const override { return minimumSize(); }
  QSize minimumSize() const override {
    QSize size;
    for (auto* item : items_) {
      const QWidget* widget = item->widget();
      if (widget != nullptr && widget->isHidden()) {
        continue;
      }
      size = size.expandedTo(item->minimumSize());
    }
    const auto margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
  }

private:
  int do_layout(const QRect& rect, bool test_only) const {
    const auto margins = contentsMargins();
    const QRect effective = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = effective.x();
    int y = effective.y();
    int line_height = 0;
    for (auto* item : items_) {
      QWidget* widget = item->widget();
      if (widget != nullptr && widget->isHidden()) {
        continue;
      }
      const QSize hint = item->sizeHint();
      int next_x = x + hint.width() + horizontal_spacing_;
      if (next_x - horizontal_spacing_ > effective.right() + 1 && line_height > 0) {
        x = effective.x();
        y = y + line_height + vertical_spacing_;
        next_x = x + hint.width() + horizontal_spacing_;
        line_height = 0;
      }
      if (!test_only) {
        item->setGeometry(QRect(QPoint(x, y), hint));
      }
      x = next_x;
      line_height = std::max(line_height, hint.height());
    }
    return y + line_height - rect.y() + margins.bottom();
  }

  QList<QLayoutItem*> items_;
  int horizontal_spacing_;
  int vertical_spacing_;
};

// Hosts the Options bar controls in a FlowLayout and reports the wrapped height
// for its current width so the surrounding QToolBar grows to a second row.
class OptionsFlowContainer final : public QWidget {
public:
  using QWidget::QWidget;

  QSize sizeHint() const override {
    const int available = width() > 0 ? width() : 1200;
    const int height = layout() != nullptr ? layout()->heightForWidth(available) : 0;
    return QSize(available, height);
  }
  QSize minimumSizeHint() const override {
    const int available = width() > 0 ? width() : 0;
    const int height = layout() != nullptr ? layout()->heightForWidth(std::max(available, 1)) : 0;
    return QSize(layout() != nullptr ? layout()->minimumSize().width() : 0, height);
  }

protected:
  void resizeEvent(QResizeEvent* event) override {
    QWidget::resizeEvent(event);
    // Width changed: the wrapped height may differ, so ask the toolbar to relayout.
    updateGeometry();
  }
};

}  // namespace

CheckGlyphBox::CheckGlyphBox(const QString& text, QWidget* parent) : QCheckBox(text, parent) {
  setMinimumHeight(24);
}

QSize CheckGlyphBox::sizeHint() const {
  const auto text_width = fontMetrics().horizontalAdvance(text());
  const auto minimum = objectName() == QStringLiteral("shapeFillCheck") ? 58 : 92;
  return QSize(std::max(minimum, text_width + 34), 24);
}

void CheckGlyphBox::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const bool framed = objectName() == QStringLiteral("moveAutoSelectCheck") ||
                      objectName() == QStringLiteral("selectionAntiAliasCheck") ||
                      objectName() == QStringLiteral("cloneAlignedCheck") ||
                      objectName() == QStringLiteral("shapeFillCheck");
  // Every color here is a role: this glyph is painted, not styled, so it would
  // otherwise stay dark-on-dark in the Light scheme.
  const auto& colors = theme();
  if (framed) {
    painter.fillRect(rect(), colors.field_bg);
    painter.setPen(QPen(colors.field_inset_border, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    painter.setPen(QPen(colors.field_bevel_top, 1));
    painter.drawLine(rect().topLeft(), rect().topRight());
  }

  const QRect box(7, (height() - 14) / 2, 14, 14);
  painter.setBrush(isChecked() ? colors.accent : colors.checkbox_compact_bg);
  painter.setPen(
      QPen(isChecked() ? colors.checkbox_accent_border : colors.checkbox_compact_border, 1));
  painter.drawRect(box);
  if (isChecked()) {
    painter.setPen(QPen(colors.text_on_accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(box.left() + 3.0, box.center().y() + 0.5), QPointF(box.left() + 6.0, box.bottom() - 3.0));
    painter.drawLine(QPointF(box.left() + 6.0, box.bottom() - 3.0), QPointF(box.right() - 2.0, box.top() + 3.0));
  }

  painter.setPen(isEnabled() ? colors.text_bright : colors.text_disabled);
  painter.drawText(QRect(box.right() + 7, 0, width() - box.right() - 10, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   text());
}

void MainWindow::build_options_bar(ActionBuildContext& ctx) {
  // The startup-defaults donor canvas resolved by create_actions() (see the
  // comment there); a local alias keeps the moved body identical.
  auto* canvas_defaults = ctx.canvas_defaults;
  auto* toolbar = new QToolBar(tr("Options"), this);
  toolbar->setObjectName(QStringLiteral("Options"));
  toolbar->setMovable(false);
  toolbar->setFloatable(false);
  toolbar->setAllowedAreas(Qt::TopToolBarArea);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  toolbar->setIconSize(QSize(18, 18));
  addToolBar(Qt::TopToolBarArea, toolbar);

  // Host the tool options in a wrapping flow layout so they fold onto a second
  // row when the window is too narrow, instead of being clipped off the edge.
  auto* options_content = new OptionsFlowContainer(toolbar);
  options_content->setObjectName(QStringLiteral("OptionsContent"));
  options_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto* options_flow = new FlowLayout(options_content, 5, 4);
  // Side padding so the first control does not butt against the window edge.
  options_flow->setContentsMargins(6, 3, 0, 3);
  options_content->setLayout(options_flow);
  toolbar->addWidget(options_content);
  options_flow_container_ = options_content;

  option_actions_.clear();
  transform_option_actions_.clear();
  warp_option_actions_.clear();
  puppet_warp_option_actions_.clear();
  transform_session_actions_.clear();
  perspective_warp_option_actions_.clear();
  perspective_warp_straighten_buttons_.clear();
  const auto make_option_separator = [options_content, options_flow]() -> QWidget* {
    auto* line = new QFrame(options_content);
    line->setObjectName(QStringLiteral("optionSeparator"));
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(24);
    options_flow->addWidget(line);
    return line;
  };
  const auto add_option_separator = [this, make_option_separator](std::initializer_list<CanvasTool> tools) {
    register_option_action(make_option_separator(), tools);
  };
  const auto add_option_action = [this, options_content, options_flow](const QIcon& icon, const char* source,
                                                                       std::initializer_list<CanvasTool> tools) {
    auto* action = new QAction(icon, tr(source), this);
    bind_action_text(action, source);
    auto* button = new QToolButton(options_content);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(true);
    // Tagged for the compact Options-bar button style (see the app stylesheet).
    // The default QToolButton min-height + padding makes it the tallest item in
    // the row, which grows the whole Options toolbar when a selection tool is
    // active. The icon keeps its full size; only the padding around it shrinks.
    button->setProperty("optionsBarButton", true);
    options_flow->addWidget(button);
    register_option_action(button, tools);
    return action;
  };
  const auto add_option_widget = [this, options_flow](QWidget* widget, std::vector<CanvasTool> tools) {
    options_flow->addWidget(widget);
    register_option_action(widget, std::move(tools));
    return widget;
  };
  const auto add_option_label = [options_content, add_option_widget](const char* source,
                                                                     std::vector<CanvasTool> tools) {
    auto* label = new QLabel(options_content);
    bind_widget_text(label, source);
    label->setProperty("optionLabel", true);
    label->setAlignment(Qt::AlignVCenter);
    return add_option_widget(label, std::move(tools));
  };

  // Unit-context providers the transform, marquee, crop and shape fields share:
  // the document PPI (300 with no document), and percent relative to the
  // document's extent along one axis.
  const auto document_ppi = [this]() -> double {
    return has_active_document() ? text_size_ppi(document()) : 300.0;
  };
  const auto document_axis_context = [this, document_ppi](bool horizontal) {
    return [this, document_ppi, horizontal]() -> UnitConversionContext {
      UnitConversionContext context;
      context.ppi = document_ppi();
      if (has_active_document()) {
        context.percent_reference_pixels =
            static_cast<double>(horizontal ? document().width() : document().height());
      }
      return context;
    };
  };

  OptionsBarBuildContext bar;
  bar.canvas_defaults = canvas_defaults;
  bar.toolbar = toolbar;
  bar.options_content = options_content;
  bar.options_flow = options_flow;
  bar.add_option_separator = add_option_separator;
  bar.add_option_action = add_option_action;
  bar.add_option_widget = add_option_widget;
  bar.add_option_label = add_option_label;
  bar.document_ppi = document_ppi;
  bar.document_axis_context = document_axis_context;

  // One builder per tool row, in the historical construction order: creation
  // order is the focus chain and the FlowLayout's on-screen order, so the
  // sequence is load-bearing.
  build_move_tool_options(bar);
  build_free_transform_options(bar);
  build_warp_transform_options(bar);
  build_puppet_warp_options(bar);
  build_transform_session_buttons(bar);
  build_perspective_warp_options(bar);
  build_selection_tool_options(bar);
  build_crop_tool_options(bar);
  build_brush_preset_option(bar);
  build_shape_mode_option(bar);
  build_brush_size_options(bar);
  build_brush_smoothing_and_mixer_options(bar);
  build_brush_symmetry_and_tip_options(bar);
  build_pattern_stamp_options(bar);
  build_gradient_tool_options(bar);
  build_retouch_tool_options(bar);
  build_toning_tool_options(bar);
  build_color_replacement_options(bar);
  build_quick_select_lasso_wand_options(bar);
  build_shape_appearance_options(bar);
  build_shape_path_options(bar);
  build_shape_pixel_options(bar);
  build_fill_tool_options(bar);
  build_zoom_tool_options(bar);
  build_type_tool_options(bar);
  build_rotate_view_options(bar);

  // Every "Label:" before a numeric field is that field's scrub handle
  // (GitHub issue 46; install_scrub_labels_in pairs them by layout order, nested
  // groups such as Feather included, so a new label+field pair opts in by itself).
  install_scrub_labels_in(options_content);

  // Export the cross-phase locals bind_action_translations() still needs.
  ctx.options_toolbar = toolbar;
  ctx.brush_smaller_action = bar.brush_smaller_action;
  ctx.brush_larger_action = bar.brush_larger_action;
  ctx.brush_much_smaller_action = bar.brush_much_smaller_action;
  ctx.brush_much_larger_action = bar.brush_much_larger_action;
  ctx.fill_shapes = bar.fill_shapes;
}

void MainWindow::build_zoom_tool_options(OptionsBarBuildContext& bar) {
  auto* canvas_defaults = bar.canvas_defaults;
  auto* toolbar = bar.toolbar;
  const auto& add_option_separator = bar.add_option_separator;
  const auto& add_option_action = bar.add_option_action;
  const auto& add_option_widget = bar.add_option_widget;

  // Zoom tool options (docs/view-navigation.md), Photoshop's row: the Zoom In /
  // Zoom Out click direction (tools/zoomToolZoomsOut; Alt inverts it), Scrubby
  // Zoom, then the 100% / Fit Screen / Fill Screen view presets. Every widget
  // carries optionsBarAllowedWhileLocked: the Zoom tool works while a preview
  // dialog locks editing, so its row stays enabled then (refresh_options_bar).
  const auto allow_while_locked = [](QWidget* widget) {
    widget->setProperty("optionsBarAllowedWhileLocked", true);
  };
  zoom_in_mode_action_ =
      add_option_action(simple_icon(QStringLiteral("zoomIn")), QT_TR_NOOP("Zoom In"), {CanvasTool::Zoom});
  zoom_in_mode_action_->setObjectName(QStringLiteral("zoomInModeAction"));
  allow_while_locked(option_actions_.back().first);
  zoom_out_mode_action_ =
      add_option_action(simple_icon(QStringLiteral("zoomOut")), QT_TR_NOOP("Zoom Out"), {CanvasTool::Zoom});
  zoom_out_mode_action_->setObjectName(QStringLiteral("zoomOutModeAction"));
  allow_while_locked(option_actions_.back().first);
  auto* zoom_mode_group = new QActionGroup(this);
  zoom_mode_group->setExclusive(true);
  for (auto* action : {zoom_in_mode_action_, zoom_out_mode_action_}) {
    action->setCheckable(true);
    zoom_mode_group->addAction(action);
  }
  zoom_in_mode_action_->setChecked(!canvas_defaults->zoom_tool_zooms_out());
  zoom_out_mode_action_->setChecked(canvas_defaults->zoom_tool_zooms_out());
  connect(zoom_mode_group, &QActionGroup::triggered, this, [this](QAction* action) {
    const bool zooms_out = action == zoom_out_mode_action_;
    current_zoom_tool_zooms_out_ = zooms_out;
    if (canvas_ != nullptr) {
      canvas_->set_zoom_tool_zooms_out(zooms_out);
      save_tool_settings();
    }
  });
  add_option_separator({CanvasTool::Zoom});
  // Scrubby Zoom (GitHub issue 51, Photoshop's gesture): a persisted view
  // preference (tools/zoomScrubby, default off) mirrored into every session
  // canvas.
  zoom_scrubby_check_ = new CheckGlyphBox(tr("Scrubby Zoom"), toolbar);
  allow_while_locked(zoom_scrubby_check_);
  zoom_scrubby_check_->setObjectName(QStringLiteral("zoomScrubbyCheck"));
  zoom_scrubby_check_->setChecked(canvas_defaults->zoom_scrubby());
  bind_tooltip(zoom_scrubby_check_,
               QT_TR_NOOP("Drag right to zoom in and left to zoom out around the point you pressed. "
                          "Off: drag a rectangle to zoom to it"));
  add_option_widget(zoom_scrubby_check_, {CanvasTool::Zoom});
  connect(zoom_scrubby_check_, &QCheckBox::toggled, this, [this](bool checked) {
    current_zoom_scrubby_ = checked;
    if (canvas_ != nullptr) {
      canvas_->set_zoom_scrubby(checked);
      save_tool_settings();
    }
  });
  add_option_separator({CanvasTool::Zoom});
  // The view presets call the canvas directly (the View menu actions do the
  // same), so the row does not depend on the menu build order.
  const auto add_zoom_view_button = [this, toolbar, add_option_widget, allow_while_locked](
                                        const char* source, const QString& object_name, const char* tooltip,
                                        std::function<void(CanvasWidget&)> apply) {
    auto* button = new QPushButton(tr(source), toolbar);
    button->setObjectName(object_name);
    bind_tooltip(button, tooltip);
    allow_while_locked(button);
    add_option_widget(button, {CanvasTool::Zoom});
    connect(button, &QPushButton::clicked, this, [this, apply] {
      if (canvas_ != nullptr) {
        apply(*canvas_);
      }
    });
    return button;
  };
  zoom_actual_pixels_button_ =
      add_zoom_view_button(QT_TR_NOOP("100%"), QStringLiteral("zoomActualPixelsButton"),
                           QT_TR_NOOP("Show the image at actual pixels (View > Actual Pixels)"),
                           [](CanvasWidget& canvas) { canvas.set_view_zoom_centered(1.0); });
  zoom_fit_screen_button_ =
      add_zoom_view_button(QT_TR_NOOP("Fit Screen"), QStringLiteral("zoomFitScreenButton"),
                           QT_TR_NOOP("Fit the whole image in the window (View > Fit on Screen)"),
                           [](CanvasWidget& canvas) { canvas.fit_to_view(); });
  zoom_fill_screen_button_ =
      add_zoom_view_button(QT_TR_NOOP("Fill Screen"), QStringLiteral("zoomFillScreenButton"),
                           QT_TR_NOOP("Zoom until the image fills the window (View > Fill Screen)"),
                           [](CanvasWidget& canvas) { canvas.fill_to_view(); });
}

void MainWindow::build_rotate_view_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  // Rotate View: Photoshop's Rotation Angle field and Reset View button. The
  // angle belongs to the active canvas (per document, never saved);
  // sync_view_rotation_controls mirrors it back after drags and tab switches.
  add_option_label(QT_TR_NOOP("Rotation Angle:"), {CanvasTool::RotateView});
  rotate_view_angle_spin_ = new UnitSpinBox(SpinUnit::Degrees, toolbar);
  rotate_view_angle_spin_->setObjectName(QStringLiteral("rotateViewAngleSpin"));
  rotate_view_angle_spin_->setRange(-180.0, 180.0);
  rotate_view_angle_spin_->setDecimals(0);
  rotate_view_angle_spin_->setWrapping(true);
  rotate_view_angle_spin_->setKeyboardTracking(false);
  bind_tooltip(rotate_view_angle_spin_, QT_TR_NOOP("View rotation angle"));
  configure_toolbar_spinbox(rotate_view_angle_spin_, 64);
  add_option_widget(rotate_view_angle_spin_, {CanvasTool::RotateView});
  connect(rotate_view_angle_spin_, &QDoubleSpinBox::valueChanged, this, [this](double degrees) {
    if (canvas_ != nullptr) {
      canvas_->set_view_rotation(degrees);
    }
  });
  register_retranslation([this] {
    if (rotate_view_angle_spin_ != nullptr) {
      rotate_view_angle_spin_->refresh_suffix();
    }
  });
  rotate_view_reset_button_ = new QPushButton(tr("Reset View"), toolbar);
  rotate_view_reset_button_->setObjectName(QStringLiteral("rotateViewResetButton"));
  bind_widget_text(rotate_view_reset_button_, QT_TR_NOOP("Reset View"));
  bind_tooltip(rotate_view_reset_button_, QT_TR_NOOP("Turn the view back to 0 degrees"));
  add_option_widget(rotate_view_reset_button_, {CanvasTool::RotateView});
  connect(rotate_view_reset_button_, &QPushButton::clicked, this, [this] { reset_view_rotation(); });
}

}  // namespace patchy::ui
