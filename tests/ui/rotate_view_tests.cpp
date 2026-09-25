// Rotate View: the view/widget mapping under rotation, input landing on the
// right document pixels, the tool's drag/snap/reset gestures, the MainWindow
// wiring (R, options bar, per-document angle, no history), and rendered
// artifacts of a rotated canvas. See docs/rotate-view.md.

#include "ui_test_support.hpp"
#include "ui_test_access.hpp"

#include "ui_test_groups.hpp"

#include "ui/theme_palette.hpp"

#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTabletEvent>
#include <QtMath>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>

#include <cmath>

namespace {

using namespace patchy::test::ui;
using patchy::ui::CanvasTool;
using patchy::ui::CanvasWidget;

// Exact (sub-pixel) pointer events: rotated widget positions are fractional,
// and rounding them to QPoint would move the document point being tested.
void send_mouse_at(CanvasWidget& canvas, QEvent::Type type, QPointF position, Qt::MouseButton button,
                   Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QMouseEvent event(type, position, canvas.mapToGlobal(position), button, buttons, modifiers);
  QApplication::sendEvent(&canvas, &event);
  QApplication::processEvents();
}

// A press/move/release through document points, each sent at the exact widget
// position of the pixel center.
void drag_document_centers(CanvasWidget& canvas, QPointF from, QPointF to, int steps) {
  const auto at = [&canvas](QPointF document_point) {
    return canvas.widget_point_for_document_point(document_point + QPointF(0.5, 0.5));
  };
  send_mouse_at(canvas, QEvent::MouseButtonPress, at(from), Qt::LeftButton, Qt::LeftButton);
  for (int i = 1; i <= steps; ++i) {
    send_mouse_at(canvas, QEvent::MouseMove, at(from + (to - from) * (static_cast<double>(i) / steps)),
                  Qt::NoButton, Qt::LeftButton);
  }
  send_mouse_at(canvas, QEvent::MouseButtonRelease, at(to), Qt::LeftButton, Qt::NoButton);
}

bool near(QPointF a, QPointF b, double tolerance) {
  return std::abs(a.x() - b.x()) <= tolerance && std::abs(a.y() - b.y()) <= tolerance;
}

void ui_rotate_view_maps_points_and_turns_about_the_center() {
  patchy::Document document(240, 160, patchy::PixelFormat::rgba8());
  CanvasWidget canvas;
  canvas.resize(420, 300);
  canvas.set_document(&document);
  canvas.show();
  QApplication::processEvents();
  const QPointF widget_center(canvas.width() / 2.0, canvas.height() / 2.0);

  for (const double zoom : {0.25, 1.0, 3.0, 12.0}) {
    for (const double angle : {30.0, 90.0, -135.0, 180.0}) {
      canvas.set_view_rotation(0.0);
      canvas.set_zoom_centered(zoom);
      canvas.center_document_in_view();
      const auto centered = canvas.document_point_for_widget_position(widget_center);
      canvas.set_view_rotation(angle);
      CHECK(canvas.view_rotated());
      // The view turns about the viewport center: the document point there stays put.
      CHECK(near(canvas.document_point_for_widget_position(widget_center), centered, 1e-6));
      // Widget and document mapping invert each other at every angle and zoom.
      for (const QPointF point : {QPointF(0.0, 0.0), QPointF(17.25, 9.5), QPointF(120.0, 80.0),
                                  QPointF(239.75, 159.5)}) {
        const auto widget_point = canvas.widget_point_for_document_point(point);
        CHECK(near(canvas.document_point_for_widget_position(widget_point), point, 1e-6));
      }
      // Positive angles turn clockwise on screen.
      const auto turned = QTransform().rotate(angle).map(QPointF(10.0 * zoom, 0.0));
      CHECK(near(canvas.widget_point_for_document_point(centered + QPointF(10.0, 0.0)), widget_center + turned,
                 1e-6));
    }
  }

  // The angle stays in (-180, 180]; a full turn is unrotated again.
  canvas.set_view_rotation(190.0);
  CHECK(std::abs(canvas.view_rotation() + 170.0) < 1e-9);
  canvas.set_view_rotation(-180.0);
  CHECK(std::abs(canvas.view_rotation() - 180.0) < 1e-9);
  canvas.set_view_rotation(360.0);
  CHECK(canvas.view_rotation() == 0.0);
  CHECK(!canvas.view_rotated());

  // The scroll bars follow the on-screen bounds: turned 90 degrees at 100%, the
  // horizontal bar spans the document's 160 px height, clamped like the pan
  // (16 px must stay visible in a 420 px viewport): (420 - 16) - (16 - 160).
  canvas.set_zoom_centered(1.0);
  canvas.set_view_rotation(90.0);
  auto* horizontal = canvas.findChild<QScrollBar*>(QStringLiteral("canvasHorizontalScrollBar"));
  CHECK(horizontal != nullptr);
  CHECK(horizontal->maximum() == 548);
  CHECK(horizontal->pageStep() == 420);
}

void ui_rotate_view_input_lands_on_document_pixels() {
  patchy::Document document(200, 160, patchy::PixelFormat::rgba8());
  auto& layer = document.add_pixel_layer("Paint",
                                         solid_pixels(200, 160, patchy::PixelFormat::rgba8(), QColor(Qt::white)));
  CanvasWidget canvas;
  canvas.resize(420, 340);
  canvas.set_document(&document);
  canvas.show();
  QApplication::processEvents();
  canvas.set_zoom_centered(1.5);

  // A marquee dragged on a rotated view selects the same document rectangle
  // as the same document-space drag on an unrotated view.
  canvas.set_tool(CanvasTool::Marquee);
  drag_document_centers(canvas, QPointF(30.0, 100.0), QPointF(130.0, 140.0), 4);
  const auto unrotated_selection = canvas.selected_document_rect();
  CHECK(unrotated_selection.has_value());
  for (const double angle : {37.0, 90.0, -150.0}) {
    canvas.clear_selection();  // a press inside a selection would move it
    canvas.set_view_rotation(angle);
    drag_document_centers(canvas, QPointF(30.0, 100.0), QPointF(130.0, 140.0), 4);
    CHECK(canvas.selected_document_rect() == unrotated_selection);
  }
  canvas.select_all();

  // A brush stroke along the document's x axis paints that row, whatever
  // direction it runs on screen.
  canvas.set_tool(CanvasTool::Brush);
  canvas.set_primary_color(Qt::black);
  canvas.set_brush_size(6);
  canvas.set_brush_opacity(100);
  canvas.set_brush_softness(0);
  canvas.set_view_rotation(37.0);
  drag_document_centers(canvas, QPointF(40.0, 50.0), QPointF(160.0, 50.0), 24);
  CHECK(layer.pixels().pixel(100, 50)[0] < 40);
  CHECK(layer.pixels().pixel(100, 40)[0] > 240);
  CHECK(layer.pixels().pixel(100, 60)[0] > 240);
  CHECK(layer.pixels().pixel(30, 50)[0] > 240);

  // Tablet input maps the same way, and the pen's screen-relative tilt is
  // turned into document terms (90 degrees clockwise: screen +x is document -y).
  canvas.set_view_rotation(90.0);
  const auto target = QPointF(70.5, 120.5);
  const auto widget_point = canvas.widget_point_for_document_point(target);
  const auto& pen = tablet_test_device(QPointingDevice::PointerType::Pen,
                                       QInputDevice::Capability::Position | QInputDevice::Capability::Pressure |
                                           QInputDevice::Capability::XTilt | QInputDevice::Capability::YTilt);
  QTabletEvent hover(QEvent::TabletMove, &pen, widget_point, canvas.mapToGlobal(widget_point), 0.0, 30.0F, 0.0F,
                     0.0F, 0.0, 0.0F, Qt::NoModifier, Qt::NoButton, Qt::NoButton);
  QApplication::sendEvent(&canvas, &hover);
  const auto sample = canvas.last_pen_input_sample();
  CHECK(sample.has_value());
  CHECK(near(sample->document_position, target, 1e-6));
  CHECK(std::abs(sample->x_tilt) < 1e-4F);
  CHECK(std::abs(sample->y_tilt + 30.0F) < 1e-4F);
}

void ui_rotate_view_tool_drags_snaps_and_resets() {
  patchy::Document document(200, 160, patchy::PixelFormat::rgba8());
  const auto& layer =
      document.add_pixel_layer("Paint", solid_pixels(200, 160, patchy::PixelFormat::rgba8(), QColor(Qt::white)));
  CanvasWidget canvas;
  canvas.resize(400, 300);
  canvas.set_document(&document);
  canvas.set_tool(CanvasTool::RotateView);
  canvas.show();
  QApplication::processEvents();
  const QPointF center(canvas.width() / 2.0, canvas.height() / 2.0);
  const auto revision = layer.content_revision();

  // Dragging from the right of the center to below it turns the view a
  // quarter clockwise, like turning paper on a desk.
  send_mouse_at(canvas, QEvent::MouseButtonPress, center + QPointF(100.0, 0.0), Qt::LeftButton, Qt::LeftButton);
  send_mouse_at(canvas, QEvent::MouseMove, center + QPointF(0.0, 100.0), Qt::NoButton, Qt::LeftButton);
  send_mouse_at(canvas, QEvent::MouseButtonRelease, center + QPointF(0.0, 100.0), Qt::LeftButton, Qt::NoButton);
  CHECK(std::abs(canvas.view_rotation() - 90.0) < 1e-6);

  // A second drag continues from the current angle; Shift snaps the result to 15 degrees.
  const auto toward = [&center](double degrees) {
    return center + QPointF(100.0 * std::cos(qDegreesToRadians(degrees)), 100.0 * std::sin(qDegreesToRadians(degrees)));
  };
  send_mouse_at(canvas, QEvent::MouseButtonPress, toward(0.0), Qt::LeftButton, Qt::LeftButton);
  send_mouse_at(canvas, QEvent::MouseMove, toward(-52.0), Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
  CHECK(std::abs(canvas.view_rotation() - 45.0) < 1e-6);
  send_mouse_at(canvas, QEvent::MouseMove, toward(-52.0), Qt::NoButton, Qt::LeftButton);
  CHECK(std::abs(canvas.view_rotation() - 38.0) < 1e-6);
  send_mouse_at(canvas, QEvent::MouseButtonRelease, toward(-52.0), Qt::LeftButton, Qt::NoButton);

  // Esc with the tool active resets the view. None of it touched the document.
  send_key(canvas, Qt::Key_Escape);
  CHECK(canvas.view_rotation() == 0.0);
  CHECK(layer.content_revision() == revision);
}

void ui_rotate_view_tool_wiring_in_main_window() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);

  // R is Rotate View (Photoshop's default); Smudge ships unbound.
  auto* rotate_action = require_hotkey_action(window, QStringLiteral("tools.rotate_view"));
  CHECK(rotate_action->shortcut() == QKeySequence(Qt::Key_R));
  CHECK(require_hotkey_action(window, QStringLiteral("tools.smudge"))->shortcut().isEmpty());
  rotate_action->trigger();
  QApplication::processEvents();
  CHECK(canvas->tool() == CanvasTool::RotateView);

  auto* angle = window.findChild<QDoubleSpinBox*>(QStringLiteral("rotateViewAngleSpin"));
  auto* reset = window.findChild<QPushButton*>(QStringLiteral("rotateViewResetButton"));
  CHECK(angle != nullptr && reset != nullptr);
  CHECK(angle->isVisible() && reset->isVisible());

  // The field drives the active canvas; the rotation is view state only, so it
  // adds no history and leaves the document unmodified.
  const auto undo_depth = patchy::ui::MainWindowTestAccess::active_session_undo_depth(window);
  const auto modified = patchy::ui::MainWindowTestAccess::active_session_is_modified(window);
  angle->setValue(30.0);
  CHECK(std::abs(canvas->view_rotation() - 30.0) < 1e-9);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_undo_depth(window) == undo_depth);
  CHECK(patchy::ui::MainWindowTestAccess::active_session_is_modified(window) == modified);

  // Each document keeps its own angle, and the field follows the active one.
  patchy::ui::MainWindowTestAccess::create_default_document(window);
  QApplication::processEvents();
  auto* second = require_canvas(window);
  CHECK(second != canvas);
  CHECK(second->view_rotation() == 0.0);
  CHECK(angle->value() == 0.0);
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  CHECK(tabs != nullptr);
  tabs->setCurrentIndex(0);
  QApplication::processEvents();
  CHECK(require_canvas(window) == canvas);
  CHECK(std::abs(angle->value() - 30.0) < 1e-9);

  // An inline text session shows the view upright while it is open (its
  // editor widget cannot turn) and brings the angle back when it ends. The
  // click still places the text at the document point under the pointer.
  require_action_by_text(window, QStringLiteral("Type"))->trigger();
  const auto open_text_session = [canvas] {
    const auto click = canvas->widget_point_for_document_point(QPointF(300.5, 200.5));
    send_mouse_at(*canvas, QEvent::MouseButtonPress, click, Qt::LeftButton, Qt::LeftButton);
    send_mouse_at(*canvas, QEvent::MouseButtonRelease, click, Qt::LeftButton, Qt::NoButton);
    auto* editor = canvas->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor"));
    CHECK(editor != nullptr);
    return editor;
  };
  canvas->set_view_rotation(0.0);
  auto* editor = open_text_session();
  const QPoint unrotated_anchor(editor->property("patchy.documentTextX").toInt(),
                                editor->property("patchy.documentTextY").toInt());
  send_key(*editor, Qt::Key_Escape);
  canvas->set_view_rotation(30.0);
  editor = open_text_session();
  CHECK(QPoint(editor->property("patchy.documentTextX").toInt(), editor->property("patchy.documentTextY").toInt()) ==
        unrotated_anchor);
  CHECK(!canvas->view_rotated());
  CHECK(std::abs(canvas->view_rotation() - 30.0) < 1e-9);
  send_key(*editor, Qt::Key_Escape);
  CHECK(canvas->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor")) == nullptr);
  CHECK(canvas->view_rotated());
  rotate_action->trigger();

  // Fit on Screen keeps the angle; Reset View and a double-click on the tool
  // button both return to 0.
  require_action(window, "viewFitOnScreenAction")->trigger();
  CHECK(std::abs(canvas->view_rotation() - 30.0) < 1e-9);
  reset->click();
  CHECK(canvas->view_rotation() == 0.0);
  CHECK(angle->value() == 0.0);
  canvas->set_view_rotation(-60.0);
  auto* view_button = window.findChild<QToolButton*>(QStringLiteral("viewToolButton"));
  CHECK(view_button != nullptr);
  CHECK(view_button->defaultAction() == rotate_action);
  send_double_click(*view_button, view_button->rect().center());
  CHECK(canvas->view_rotation() == 0.0);
}

// Rendered checks plus artifacts for eyeballing (test-artifacts/rotate_view_*.png):
// the document turns about the viewport center, the backdrop fills the
// uncovered corners, deep zoom keeps pixel squares crisp, and the selection
// outline, grid and large-brush outline all turn with the document.
void ui_rotate_view_renders_the_turned_canvas() {
  patchy::Document document(240, 160, patchy::PixelFormat::rgba8());
  auto pixels = solid_pixels(240, 160, patchy::PixelFormat::rgba8(), QColor(235, 235, 235));
  fill_pixel_rect(pixels, QRect(0, 0, 120, 80), QColor(200, 40, 40));
  fill_pixel_rect(pixels, QRect(120, 80, 120, 80), QColor(40, 80, 200));
  fill_pixel_rect(pixels, QRect(118, 78, 2, 2), QColor(20, 20, 20));
  fill_pixel_rect(pixels, QRect(160, 0, 80, 40), QColor(0, 0, 0, 0));  // transparent corner
  document.add_pixel_layer("Paint", std::move(pixels));
  CanvasWidget canvas;
  canvas.resize(480, 360);
  canvas.set_document(&document);
  canvas.set_tool(CanvasTool::Brush);
  canvas.show();
  QApplication::processEvents();

  // Low zoom: whole document, selection, grid and a large brush outline.
  canvas.set_zoom_centered(1.2);
  canvas.set_view_rotation(30.0);
  canvas.set_tool(CanvasTool::Marquee);
  drag_document_centers(canvas, QPointF(20.0, 20.0), QPointF(100.0, 120.0), 3);
  canvas.set_grid_visible(true);
  canvas.set_tool(CanvasTool::Brush);
  canvas.set_brush_size(160);
  send_mouse_at(canvas, QEvent::MouseMove, canvas.widget_point_for_document_point(QPointF(170.0, 110.0)),
                Qt::NoButton, Qt::NoButton);
  const auto low = canvas.grab().toImage();
  save_image_artifact("rotate_view_low_zoom", low);
  // A small square brush uses an OS cursor; its footprint turns with the view.
  canvas.set_brush_shape(patchy::BrushShape::Square);
  canvas.set_brush_size(40);
  CHECK(canvas.cursor().shape() == Qt::BitmapCursor);
  save_image_artifact("rotate_view_square_brush_cursor", canvas.cursor().pixmap().toImage());
  canvas.set_brush_shape(patchy::BrushShape::Round);
  const auto color_at = [&low, &canvas](QPointF document_point) {
    return low.pixelColor(canvas.widget_point_for_document_point(document_point).toPoint());
  };
  CHECK(color_close(color_at(QPointF(60.0, 60.0)), QColor(200, 40, 40), 12));
  CHECK(color_close(color_at(QPointF(200.0, 130.0)), QColor(40, 80, 200), 12));
  // Past the turned document's edge the canvas backdrop shows.
  CHECK(color_close(low.pixelColor(2, 2), patchy::ui::theme().canvas_backdrop, 4));

  // Deep zoom on the dark 2x2 block: nearest sampling keeps it a solid square.
  canvas.set_grid_visible(false);
  canvas.select_all();
  canvas.set_zoom_centered(24.0);
  const auto block_center = canvas.widget_point_for_document_point(QPointF(119.0, 79.0));
  const auto shift = QPointF(canvas.width() / 2.0, canvas.height() / 2.0) - block_center;
  // Pan the block to the viewport center with the hand tool.
  canvas.set_tool(CanvasTool::Pan);
  send_mouse_at(canvas, QEvent::MouseButtonPress, block_center, Qt::LeftButton, Qt::LeftButton);
  send_mouse_at(canvas, QEvent::MouseMove, block_center + shift, Qt::NoButton, Qt::LeftButton);
  send_mouse_at(canvas, QEvent::MouseButtonRelease, block_center + shift, Qt::LeftButton, Qt::NoButton);
  canvas.set_tool(CanvasTool::Brush);
  canvas.set_brush_size(12);
  send_mouse_at(canvas, QEvent::MouseMove, canvas.widget_point_for_document_point(QPointF(116.0, 81.0)),
                Qt::NoButton, Qt::NoButton);
  const auto deep = canvas.grab().toImage();
  save_image_artifact("rotate_view_deep_zoom", deep);
  const auto block = canvas.widget_point_for_document_point(QPointF(119.0, 79.0)).toPoint();
  CHECK(color_close(deep.pixelColor(block), QColor(20, 20, 20), 6));
  // Every pixel well inside the turned 2x2 block is the block color: no
  // smoothing bleeds across document pixels at this zoom.
  int solid = 0;
  for (int dy = -12; dy <= 12; ++dy) {
    for (int dx = -12; dx <= 12; ++dx) {
      solid += color_close(deep.pixelColor(block + QPoint(dx, dy)), QColor(20, 20, 20), 6) ? 1 : 0;
    }
  }
  CHECK(solid == 25 * 25);

  // A 2 px grid at deep zoom draws a line on every pixel edge; for eyeballing
  // that the turned lines sit exactly on the turned pixel squares.
  document.grid_settings().horizontal_cycle_32 = 2 * 32;
  document.grid_settings().vertical_cycle_32 = 2 * 32;
  canvas.set_grid_visible(true);
  save_widget_artifact("rotate_view_deep_zoom_grid", canvas);
  canvas.set_grid_visible(false);
}

}  // namespace

std::vector<patchy::test::TestCase> rotate_view_tests() {
  return {
      {"ui_rotate_view_maps_points_and_turns_about_the_center", ui_rotate_view_maps_points_and_turns_about_the_center},
      {"ui_rotate_view_input_lands_on_document_pixels", ui_rotate_view_input_lands_on_document_pixels},
      {"ui_rotate_view_tool_drags_snaps_and_resets", ui_rotate_view_tool_drags_snaps_and_resets},
      {"ui_rotate_view_tool_wiring_in_main_window", ui_rotate_view_tool_wiring_in_main_window},
      {"ui_rotate_view_renders_the_turned_canvas", ui_rotate_view_renders_the_turned_canvas},
  };
}
