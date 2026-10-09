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

}  // namespace

std::vector<patchy::test::TestCase> brush_tip_engine_tests() {
  return {
      {"ui_working_brush_spacing_drives_procedural_dabs", ui_working_brush_spacing_drives_procedural_dabs},
  };
}
