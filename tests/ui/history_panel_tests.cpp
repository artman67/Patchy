// History panel interaction tests: per-session rebuild, click-to-jump across
// the undo/redo stacks, cap eviction, keyboard undo/redo highlight moves, the
// preview-lock disable, and the History Brush and its panel source.

#include "ui/canvas_widget.hpp"
#include "ui/main_window.hpp"
#include "ui/theme_palette.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QFocusEvent>
#include <QListWidget>
#include <QStatusBar>
#include <QTest>
#include <QTimer>

#include "test_harness.hpp"
#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

namespace {

using namespace patchy::test::ui;
using patchy::ui::MainWindowTestAccess;

QListWidget* require_history_list(patchy::ui::MainWindow& window) {
  auto* list = window.findChild<QListWidget*>(QStringLiteral("historyList"));
  CHECK(list != nullptr);
  return list;
}

void click_history_row(QListWidget& list, int row) {
  auto* item = list.item(row);
  CHECK(item != nullptr);
  list.scrollToItem(item);
  QTest::mouseClick(list.viewport(), Qt::LeftButton, Qt::NoModifier,
                    list.visualItemRect(item).center());
  QApplication::processEvents();
}

void fill_with(patchy::ui::MainWindow& window, patchy::ui::CanvasWidget& canvas, QColor color) {
  canvas.set_primary_color(color);
  use_solid_fill_settings(&canvas);
  require_action(window, "layerFillForegroundAction")->trigger();
  QApplication::processEvents();
}

void ui_history_panel_lists_states_oldest_first_with_current_highlight() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  CHECK(history->isEnabled());
  // The fresh document contributes exactly one state (its creation).
  CHECK(history->count() == 1);
  CHECK(history->currentRow() == 0);

  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 60, 220));
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 2);
  CHECK(history->item(2)->text().contains(QStringLiteral("Fill")));
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 2);

  MainWindowTestAccess::undo(window);
  QApplication::processEvents();
  // Undo moves the highlight instead of inserting an "Undo" row; the undone
  // state stays listed, dimmed with the future-state theme role.
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 1);
  CHECK(history->item(2)->foreground().color() == patchy::ui::theme().history_future_text);
  CHECK(history->item(0)->foreground().color() != patchy::ui::theme().history_future_text);
}

void ui_history_click_jumps_backward_and_forward() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);

  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 160, 40));
  fill_with(window, *canvas, QColor(30, 60, 220));
  CHECK(history->count() == 4);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(30, 60, 220), 6));

  // Jump two states back in one click; the abandoned future stays listed.
  click_history_row(*history, 1);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(200, 30, 30), 6));
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 1);
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == 2);
  CHECK(history->count() == 4);
  CHECK(history->currentRow() == 1);
  CHECK(history->item(3)->foreground().color() == patchy::ui::theme().history_future_text);

  // Jump forward again to the newest state.
  click_history_row(*history, 3);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(30, 60, 220), 6));
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 3);
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == 0);
  CHECK(history->currentRow() == 3);

  // Clicking the current row changes nothing.
  click_history_row(*history, 3);
  CHECK(history->currentRow() == 3);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 3);
}

void ui_history_new_edit_after_rollback_discards_future_rows() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);

  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 160, 40));
  fill_with(window, *canvas, QColor(30, 60, 220));
  click_history_row(*history, 1);
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == 2);

  fill_with(window, *canvas, QColor(240, 240, 30));
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == 0);
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 2);
  for (int row = 0; row < history->count(); ++row) {
    CHECK(history->item(row)->foreground().color() != patchy::ui::theme().history_future_text);
  }
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(240, 240, 30), 6));
}

void ui_history_cap_eviction_keeps_rows_consistent() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);

  for (int index = 0; index < 45; ++index) {
    fill_with(window, *canvas,
              index % 2 == 0 ? QColor(200, 30, 30) : QColor(30, 60, 220));
  }
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 40);
  CHECK(history->count() == 41);
  CHECK(history->currentRow() == 40);

  // The oldest surviving snapshot is the document after fill #5 (the initial
  // state and fills 1-4 were evicted); fill #5 used the even-index red.
  click_history_row(*history, 0);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 0);
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == 40);
  CHECK(history->count() == 41);
  CHECK(history->currentRow() == 0);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(200, 30, 30), 6));
}

// The history byte budget reads PATCHY_HISTORY_BUDGET_TEST_MB on every call;
// CHECK throws on failure, so the override must unset itself via RAII or a
// failing test would poison every test after it.
struct HistoryBudgetOverride {
  explicit HistoryBudgetOverride(const char* mb) {
    qputenv("PATCHY_HISTORY_BUDGET_TEST_MB", mb);
  }
  ~HistoryBudgetOverride() { qunsetenv("PATCHY_HISTORY_BUDGET_TEST_MB"); }
};

void ui_history_budget_evicts_oldest_but_keeps_floor() {
  const HistoryBudgetOverride budget("0");
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);

  const QColor colors[] = {QColor(200, 30, 30),  QColor(30, 160, 40),  QColor(30, 60, 220),
                           QColor(240, 240, 30), QColor(140, 30, 200), QColor(30, 220, 220)};
  for (const auto& color : colors) {
    fill_with(window, *canvas, color);
  }
  // A zero budget evicts to the floor on every push; the panel mirrors the
  // survivors (three snapshots plus the current state).
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 3);
  CHECK(history->count() == 4);
  CHECK(history->currentRow() == 3);

  // The floor states still undo: three steps back lands on fill #3's result.
  MainWindowTestAccess::undo(window);
  MainWindowTestAccess::undo(window);
  MainWindowTestAccess::undo(window);
  QApplication::processEvents();
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 0);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), colors[2], 6));
}

void ui_history_budget_is_global_across_sessions() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* first_canvas = require_canvas(window);
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  CHECK(tabs != nullptr);

  // Five states in the first tab under the default (roomy) budget.
  for (int index = 0; index < 5; ++index) {
    fill_with(window, *first_canvas, index % 2 == 0 ? QColor(200, 30, 30) : QColor(30, 60, 220));
  }
  CHECK(MainWindowTestAccess::undo_depth_for_canvas(window, first_canvas) == 5);

  // Shrink the budget to zero and push one edit in a SECOND tab: global
  // enforcement evicts the background tab down to the floor, never below.
  const HistoryBudgetOverride budget("0");
  MainWindowTestAccess::create_default_document(window);
  QApplication::processEvents();
  auto* second_canvas = require_canvas(window);
  CHECK(second_canvas != first_canvas);
  fill_with(window, *second_canvas, QColor(30, 160, 40));
  CHECK(MainWindowTestAccess::undo_depth_for_canvas(window, first_canvas) == 3);
  CHECK(MainWindowTestAccess::undo_depth_for_canvas(window, second_canvas) == 1);

  // The evicted background tab still undoes cleanly.
  tabs->setCurrentIndex(0);
  QApplication::processEvents();
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 3);
  MainWindowTestAccess::undo(window);
  QApplication::processEvents();
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 2);
}

void ui_history_keyboard_undo_redo_moves_highlight() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);

  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 60, 220));
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 2);

  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 1);

  require_action_by_text(window, QStringLiteral("Redo"))->trigger();
  QApplication::processEvents();
  CHECK(history->count() == 3);
  CHECK(history->currentRow() == 2);
}

void ui_history_panel_rebuilds_on_activation() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  CHECK(tabs != nullptr);

  fill_with(window, *canvas, QColor(200, 30, 30));
  CHECK(history->count() == 2);

  MainWindowTestAccess::create_default_document(window);
  QApplication::processEvents();
  CHECK(tabs->count() == 2);
  CHECK(history->count() == 1);

  tabs->setCurrentIndex(0);
  QApplication::processEvents();
  CHECK(history->count() == 2);
  CHECK(history->currentRow() == 1);

  tabs->setCurrentIndex(1);
  QApplication::processEvents();
  CHECK(history->count() == 1);
  CHECK(history->currentRow() == 0);
}

void ui_history_new_document_from_state_creates_independent_session() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  CHECK(tabs != nullptr);

  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 60, 220));
  CHECK(history->count() == 3);
  const auto red_state_id = history->item(1)->data(Qt::UserRole).toLongLong();

  MainWindowTestAccess::open_history_state_as_new_document(window, red_state_id);
  QApplication::processEvents();
  CHECK(MainWindowTestAccess::session_count(window) == 2);
  CHECK(tabs->currentIndex() == 1);
  auto* spawned_canvas = require_canvas(window);
  CHECK(spawned_canvas != canvas);
  CHECK(color_close(canvas_pixel(*spawned_canvas, QPoint(40, 40)), QColor(200, 30, 30), 6));
  CHECK(history->count() == 1);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 0);

  // Editing the spawned document must not leak into the original (the copy is
  // copy-on-write, so shared pixels detach on the first mutation).
  fill_with(window, *spawned_canvas, QColor(30, 160, 40));
  tabs->setCurrentIndex(0);
  QApplication::processEvents();
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(30, 60, 220), 6));
  CHECK(history->count() == 3);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 2);

  // The original still jumps within its own history.
  click_history_row(*history, 1);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(200, 30, 30), 6));
}

void ui_history_clicks_blocked_during_preview_lock() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  fill_with(window, *canvas, QColor(200, 30, 30));
  CHECK(history->isEnabled());

  bool saw_disabled_history = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      if (widget->objectName() != QStringLiteral("patchyFilterDialog")) {
        continue;
      }
      auto* dialog = qobject_cast<QDialog*>(widget);
      CHECK(dialog != nullptr);
      saw_disabled_history = !history->isEnabled();
      dialog->reject();
      return;
    }
    CHECK(false);
  });
  require_action(window, "imageAdjustInvertAction")->trigger();
  CHECK(saw_disabled_history);
  CHECK(history->isEnabled());
}

}  // namespace


void ui_history_refuses_undo_during_live_gesture_and_clears_focus_latches() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  fill_with(window, *canvas, QColor(30, 60, 220));
  canvas->set_tool(patchy::ui::CanvasTool::Brush);
  const auto point = canvas->widget_position_for_document_point(QPoint(120, 100));
  QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, point);
  CHECK(canvas->pointer_gesture_active());
  const auto depth = MainWindowTestAccess::active_session_undo_depth(window);
  MainWindowTestAccess::undo(window);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == depth);
  QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, point);
  CHECK(!canvas->pointer_gesture_active());
  MainWindowTestAccess::undo(window);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) + 1 == depth);
  canvas->set_tool(patchy::ui::CanvasTool::Marquee);
  QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, point);
  CHECK(canvas->pointer_gesture_active());
  QFocusEvent lost(QEvent::FocusOut, Qt::OtherFocusReason);
  QApplication::sendEvent(canvas, &lost);
  CHECK(!canvas->pointer_gesture_active());
}


void ui_nested_layer_move_and_noop_preserve_history() {
  patchy::ui::MainWindow window;
  show_window(window);
  patchy::Document document(32, 32, patchy::PixelFormat::rgba8());
  patchy::Layer group(document.allocate_layer_id(), "Folder", patchy::LayerKind::Group);
  const auto first_id = document.allocate_layer_id();
  const auto second_id = document.allocate_layer_id();
  group.add_child(patchy::Layer(first_id, "First", patchy::PixelBuffer(1,1,patchy::PixelFormat::rgba8())));
  group.add_child(patchy::Layer(second_id, "Second", patchy::PixelBuffer(1,1,patchy::PixelFormat::rgba8())));
  document.add_layer(std::move(group));
  document.set_active_layer(first_id);
  window.add_document_session(std::move(document), QStringLiteral("Nested"));
  auto* canvas = require_canvas(window);
  canvas->set_selected_layer_ids({first_id});
  auto* up = window.hotkey_registry().find_command(QStringLiteral("layer.move_up"))->action.data();
  auto* down = window.hotkey_registry().find_command(QStringLiteral("layer.move_down"))->action.data();
  CHECK(up != nullptr && down != nullptr);
  up->trigger();
  auto& live = MainWindowTestAccess::document(window);
  CHECK(std::as_const(live).layers().front().children().back().id() == first_id);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 1);
  MainWindowTestAccess::undo(window);
  const auto redo = MainWindowTestAccess::active_session_redo_depth(window);
  down->trigger();
  CHECK(std::as_const(live).layers().front().children().front().id() == first_id);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == 0);
  CHECK(MainWindowTestAccess::active_session_redo_depth(window) == redo);
}

void ui_tool_settings_follow_canvas_activation() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* first = require_canvas(window);
  first->set_fill_opacity(37);
  first->set_fill_softness(61);
  first->set_quick_select_size(17);
  first->set_quick_select_sample_all_layers(true);
  first->set_quick_select_enhance_edge(true);
  first->set_transform_interpolation(patchy::ui::CanvasWidget::TransformInterpolation::NearestNeighbor);
  first->set_polygon_sides(9);
  first->set_polygon_star_inset(23);
  patchy::Document document(32,32,patchy::PixelFormat::rgba8());
  window.add_document_session(std::move(document), QStringLiteral("Second"));
  auto* second = require_canvas(window);
  CHECK(second != first);
  CHECK(second->fill_opacity() == 37 && second->fill_softness() == 61);
  CHECK(second->quick_select_size() == 17 && second->quick_select_sample_all_layers());
  CHECK(second->quick_select_enhance_edge());
  CHECK(second->transform_interpolation() == patchy::ui::CanvasWidget::TransformInterpolation::NearestNeighbor);
  CHECK(second->polygon_sides() == 9 && second->polygon_star_inset() == 23);
}

const std::uint8_t* layer_pixel(patchy::ui::MainWindow& window, patchy::LayerId id, QPoint point) {
  const auto* layer = std::as_const(MainWindowTestAccess::document(window)).find_layer(id);
  CHECK(layer != nullptr);
  const auto bounds = layer->bounds();
  return layer->pixels().pixel(point.x() - bounds.x, point.y() - bounds.y);
}

void use_hard_history_brush(patchy::ui::MainWindow& window, patchy::ui::CanvasWidget& canvas, int opacity) {
  require_action(window, "toolHistoryBrushAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas.tool() == patchy::ui::CanvasTool::HistoryBrush);
  canvas.set_brush_size(9);
  canvas.set_brush_opacity(opacity);
  canvas.set_brush_flow(100);
  canvas.set_brush_softness(0);
}

void ui_history_brush_restores_source_under_stroke_in_one_undo_step() {
  patchy::ui::MainWindow window;
  show_window(window);
  // Left half blue, right half transparent: the opened state is the source.
  patchy::Document document(80, 60, patchy::PixelFormat::rgba8());
  auto pixels = solid_pixels(80, 60, patchy::PixelFormat::rgba8(), Qt::transparent);
  fill_pixel_rect(pixels, QRect(0, 0, 40, 60), QColor(30, 60, 220));
  const auto layer_id = document.add_pixel_layer("Paint", std::move(pixels)).id();
  window.add_document_session(std::move(document), QStringLiteral("History Brush"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  fill_with(window, *canvas, QColor(200, 30, 30));
  CHECK(layer_pixel(window, layer_id, QPoint(60, 20))[0] == 200U);

  // A selection limits the restore; the stroke crosses its right edge at x 50.
  canvas->set_tool(patchy::ui::CanvasTool::Marquee);
  canvas->set_selection_feather_radius(0);
  drag(*canvas, canvas->widget_position_for_document_point(QPoint(0, 0)),
       canvas->widget_position_for_document_point(QPoint(50, 60)));
  CHECK(canvas->selected_document_rect().has_value());
  const auto depth = MainWindowTestAccess::active_session_undo_depth(window);
  use_hard_history_brush(window, *canvas, 100);
  drag_document_path(*canvas, {QPoint(10, 20), QPoint(70, 20)}, 12);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == depth + 1);
  CHECK(history->currentItem()->text() == QStringLiteral("History Brush"));

  const auto* restored = layer_pixel(window, layer_id, QPoint(20, 20));
  CHECK(restored[0] == 30U && restored[1] == 60U && restored[2] == 220U && restored[3] == 255U);
  // The source was transparent here, so the later paint is removed, alpha included.
  CHECK(layer_pixel(window, layer_id, QPoint(45, 20))[3] == 0U);
  const auto* unselected = layer_pixel(window, layer_id, QPoint(60, 20));
  CHECK(unselected[0] == 200U && unselected[3] == 255U);
  const auto* off_stroke = layer_pixel(window, layer_id, QPoint(20, 40));
  CHECK(off_stroke[0] == 200U && off_stroke[2] == 30U);

  // Opacity caps the blend toward the source; the stroke is one undo step.
  canvas->clear_selection();
  QApplication::processEvents();
  const auto before_half = MainWindowTestAccess::active_session_undo_depth(window);
  drag_document_path(*canvas, {QPoint(5, 50), QPoint(30, 50)}, 8);
  canvas->set_brush_opacity(50);
  drag_document_path(*canvas, {QPoint(5, 45), QPoint(30, 45)}, 8);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == before_half + 2);
  const auto* half = layer_pixel(window, layer_id, QPoint(15, 45));
  CHECK(color_close(QColor(half[0], half[1], half[2]), QColor(115, 45, 125), 3));
  MainWindowTestAccess::undo(window);
  QApplication::processEvents();
  CHECK(layer_pixel(window, layer_id, QPoint(15, 45))[0] == 200U);
  CHECK(layer_pixel(window, layer_id, QPoint(15, 50))[2] == 220U);
  CHECK(layer_pixel(window, layer_id, QPoint(20, 20))[2] == 220U);
  save_widget_artifact("ui_history_brush_restore", *canvas);
}

void ui_history_brush_source_follows_panel_and_refuses_unusable_states() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* history = require_history_list(window);
  fill_with(window, *canvas, QColor(200, 30, 30));
  fill_with(window, *canvas, QColor(30, 160, 40));
  CHECK(history->count() == 3);
  // The opened state is the default source and carries the marker.
  const auto source_tip = QStringLiteral("Source for the History Brush");
  CHECK(history->item(0)->toolTip() == source_tip);
  CHECK(history->item(1)->toolTip().isEmpty());

  MainWindowTestAccess::set_history_brush_source(window, history->item(1)->data(Qt::UserRole).toLongLong());
  QApplication::processEvents();
  CHECK(history->item(0)->toolTip().isEmpty());
  CHECK(history->item(1)->toolTip() == source_tip);
  use_hard_history_brush(window, *canvas, 100);
  drag_document_path(*canvas, {QPoint(30, 40), QPoint(50, 40)}, 6);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 40)), QColor(200, 30, 30), 2));
  CHECK(color_close(canvas_pixel(*canvas, QPoint(40, 80)), QColor(30, 160, 40), 2));
  if (auto* toggle = window.findChild<QAbstractButton*>(QStringLiteral("historyDockCollapseButton"));
      toggle != nullptr && !history->isVisible()) {
    toggle->click();
    QApplication::processEvents();
  }
  save_widget_artifact("ui_history_brush_source_marker", window);

  // A layer added after the source state has no counterpart there: refused,
  // nothing painted, no history entry (Photoshop's rule).
  auto* new_layer = window.findChild<QAbstractButton*>(QStringLiteral("layerNewButton"));
  CHECK(new_layer != nullptr);
  new_layer->click();
  QApplication::processEvents();
  const auto depth = MainWindowTestAccess::active_session_undo_depth(window);
  drag_document_path(*canvas, {QPoint(30, 60), QPoint(50, 60)}, 6);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == depth);
  CHECK(window.statusBar()->currentMessage().contains(QStringLiteral("corresponding layer")));

  // A source with another canvas size is refused the same way.
  patchy::Document small(40, 30, patchy::PixelFormat::rgba8());
  small.add_pixel_layer("Small", solid_pixels(40, 30, patchy::PixelFormat::rgba8(), Qt::white));
  patchy::Document large(60, 40, patchy::PixelFormat::rgba8());
  const auto large_id =
      large.add_pixel_layer("Large", solid_pixels(60, 40, patchy::PixelFormat::rgba8(), Qt::black)).id();
  patchy::ui::CanvasWidget bare;
  bare.resize(160, 120);
  bare.set_document(&large);
  QString error;
  bare.set_error_status_callback([&error](QString message) { error = std::move(message); });
  bare.set_history_brush_source(std::make_shared<const patchy::Document>(small));
  bare.set_tool(patchy::ui::CanvasTool::HistoryBrush);
  bare.set_brush_size(9);
  bare.show();
  QApplication::processEvents();
  drag_document_path(bare, {QPoint(10, 10), QPoint(30, 10)}, 4);
  CHECK(error.contains(QStringLiteral("canvas size")));
  CHECK(std::as_const(large).find_layer(large_id)->pixels().pixel(20, 10)[0] == 0U);
}

std::vector<patchy::test::TestCase> history_panel_tests() {
  return {
      {"ui_history_panel_lists_states_oldest_first_with_current_highlight",
       ui_history_panel_lists_states_oldest_first_with_current_highlight},
      {"ui_history_click_jumps_backward_and_forward", ui_history_click_jumps_backward_and_forward},
      {"ui_history_new_edit_after_rollback_discards_future_rows",
       ui_history_new_edit_after_rollback_discards_future_rows},
      {"ui_history_cap_eviction_keeps_rows_consistent", ui_history_cap_eviction_keeps_rows_consistent},
      {"ui_history_budget_evicts_oldest_but_keeps_floor", ui_history_budget_evicts_oldest_but_keeps_floor},
      {"ui_history_budget_is_global_across_sessions", ui_history_budget_is_global_across_sessions},
      {"ui_history_keyboard_undo_redo_moves_highlight", ui_history_keyboard_undo_redo_moves_highlight},
      {"ui_history_panel_rebuilds_on_activation", ui_history_panel_rebuilds_on_activation},
      {"ui_history_new_document_from_state_creates_independent_session",
       ui_history_new_document_from_state_creates_independent_session},
      {"ui_history_clicks_blocked_during_preview_lock", ui_history_clicks_blocked_during_preview_lock},
      {"ui_history_refuses_undo_during_live_gesture_and_clears_focus_latches", ui_history_refuses_undo_during_live_gesture_and_clears_focus_latches},
      {"ui_nested_layer_move_and_noop_preserve_history", ui_nested_layer_move_and_noop_preserve_history},
      {"ui_tool_settings_follow_canvas_activation", ui_tool_settings_follow_canvas_activation},
      {"ui_history_brush_restores_source_under_stroke_in_one_undo_step",
       ui_history_brush_restores_source_under_stroke_in_one_undo_step},
      {"ui_history_brush_source_follows_panel_and_refuses_unusable_states",
       ui_history_brush_source_follows_panel_and_refuses_unusable_states},
  };
}
