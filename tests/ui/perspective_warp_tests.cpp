// Perspective Warp (Edit > Perspective Warp, docs/perspective-warp.md): the
// Layout/Warp session on the canvas, its options-bar row, the one-shot
// straighten, Esc and in-session undo, the single undo step on commit, and the
// Smart Object refusal.

#include "core/document.hpp"
#include "core/pixel_buffer.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/main_window.hpp"

#include "test_harness.hpp"
#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QPoint>
#include <QPushButton>
#include <QStatusBar>

#include <cmath>
#include <cstdint>
#include <vector>

using patchy::test::ui::drag_document_path;
using patchy::test::ui::open_smart_object_fixture;
using patchy::test::ui::require_action;
using patchy::test::ui::require_action_by_text;
using patchy::test::ui::save_widget_artifact;
using patchy::test::ui::send_key;
using patchy::test::ui::show_window;

namespace {

using patchy::PerspectiveQuad;
using Mode = patchy::ui::CanvasWidget::PerspectiveWarpMode;

// A pixel layer whose bytes vary everywhere, so any resample shows.
patchy::LayerId add_gradient_layer(patchy::ui::MainWindow& window, int document_width, int document_height,
                                   patchy::Rect bounds) {
  patchy::Document built(document_width, document_height, patchy::PixelFormat::rgba8());
  patchy::PixelBuffer pixels(bounds.width, bounds.height, patchy::PixelFormat::rgba8());
  for (std::int32_t y = 0; y < bounds.height; ++y) {
    for (std::int32_t x = 0; x < bounds.width; ++x) {
      auto* px = pixels.pixel(x, y);
      px[0] = static_cast<std::uint8_t>(30 + x * 2);
      px[1] = static_cast<std::uint8_t>(220 - y * 2);
      px[2] = static_cast<std::uint8_t>((x * 7 + y * 3) % 256);
      px[3] = 255;
    }
  }
  patchy::Layer layer(built.allocate_layer_id(), "plane", std::move(pixels));
  layer.set_bounds(bounds);
  built.add_layer(std::move(layer));
  const auto id = built.layers().back().id();
  built.set_active_layer(id);
  window.add_document_session(std::move(built), QStringLiteral("Perspective"));
  QApplication::processEvents();
  return id;
}

std::vector<std::uint8_t> layer_bytes(patchy::ui::MainWindow& window, patchy::LayerId id) {
  const auto data = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).find_layer(id)->pixels().data();
  return {data.begin(), data.end()};
}

bool same_rect(patchy::Rect a, patchy::Rect b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

PerspectiveQuad rect_quad(double left, double top, double right, double bottom) {
  return {left, top, right, top, right, bottom, left, bottom};
}

void ui_perspective_warp_session_commits_one_undo_step_and_escape_restores() {
  patchy::ui::MainWindow window;
  show_window(window);
  const auto layer_id = add_gradient_layer(window, 160, 120, patchy::Rect{20, 15, 120, 90});
  auto* canvas = patchy::ui::MainWindowTestAccess::canvas(window);
  CHECK(canvas != nullptr);
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  const auto original_bounds = document.find_layer(layer_id)->bounds();
  const auto original_bytes = layer_bytes(window, layer_id);
  const auto undo_depth_before = patchy::ui::MainWindowTestAccess::active_session_undo_depth(window);

  // The Edit menu opens Layout mode and the session's own options-bar row.
  require_action(window, "editPerspectiveWarpAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->perspective_warp_active());
  CHECK(canvas->perspective_warp_mode() == Mode::Layout);
  auto* layout_button = window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpLayoutButton"));
  auto* warp_button = window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpWarpButton"));
  auto* straighten = window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpStraightenVerticalButton"));
  auto* remove_all = window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpRemoveAllButton"));
  CHECK(layout_button != nullptr && layout_button->isVisible() && layout_button->isChecked());
  CHECK(warp_button != nullptr && warp_button->isVisible() && !warp_button->isChecked());
  CHECK(straighten != nullptr && !straighten->isVisible());
  CHECK(remove_all != nullptr && remove_all->isVisible() && !remove_all->isEnabled());
  // Like Warp, the session greys document commands and Free Transform.
  CHECK(!require_action(window, "imageAdjustInvertAction")->isEnabled());
  CHECK(!require_action(window, "editFreeTransformAction")->isEnabled());
  CHECK(window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpApplyButton"))->isVisible());

  // Warp needs a quad. Dragging on the canvas lays one out; a second one that
  // would touch it is refused, one with a gap is not.
  warp_button->click();
  QApplication::processEvents();
  CHECK(canvas->perspective_warp_mode() == Mode::Layout);
  CHECK(!warp_button->isChecked());
  drag_document_path(*canvas, {QPoint(30, 25), QPoint(70, 85)}, 4);
  CHECK(canvas->perspective_warp_quad_count() == 1);
  CHECK(std::abs(canvas->perspective_warp_corner(0, 0).x() - 30.0) < 1.0);
  drag_document_path(*canvas, {QPoint(110, 80), QPoint(70, 30)}, 4);
  CHECK(canvas->perspective_warp_quad_count() == 1);
  CHECK(window.statusBar()->currentMessage().contains(QStringLiteral("gap")));
  drag_document_path(*canvas, {QPoint(85, 30), QPoint(125, 80)}, 4);
  CHECK(canvas->perspective_warp_quad_count() == 2);
  // A layout corner can't be dragged onto the other quad either.
  CHECK(!canvas->set_perspective_warp_corner(1, 0, canvas->perspective_warp_corner(0, 1)));

  warp_button->click();
  QApplication::processEvents();
  CHECK(canvas->perspective_warp_mode() == Mode::Warp);
  CHECK(straighten->isVisible());
  // Drag the first quad's top-left corner out; in-session Undo steps it back
  // without touching the document history, and the drag is redone after.
  const auto corner = canvas->perspective_warp_corner(0, 0);
  drag_document_path(*canvas, {corner.toPoint(), corner.toPoint() + QPoint(-10, -8)}, 4);
  CHECK(canvas->perspective_warp_corner(0, 0).x() < corner.x() - 8.0);
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(canvas->perspective_warp_active());
  CHECK(canvas->perspective_warp_corner(0, 0) == corner);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before);
  CHECK(canvas->set_perspective_warp_corner(0, 0, corner + QPointF(-10.0, -8.0)));
  // Warped quads stay apart too.
  CHECK(!canvas->set_perspective_warp_corner(1, 0, canvas->perspective_warp_corner(0, 1)));

  // Esc leaves the layer exactly as it was.
  send_key(*canvas, Qt::Key_Escape);
  QApplication::processEvents();
  CHECK(!canvas->perspective_warp_active());
  CHECK(!layout_button->isVisible());
  CHECK(same_rect(document.find_layer(layer_id)->bounds(), original_bounds));
  CHECK(layer_bytes(window, layer_id) == original_bytes);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before);

  // Untouched corners commit nothing.
  CHECK(canvas->begin_perspective_warp());
  CHECK(canvas->add_perspective_warp_quad(rect_quad(30, 25, 70, 85)));
  send_key(*canvas, Qt::Key_Return);
  QApplication::processEvents();
  CHECK(!canvas->perspective_warp_active());
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before);

  // Enter bakes the warp as one undo step; Undo restores the exact bytes.
  CHECK(canvas->begin_perspective_warp());
  CHECK(canvas->add_perspective_warp_quad(rect_quad(30, 25, 70, 85)));
  CHECK(canvas->set_perspective_warp_mode(Mode::Warp));
  CHECK(canvas->set_perspective_warp_corner(0, 0, QPointF(18.0, 12.0)));
  send_key(*canvas, Qt::Key_Return);
  QApplication::processEvents();
  CHECK(!canvas->perspective_warp_active());
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before + 1);
  const auto warped_bounds = document.find_layer(layer_id)->bounds();
  CHECK(warped_bounds.x < original_bounds.x && warped_bounds.y < original_bounds.y);
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  auto& restored = patchy::ui::MainWindowTestAccess::document(window);
  CHECK(same_rect(restored.find_layer(layer_id)->bounds(), original_bounds));
  CHECK(layer_bytes(window, layer_id) == original_bytes);
}

void ui_perspective_warp_straightens_a_leaning_side_in_the_pixels() {
  // A dark "wall" whose left side leans (x 60 at the top, 80 at the bottom) on
  // a light layer. The quad traces the wall; the straighten button stands that
  // side upright and the committed pixels follow.
  patchy::ui::MainWindow window;
  show_window(window);
  const auto layer_id = add_gradient_layer(window, 200, 160, patchy::Rect{0, 0, 200, 160});
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  {
    auto& pixels = document.find_layer(layer_id)->pixels();
    for (std::int32_t y = 0; y < 160; ++y) {
      for (std::int32_t x = 0; x < 200; ++x) {
        const double left_edge = 60.0 + (y - 20) * 20.0 / 120.0;
        const bool wall = y >= 20 && y < 140 && x + 0.5 >= left_edge && x < 140;
        auto* px = pixels.pixel(x, y);
        px[0] = px[1] = px[2] = wall ? 40 : 230;
        px[3] = 255;
      }
    }
  }
  auto* canvas = patchy::ui::MainWindowTestAccess::canvas(window);
  canvas->document_changed(QRect(0, 0, 200, 160));
  const auto wall_left = [&window, layer_id](int row) {
    const auto& pixels = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).find_layer(layer_id)->pixels();
    const auto bounds = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).find_layer(layer_id)->bounds();
    for (std::int32_t x = 0; x < pixels.width(); ++x) {
      // Opaque and dark: the membrane may leave transparent slivers at the edges.
      if (const auto* px = pixels.pixel(x, row - bounds.y); px[3] > 128 && px[0] < 135) {
        return x + bounds.x;
      }
    }
    return -1;
  };
  CHECK(wall_left(30) == 62);
  CHECK(wall_left(130) == 78);

  require_action(window, "editPerspectiveWarpAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->add_perspective_warp_quad({60, 20, 140, 20, 140, 140, 80, 140}));
  CHECK(canvas->set_perspective_warp_mode(Mode::Warp));
  canvas->select_perspective_warp_quad(-1);
  QApplication::processEvents();
  auto* straighten = window.findChild<QPushButton*>(QStringLiteral("perspectiveWarpStraightenVerticalButton"));
  CHECK(straighten != nullptr && straighten->isVisible() && !straighten->isEnabled());
  canvas->select_perspective_warp_quad(0);
  QApplication::processEvents();
  CHECK(straighten->isEnabled());
  save_widget_artifact("perspective_warp_before", *canvas);
  straighten->click();
  QApplication::processEvents();
  // Exactly vertical, from the quad's own corners only: x 60/80 average to 70.
  CHECK(canvas->perspective_warp_corner(0, 0) == QPointF(70.0, 20.0));
  CHECK(canvas->perspective_warp_corner(0, 3) == QPointF(70.0, 140.0));
  CHECK(canvas->perspective_warp_corner(0, 1) == QPointF(140.0, 20.0));
  save_widget_artifact("perspective_warp_preview", *canvas);
  send_key(*canvas, Qt::Key_Return);
  QApplication::processEvents();
  CHECK(!canvas->perspective_warp_active());
  CHECK(std::abs(wall_left(30) - 70) <= 1);
  CHECK(std::abs(wall_left(80) - 70) <= 1);
  CHECK(std::abs(wall_left(130) - 70) <= 1);
  save_widget_artifact("perspective_warp_after", *canvas);
}

void ui_perspective_warp_refuses_smart_objects() {
  patchy::ui::MainWindow window;
  show_window(window);
  open_smart_object_fixture(window);
  auto* canvas = patchy::ui::MainWindowTestAccess::canvas(window);
  CHECK(canvas != nullptr);
  require_action(window, "editPerspectiveWarpAction")->trigger();
  QApplication::processEvents();
  CHECK(!canvas->perspective_warp_active());
  CHECK(window.statusBar()->currentMessage() ==
        QStringLiteral("Rasterize the Smart Object before using Perspective Warp"));
  CHECK(!canvas->begin_perspective_warp());
}

}  // namespace

std::vector<patchy::test::TestCase> perspective_warp_tests() {
  return {
      {"ui_perspective_warp_session_commits_one_undo_step_and_escape_restores",
       ui_perspective_warp_session_commits_one_undo_step_and_escape_restores},
      {"ui_perspective_warp_straightens_a_leaning_side_in_the_pixels",
       ui_perspective_warp_straightens_a_leaning_side_in_the_pixels},
      {"ui_perspective_warp_refuses_smart_objects", ui_perspective_warp_refuses_smart_objects},
  };
}
