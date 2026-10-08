// MainWindow::build_tool_palette(): the tool-palette phase of create_actions()
// (the left QToolBar, tool actions and flyouts, color buttons and the quick-
// mask button), split out of main_window_actions.cpp along with
// add_tool_action (this TU is its only caller) and the anonymous-namespace
// tool helpers only they use.
// Pure function moves; behavior must stay identical, and the construction
// order is load-bearing (see create_actions() for the phase order).

#include "ui/main_window.hpp"
#include "ui/icon_theme.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/main_window_actions_internal.hpp"

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
#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
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
#include "ui/tool_traits.hpp"
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

QString tool_action_object_name(CanvasTool tool) {
  auto name = QString::fromLatin1(tool_traits(tool).name);
  name.remove(QLatin1Char(' '));
  return QStringLiteral("tool") + name + QStringLiteral("Action");
}

QKeySequence tool_default_shortcut(const ToolTraits& traits) {
  return traits.default_key != 0 ? QKeySequence(traits.default_key) : QKeySequence();
}

class MouseDoubleClickFilter final : public QObject {
public:
  MouseDoubleClickFilter(std::function<void()> callback, QObject* parent)
      : QObject(parent), callback_(std::move(callback)) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto* mouse_event = static_cast<QMouseEvent*>(event);
      if (mouse_event->button() == Qt::LeftButton) {
        if (callback_) {
          callback_();
        }
        mouse_event->accept();
        return true;
      }
    }
    return QObject::eventFilter(watched, event);
  }

private:
  std::function<void()> callback_;
};

class ToolFlyoutEventFilter final : public QObject {
public:
  // `double_click` gets the first say on a left double-click (true consumes
  // it); by default a double-click opens the menu.
  ToolFlyoutEventFilter(std::function<void()> open_menu, QObject* parent, std::function<bool()> double_click = {})
      : QObject(parent), open_menu_(std::move(open_menu)), double_click_(std::move(double_click)) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
      auto* mouse_event = static_cast<QMouseEvent*>(event);
      if (mouse_event->button() == Qt::RightButton && event->type() == QEvent::MouseButtonPress) {
        if (open_menu_) {
          open_menu_();
        }
        mouse_event->accept();
        return true;
      }
      if (mouse_event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonDblClick) {
        if (!(double_click_ && double_click_()) && open_menu_) {
          open_menu_();
        }
        mouse_event->accept();
        return true;
      }
    }
    return QObject::eventFilter(watched, event);
  }

private:
  std::function<void()> open_menu_;
  std::function<bool()> double_click_;
};

// Stock QToolBar collapses an expanded overflow bar half a second after the
// pointer leaves it, which makes the palette's second column nearly
// unreachable. Swallowing Leave while the extension button is checked turns
// the expansion into a real toggle; collapse happens through the extension
// button, or automatically once every item fits again (see
// ToolPaletteExpansionReservation).
class ToolPaletteExpansionLock final : public QObject {
public:
  ToolPaletteExpansionLock(QToolButton* extension, QObject* parent)
      : QObject(parent), extension_(extension) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::Leave && extension_ != nullptr && extension_->isChecked()) {
      return true;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  QPointer<QToolButton> extension_;
};

constexpr int kToolPaletteCollapsedMinWidth = 43;

// Qt expands the overflow columns by giving the bar a geometry wider than its
// toolbar-area slot, overlaying the canvas. While the extension button stays
// checked, this controller copies the measured expanded width into the bar's
// minimum width so the main-window layout reserves the columns and the canvas
// shrinks instead of sitting underneath them. Each pass releases the minimum
// first (a standing minimum would stop Qt from dropping back to fewer columns
// when the window grows), measures the width Qt actually applied, and re-pins
// it; identical measurements converge to a no-op. Once the whole palette fits
// its slot again the expansion has nothing left to reveal, so the controller
// clicks the extension closed instead of holding a stale empty column.
class ToolPaletteExpansionReservation final : public QObject {
public:
  ToolPaletteExpansionReservation(QToolBar* palette, QToolButton* extension)
      : QObject(palette), palette_(palette), extension_(extension) {
    palette->installEventFilter(this);
    connect(extension, &QToolButton::toggled, this, [this](bool checked) {
      if (checked) {
        schedule_reservation();
        return;
      }
      if (palette_ == nullptr) {
        return;
      }
      palette_->setMinimumWidth(kToolPaletteCollapsedMinWidth);
      // One turn later (after Qt's own collapse handling), force a toolbar
      // layout pass: when the collapsed geometry matches the expanded one (a
      // tall window whose expansion had nothing left to reveal), Qt skips
      // setGeometry and would leave the extension button visible.
      QTimer::singleShot(0, this, [this] {
        if (palette_ == nullptr || extension_ == nullptr || extension_->isChecked()) {
          return;
        }
        if (auto* bar_layout = palette_->layout(); bar_layout != nullptr) {
          bar_layout->invalidate();
          bar_layout->activate();
        }
      });
    });
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::Resize && !applying_ && extension_ != nullptr && extension_->isChecked()) {
      schedule_reservation();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void schedule_reservation() {
    if (scheduled_) {
      return;
    }
    scheduled_ = true;
    // Deferred one event-loop turn so a single pass covers a burst of layout
    // activity (the toggle itself, animation frames, a window resize).
    QTimer::singleShot(0, this, [this] {
      scheduled_ = false;
      apply_reservation();
    });
  }

  void apply_reservation() {
    if (palette_ == nullptr || extension_ == nullptr || !extension_->isChecked()) {
      return;
    }
    applying_ = true;
    palette_->setMinimumWidth(kToolPaletteCollapsedMinWidth);
    if (auto* window_layout = palette_->window()->layout(); window_layout != nullptr) {
      // setMinimumWidth short-circuits when the value is unchanged, so force
      // the relayout that snaps any in-flight expansion animation and lets Qt
      // settle on the geometry it really wants before it is measured.
      window_layout->invalidate();
      window_layout->activate();
    }
    const bool fits_collapsed = palette_->sizeHint().height() <= palette_->height();
    const int expanded_width = palette_->width();
    if (!fits_collapsed && expanded_width > kToolPaletteCollapsedMinWidth) {
      palette_->setMinimumWidth(expanded_width);
    }
    applying_ = false;
    if (fits_collapsed) {
      // The window grew tall enough that nothing overflows. Qt never unchecks
      // the extension on its own, so close the expansion formally.
      QTimer::singleShot(0, extension_, [extension = extension_] {
        if (extension != nullptr && extension->isChecked()) {
          extension->click();
        }
      });
    }
  }

  QPointer<QToolBar> palette_;
  QPointer<QToolButton> extension_;
  bool scheduled_{false};
  bool applying_{false};
};

// Tool icons are hand-authored SVGs in src/ui/icons/tool-*.svg (32x32 viewBox,
// icon_ink strokes with one optional icon_accent accent). Author them in the dark
// values from theme_palette.hpp; icon_theme.cpp recolors them per scheme at paint
// time. Review them with the ui_tool_palette_icons_render_sheet visual test.
QIcon tool_icon(CanvasTool tool) {
  static const int icon_resources = ::qInitResources_icons();
  (void)icon_resources;
  return themed_svg_icon(QLatin1String(tool_traits(tool).icon));
}

}  // namespace

void MainWindow::build_tool_palette(ActionBuildContext& ctx) {
  auto* tool_palette = new QToolBar(tr("Tool Palette"), this);
  tool_palette->setObjectName(QStringLiteral("toolPalette"));
  tool_palette->setOrientation(Qt::Vertical);
  tool_palette->setMovable(false);
  tool_palette->setFloatable(false);
  tool_palette->setAllowedAreas(Qt::LeftToolBarArea);
  tool_palette->setToolButtonStyle(Qt::ToolButtonIconOnly);
  tool_palette->setIconSize(QSize(20, 20));
  // Minimum, not fixed: the overflow extension button reveals hidden items by
  // widening the bar into extra columns, and a max width clamps that geometry
  // into a clipped sliver. While expanded, ToolPaletteExpansionReservation
  // raises this minimum to the measured expanded width so the layout reserves
  // the columns. QSS caps the children narrower, so the collapsed width stays
  // exactly 43.
  tool_palette->setMinimumWidth(kToolPaletteCollapsedMinWidth);
  addToolBar(Qt::LeftToolBarArea, tool_palette);

  auto* tool_group = new QActionGroup(this);
  tool_group->setExclusive(true);
  tool_action_group_ = tool_group;
  // The palette is ordered in clusters split by separators: select, paint,
  // retouch, draw/type, view. Tools sharing a slot get a flyout button.
  const auto create_flyout_tool_action =
      [this, tool_group](QMenu* menu, CanvasTool tool) {
        const auto& traits = tool_traits(tool);
        auto* action = new QAction(tr(traits.name), this);
        bind_action_text(action, traits.name);
        action->setIcon(tool_icon(tool));
        action->setCheckable(true);
        action->setData(static_cast<int>(tool));
        action->setObjectName(tool_action_object_name(tool));
        if (traits.tooltip_detail != nullptr) {
          action->setProperty(kActionTooltipDetailProperty, QString::fromLatin1(traits.tooltip_detail));
        }
        register_hotkey(action, QString::fromLatin1(traits.hotkey_id), tool_default_shortcut(traits),
                        QStringLiteral("tools"));
        tool_group->addAction(action);
        menu->addAction(action);
        addAction(action);
        register_document_action(action);
        return action;
      };
  // Every flyout also owns a "Cycle <Group> Tools" command on Shift+<the group's
  // letter>, Photoshop's Shift+key convention (GitHub issue 45): it selects the
  // member after the active tool when the active tool is in the group, otherwise
  // the member after the one the button shows, wrapping. Qt fires neither of two
  // actions that share a sequence, so the members carry no Shift default of their
  // own; a user override that puts Shift+<letter> back on a member wins through
  // the registry's resolution and suppresses the cycle default.
  const auto configure_tool_flyout = [this](QToolBar* palette, QMenu* menu, QToolButton* button,
                                            QAction* default_action, std::initializer_list<QAction*> actions,
                                            const char* cycle_source, const char* cycle_object_name,
                                            const char* cycle_id, QKeySequence cycle_shortcut,
                                            std::function<bool()> double_click = {}) {
    button->setProperty("toolFlyout", true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setPopupMode(QToolButton::DelayedPopup);
    button->setMenu(menu);
    button->setDefaultAction(default_action);
    button->setToolTip(default_action->toolTip());
    palette->addWidget(button);
    // Press-and-hold opens the flyout after SH_ToolButton_PopupDelay (the
    // InteractionHintsStyle in main.cpp). A left double-click is the second
    // way in: Qt would otherwise route the pair's second press back to
    // mousePressEvent and restart the hold timer, so swallow it and open the
    // menu through the same showMenu() path the timer uses. The first click
    // of the pair still selects the default tool, as in Photoshop.
    button->installEventFilter(
        new ToolFlyoutEventFilter([button] { button->showMenu(); }, button, std::move(double_click)));
    for (auto* action : actions) {
      QObject::connect(action, &QAction::triggered, button, [button, menu, action] {
        button->setDefaultAction(action);
        button->setMenu(menu);
        button->setToolTip(action->toolTip());
      });
    }
    const QList<QAction*> members(actions);
    auto* cycle = new QAction(tr(cycle_source), this);
    bind_action_text(cycle, cycle_source);
    cycle->setObjectName(QString::fromLatin1(cycle_object_name));
    cycle->setMenuRole(QAction::NoRole);
    register_hotkey(cycle, QString::fromLatin1(cycle_id), cycle_shortcut, QStringLiteral("tools"));
    addAction(cycle);
    register_document_action(cycle);
    // Listed after the members so the flyout shows the key that walks it.
    menu->addSeparator();
    menu->addAction(cycle);
    QObject::connect(cycle, &QAction::triggered, button, [button, members] {
      int current = -1;
      for (int i = 0; i < members.size(); ++i) {
        if (members[i]->isChecked()) {
          current = i;
        }
      }
      if (current < 0) {
        current = static_cast<int>(members.indexOf(button->defaultAction()));
      }
      members[(current + 1) % members.size()]->trigger();
    });
  };

  move_tool_action_ = add_tool_action(tool_palette, tool_group, CanvasTool::Move);
  auto* marquee_menu = new QMenu(tr("Marquee Tools"), tool_palette);
  marquee_menu->setObjectName(QStringLiteral("marqueeToolMenu"));
  bind_widget_text(marquee_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Marquee Tools"));
  auto* rect_marquee_action = create_flyout_tool_action(marquee_menu, CanvasTool::Marquee);
  auto* elliptical_marquee_action = create_flyout_tool_action(marquee_menu, CanvasTool::EllipticalMarquee);
  auto* marquee_tool_button = new QToolButton(tool_palette);
  marquee_tool_button->setObjectName(QStringLiteral("marqueeToolButton"));
  configure_tool_flyout(tool_palette, marquee_menu, marquee_tool_button, rect_marquee_action,
                        {rect_marquee_action, elliptical_marquee_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Marquee Tools"),
                        "toolCycleMarqueeAction", "tools.cycle.marquee", QKeySequence(Qt::SHIFT | Qt::Key_M));
  auto* lasso_menu = new QMenu(tr("Lasso Tools"), tool_palette);
  lasso_menu->setObjectName(QStringLiteral("lassoToolMenu"));
  bind_widget_text(lasso_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Lasso Tools"));
  auto* lasso_action = create_flyout_tool_action(lasso_menu, CanvasTool::Lasso);
  auto* magnetic_lasso_action = create_flyout_tool_action(lasso_menu, CanvasTool::MagneticLasso);
  auto* lasso_tool_button = new QToolButton(tool_palette);
  lasso_tool_button->setObjectName(QStringLiteral("lassoToolButton"));
  configure_tool_flyout(tool_palette, lasso_menu, lasso_tool_button, lasso_action,
                        {lasso_action, magnetic_lasso_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Lasso Tools"),
                        "toolCycleLassoAction", "tools.cycle.lasso", QKeySequence(Qt::SHIFT | Qt::Key_L));
  auto* wand_menu = new QMenu(tr("Wand Tools"), tool_palette);
  wand_menu->setObjectName(QStringLiteral("wandToolMenu"));
  bind_widget_text(wand_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Wand Tools"));
  auto* magic_wand_action = create_flyout_tool_action(wand_menu, CanvasTool::MagicWand);
  auto* quick_select_action = create_flyout_tool_action(wand_menu, CanvasTool::QuickSelect);
  auto* wand_tool_button = new QToolButton(tool_palette);
  wand_tool_button->setObjectName(QStringLiteral("wandToolButton"));
  configure_tool_flyout(tool_palette, wand_menu, wand_tool_button, magic_wand_action,
                        {magic_wand_action, quick_select_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Wand Tools"),
                        "toolCycleWandAction", "tools.cycle.wand", QKeySequence(Qt::SHIFT | Qt::Key_W));
  add_tool_action(tool_palette, tool_group, CanvasTool::Crop);
  tool_palette->addSeparator();

  // The Brush Tools flyout: Brush (B, the default) plus Color Replacement,
  // Photoshop's brush group. Mixer Brush stays in the Detail flyout.
  auto* brush_menu = new QMenu(tr("Brush Tools"), tool_palette);
  brush_menu->setObjectName(QStringLiteral("brushToolMenu"));
  bind_widget_text(brush_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Brush Tools"));
  auto* brush_action = create_flyout_tool_action(brush_menu, CanvasTool::Brush);
  brush_action->setChecked(true);
  auto* color_replacement_action = create_flyout_tool_action(brush_menu, CanvasTool::ColorReplacement);
  auto* brush_tool_button = new QToolButton(tool_palette);
  brush_tool_button->setObjectName(QStringLiteral("brushToolButton"));
  configure_tool_flyout(tool_palette, brush_menu, brush_tool_button, brush_action,
                        {brush_action, color_replacement_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Brush Tools"),
                        "toolCycleBrushAction", "tools.cycle.brush", QKeySequence(Qt::SHIFT | Qt::Key_B));
  add_tool_action(tool_palette, tool_group, CanvasTool::Eraser);
  auto* gradient_menu = new QMenu(tr("Fill Tools"), tool_palette);
  gradient_menu->setObjectName(QStringLiteral("gradientToolMenu"));
  bind_widget_text(gradient_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Fill Tools"));
  auto* gradient_action = create_flyout_tool_action(gradient_menu, CanvasTool::Gradient);
  auto* fill_action = create_flyout_tool_action(gradient_menu, CanvasTool::Fill);
  auto* gradient_tool_button = new QToolButton(tool_palette);
  gradient_tool_button->setObjectName(QStringLiteral("gradientToolButton"));
  configure_tool_flyout(tool_palette, gradient_menu, gradient_tool_button, gradient_action,
                        {gradient_action, fill_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Fill Tools"),
                        "toolCycleFillAction", "tools.cycle.gradient", QKeySequence(Qt::SHIFT | Qt::Key_G));
  tool_palette->addSeparator();

  auto* stamp_menu = new QMenu(tr("Stamp Tools"), tool_palette);
  stamp_menu->setObjectName(QStringLiteral("stampToolMenu"));
  bind_widget_text(stamp_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Stamp Tools"));
  auto* clone_action = create_flyout_tool_action(stamp_menu, CanvasTool::Clone);
  auto* pattern_stamp_action = create_flyout_tool_action(stamp_menu, CanvasTool::PatternStamp);
  auto* stamp_tool_button = new QToolButton(tool_palette);
  stamp_tool_button->setObjectName(QStringLiteral("stampToolButton"));
  configure_tool_flyout(tool_palette, stamp_menu, stamp_tool_button, clone_action,
                        {clone_action, pattern_stamp_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Stamp Tools"),
                        "toolCycleStampAction", "tools.cycle.stamp", QKeySequence(Qt::SHIFT | Qt::Key_S));
  // Photoshop pairs the History Brush with the Art History Brush in a flyout;
  // Patchy has only the first, so it is a plain button with no Shift+Y cycle.
  add_tool_action(tool_palette, tool_group, CanvasTool::HistoryBrush);
  auto* healing_menu = new QMenu(tr("Healing Tools"), tool_palette);
  healing_menu->setObjectName(QStringLiteral("healingToolMenu"));
  bind_widget_text(healing_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Healing Tools"));
  auto* healing_action = create_flyout_tool_action(healing_menu, CanvasTool::Healing);
  auto* spot_healing_action = create_flyout_tool_action(healing_menu, CanvasTool::SpotHealing);
  auto* patch_action = create_flyout_tool_action(healing_menu, CanvasTool::PatchTool);
  auto* healing_tool_button = new QToolButton(tool_palette);
  healing_tool_button->setObjectName(QStringLiteral("healingToolButton"));
  configure_tool_flyout(tool_palette, healing_menu, healing_tool_button, healing_action,
                        {healing_action, spot_healing_action, patch_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Healing Tools"),
                        "toolCycleHealingAction", "tools.cycle.healing", QKeySequence(Qt::SHIFT | Qt::Key_J));

  auto* detail_menu = new QMenu(tr("Detail Tools"), tool_palette);
  detail_menu->setObjectName(QStringLiteral("detailToolMenu"));
  bind_widget_text(detail_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Detail Tools"));
  auto* smudge_action = create_flyout_tool_action(detail_menu, CanvasTool::Smudge);
  auto* mixer_brush_action = create_flyout_tool_action(detail_menu, CanvasTool::MixerBrush);
  auto* blur_action = create_flyout_tool_action(detail_menu, CanvasTool::BlurBrush);
  auto* sharpen_action = create_flyout_tool_action(detail_menu, CanvasTool::SharpenBrush);
  auto* detail_button = new QToolButton(tool_palette);
  detail_button->setObjectName(QStringLiteral("detailToolButton"));
  configure_tool_flyout(tool_palette, detail_menu, detail_button, smudge_action,
                        {smudge_action, mixer_brush_action, blur_action, sharpen_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Detail Tools"),
                        "toolCycleDetailAction", "tools.cycle.detail", QKeySequence(Qt::SHIFT | Qt::Key_R));

  auto* tone_menu = new QMenu(tr("Toning Tools"), tool_palette);
  tone_menu->setObjectName(QStringLiteral("toneToolMenu"));
  bind_widget_text(tone_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Toning Tools"));
  auto* dodge_action = create_flyout_tool_action(tone_menu, CanvasTool::Dodge);
  auto* burn_action = create_flyout_tool_action(tone_menu, CanvasTool::Burn);
  auto* sponge_action = create_flyout_tool_action(tone_menu, CanvasTool::Sponge);
  auto* tone_button = new QToolButton(tool_palette);
  tone_button->setObjectName(QStringLiteral("toneToolButton"));
  configure_tool_flyout(tool_palette, tone_menu, tone_button, dodge_action,
                        {dodge_action, burn_action, sponge_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Toning Tools"),
                        "toolCycleToningAction", "tools.cycle.tone", QKeySequence(Qt::SHIFT | Qt::Key_O));
  tool_palette->addSeparator();

  // The Pen Tools flyout: the Pen (default, and what P re-selects) plus the
  // dedicated Add/Delete/Convert anchor tools Photoshop users look for.
  auto* pen_menu = new QMenu(tr("Pen Tools"), tool_palette);
  pen_menu->setObjectName(QStringLiteral("penToolMenu"));
  bind_widget_text(pen_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pen Tools"));
  auto* pen_action = create_flyout_tool_action(pen_menu, CanvasTool::Pen);
  auto* add_anchor_action = create_flyout_tool_action(pen_menu, CanvasTool::AddAnchor);
  auto* delete_anchor_action = create_flyout_tool_action(pen_menu, CanvasTool::DeleteAnchor);
  auto* convert_point_action = create_flyout_tool_action(pen_menu, CanvasTool::ConvertPoint);
  auto* pen_button = new QToolButton(tool_palette);
  pen_button->setObjectName(QStringLiteral("penToolButton"));
  configure_tool_flyout(tool_palette, pen_menu, pen_button, pen_action,
                        {pen_action, add_anchor_action, delete_anchor_action, convert_point_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Pen Tools"),
                        "toolCyclePenAction", "tools.cycle.pen", QKeySequence(Qt::SHIFT | Qt::Key_P));
  // The Path Tools flyout sits directly after the Pen: drawing and adjusting
  // paths alternate constantly, so the two slots stay adjacent.
  auto* path_select_menu = new QMenu(tr("Path Tools"), tool_palette);
  path_select_menu->setObjectName(QStringLiteral("pathSelectToolMenu"));
  bind_widget_text(path_select_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Path Tools"));
  auto* path_select_action = create_flyout_tool_action(path_select_menu, CanvasTool::PathSelect);
  auto* direct_select_action = create_flyout_tool_action(path_select_menu, CanvasTool::DirectSelect);
  auto* path_select_button = new QToolButton(tool_palette);
  path_select_button->setObjectName(QStringLiteral("pathSelectToolButton"));
  configure_tool_flyout(tool_palette, path_select_menu, path_select_button, path_select_action,
                        {path_select_action, direct_select_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Path Tools"),
                        "toolCyclePathAction", "tools.cycle.path_select", QKeySequence(Qt::SHIFT | Qt::Key_A));
  auto* shape_menu = new QMenu(tr("Shape Tools"), tool_palette);
  shape_menu->setObjectName(QStringLiteral("shapeToolMenu"));
  bind_widget_text(shape_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Shape Tools"));
  auto* line_tool_action = create_flyout_tool_action(shape_menu, CanvasTool::Line);
  auto* rect_tool_action = create_flyout_tool_action(shape_menu, CanvasTool::Rectangle);
  auto* ellipse_tool_action = create_flyout_tool_action(shape_menu, CanvasTool::Ellipse);
  auto* polygon_tool_action = create_flyout_tool_action(shape_menu, CanvasTool::Polygon);
  auto* custom_shape_tool_action = create_flyout_tool_action(shape_menu, CanvasTool::CustomShape);
  auto* shape_tool_button = new QToolButton(tool_palette);
  shape_tool_button->setObjectName(QStringLiteral("shapeToolButton"));
  configure_tool_flyout(tool_palette, shape_menu, shape_tool_button, rect_tool_action,
                        {line_tool_action, rect_tool_action, ellipse_tool_action,
                         polygon_tool_action, custom_shape_tool_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle Shape Tools"),
                        "toolCycleShapeAction", "tools.cycle.shape", QKeySequence(Qt::SHIFT | Qt::Key_U));
  type_tool_action_ = add_tool_action(tool_palette, tool_group, CanvasTool::Text);
  tool_palette->addSeparator();

  add_tool_action(tool_palette, tool_group, CanvasTool::Eyedropper);
  // The View Tools flyout pairs the Hand with Rotate View, as in Photoshop.
  // R belongs to Rotate View (Photoshop's default); Smudge ships unbound and
  // Shift+H walks the flyout (Shift+R stays on the Detail flyout).
  auto* view_menu = new QMenu(tr("View Tools"), tool_palette);
  view_menu->setObjectName(QStringLiteral("viewToolMenu"));
  bind_widget_text(view_menu, QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "View Tools"));
  auto* hand_action = create_flyout_tool_action(view_menu, CanvasTool::Pan);
  auto* rotate_view_action = create_flyout_tool_action(view_menu, CanvasTool::RotateView);
  auto* view_tool_button = new QToolButton(tool_palette);
  view_tool_button->setObjectName(QStringLiteral("viewToolButton"));
  // Double-clicking the Rotate View tool resets the view angle (Photoshop);
  // with the Hand showing, the double-click opens the flyout like every other.
  configure_tool_flyout(tool_palette, view_menu, view_tool_button, hand_action, {hand_action, rotate_view_action},
                        QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Cycle View Tools"),
                        "toolCycleViewAction", "tools.cycle.view", QKeySequence(Qt::SHIFT | Qt::Key_H),
                        [this, view_tool_button, rotate_view_action] {
                          if (view_tool_button->defaultAction() != rotate_view_action) {
                            return false;
                          }
                          reset_view_rotation();
                          return true;
                        });
  auto* zoom_tool_action = add_tool_action(tool_palette, tool_group, CanvasTool::Zoom);
  if (auto* zoom_button = qobject_cast<QToolButton*>(tool_palette->widgetForAction(zoom_tool_action));
      zoom_button != nullptr) {
    zoom_button->setObjectName(QStringLiteral("zoomToolButton"));
    zoom_button->installEventFilter(new MouseDoubleClickFilter(
        [this] {
          if (canvas_ != nullptr) {
            canvas_->set_view_zoom_centered(1.0);
            refresh_document_info();
            statusBar()->showMessage(tr("Actual Pixels"));
          }
        },
        zoom_button));
  }
  connect(tool_group, &QActionGroup::triggered, this, [this](QAction* action) {
    if (canvas_ == nullptr) {
      return;
    }
    const auto selected = static_cast<CanvasTool>(action->data().toInt());
    if (canvas_->layer_transform_session_active()) {
      canvas_->commit_modal_session();
    }
    if (selected != CanvasTool::Text) {
      finish_active_text_editor();
    }
    current_tool_ = selected;
    canvas_->set_tool(selected);
    apply_selection_edge_settings_for_tool(selected);
    set_eraser_brush_settings_active(selected == CanvasTool::Eraser);
    if (selected != CanvasTool::Text ||
        canvas_->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor")) == nullptr) {
      canvas_->setFocus(Qt::OtherFocusReason);
    }
    refresh_options_bar();
    refresh_document_info();
    // Tools whose gestures the name does not explain get a one-line hint.
    if (const auto* hint = tool_traits(selected).activation_hint; hint != nullptr) {
      statusBar()->showMessage(resolve_modifier_names(tr(hint)));
    } else {
      statusBar()->showMessage(tool_name(selected));
    }
  });
  ctx.type_menu->addAction(type_tool_action_);

  auto* palette_spacer = new QWidget(tool_palette);
  palette_spacer->setObjectName(QStringLiteral("toolPaletteSpacer"));
  palette_spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  tool_palette->addWidget(palette_spacer);
  tool_palette->addSeparator();
  // Swatches first on purpose: vertical overflow hides items tail-first into
  // the extension button, so Quick Mask, then Swap/Default, disappear before
  // the FG/BG swatches.
  primary_color_button_ = new QPushButton(tr("FG"), tool_palette);
  secondary_color_button_ = new QPushButton(tr("BG"), tool_palette);
  primary_color_button_->setObjectName(QStringLiteral("foregroundColorButton"));
  secondary_color_button_->setObjectName(QStringLiteral("backgroundColorButton"));
  primary_color_button_->setToolTip(tr("Foreground color"));
  secondary_color_button_->setToolTip(tr("Background color"));
  tool_palette->addWidget(primary_color_button_);
  tool_palette->addWidget(secondary_color_button_);
  auto* default_colors_action = tool_palette->addAction(tr("Default Colors"));
  auto* swap_colors_action = tool_palette->addAction(tr("Swap Colors"));
  default_colors_action->setObjectName(QStringLiteral("colorDefaultAction"));
  swap_colors_action->setObjectName(QStringLiteral("colorSwapAction"));
  default_colors_action->setIcon(simple_icon(QStringLiteral("D")));
  swap_colors_action->setIcon(simple_icon(QStringLiteral("X")));
  register_hotkey(default_colors_action, "color.default", QKeySequence(Qt::Key_D), QStringLiteral("color"));
  register_hotkey(swap_colors_action, "color.swap", QKeySequence(Qt::Key_X), QStringLiteral("color"));
  tool_palette->addSeparator();
  tool_palette->addAction(quick_mask_action_);
  if (auto* quick_mask_button = qobject_cast<QToolButton*>(
          tool_palette->widgetForAction(quick_mask_action_));
      quick_mask_button != nullptr) {
    quick_mask_button->setObjectName(QStringLiteral("quickMaskButton"));
  }
  connect(primary_color_button_, &QPushButton::clicked, this, [this] { choose_primary_color(); });
  connect(secondary_color_button_, &QPushButton::clicked, this, [this] { choose_secondary_color(); });
  connect(swap_colors_action, &QAction::triggered, this, [this] { swap_colors(); });
  connect(default_colors_action, &QAction::triggered, this, [this] { default_colors(); });
  register_document_action(default_colors_action);
  register_document_action(swap_colors_action);

  // The overflow expansion behaves as a sticky toggle: the Leave filter keeps
  // it open while the pointer wanders, and the reservation controller widens
  // the bar's minimum so the layout reserves the extra columns instead of
  // letting them overlay the canvas. Picking a tool leaves the expansion open
  // (an auto-collapse there ate the first click of the Zoom button's
  // double-click); it closes on an extension re-click or once the window grows
  // tall enough that nothing overflows.
  if (auto* extension_button = tool_palette->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
      extension_button != nullptr) {
    tool_palette->installEventFilter(new ToolPaletteExpansionLock(extension_button, tool_palette));
    new ToolPaletteExpansionReservation(tool_palette, extension_button);
  }

  // Export the cross-phase locals bind_action_translations() still needs.
  ctx.tool_palette = tool_palette;
  ctx.default_colors_action = default_colors_action;
  ctx.swap_colors_action = swap_colors_action;
}

QAction* MainWindow::add_tool_action(QToolBar* palette, QActionGroup* group, CanvasTool tool) {
  const auto& traits = tool_traits(tool);
  auto* action = palette->addAction(tr(traits.name));
  bind_action_text(action, traits.name);
  action->setIcon(tool_icon(tool));
  action->setCheckable(true);
  action->setData(static_cast<int>(tool));
  action->setObjectName(tool_action_object_name(tool));
  if (traits.tooltip_detail != nullptr) {
    action->setProperty(kActionTooltipDetailProperty, QString::fromLatin1(traits.tooltip_detail));
  }
  register_hotkey(action, QString::fromLatin1(traits.hotkey_id), tool_default_shortcut(traits),
                  QStringLiteral("tools"));
  group->addAction(action);
  register_document_action(action);
  return action;
}

}  // namespace patchy::ui
