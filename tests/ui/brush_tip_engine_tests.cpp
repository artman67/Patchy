// Brush-engine features that only show through the real canvas: the working brush's settings
// must reach the stamp path that MainWindow::push_current_brush_to_canvas feeds.

#include "ui/brush_tip_library.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/current_brush.hpp"
#include "ui/main_window.hpp"

#include "test_harness.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>

#include <optional>
#include <vector>

#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

namespace {

using namespace patchy::test::ui;

// Paints one horizontal drag and reports whether the gap between the first two dabs stayed
// unpainted (the stroke shows separate dabs) and the dab centers are painted.
struct DabStrokeProbe {
  bool first_dab_painted{false};
  bool second_dab_painted{false};
  bool gap_painted{false};
  bool beside_first_painted{false};  // an off-cadence dab next to the press dab
  bool first_corner_painted{false};  // only a square footprint reaches the corner
};

DabStrokeProbe paint_probe_stroke(patchy::ui::CanvasWidget& canvas, int y) {
  // Size 20 at spacing 3.0 places dab centers 60 px apart: x = 60, 120, 180...
  drag_document_path(canvas, {QPoint(60, y), QPoint(300, y)}, 24);
  QApplication::processEvents();
  const auto dark = [&canvas](int x, int row) { return canvas_pixel(canvas, QPoint(x, row)).lightness() < 128; };
  return DabStrokeProbe{dark(60, y), dark(120, y), dark(90, y), dark(73, y), dark(68, y + 8)};
}

// The working brush persists under tools/workingBrush and later tests launch with it, so every
// test here leaves the plain Round brush behind.
void restore_default_working_brush(patchy::ui::MainWindow& window) {
  patchy::ui::BrushEdit edit;
  edit.size = 25;
  edit.spacing = std::optional<double>{};
  edit.dynamics = patchy::BrushDynamics{};
  window.set_active_brush_tip(patchy::ui::builtin_round_brush_tip_id(), false);
  window.current_brush().edit(edit);
  clear_brush_tip_test_state();
}

void ui_working_brush_spacing_drives_procedural_dabs() {
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  canvas->set_zoom(1.0);
  canvas->set_primary_color(Qt::black);
  QApplication::processEvents();

  auto& brush = window.current_brush();
  const auto set_spacing = [&brush](std::optional<double> spacing) {
    patchy::ui::BrushEdit edit;
    edit.spacing = spacing;
    brush.edit(edit);
  };
  const auto base_edit = [&brush] {
    patchy::ui::BrushEdit edit;
    edit.size = 20;
    edit.opacity = 100;
    edit.flow = 100;
    edit.softness = 0;
    edit.airbrush = false;
    brush.edit(edit);
  };

  // Hard Round: an explicit spacing leaves visible gaps; clearing it restores the continuous
  // capsule (the historical path, byte-pinned elsewhere).
  window.set_active_brush_tip(patchy::ui::builtin_round_brush_tip_id(), false);
  base_edit();
  set_spacing(3.0);
  CHECK(canvas->brush_spacing_override() == std::optional<double>(3.0));
  const auto spaced = paint_probe_stroke(*canvas, 80);
  CHECK(spaced.first_dab_painted);
  CHECK(spaced.second_dab_painted);
  CHECK(!spaced.gap_painted);
  CHECK(!spaced.beside_first_painted);
  CHECK(!spaced.first_corner_painted);
  set_spacing(std::nullopt);
  CHECK(!canvas->brush_spacing_override().has_value());
  CHECK(paint_probe_stroke(*canvas, 130).gap_painted);

  // Square: the same override drives its dab path (a tip pick resets spacing, so set it after).
  window.set_active_brush_tip(patchy::ui::builtin_square_brush_tip_id(), false);
  base_edit();
  set_spacing(3.0);
  const auto square = paint_probe_stroke(*canvas, 180);
  CHECK(square.first_dab_painted);
  CHECK(square.second_dab_painted);
  CHECK(!square.gap_painted);
  CHECK(!square.beside_first_painted);
  CHECK(square.first_corner_painted);

  // Round with active dynamics stamps the synthesized disc tip at the same spacing. A pen
  // pressure size control activates dynamics but paints full size with a mouse.
  window.set_active_brush_tip(patchy::ui::builtin_round_brush_tip_id(), false);
  base_edit();
  patchy::ui::BrushEdit dynamics_edit;
  patchy::BrushDynamics dynamics;
  dynamics.size_control = patchy::BrushDynamicControl::PenPressure;
  dynamics_edit.dynamics = dynamics;
  dynamics_edit.spacing = 3.0;
  brush.edit(dynamics_edit);
  CHECK(canvas->brush_dynamics().active());
  const auto dynamic = paint_probe_stroke(*canvas, 230);
  CHECK(dynamic.first_dab_painted);
  CHECK(dynamic.second_dab_painted);
  CHECK(!dynamic.gap_painted);
  CHECK(!dynamic.beside_first_painted);

  save_widget_artifact("ui_working_brush_spacing_drives_procedural_dabs", *canvas);
  restore_default_working_brush(window);
}

// 32x32 coverage with the lower-left triangle painted (x <= y), so a mirror is visible and
// cropping to content keeps the full square.
QImage make_triangle_tip_image() {
  QImage mask(32, 32, QImage::Format_Grayscale8);
  for (int y = 0; y < 32; ++y) {
    auto* row = mask.scanLine(y);
    for (int x = 0; x < 32; ++x) {
      row[x] = x <= y ? 255 : 0;
    }
  }
  return mask;
}

void click_canvas(patchy::ui::CanvasWidget& canvas, QPoint document_point) {
  const auto position = canvas.widget_position_for_document_point(document_point);
  send_mouse(canvas, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
  send_mouse(canvas, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
}

void ui_static_tip_flip_persists_and_reaches_strokes() {
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  canvas->set_zoom(1.0);
  canvas->set_primary_color(Qt::black);
  QApplication::processEvents();

  // The flips persist with the dynamics JSON and count as a non-default tip setting.
  patchy::BrushDynamics flipped;
  flipped.tip_flip_x = true;
  CHECK(!patchy::ui::brush_dynamics_is_default(flipped));
  const auto json = patchy::ui::brush_dynamics_to_json(flipped);
  CHECK(json.value(QStringLiteral("tipFlipX")).toBool());
  CHECK(!json.value(QStringLiteral("tipFlipY")).toBool(true));
  CHECK(patchy::ui::brush_dynamics_from_json(json).tip_flip_x);
  CHECK(!patchy::ui::brush_dynamics_from_json(QJsonObject{}).tip_flip_x);

  auto& library = window.brush_tip_library();
  const auto tip_id = library.add_tip(QStringLiteral("Triangle"), make_triangle_tip_image(), 0.25);
  CHECK(!tip_id.isEmpty());
  CHECK(library.set_tip_dynamics(tip_id, flipped, 0.0, 100.0));
  library.refresh_from_disk();
  const auto* entry = library.find_entry(tip_id);
  CHECK(entry != nullptr && entry->dynamics.tip_flip_x && !entry->dynamics.tip_flip_y);

  // Picking the tip loads its stored flip; the stroke mirrors the stamp.
  window.set_active_brush_tip(tip_id, false);
  patchy::ui::BrushEdit size_edit;
  size_edit.size = 32;
  size_edit.softness = 0;
  window.current_brush().edit(size_edit);
  CHECK(window.current_brush().brush().dynamics.tip_flip_x);
  const auto dark = [canvas](QPoint point) { return canvas_pixel(*canvas, point).lightness() < 128; };
  click_canvas(*canvas, QPoint(100, 100));
  CHECK(!dark(QPoint(90, 105)));  // lower-left is empty once mirrored
  CHECK(dark(QPoint(110, 105)));

  // Clearing the flip restores the authored orientation, with no dynamics involved.
  patchy::ui::BrushEdit unflip;
  unflip.dynamics = patchy::BrushDynamics{};
  window.current_brush().edit(unflip);
  CHECK(!canvas->brush_dynamics().active());
  click_canvas(*canvas, QPoint(200, 100));
  CHECK(dark(QPoint(190, 105)));
  CHECK(!dark(QPoint(210, 105)));

  save_widget_artifact("ui_static_tip_flip_persists_and_reaches_strokes", *canvas);
  restore_default_working_brush(window);
}

}  // namespace

std::vector<patchy::test::TestCase> brush_tip_engine_tests() {
  return {
      {"ui_working_brush_spacing_drives_procedural_dabs", ui_working_brush_spacing_drives_procedural_dabs},
      {"ui_static_tip_flip_persists_and_reaches_strokes", ui_static_tip_flip_persists_and_reaches_strokes},
  };
}
