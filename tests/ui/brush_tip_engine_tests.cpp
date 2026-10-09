// Brush-engine features that only show through the real canvas or the libraries: the working
// brush's settings must reach the stamp path that MainWindow::push_current_brush_to_canvas feeds,
// and .abr imports must land in the brush and pattern libraries MainWindow wires together.

#include "psd/psd_binary.hpp"
#include "psd/psd_descriptor.hpp"
#include "psd/psd_patterns.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/current_brush.hpp"
#include "ui/main_window.hpp"
#include "ui/pattern_library.hpp"

#include "test_harness.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QTemporaryDir>

#include <memory>
#include <optional>
#include <string>
#include <utility>
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

// Every tip setting added after the original dynamics set round-trips through the sidecar JSON,
// counts as non-default, and reads back as off from an older sidecar that lacks the key.
void ui_brush_tip_settings_json_round_trip() {
  patchy::BrushDynamics dynamics;
  dynamics.noise = true;
  dynamics.texture_mode = patchy::BrushTextureMode::LinearHeight;
  dynamics.texture_brightness = -40.0;
  dynamics.texture_contrast = 75.0;
  dynamics.texture_pattern_id = "b2a4c1d0-1111-2222-3333-444455556666";
  dynamics.texture_pattern_name = "Burlap";
  const auto json = patchy::ui::brush_dynamics_to_json(dynamics);
  CHECK(json.value(QStringLiteral("noise")).toBool());
  CHECK(json.value(QStringLiteral("textureMode")).toString() == QStringLiteral("linearHeight"));
  CHECK(json.value(QStringLiteral("texturePatternName")).toString() == QStringLiteral("Burlap"));
  const auto read = patchy::ui::brush_dynamics_from_json(json);
  CHECK(read.noise);
  CHECK(read.texture_mode == patchy::BrushTextureMode::LinearHeight);
  CHECK(read.texture_brightness == -40.0 && read.texture_contrast == 75.0);
  CHECK(read.texture_pattern_id == dynamics.texture_pattern_id);
  CHECK(patchy::ui::brush_dynamics_to_json(read) == json);

  // Each new field alone is a non-default setting (sidecars get written for it).
  const auto non_default = [](auto&& mutate) {
    patchy::BrushDynamics single;
    mutate(single);
    return !patchy::ui::brush_dynamics_is_default(single);
  };
  CHECK(non_default([](patchy::BrushDynamics& d) { d.noise = true; }));
  CHECK(non_default([](patchy::BrushDynamics& d) { d.texture_mode = patchy::BrushTextureMode::Height; }));
  CHECK(non_default([](patchy::BrushDynamics& d) { d.texture_brightness = 1.0; }));
  CHECK(non_default([](patchy::BrushDynamics& d) { d.texture_contrast = -1.0; }));
  CHECK(non_default([](patchy::BrushDynamics& d) { d.texture_pattern_id = "x"; }));
  CHECK(non_default([](patchy::BrushDynamics& d) { d.texture_pattern_name = "x"; }));

  // Older sidecars without the keys, and unknown tokens, read as the defaults.
  auto legacy = json;
  for (const auto* key : {"noise", "textureMode", "textureBrightness", "textureContrast",
                          "texturePatternId", "texturePatternName"}) {
    legacy.remove(QLatin1String(key));
  }
  CHECK(patchy::ui::brush_dynamics_is_default(patchy::ui::brush_dynamics_from_json(legacy)));
  legacy.insert(QStringLiteral("textureMode"), QStringLiteral("notAMode"));
  CHECK(patchy::ui::brush_dynamics_from_json(legacy).texture_mode == patchy::BrushTextureMode::Multiply);
}

// A Pattern library tile named by the working brush textures its strokes; deleting the pattern
// falls back to the procedural grain without failing.
void ui_brush_texture_pattern_reaches_stroke() {
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  canvas->set_zoom(1.0);
  canvas->set_primary_color(Qt::black);
  QApplication::processEvents();

  // 8 px vertical stripes: document columns x % 16 < 8 are white (take paint), the rest black.
  const QString pattern_id = QStringLiteral("patchy-test-brush-texture-stripes");
  patchy::PixelBuffer stripes(16, 16, patchy::PixelFormat::rgba8());
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      auto* px = stripes.pixel(x, y);
      const auto value = static_cast<std::uint8_t>(x < 8 ? 255 : 0);
      px[0] = value;
      px[1] = value;
      px[2] = value;
      px[3] = 255;
    }
  }
  auto& patterns = window.pattern_library();
  if (const auto* stale = patterns.find_entry_by_pattern_id(pattern_id); stale != nullptr) {
    CHECK(patterns.remove_pattern(stale->storage_id));
  }
  const auto storage_id = patterns.add_pattern(QStringLiteral("Brush Texture Stripes"), stripes,
                                               QStringLiteral("Tests"), pattern_id);
  CHECK(!storage_id.isEmpty());

  window.set_active_brush_tip(patchy::ui::builtin_square_brush_tip_id(), false);
  patchy::ui::BrushEdit edit;
  edit.size = 40;
  edit.softness = 0;
  patchy::BrushDynamics dynamics;
  dynamics.texture_enabled = true;
  dynamics.texture_depth = 1.0;
  dynamics.texture_pattern_id = pattern_id.toStdString();
  edit.dynamics = dynamics;
  window.current_brush().edit(edit);
  CHECK(canvas->has_brush_texture_pattern());

  const auto dark = [canvas](QPoint point) { return canvas_pixel(*canvas, point).lightness() < 128; };
  click_canvas(*canvas, QPoint(100, 100));
  CHECK(dark(QPoint(98, 100)));    // 98 % 16 = 2: white stripe takes paint
  CHECK(!dark(QPoint(106, 100)));  // 106 % 16 = 10: black stripe stays empty

  // Without the pattern the stroke keeps painting with the procedural grain.
  CHECK(patterns.remove_pattern(storage_id));
  QApplication::processEvents();
  CHECK(!canvas->has_brush_texture_pattern());
  click_canvas(*canvas, QPoint(200, 100));
  int painted = 0;
  for (int x = 185; x <= 215; ++x) {
    painted += dark(QPoint(x, 100)) ? 1 : 0;
  }
  CHECK(painted > 0);

  save_widget_artifact("ui_brush_texture_pattern_reaches_stroke", *canvas);
  restore_default_working_brush(window);
}

// --- A small v10 .abr built with the descriptor writer ---

using patchy::psd::DescriptorValue;

DescriptorValue abr_value(DescriptorValue::Type type) {
  DescriptorValue value;
  value.type = type;
  return value;
}

DescriptorValue abr_text(std::string text) {
  auto value = abr_value(DescriptorValue::Type::String);
  value.string_value = std::move(text);
  return value;
}

DescriptorValue abr_number(double number) {
  auto value = abr_value(DescriptorValue::Type::Double);
  value.double_value = number;
  return value;
}

DescriptorValue abr_bool(bool flag) {
  auto value = abr_value(DescriptorValue::Type::Bool);
  value.bool_value = flag;
  return value;
}

DescriptorValue abr_object(std::string class_id, std::vector<std::pair<std::string, DescriptorValue>> items) {
  auto object = std::make_shared<patchy::psd::DescriptorObject>();
  object->class_id = std::move(class_id);
  object->class_id_long_form = object->class_id.size() != 4U;
  for (auto& [key, item] : items) {
    object->key_order.push_back({key, key.size() != 4U});
    object->values[key] = std::move(item);
  }
  auto value = abr_value(DescriptorValue::Type::Object);
  value.object_value = std::move(object);
  return value;
}

DescriptorValue abr_list(std::vector<DescriptorValue> items) {
  auto value = abr_value(DescriptorValue::Type::List);
  value.list_value = std::move(items);
  return value;
}

void abr_block(patchy::psd::BigEndianWriter& file, const char* key, const std::vector<std::uint8_t>& block) {
  for (const auto* text : {"8BIM", key}) {
    for (int i = 0; i < 4; ++i) {
      file.write_u8(static_cast<std::uint8_t>(text[i]));
    }
  }
  file.write_u32(static_cast<std::uint32_t>(block.size()));
  file.write_bytes(block);
  while (file.bytes().size() % 4U != 0U) {
    file.write_u8(0);
  }
}

std::vector<std::uint8_t> abr_descriptor_block(const DescriptorValue& root) {
  patchy::psd::BigEndianWriter writer;
  writer.write_u32(16);
  patchy::psd::write_descriptor(writer, *root.object_value);
  return writer.bytes();
}

// A computed "Soft Oval" (40 px, size captured) inside folder "Inks", and a sampled "Grain Ink"
// textured with an embedded pattern plus a "Ghost Ink" whose pattern is nowhere; spacing is
// turned off on one tip so the import reports it.
QByteArray make_ui_test_abr(const std::string& pattern_id) {
  const auto computed = abr_object(
      "brushPreset", {{"Nm  ", abr_text("Soft Oval")},
                      {"Brsh", abr_object("computedBrush", {{"Dmtr", abr_number(40.0)},
                                                            {"Hrdn", abr_number(0.0)},
                                                            {"Rndn", abr_number(50.0)},
                                                            {"Spcn", abr_number(25.0)},
                                                            {"Intr", abr_bool(false)}})},
                      {"useBrushSize", abr_bool(true)}});
  const auto textured = [&pattern_id](const char* name, const std::string& id, const char* pattern_name) {
    return abr_object(
        "brushPreset",
        {{"Nm  ", abr_text(name)},
         {"Brsh", abr_object("sampledBrush", {{"Dmtr", abr_number(8.0)}, {"sampledData", abr_text("tip")}})},
         {"useBrushSize", abr_bool(false)},
         {"useTexture", abr_bool(true)},
         {"textureDepth", abr_number(100.0)},
         {"Txtr", abr_object("Ptrn", {{"Nm  ", abr_text(pattern_name)}, {"Idnt", abr_text(id)}})}});
  };
  const auto desc = abr_descriptor_block(abr_object(
      "null", {{"Brsh", abr_list({computed, textured("Grain Ink", pattern_id, "Grain"),
                                  textured("Ghost Ink", "pat-ghost-missing", "Ghost")})}}));
  const auto hierarchy = abr_descriptor_block(abr_object(
      "null", {{"hierarchy", abr_list({abr_object("Grup", {{"Nm  ", abr_text("Inks")}}), abr_object("preset", {}),
                                       abr_object("groupEnd", {}), abr_object("preset", {}),
                                       abr_object("preset", {})})}}));
  patchy::psd::BigEndianWriter samp;
  {
    patchy::psd::BigEndianWriter entry;
    entry.write_u8(3);
    for (const char c : std::string("tip")) {
      entry.write_u8(static_cast<std::uint8_t>(c));
    }
    for (int i = 4; i < 47; ++i) {
      entry.write_u8(0);
    }
    for (const auto bound : {0U, 0U, 8U, 8U}) {
      entry.write_u32(bound);
    }
    entry.write_u16(8);
    entry.write_u8(0);
    for (int i = 0; i < 64; ++i) {
      entry.write_u8(255);
    }
    samp.write_u32(static_cast<std::uint32_t>(entry.bytes().size()));
    samp.write_bytes(entry.bytes());
    while (samp.bytes().size() % 4U != 0U) {
      samp.write_u8(0);
    }
  }
  patchy::PatternResource grain;
  grain.id = pattern_id;
  grain.name = "Grain";
  grain.tile = patchy::PixelBuffer(4, 4, patchy::PixelFormat::rgba8());
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      auto* px = grain.tile.pixel(x, y);
      px[0] = px[1] = px[2] = static_cast<std::uint8_t>((x + y) % 2 == 0 ? 255 : 0);
      px[3] = 255;
    }
  }
  const std::vector<patchy::PatternResource> patterns{grain};

  patchy::psd::BigEndianWriter file;
  file.write_u16(10);
  file.write_u16(1);
  abr_block(file, "samp", samp.bytes());
  abr_block(file, "patt", patchy::psd::serialize_patterns_block(patterns));
  abr_block(file, "desc", desc);
  abr_block(file, "phry", hierarchy);
  const auto bytes = file.bytes();
  return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

// The import through MainWindow's own libraries: preset folders nest under the file's folder,
// the computed tip imports, the embedded pattern joins the Pattern library and textures the
// brush, picking the tip applies its captured size, and one line each reports the unmapped
// settings and the pattern found nowhere.
void ui_abr_import_fills_folders_patterns_and_size() {
  clear_brush_tip_test_state();
  QTemporaryDir scratch;
  CHECK(scratch.isValid());
  const auto path = QDir(scratch.path()).filePath(QStringLiteral("Test Set.abr"));
  const std::string pattern_id = "patchy-test-abr-embedded-grain";
  {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(make_ui_test_abr(pattern_id)) > 0);
  }

  patchy::ui::MainWindow window;
  show_window(window);
  auto& patterns = window.pattern_library();
  const auto qt_pattern_id = QString::fromStdString(pattern_id);
  if (const auto* stale = patterns.find_entry_by_pattern_id(qt_pattern_id); stale != nullptr) {
    CHECK(patterns.remove_pattern(stale->storage_id));
  }

  auto& library = window.brush_tip_library();
  QString error;
  QStringList warnings;
  const auto first_id = library.import_abr(path, error, warnings);
  CHECK(!first_id.isEmpty());
  CHECK(error.isEmpty());

  const auto find = [&library](const QString& name) -> const patchy::ui::BrushTipEntry* {
    for (const auto& entry : library.entries()) {
      if (entry.name == name) {
        return &entry;
      }
    }
    return nullptr;
  };
  const auto* oval = find(QStringLiteral("Soft Oval"));
  const auto* grain_ink = find(QStringLiteral("Grain Ink"));
  CHECK(oval != nullptr && grain_ink != nullptr && find(QStringLiteral("Ghost Ink")) != nullptr);
  CHECK(oval->folder == QStringLiteral("Test Set / Inks"));
  CHECK(grain_ink->folder == QStringLiteral("Test Set"));
  CHECK(oval->default_size == std::optional<int>(40));
  CHECK(!grain_ink->default_size.has_value());
  CHECK(oval->base_roundness == 50.0);

  const auto* pattern_entry = patterns.find_entry_by_pattern_id(qt_pattern_id);
  CHECK(pattern_entry != nullptr);
  CHECK(pattern_entry->folder == QStringLiteral("Test Set"));
  CHECK(grain_ink->dynamics.texture_pattern_id == pattern_id);

  int unmapped_lines = 0;
  int missing_pattern_lines = 0;
  for (const auto& warning : warnings) {
    unmapped_lines += warning.contains(QStringLiteral("no Patchy equivalent")) &&
                              warning.contains(QStringLiteral("Spacing turned off"))
                          ? 1
                          : 0;
    missing_pattern_lines += warning.contains(QStringLiteral("Ghost")) ? 1 : 0;
  }
  CHECK(unmapped_lines == 1);
  CHECK(missing_pattern_lines == 1);

  // The captured size survives a reload and applies on pick; the pattern reaches the canvas.
  const auto oval_id = oval->id;
  const auto grain_id = grain_ink->id;
  library.refresh_from_disk();
  CHECK(library.find_entry(oval_id)->default_size == std::optional<int>(40));
  window.set_active_brush_tip(oval_id, false);
  CHECK(window.current_brush().brush().size == 40);
  window.set_active_brush_tip(grain_id, false);
  CHECK(window.current_brush().brush().size == 40);  // a tip without a captured size keeps it
  CHECK(require_canvas(window)->has_brush_texture_pattern());

  if (const auto* imported = patterns.find_entry_by_pattern_id(qt_pattern_id); imported != nullptr) {
    CHECK(patterns.remove_pattern(imported->storage_id));
  }
  restore_default_working_brush(window);
}

}  // namespace

std::vector<patchy::test::TestCase> brush_tip_engine_tests() {
  return {
      {"ui_working_brush_spacing_drives_procedural_dabs", ui_working_brush_spacing_drives_procedural_dabs},
      {"ui_static_tip_flip_persists_and_reaches_strokes", ui_static_tip_flip_persists_and_reaches_strokes},
      {"ui_brush_tip_settings_json_round_trip", ui_brush_tip_settings_json_round_trip},
      {"ui_brush_texture_pattern_reaches_stroke", ui_brush_texture_pattern_reaches_stroke},
      {"ui_abr_import_fills_folders_patterns_and_size", ui_abr_import_fills_folders_patterns_and_size},
  };
}
