// Edit > Puppet Warp: pins on a pixel layer, the options bar, Enter/Esc, the
// in-session pin undo, pin depth, and the Smart Object refusal. See
// docs/puppet-warp.md.

#include "ui_test_support.hpp"

#include "ui_test_groups.hpp"

#include "ui/ui_test_access.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QStatusBar>

namespace {

using namespace patchy::test::ui;

const QColor kRed(220, 40, 30);
const QColor kBlue(30, 60, 210);

// A 200x120 document holding one 120x24 bar at (40, 48): red left half, blue
// right half, transparent elsewhere.
patchy::LayerId add_bar_document(patchy::ui::MainWindow& window) {
  patchy::Document built(200, 120, patchy::PixelFormat::rgba8());
  auto pixels = solid_pixels(120, 24, patchy::PixelFormat::rgba8(), kRed);
  fill_pixel_rect(pixels, QRect(60, 0, 60, 24), kBlue);
  patchy::Layer layer(built.allocate_layer_id(), "bar", std::move(pixels));
  layer.set_bounds(patchy::Rect{40, 48, 120, 24});
  built.add_layer(std::move(layer));
  const auto layer_id = built.layers().back().id();
  built.set_active_layer(layer_id);
  window.add_document_session(std::move(built), QStringLiteral("Puppet"));
  QApplication::processEvents();
  return layer_id;
}

// The layer's color at a document pixel (transparent outside its bounds).
QColor layer_color_at(const patchy::Layer& layer, int x, int y) {
  const auto bounds = layer.bounds();
  if (x < bounds.x || y < bounds.y || x >= bounds.x + bounds.width || y >= bounds.y + bounds.height) {
    return QColor(0, 0, 0, 0);
  }
  const auto* px = layer.pixels().pixel(x - bounds.x, y - bounds.y);
  return QColor(px[0], px[1], px[2], px[3]);
}

bool same_layer_pixels(const patchy::Layer& layer, patchy::Rect bounds, const patchy::PixelBuffer& pixels) {
  const auto actual = layer.bounds();
  return actual.x == bounds.x && actual.y == bounds.y && actual.width == bounds.width &&
         actual.height == bounds.height && std::ranges::equal(layer.pixels().data(), pixels.data());
}

QPoint widget_point(patchy::ui::CanvasWidget& canvas, QPointF document_point) {
  return canvas.widget_position_f(document_point).toPoint();
}

void click(patchy::ui::CanvasWidget& canvas, QPointF document_point, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  const auto at = widget_point(canvas, document_point);
  send_mouse(canvas, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton, modifiers);
  send_mouse(canvas, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton, modifiers);
}

void ui_puppet_warp_pins_bend_layer_as_one_undo_step() {
  patchy::ui::MainWindow window;
  show_window(window);
  const auto layer_id = add_bar_document(window);
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  auto* canvas = require_canvas(window);
  const auto original_bounds = std::as_const(document).find_layer(layer_id)->bounds();
  const auto original_pixels = std::as_const(document).find_layer(layer_id)->pixels();
  const auto undo_depth_before = patchy::ui::MainWindowTestAccess::active_session_undo_depth(window);

  // The Edit menu entry starts the session; the options bar swaps in its controls,
  // keeps apply/cancel, and hides the free-transform/warp toggle.
  require_action(window, "editPuppetWarpAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->puppet_warp_active());
  auto* mode_combo = window.findChild<QComboBox*>(QStringLiteral("puppetWarpModeCombo"));
  auto* rotate_combo = window.findChild<QComboBox*>(QStringLiteral("puppetWarpRotateCombo"));
  CHECK(mode_combo != nullptr && mode_combo->isVisible());
  CHECK(rotate_combo != nullptr && rotate_combo->isVisible() && !rotate_combo->isEnabled());  // no pin yet
  CHECK(window.findChild<QWidget*>(QStringLiteral("freeTransformApplyButton"))->isVisible());
  CHECK(!window.findChild<QWidget*>(QStringLiteral("transformWarpModeButton"))->isVisible());
  CHECK(canvas->puppet_warp_options().mode == patchy::PuppetWarpMode::Normal);
  // Like Warp, the session greys document commands and Free Transform.
  CHECK(!require_action(window, "imageAdjustInvertAction")->isEnabled());
  CHECK(!require_action(window, "editFreeTransformAction")->isEnabled());

  // Clicks on the layer add pins; a click well off the mesh adds none.
  const QPointF left_end(45.5, 60.5);
  const QPointF right_end(155.5, 60.5);
  click(*canvas, left_end);
  CHECK(canvas->puppet_pin_count() == 1);
  CHECK(rotate_combo->isEnabled());
  click(*canvas, QPointF(150.5, 10.5));
  CHECK(canvas->puppet_pin_count() == 1);
  CHECK(canvas->selected_puppet_pins().empty());
  // Press on the right end adds a pin there and drags it up.
  const QPointF lifted(155.5, 30.5);
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(*canvas, right_end), Qt::LeftButton, Qt::LeftButton,
             Qt::NoModifier);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(*canvas, lifted), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(*canvas, lifted), Qt::LeftButton, Qt::NoButton,
             Qt::NoModifier);
  CHECK(canvas->puppet_pin_count() == 2);
  CHECK(std::abs(canvas->puppet_pin_position(1).y() - lifted.y()) < 1.5);
  CHECK(std::abs(canvas->puppet_pin_position(0).x() - left_end.x()) < 1.5);
  const auto held = canvas->puppet_pin_position(0);

  // Alt-click removes a pin; Undo inside the session restores it without touching
  // the document history.
  click(*canvas, held, Qt::AltModifier);
  CHECK(canvas->puppet_pin_count() == 1);
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(canvas->puppet_pin_count() == 2);
  CHECK(canvas->puppet_pin_position(0) == held);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before);

  // Options-bar wiring: Mode reaches the session; Rotate applies to the selected pin.
  mode_combo->setCurrentIndex(mode_combo->findData(static_cast<int>(patchy::PuppetWarpMode::Rigid)));
  CHECK(canvas->puppet_warp_options().mode == patchy::PuppetWarpMode::Rigid);
  click(*canvas, held);
  CHECK(canvas->selected_puppet_pins() == std::vector<int>{0});
  rotate_combo->setCurrentIndex(rotate_combo->findData(true));
  CHECK(canvas->puppet_pin_controls().fixed_rotation);
  CHECK(window.findChild<QWidget*>(QStringLiteral("puppetWarpRotateAngleSpin"))->isEnabled());
  rotate_combo->setCurrentIndex(rotate_combo->findData(false));
  CHECK(!canvas->puppet_pin_controls().fixed_rotation);
  save_widget_artifact("puppet_warp_session_mesh", *canvas);

  // Enter commits one undo step: the held end stays red, the lifted end is blue
  // where there was nothing before.
  send_key(*canvas, Qt::Key_Return);
  CHECK(!canvas->puppet_warp_active());
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth_before + 1);
  const auto* warped = std::as_const(document).find_layer(layer_id);
  CHECK(color_close(layer_color_at(*warped, 45, 60), kRed, 2));
  CHECK(color_close(layer_color_at(*warped, 155, 30), kBlue, 2));
  save_widget_artifact("puppet_warp_committed", *canvas);

  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  const auto* restored = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).find_layer(layer_id);
  CHECK(same_layer_pixels(*restored, original_bounds, original_pixels));
}


// Nothing moved means nothing changes (no undo step, identical bytes), Esc throws
// the session away, and one dragged pin is an exact translation of the layer.
void ui_puppet_warp_no_op_cancel_and_single_pin_translation() {
  patchy::ui::MainWindow window;
  show_window(window);
  const auto layer_id = add_bar_document(window);
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  auto* canvas = require_canvas(window);
  const auto& read_only = std::as_const(document);
  const auto bounds = read_only.find_layer(layer_id)->bounds();
  const auto pixels = read_only.find_layer(layer_id)->pixels();
  const auto depth = patchy::ui::MainWindowTestAccess::active_session_undo_depth(window);
  const auto action = require_action(window, "editPuppetWarpAction");

  action->trigger();
  send_key(*canvas, Qt::Key_Return);  // no pins
  CHECK(!canvas->puppet_warp_active());
  action->trigger();
  CHECK(canvas->add_puppet_pin(QPointF(50.5, 55.5)) == 0);
  CHECK(canvas->add_puppet_pin(QPointF(140.5, 65.5)) == 1);
  CHECK(canvas->add_puppet_pin(QPointF(150.5, 5.5)) == -1);  // off the mesh
  send_key(*canvas, Qt::Key_Return);  // unmoved pins
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == depth);
  CHECK(same_layer_pixels(*read_only.find_layer(layer_id), bounds, pixels));

  action->trigger();
  CHECK(canvas->add_puppet_pin(QPointF(50.5, 55.5)) == 0);
  canvas->set_puppet_pin_position(0, QPointF(80.5, 20.5));
  send_key(*canvas, Qt::Key_Escape);
  CHECK(!canvas->puppet_warp_active());
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == depth);
  CHECK(same_layer_pixels(*read_only.find_layer(layer_id), bounds, pixels));

  action->trigger();
  CHECK(canvas->add_puppet_pin(QPointF(50.5, 55.5)) == 0);
  canvas->set_puppet_pin_position(0, QPointF(62.5, 48.5));  // +12, -7
  send_key(*canvas, Qt::Key_Return);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == depth + 1);
  const auto* moved = read_only.find_layer(layer_id);
  for (int y = 0; y < bounds.height; ++y) {
    for (int x = 0; x < bounds.width; ++x) {
      const auto* expected = pixels.pixel(x, y);
      const auto actual = layer_color_at(*moved, bounds.x + x + 12, bounds.y + y - 7);
      CHECK(actual == QColor(expected[0], expected[1], expected[2], expected[3]));
    }
  }
}

// Fold the blue half back over the red half: with equal depths the red half wins
// the overlap (row-major order); bringing the folded pin forward puts blue on top.
void ui_puppet_warp_pin_depth_decides_overlap() {
  patchy::ui::MainWindow window;
  show_window(window);
  const auto layer_id = add_bar_document(window);
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  auto* canvas = require_canvas(window);
  auto* forward = window.findChild<QPushButton*>(QStringLiteral("puppetWarpPinForwardButton"));
  CHECK(forward != nullptr);
  const QPointF folded(64.5, 60.5);
  const auto fold = [&](bool bring_forward) {
    require_action(window, "editPuppetWarpAction")->trigger();
    CHECK(canvas->puppet_warp_active());
    CHECK(canvas->add_puppet_pin(QPointF(44.5, 60.5)) == 0);
    CHECK(canvas->add_puppet_pin(QPointF(100.5, 60.5)) == 1);
    CHECK(canvas->add_puppet_pin(QPointF(155.5, 60.5)) == 2);
    canvas->set_puppet_pin_position(2, folded);
    if (bring_forward) {
      CHECK(canvas->selected_puppet_pins() == std::vector<int>{2});
      CHECK(forward->isVisible() && forward->isEnabled());
      forward->click();
    }
    send_key(*canvas, Qt::Key_Return);
    CHECK(!canvas->puppet_warp_active());
    return layer_color_at(*std::as_const(document).find_layer(layer_id), 64, 60);
  };
  CHECK(color_close(fold(false), kRed, 2));
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(color_close(fold(true), kBlue, 2));
  save_widget_artifact("puppet_warp_depth_fold", *canvas);
}

// Smart Objects are refused like Liquify: rasterize first.
void ui_puppet_warp_refuses_smart_object() {
  patchy::ui::MainWindow window;
  show_window(window);
  open_smart_object_fixture(window);
  auto* canvas = require_canvas(window);
  require_action(window, "editPuppetWarpAction")->trigger();
  QApplication::processEvents();
  CHECK(!canvas->puppet_warp_active());
  CHECK(window.statusBar()->currentMessage() == QStringLiteral("Rasterize the Smart Object before using Puppet Warp"));
}

}  // namespace

std::vector<patchy::test::TestCase> puppet_warp_tests() {
  return {
      {"ui_puppet_warp_pins_bend_layer_as_one_undo_step", ui_puppet_warp_pins_bend_layer_as_one_undo_step},
      {"ui_puppet_warp_no_op_cancel_and_single_pin_translation",
       ui_puppet_warp_no_op_cancel_and_single_pin_translation},
      {"ui_puppet_warp_pin_depth_decides_overlap", ui_puppet_warp_pin_depth_decides_overlap},
      {"ui_puppet_warp_refuses_smart_object", ui_puppet_warp_refuses_smart_object},
  };
}
