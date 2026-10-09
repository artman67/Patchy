// MainWindow's right dock column machinery, split out of main_window.cpp: the
// right-dock-stack resize plumbing (update_right_dock_resize_handle_geometry,
// set_right_dock_stack_width, update_right_dock_minimum_width,
// handle_right_dock_resize_event), floating tab-group chrome, the collapsible
// dock title helper, and the saved panel arrangement
// (restore/save/reset_panel_layout, docs/dock-panels.md). The panels
// themselves are built in main_window_dock_builders.cpp.
// Pure function moves from main_window.cpp; behavior must stay identical.

#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/main_window_docks_internal.hpp"
#include "ui/modifier_names.hpp"
#include "ui/paths_panel.hpp"

#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/vector_shape.hpp"
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
#include "ui/brush_settings_panel.hpp"
#include "ui/brushes_panel.hpp"
#include "ui/current_brush.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/raw_develop_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
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
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
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

constexpr int kRightDockMinimumWidth = 280;
// Width the dock chrome adds around a panel: a dock at width W gives its
// content widget W minus this.
constexpr int kRightDockChromeWidth = 18;

// The docks that share one width as the right panel stack. Every dock in the
// column must be listed: one left out keeps its own minimum width and renders
// as a shorter strip whenever the pinned or measured width exceeds it.
const std::array<QString, 10>& right_dock_stack_names() {
  static const std::array<QString, 10> names{
      QStringLiteral("colorDock"),      QStringLiteral("brushesDock"),
      QStringLiteral("brushSettingsDock"), QStringLiteral("layersDock"),
      QStringLiteral("channelsDock"),   QStringLiteral("pathsDock"),
      QStringLiteral("historyDock"),    QStringLiteral("propertiesDock"),
      QStringLiteral("infoDock"),       QStringLiteral("paletteDock")};
  return names;
}

// The QDockWidgetGroupWindow class name is reliable (it has Q_OBJECT); the
// patchy.dockGroupWindow property is a seam for tests, which cannot construct
// the private Qt type.
constexpr int kGroupWindowResizeMargin = 10;
// Added as contents margins so the resize frame is a real, hittable strip;
// Qt's own frame is only PM_DockWidgetFrameWidth thin.
constexpr int kGroupWindowFrameMargin = 8;

// A window that gets the hand-rolled floating chrome (widened resize frame,
// edge cursors, blank-area drags): Qt's tab-group window, or one of the
// right-column docks floating on its own.
bool is_floating_chrome_window(const QWidget* widget) {
  if (widget == nullptr || !widget->isWindow()) {
    return false;
  }
  if (qstrcmp(widget->metaObject()->className(), "QDockWidgetGroupWindow") == 0 ||
      widget->property("patchy.dockGroupWindow").toBool()) {
    return true;
  }
  const auto* dock = qobject_cast<const QDockWidget*>(widget);
  return dock != nullptr &&
         dock->findChild<QWidget*>(QStringLiteral("rightDockResizeHandle"), Qt::FindDirectChildrenOnly) !=
             nullptr;
}

Qt::Edges group_window_resize_edges(const QWidget* window, QPoint position) {
  Qt::Edges edges;
  if (position.x() <= kGroupWindowResizeMargin) {
    edges |= Qt::LeftEdge;
  }
  if (position.x() >= window->width() - kGroupWindowResizeMargin) {
    edges |= Qt::RightEdge;
  }
  if (position.y() <= kGroupWindowResizeMargin) {
    edges |= Qt::TopEdge;
  }
  if (position.y() >= window->height() - kGroupWindowResizeMargin) {
    edges |= Qt::BottomEdge;
  }
  return edges;
}

Qt::CursorShape group_window_resize_cursor(Qt::Edges edges) {
  const bool left = edges.testFlag(Qt::LeftEdge);
  const bool right = edges.testFlag(Qt::RightEdge);
  const bool top = edges.testFlag(Qt::TopEdge);
  const bool bottom = edges.testFlag(Qt::BottomEdge);
  if ((top && left) || (bottom && right)) {
    return Qt::SizeFDiagCursor;
  }
  if ((top && right) || (bottom && left)) {
    return Qt::SizeBDiagCursor;
  }
  if (left || right) {
    return Qt::SizeHorCursor;
  }
  return Qt::SizeVerCursor;
}

QToolButton* dock_collapse_toggle(const QDockWidget* dock) {
  const auto* title = dock->titleBarWidget();
  if (title == nullptr) {
    return nullptr;
  }
  for (auto* button : title->findChildren<QToolButton*>()) {
    if (button->property("dockCollapseButton").toBool()) {
      return button;
    }
  }
  return nullptr;
}

// A docked panel sharing a tab group in the main window. Qt sizes a tab group
// by its most restrictive tab (the smallest maximum height), so per-panel
// height pins affect every tab in the group.
bool dock_is_tabbed(QDockWidget* dock) {
  auto* main_window = qobject_cast<QMainWindow*>(dock->parentWidget());
  return main_window != nullptr && !main_window->tabifiedDockWidgets(dock).isEmpty();
}

// The height cap an expanded panel keeps while it stands alone (Properties
// stays short); in a tab group it would cap the whole group.
int expanded_dock_maximum_height(QDockWidget* dock) {
  return dock_is_tabbed(dock) ? QWIDGETSIZE_MAX : dock->property("patchy.expandedMaximumHeight").toInt();
}

}  // namespace

void install_collapsible_dock_title(QDockWidget* dock,
                                    QWidget* content,
                                    const QString& object_prefix,
                                    int expanded_minimum_height,
                                    int expanded_maximum_height,
                                    bool initially_expanded,
                                    int expanded_preferred_height,
                                    std::function<void(bool)> panel_toggled) {
  dock->setMinimumWidth(kRightDockMinimumWidth);
  content->setMinimumWidth(kRightDockMinimumWidth - kRightDockChromeWidth);

  auto* title = new QWidget(dock);
  title->setObjectName(object_prefix + QStringLiteral("DockTitle"));
  title->setMinimumWidth(kRightDockMinimumWidth - kRightDockChromeWidth);
  // The dock's width-drag handle overlays the leftmost strip of the dock,
  // title bar included; this inset keeps the toggle and label clear of it.
  title->setContentsMargins(kRightDockResizeHandleWidth, 0, 0, 0);
  auto* layout = new QHBoxLayout(title);
  layout->setContentsMargins(7, 3, 7, 3);
  layout->setSpacing(6);

  auto* toggle = new QToolButton(title);
  toggle->setObjectName(object_prefix + QStringLiteral("DockCollapseButton"));
  toggle->setProperty("dockCollapseButton", true);
  toggle->setAutoRaise(false);
  toggle->setCheckable(true);
  toggle->setChecked(initially_expanded);
  toggle->setText(initially_expanded ? QStringLiteral("v") : QStringLiteral(">"));
  toggle->setFixedSize(18, 18);
  bind_translated_tooltip(toggle, initially_expanded ? QT_TRANSLATE_NOOP("QObject", "Collapse panel")
                                                   : QT_TRANSLATE_NOOP("QObject", "Expand panel"), "QObject");
  apply_bound_translation(toggle);
  layout->addWidget(toggle);

  auto* label = new QLabel(dock->windowTitle(), title);
  label->setObjectName(object_prefix + QStringLiteral("DockTitleLabel"));
  if (dock->property(kTranslationTextProperty).isValid()) {
    label->setProperty(kTranslationContextProperty, dock->property(kTranslationContextProperty));
    label->setProperty(kTranslationTextProperty, dock->property(kTranslationTextProperty));
    apply_bound_translation(label);
  }
  layout->addWidget(label, 1);

  const auto expanded_boost_height = std::max(expanded_minimum_height, expanded_preferred_height);
  dock->setProperty("patchy.expandedMaximumHeight", expanded_maximum_height);
  const auto apply_expanded_state = [dock, content, toggle, expanded_boost_height,
                                     panel_toggled = std::move(panel_toggled)](bool expanded) {
    content->setVisible(expanded);
    toggle->setText(expanded ? QStringLiteral("v") : QStringLiteral(">"));
    bind_translated_tooltip(toggle, expanded ? QT_TRANSLATE_NOOP("QObject", "Collapse panel")
                                            : QT_TRANSLATE_NOOP("QObject", "Expand panel"), "QObject");
    apply_bound_translation(toggle);
    // Collapsed docks pin min == max to the exact title-bar height: every
    // collapsed panel renders as the same strip, and the dock area can
    // neither stretch it nor leave a dead band under the header. Expanding
    // demands the preferred height through a temporary minimum (the only
    // request the dock area is guaranteed to honor);
    // MainWindow::handle_right_dock_panel_toggled hands the slack back one
    // event-loop hop later so the demand never lingers in the window's
    // minimum size. A floor of 0 means the layout-derived natural minimum.
    const auto collapsed_height = dock->titleBarWidget()->sizeHint().height();
    dock->setMinimumHeight(expanded ? expanded_boost_height : collapsed_height);
    dock->setMaximumHeight(expanded ? expanded_dock_maximum_height(dock) : collapsed_height);
    dock->updateGeometry();
    if (panel_toggled) {
      panel_toggled(expanded);
    }
  };

  QObject::connect(toggle, &QToolButton::toggled, dock, apply_expanded_state);

  dock->setTitleBarWidget(title);
  apply_expanded_state(initially_expanded);
}

void MainWindow::install_right_dock_width_handle(QDockWidget* dock) {
  // Every dock in the right column hosts the 7px width handle at its left
  // edge, so the column divider is grabbable along its entire height. The
  // handle overlays the dock; title bars and panel contents carry a matching
  // left inset to stay clear of it.
  dock->setProperty("patchy.rightDockResizeHost", true);
  dock->installEventFilter(this);
  auto* handle = new QWidget(dock);
  handle->setObjectName(QStringLiteral("rightDockResizeHandle"));
  handle->setProperty("patchy.rightDockResizeHandle", true);
  handle->setAttribute(Qt::WA_StyledBackground, true);
  handle->setCursor(Qt::SplitHCursor);
  handle->installEventFilter(this);
  // Hover events drive the floating-frame edge cursors; harmless while
  // docked.
  dock->setAttribute(Qt::WA_Hover, true);
  dock->setMouseTracking(true);
  // A floating dock is not part of the column, so the column-width handle
  // hides until the dock returns to the stack. Floating also widens the
  // dock's frame into the same hittable, visibly-styled resize strip the tab
  // group window gets (the floatingChrome property carries the styling).
  connect(dock, &QDockWidget::topLevelChanged, handle, [dock, handle](bool floating) {
    handle->setVisible(!floating);
    dock->setContentsMargins(floating
                                 ? QMargins(kGroupWindowFrameMargin, kGroupWindowFrameMargin,
                                            kGroupWindowFrameMargin, kGroupWindowFrameMargin)
                                 : QMargins());
    dock->setProperty("floatingChrome", floating);
    dock->style()->unpolish(dock);
    dock->style()->polish(dock);
  });
  connect(dock, &QDockWidget::topLevelChanged, this, [this, dock](bool floating) {
    if (floating) {
      return;
    }
    // Re-docking goes through Qt's drop-preview gap item, whose slot can be
    // taller than a collapsed dock's pinned strip; the widget clamps to the
    // pin but the slot keeps the extra as a gap under the header that grows
    // with every float/re-dock cycle. Snap the slot back to the strip once
    // the plug has settled.
    QTimer::singleShot(0, this, [this, dock] {
      if (dock->isFloating() || dock->widget() == nullptr || dock->widget()->isVisible()) {
        return;
      }
      resizeDocks({dock}, {dock->minimumHeight()}, Qt::Vertical);
    });
  });
  // A move can put the panel in or out of a tab group or the column. Deferred
  // a hop because window() still reports the old top-level while
  // topLevelChanged is being emitted.
  const auto sync_later = [this] { QTimer::singleShot(0, this, [this] { sync_panel_collapse_toggles(); }); };
  connect(dock, &QDockWidget::topLevelChanged, this, sync_later);
  connect(dock, &QDockWidget::dockLocationChanged, this, sync_later);
  update_right_dock_resize_handle_geometry(dock);
}

void MainWindow::sync_panel_collapse_toggles() {
  for (auto* dock : findChildren<QDockWidget*>()) {
    auto* toggle = dock_collapse_toggle(dock);
    if (toggle == nullptr) {
      continue;
    }
    // Floating panels are always expanded: a collapsed strip is pinned to
    // min == max, and Qt cannot plug that into a floating tab group (the
    // group's layout has no room for the pin plus its tab bar, and the docked
    // partner blinks away). The collapse toggle only shows while the panel
    // sits in the main window's column.
    const bool in_main_window_column = qobject_cast<QMainWindow*>(dock->window()) != nullptr;
    toggle->setVisible(in_main_window_column);
    // A tab group collapses and expands as one (handle_right_dock_panel_toggled):
    // one collapsed tab would pin the whole group to a title strip with a
    // dead band below it, so a panel joining a group with an expanded tab
    // expands too.
    bool expand = !in_main_window_column;
    for (auto* partner : tabifiedDockWidgets(dock)) {
      const auto* partner_toggle = dock_collapse_toggle(partner);
      expand = expand || (partner_toggle != nullptr && partner_toggle->isChecked());
    }
    if (expand && !toggle->isChecked()) {
      toggle->setChecked(true);
    } else if (toggle->isChecked()) {
      dock->setMaximumHeight(expanded_dock_maximum_height(dock));
    }
  }
}

void MainWindow::update_right_dock_resize_handle_geometry(QWidget* host) {
  if (host == nullptr) {
    return;
  }
  auto* handle = host->findChild<QWidget*>(QStringLiteral("rightDockResizeHandle"), Qt::FindDirectChildrenOnly);
  if (handle == nullptr) {
    return;
  }
  handle->setGeometry(0, 0, kRightDockResizeHandleWidth, host->height());
  handle->raise();
}

void MainWindow::set_right_dock_stack_width(int width) {
  // Re-measure lazily: styling and retranslation both change the layers
  // panel's minimum after the docks are built.
  update_right_dock_minimum_width();
  const auto minimum = std::max(kRightDockMinimumWidth, right_dock_minimum_width_);
  const auto max_width = std::max(minimum, this->width() - 260);
  const auto target_width = std::clamp(width, minimum, max_width);
  right_dock_pinned_width_ = target_width;
  for (const auto& object_name : right_dock_stack_names()) {
    auto* dock = findChild<QDockWidget*>(object_name);
    if (dock == nullptr || dock->isFloating()) {
      // A floating dock keeps its own size; the column pin re-applies when
      // it docks back (the fixed widths of the docked stack set the column).
      continue;
    }
    dock->setFixedWidth(target_width);
    dock->updateGeometry();
  }
}

void MainWindow::update_right_dock_minimum_width() {
  const auto* layers_panel = findChild<QWidget*>(QStringLiteral("layersPanel"));
  const auto* panel_layout = layers_panel != nullptr ? layers_panel->layout() : nullptr;
  if (panel_layout == nullptr) {
    return;
  }
  if (blend_combo_ != nullptr) {
    // Under the app stylesheet the popup list is sized to the field, and the
    // open list spends part of that width on its frame and scroll bar, so the
    // field needs that much slack for the longest mode name not to elide in
    // the list. (Widening only the view does nothing: the styled popup
    // geometry ignores the view's minimum.)
    const auto scroll_extent =
        blend_combo_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, blend_combo_);
    blend_combo_->setMinimumWidth(blend_combo_->minimumSizeHint().width() + scroll_extent + 8);
  }
  // The platform styles the dock frame, so the chrome around the panel can
  // only be trusted once the dock has been laid out. An estimate that is off
  // in either direction breaks the minimum-width geometry: too small lets the
  // blend/opacity row overflow past the panel's right inset, too large leaves
  // slack that the row's trailing stretch absorbs, pulling the Fill spin box
  // out of line with the filter edit's right edge.
  auto* layers_dock = findChild<QDockWidget*>(QStringLiteral("layersDock"));
  if (layers_dock != nullptr && layers_panel->isVisible() && layers_panel->width() > 0 &&
      layers_dock->width() >= layers_panel->width()) {
    right_dock_chrome_width_ = std::min(layers_dock->width() - layers_panel->width(), 64);
  }
  const auto chrome =
      right_dock_chrome_width_ >= 0 ? right_dock_chrome_width_ : kRightDockChromeWidth;
  // The blend/opacity row is the widest content in the stack, and its spin
  // boxes re-measure for the translated prefixes, so a stack narrowed to the
  // hardcoded floor can clip the Fill spin box and the popup chevrons. The
  // panel layout's own minimum already accounts for those fixed widths.
  const auto minimum =
      std::max(kRightDockMinimumWidth, panel_layout->minimumSize().width() + chrome);
  if (minimum == right_dock_minimum_width_) {
    return;
  }
  right_dock_minimum_width_ = minimum;
  // Only the explicit pin tracks whether the stack holds an exact width: Qt's
  // main-window layout rewrites dock maximum sizes on its own, so they cannot
  // distinguish a pinned stack from a free one.
  if (right_dock_pinned_width_ > 0 && right_dock_pinned_width_ < minimum) {
    // A language switch can widen the row mid-session; keep the pin no
    // narrower than the new minimum.
    right_dock_pinned_width_ = minimum;
  }
  for (const auto& object_name : right_dock_stack_names()) {
    auto* dock = findChild<QDockWidget*>(object_name);
    if (dock == nullptr || dock->isFloating()) {
      continue;
    }
    if (right_dock_pinned_width_ > 0) {
      dock->setFixedWidth(right_dock_pinned_width_);
    } else {
      dock->setMinimumWidth(minimum);
    }
    dock->updateGeometry();
  }
}

void MainWindow::refresh_collapsed_right_dock_heights() {
  // The collapsed pins are computed from the title bar's sizeHint, which
  // grows by the title borders once the application stylesheet is applied.
  // Re-pin every still-collapsed dock so they all sit at the styled height;
  // docks toggled later recompute from the styled title on their own.
  for (auto* dock : findChildren<QDockWidget*>()) {
    auto* title = dock->titleBarWidget();
    auto* content = dock->widget();
    if (title == nullptr || content == nullptr || content->isVisibleTo(dock)) {
      continue;
    }
    const auto collapsed_height = title->sizeHint().height();
    dock->setMinimumHeight(collapsed_height);
    dock->setMaximumHeight(collapsed_height);
    dock->updateGeometry();
  }
}

void MainWindow::handle_right_dock_panel_toggled(QDockWidget* dock, bool expanded,
                                                 int expanded_minimum_height) {
  // Construction-time apply_expanded_state: the dock area is not laid out
  // yet, and no dock that starts expanded boosts past its floor.
  if (!isVisible()) {
    return;
  }
  // A tab group collapses and expands as one: its height follows the most
  // restrictive tab, so a lone collapsed tab leaves a dead band. Partners
  // already in the new state emit nothing, which ends the recursion.
  for (auto* partner : tabifiedDockWidgets(dock)) {
    if (auto* toggle = dock_collapse_toggle(partner); toggle != nullptr) {
      toggle->setChecked(expanded);
    }
  }
  const auto height_before = height();
  // One event-loop hop: the boosted minimum from apply_expanded_state has
  // been granted once the posted layout pass ran (timers fire after it).
  QTimer::singleShot(0, this, [this, dock, expanded, expanded_minimum_height, height_before] {
    // Release the expand demand down to the real floor. The dock keeps the
    // granted height, but the window's minimum no longer carries it, so a
    // later clamp or user resize can redistribute the column freely. Skip
    // the release when the panel was re-collapsed before this hop ran (the
    // collapsed min == max pin must survive).
    if (expanded && dock->widget() != nullptr && dock->widget()->isVisible()) {
      dock->setMinimumHeight(expanded_minimum_height);
    }
    if (dock->isFloating()) {
      // No column redistributes space for a floating dock, so an expand with
      // a zero boost (Info) leaves the window at the collapsed strip size;
      // grow it to fit the panel explicitly.
      if (expanded && dock->widget() != nullptr && dock->widget()->isVisible()) {
        const auto wanted = std::max(dock->sizeHint().height(), dock->minimumSizeHint().height());
        if (dock->height() < wanted) {
          dock->resize(dock->width(), wanted);
        }
      }
      // A floating dock's backing store also keeps stale pixels over the
      // area the toggle exposed, and the panel stays undrawn until a resize
      // forces a repaint. Replicate one programmatically, the same remedy as
      // resync_native_frame_geometry. The main-window clamp below is about
      // the docked column and does not apply here.
      const auto dock_size = dock->size();
      if (dock_size.width() > 0 && dock_size.height() > 0) {
        dock->resize(dock_size.width(), dock_size.height() + 1);
        dock->resize(dock_size);
      }
      dock->update();
      return;
    }
    if (layout() != nullptr) {
      // The release above only posts a layout request, but the clamp below
      // resizes through the window's explicit minimum, which the layout owns
      // and only rewrites during activation. Activate now so the clamp sees
      // the released minimum instead of the expand demand.
      layout()->activate();
    }
#ifdef Q_OS_WASM
    // The wasm backing store is not repainted where the relayout moved the
    // docks, leaving stale panel pixels until the next browser resize.
    update();
#endif
    // Re-clamp only when this toggle grew the window: offscreen tests open
    // windows larger than the platform screen, and an unconditional clamp
    // would shrink them behind the tests' backs.
    if (height() > height_before) {
      clamp_window_to_available_screen();
    }
  });
}

bool MainWindow::handle_right_dock_resize_event(QObject* watched, QEvent* event) {
  auto* widget = qobject_cast<QWidget*>(watched);
  if (widget == nullptr) {
    return false;
  }

  if (widget->property("patchy.rightDockResizeHost").toBool()) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
      update_right_dock_resize_handle_geometry(widget);
    }
    return false;
  }

  if (!widget->property("patchy.rightDockResizeHandle").toBool()) {
    return false;
  }

  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto* mouse_event = static_cast<QMouseEvent*>(event);
      if (mouse_event->button() != Qt::LeftButton) {
        return false;
      }
      auto* dock = qobject_cast<QDockWidget*>(widget->parentWidget());
      if (dock == nullptr || dock->isFloating()) {
        return false;
      }
      right_dock_resizing_ = true;
      right_dock_resize_start_global_ = mouse_event->globalPosition().toPoint();
      right_dock_resize_start_width_ = dock->width();
      // No explicit grabMouse: Qt's implicit press-grab already routes the
      // moves and release here, and an explicit grab is unreliable on wasm.
      mouse_event->accept();
      return true;
    }
    case QEvent::MouseMove: {
      auto* mouse_event = static_cast<QMouseEvent*>(event);
      if (!right_dock_resizing_ || (mouse_event->buttons() & Qt::LeftButton) == 0) {
        return false;
      }
      const auto delta = right_dock_resize_start_global_.x() - mouse_event->globalPosition().toPoint().x();
      set_right_dock_stack_width(right_dock_resize_start_width_ + delta);
      mouse_event->accept();
      return true;
    }
    case QEvent::MouseButtonRelease: {
      auto* mouse_event = static_cast<QMouseEvent*>(event);
      if (!right_dock_resizing_ || mouse_event->button() != Qt::LeftButton) {
        return false;
      }
      const auto delta = right_dock_resize_start_global_.x() - mouse_event->globalPosition().toPoint().x();
      set_right_dock_stack_width(right_dock_resize_start_width_ + delta);
      right_dock_resizing_ = false;
      mouse_event->accept();
      return true;
    }
    default:
      break;
  }

  return false;
}

bool MainWindow::handle_right_dock_title_drag_event(QObject* watched, QEvent* event) {
  // Dragging the collapsible title of a TABBED dock detaches that dock alone,
  // exactly like dragging its tab. Left to Qt, GroupedDragging floats the
  // whole tab group instead, and the resulting QDockWidgetGroupWindow is
  // broken with custom title bars: a second redundant header, a blank strip
  // that cannot be dragged, and a jump that re-anchors the grab to the group
  // title. Consuming the title press before QDockWidget sees it keeps that
  // window from ever forming; untabbed docks keep Qt's native title drag,
  // which behaves.
  if (right_dock_title_drag_dock_ != nullptr) {
    switch (event->type()) {
      case QEvent::MouseMove: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if ((mouse_event->buttons() & Qt::LeftButton) == 0) {
          right_dock_title_drag_dock_ = nullptr;
          right_dock_title_drag_started_ = false;
          return false;
        }
        const auto global = mouse_event->globalPosition().toPoint();
        if (!right_dock_title_drag_started_ &&
            (global - right_dock_title_drag_press_global_).manhattanLength() <
                QApplication::startDragDistance()) {
          mouse_event->accept();
          return true;
        }
        if (!right_dock_title_drag_started_) {
          right_dock_title_drag_started_ = true;
          right_dock_title_drag_dock_->setFloating(true);
        }
        right_dock_title_drag_dock_->move(global - right_dock_title_drag_offset_);
        mouse_event->accept();
        return true;
      }
      case QEvent::MouseButtonRelease: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() != Qt::LeftButton) {
          return false;
        }
        const auto started = right_dock_title_drag_started_;
        right_dock_title_drag_dock_ = nullptr;
        right_dock_title_drag_started_ = false;
        if (started) {
          mouse_event->accept();
          return true;
        }
        return false;
      }
      default:
        return false;
    }
  }

  if (event->type() != QEvent::MouseButtonPress) {
    return false;
  }
  auto* dock = qobject_cast<QDockWidget*>(watched);
  if (dock == nullptr || dock->isFloating() || dock->titleBarWidget() == nullptr ||
      tabifiedDockWidgets(dock).isEmpty() ||
      dock->findChild<QWidget*>(QStringLiteral("rightDockResizeHandle"), Qt::FindDirectChildrenOnly) ==
          nullptr) {
    return false;
  }
  auto* mouse_event = static_cast<QMouseEvent*>(event);
  if (mouse_event->button() != Qt::LeftButton ||
      !dock->titleBarWidget()->geometry().contains(mouse_event->position().toPoint())) {
    return false;
  }
  right_dock_title_drag_dock_ = dock;
  right_dock_title_drag_press_global_ = mouse_event->globalPosition().toPoint();
  right_dock_title_drag_offset_ = mouse_event->position().toPoint();
  right_dock_title_drag_started_ = false;
  mouse_event->accept();
  return true;
}

bool MainWindow::handle_dock_group_window_event(QObject* watched, QEvent* event) {
  // Qt's floating dock tab-group window ships with no grabbable chrome of
  // its own: presses on the blank strip beside the tabs do nothing, and its
  // edge-resize handler moves the borders without ever showing a resize
  // cursor. Give the blank areas window-drag behavior and the edges cursor
  // feedback; presses on the edges stay with Qt's own resize handling.
  if (dock_group_drag_window_ != nullptr) {
    switch (event->type()) {
      case QEvent::MouseMove: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if ((mouse_event->buttons() & Qt::LeftButton) == 0) {
          dock_group_drag_window_ = nullptr;
          return false;
        }
        const auto global = mouse_event->globalPosition().toPoint();
        if (dock_group_drag_edges_ == Qt::Edges{}) {
          dock_group_drag_window_->move(global - dock_group_drag_offset_);
        } else {
          const auto delta = global - dock_group_drag_press_global_;
          auto rect = dock_group_drag_origin_rect_;
          if (dock_group_drag_edges_.testFlag(Qt::LeftEdge)) {
            rect.setLeft(rect.left() + delta.x());
          }
          if (dock_group_drag_edges_.testFlag(Qt::RightEdge)) {
            rect.setRight(rect.right() + delta.x());
          }
          if (dock_group_drag_edges_.testFlag(Qt::TopEdge)) {
            rect.setTop(rect.top() + delta.y());
          }
          if (dock_group_drag_edges_.testFlag(Qt::BottomEdge)) {
            rect.setBottom(rect.bottom() + delta.y());
          }
          const auto minimum =
              dock_group_drag_window_->minimumSizeHint().expandedTo(dock_group_drag_window_->minimumSize());
          if (rect.width() < minimum.width()) {
            if (dock_group_drag_edges_.testFlag(Qt::LeftEdge)) {
              rect.setLeft(rect.right() - minimum.width() + 1);
            } else {
              rect.setRight(rect.left() + minimum.width() - 1);
            }
          }
          if (rect.height() < minimum.height()) {
            if (dock_group_drag_edges_.testFlag(Qt::TopEdge)) {
              rect.setTop(rect.bottom() - minimum.height() + 1);
            } else {
              rect.setBottom(rect.top() + minimum.height() - 1);
            }
          }
          dock_group_drag_window_->setGeometry(rect);
        }
        mouse_event->accept();
        return true;
      }
      case QEvent::MouseButtonRelease: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() != Qt::LeftButton) {
          return false;
        }
        dock_group_drag_window_ = nullptr;
        mouse_event->accept();
        return true;
      }
      default:
        return false;
    }
  }

  auto* widget = qobject_cast<QWidget*>(watched);
  if (widget == nullptr) {
    return false;
  }

  if (is_floating_chrome_window(widget)) {
    switch (event->type()) {
      case QEvent::Show:
        // Hover events drive the edge cursor; the private Qt window does not
        // enable them itself. The contents margins widen Qt's few-pixel
        // frame into a real, hittable resize strip.
        widget->setAttribute(Qt::WA_Hover, true);
        widget->setMouseTracking(true);
        widget->setContentsMargins(kGroupWindowFrameMargin, kGroupWindowFrameMargin,
                                   kGroupWindowFrameMargin, kGroupWindowFrameMargin);
        return false;
      case QEvent::HoverEnter:
      case QEvent::HoverMove: {
        const auto position = static_cast<QHoverEvent*>(event)->position().toPoint();
        const auto edges = group_window_resize_edges(widget, position);
        if (edges != Qt::Edges{}) {
          widget->setCursor(group_window_resize_cursor(edges));
        } else {
          widget->unsetCursor();
        }
        return false;
      }
      case QEvent::MouseMove: {
        // Mouse tracking is enabled on the group window, and a buttonless
        // move over its frame strip is the return path from a child (Qt
        // sends no Enter for child-to-parent crossings).
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->buttons() != Qt::NoButton) {
          return false;
        }
        const auto edges = group_window_resize_edges(widget, mouse_event->position().toPoint());
        if (edges != Qt::Edges{}) {
          widget->setCursor(group_window_resize_cursor(edges));
        } else {
          widget->unsetCursor();
        }
        return false;
      }
      case QEvent::Enter: {
        // Enter is delivered without any hover attributes, and the group
        // window's own exposed surface is only its few-pixel frame strip, so
        // entering it IS being near an edge. Hover events alone miss it: by
        // the time WA_Hover applies the cursor may already be over a child.
        const auto position = static_cast<QEnterEvent*>(event)->position().toPoint();
        const auto edges = group_window_resize_edges(widget, position);
        if (edges != Qt::Edges{}) {
          widget->setCursor(group_window_resize_cursor(edges));
        } else {
          widget->unsetCursor();
        }
        return false;
      }
      case QEvent::HoverLeave:
      case QEvent::Leave:
        widget->unsetCursor();
        return false;
      case QEvent::MouseButtonPress: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() != Qt::LeftButton) {
          return false;
        }
        const auto edges = group_window_resize_edges(widget, mouse_event->position().toPoint());
        // On a single floating dock the dock IS the window, so interior
        // presses here are title or content presses that propagated up; they
        // must reach QDockWidget, whose title drag is the only path that
        // tracks drop targets and re-docks. Claim only the frame strip.
        // A group window's interior surface is genuinely blank chrome, so
        // there interior presses move the window.
        if (edges == Qt::Edges{} && qobject_cast<QDockWidget*>(widget) != nullptr) {
          return false;
        }
        // Edge presses resize (our handler owns the whole widened strip; Qt's
        // QWidgetResizeHandler only covers its own few-pixel range). Anchor
        // the move to the frame origin: move() positions the frame while
        // mouse coordinates are client-relative, and the platform can pad a
        // frame margin between the two.
        dock_group_drag_window_ = widget;
        dock_group_drag_edges_ = edges;
        dock_group_drag_press_global_ = mouse_event->globalPosition().toPoint();
        dock_group_drag_origin_rect_ = widget->geometry();
        dock_group_drag_offset_ = mouse_event->globalPosition().toPoint() - widget->pos();
        mouse_event->accept();
        return true;
      }
      default:
        return false;
    }
  }

  // Entering any child clears the frame cursor: children inherit the group
  // window's cursor, so a lingering resize shape would cover the whole panel.
  if (event->type() == QEvent::Enter && is_floating_chrome_window(widget->window()) &&
      widget != widget->window()) {
    widget->window()->unsetCursor();
    return false;
  }

  // The tab bar swallows presses on its own blank stretch, so catch those at
  // the bar and start the same window drag.
  if (auto* tab_bar = qobject_cast<QTabBar*>(widget);
      tab_bar != nullptr && event->type() == QEvent::MouseButtonPress &&
      is_floating_chrome_window(tab_bar->window())) {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() == Qt::LeftButton &&
        tab_bar->tabAt(mouse_event->position().toPoint()) < 0) {
      dock_group_drag_window_ = tab_bar->window();
      dock_group_drag_edges_ = Qt::Edges{};
      dock_group_drag_offset_ =
          mouse_event->globalPosition().toPoint() - tab_bar->window()->pos();
      mouse_event->accept();
      return true;
    }
  }
  return false;
}

void MainWindow::show_brush_settings_panel() {
  if (brush_settings_dock_ == nullptr) {
    return;
  }
  brush_settings_dock_->show();
  if (auto* collapse = dock_collapse_toggle(brush_settings_dock_); collapse != nullptr) {
    collapse->setChecked(true);
  }
  brush_settings_dock_->raise();
  if (brush_settings_dock_->isFloating() || brush_settings_dock_->window() != this) {
    brush_settings_dock_->window()->raise();
  }
}

namespace {

// Panel arrangement settings (docs/dock-panels.md); persisted, never rename.
constexpr auto kPanelLayoutKey = "window/panelLayout";
constexpr auto kPanelLayoutDocksKey = "window/panelLayoutDocks";
constexpr auto kPanelExpandedKey = "window/panelExpanded";
// Bump when a change to the built-in arrangement must discard saved layouts:
// restoreState refuses a state saved under another version.
constexpr int kPanelLayoutVersion = 1;

}  // namespace

void MainWindow::restore_panel_layout() {
  // Capture the built-in arrangement before anything moves: Reset Panel
  // Layout returns to it, and it places panels the saved layout predates.
  default_panel_layout_ = saveState(kPanelLayoutVersion);
  default_panel_placements_.clear();
  pending_panel_sections_.clear();
  // Column order, so panels a saved layout lacks are placed (and tabbed) top to bottom.
  auto docks = findChildren<QDockWidget*>(Qt::FindDirectChildrenOnly);
  const auto column_index = [](const QDockWidget* dock) {
    const auto& column = right_dock_stack_names();
    return std::distance(column.begin(), std::find(column.begin(), column.end(), dock->objectName()));
  };
  std::stable_sort(docks.begin(), docks.end(),
                   [&](const QDockWidget* a, const QDockWidget* b) { return column_index(a) < column_index(b); });
  for (auto* dock : docks) {
    DefaultPanelPlacement placement;
    placement.name = dock->objectName();
    placement.area = dockWidgetArea(dock);
    for (const auto* partner : tabifiedDockWidgets(dock)) {
      placement.tab_partners.append(partner->objectName());
    }
    // Qt reports no tab partners for a panel that starts closed (it has no tab bar yet); such
    // panels name them in kBuiltInTabPartnersProperty.
    for (const auto& partner : dock->property(kBuiltInTabPartnersProperty).toStringList()) {
      if (!placement.tab_partners.contains(partner)) {
        placement.tab_partners.append(partner);
      }
    }
    const auto* toggle = dock_collapse_toggle(dock);
    placement.expanded = toggle == nullptr || toggle->isChecked();
    // The right column's built-in order, top to bottom: the panels whose sections follow.
    const auto& column = right_dock_stack_names();
    const auto position = std::find(column.begin(), column.end(), placement.name);
    if (placement.area == Qt::RightDockWidgetArea && position != column.end()) {
      for (auto later = std::next(position); later != column.end(); ++later) {
        if (!placement.tab_partners.contains(*later)) {
          placement.followers.append(*later);
        }
      }
    }
    default_panel_placements_.push_back(std::move(placement));
  }

  const auto settings = app_settings();
  const auto state = settings.value(QLatin1StringView(kPanelLayoutKey)).toByteArray();
  if (state.isEmpty()) {
    return;
  }
  // Collapse states first, so each docked panel's height pin matches the
  // sizes the layout was saved with.
  const auto expanded = settings.value(QLatin1StringView(kPanelExpandedKey)).toMap();
  for (auto* dock : findChildren<QDockWidget*>()) {
    auto* toggle = dock_collapse_toggle(dock);
    if (toggle != nullptr && expanded.contains(dock->objectName())) {
      toggle->setChecked(expanded.value(dock->objectName()).toBool());
    }
  }
  if (!restoreState(state, kPanelLayoutVersion)) {
    // Another version or unreadable data: start from the built-in layout.
    reset_panel_layout();
    return;
  }

  // Qt leaves a panel the state does not name wherever its old index path
  // lands, which can be inside an unrelated tab group. Give such panels their
  // built-in placement: tabbed with a default partner that is docked, else a
  // new section at the end of their default area.
  if (settings.contains(QLatin1StringView(kPanelLayoutDocksKey))) {
    auto placed = settings.value(QLatin1StringView(kPanelLayoutDocksKey)).toStringList();
    QStringList newly_placed;
    for (const auto& placement : default_panel_placements_) {
      auto* dock = findChild<QDockWidget*>(placement.name);
      if (dock == nullptr || placed.contains(placement.name)) {
        continue;
      }
      QDockWidget* partner = nullptr;
      for (const auto& name : placement.tab_partners) {
        auto* candidate = findChild<QDockWidget*>(name);
        // A partner placed here a moment ago counts even while closed (the brush panels start
        // closed and must stay one group).
        if (candidate != nullptr && placed.contains(name) && candidate->window() == this &&
            (!candidate->isHidden() || newly_placed.contains(name))) {
          partner = candidate;
          break;
        }
      }
      // tabifyDockWidget does not take the panel out of its current slot.
      const bool hidden = dock->isHidden();
      removeDockWidget(dock);
      if (partner != nullptr) {
        // Tabifying onto a closed partner leaves the two as separate sections once shown;
        // join them while both are shown, then close the partner again.
        const bool partner_hidden = partner->isHidden();
        partner->show();
        dock->show();
        tabifyDockWidget(partner, dock);
        partner->setVisible(!partner_hidden);
      } else {
        // A new section at the bottom of its area for now; the first show moves it above the
        // section of its built-in successor.
        addDockWidget(placement.area, dock);
        if (!placement.followers.isEmpty()) {
          pending_panel_sections_.emplace_back(placement.name, placement.followers);
        }
      }
      dock->setVisible(!hidden);
      placed.append(placement.name);
      newly_placed.append(placement.name);
    }
  }
  // Qt reports no top-level change for panels it puts in a floating
  // tab-group window.
  sync_panel_collapse_toggles();
}

void MainWindow::place_new_panel_sections() {
  const auto pending = std::exchange(pending_panel_sections_, {});
  for (const auto& [name, followers] : pending) {
    auto* dock = findChild<QDockWidget*>(name);
    if (dock == nullptr || dock->window() != this || dock->isFloating()) {
      continue;
    }
    const auto area = dockWidgetArea(dock);
    const auto dock_group = tabifiedDockWidgets(dock);
    // The area's docked sections (tab groups count once), top to bottom by their visible tab.
    struct Section {
      int top{0};
      QList<QDockWidget*> docks;
      QDockWidget* current{nullptr};
    };
    std::vector<Section> sections;
    for (auto* candidate : findChildren<QDockWidget*>(Qt::FindDirectChildrenOnly)) {
      if (candidate == dock || dock_group.contains(candidate) || candidate->isFloating() ||
          dockWidgetArea(candidate) != area || candidate->isHidden()) {
        continue;
      }
      const auto partners = tabifiedDockWidgets(candidate);
      const auto known = std::any_of(sections.begin(), sections.end(), [&](const Section& section) {
        return section.docks.contains(candidate);
      });
      if (known) {
        continue;
      }
      Section section;
      section.docks.append(candidate);
      for (auto* partner : partners) {
        if (!partner->isFloating() && partner->window() == this) {
          section.docks.append(partner);
        }
      }
      // Qt parks the hidden tabs of a group off screen; the shown one gives the position.
      section.top = std::numeric_limits<int>::max();
      for (auto* member : section.docks) {
        const auto top_left = member->mapTo(this, QPoint(0, 0));
        if (!member->isHidden() && top_left.x() >= 0 && top_left.y() >= 0 && top_left.y() < section.top) {
          section.top = top_left.y();
          section.current = member;
        }
      }
      if (section.current != nullptr) {
        sections.push_back(std::move(section));
      }
    }
    std::sort(sections.begin(), sections.end(),
              [](const Section& a, const Section& b) { return a.top < b.top; });
    auto first_moved = sections.end();
    for (const auto& follower : followers) {
      first_moved = std::find_if(sections.begin(), sections.end(), [&](const Section& section) {
        return std::any_of(section.docks.begin(), section.docks.end(),
                           [&](const QDockWidget* member) { return member->objectName() == follower; });
      });
      if (first_moved != sections.end()) {
        break;
      }
    }
    if (first_moved == sections.end()) {
      continue;  // nothing of its built-in successors is docked here: it stays at the bottom
    }
    // QMainWindow can only append a section, so re-append everything from the successor on
    // below the new panel, keeping each section's tabs, front tab and height.
    QList<QDockWidget*> resized;
    QList<int> heights;
    for (auto it = first_moved; it != sections.end(); ++it) {
      const auto height = it->current->height();
      std::vector<std::pair<QDockWidget*, bool>> members;
      for (auto* member : it->docks) {
        members.emplace_back(member, member->isHidden());
        removeDockWidget(member);
      }
      auto* first = members.front().first;
      addDockWidget(area, first);
      for (std::size_t index = 1; index < members.size(); ++index) {
        tabifyDockWidget(first, members[index].first);
      }
      for (const auto& [member, hidden] : members) {
        member->setVisible(!hidden);
      }
      it->current->raise();
      resized.append(it->current);
      heights.append(height);
    }
    resizeDocks(resized, heights, Qt::Vertical);
  }
  sync_panel_collapse_toggles();
}

void MainWindow::save_panel_layout() const {
  QStringList docks;
  QVariantMap expanded;
  for (const auto* dock : findChildren<QDockWidget*>()) {
    docks.append(dock->objectName());
    if (const auto* toggle = dock_collapse_toggle(dock); toggle != nullptr) {
      expanded.insert(dock->objectName(), toggle->isChecked());
    }
  }
  auto settings = app_settings();
  settings.setValue(QLatin1StringView(kPanelLayoutKey), saveState(kPanelLayoutVersion));
  settings.setValue(QLatin1StringView(kPanelLayoutDocksKey), docks);
  settings.setValue(QLatin1StringView(kPanelExpandedKey), expanded);
}

void MainWindow::reset_panel_layout() {
  if (default_panel_layout_.isEmpty()) {
    return;
  }
  // restoreState reparents panels out of a floating tab-group window but
  // leaves them in the group's layout, which then floats one of them again.
  // Taking them out through addDockWidget empties the group the way a drag
  // does, and Qt deletes the empty window.
  for (auto* dock : findChildren<QDockWidget*>()) {
    if (dock->window() != this && !dock->isWindow()) {
      addDockWidget(Qt::RightDockWidgetArea, dock);
    }
  }
  restoreState(default_panel_layout_, kPanelLayoutVersion);
  for (const auto& placement : default_panel_placements_) {
    auto* dock = findChild<QDockWidget*>(placement.name);
    auto* toggle = dock != nullptr ? dock_collapse_toggle(dock) : nullptr;
    if (toggle != nullptr) {
      toggle->setChecked(placement.expanded);
    }
  }
  sync_panel_collapse_toggles();
  // The built-in column has no explicit width: drop any width-handle pin.
  right_dock_pinned_width_ = 0;
  for (const auto& name : right_dock_stack_names()) {
    if (auto* dock = findChild<QDockWidget*>(name); dock != nullptr) {
      dock->setMinimumWidth(std::max(kRightDockMinimumWidth, right_dock_minimum_width_));
      dock->setMaximumWidth(QWIDGETSIZE_MAX);
    }
  }
}

}  // namespace patchy::ui
