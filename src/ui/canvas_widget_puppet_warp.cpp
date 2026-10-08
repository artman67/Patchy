// CanvasWidget's Puppet Warp session (Edit > Puppet Warp): user-placed pins on a
// pixel layer, a moving-least-squares grid from core/puppet_warp, and a preview and
// commit through the warp renderer (resample_warped_rgba8 over the shared warp base
// cache). See docs/puppet-warp.md.
//
// PATENT BOUNDARY (docs/legal-constraints.md): pin rotation and pin depth change
// only through the options-bar setters below. Never add a pop-up or popover next to
// a pin, or a rotation ring, dial or bend handle drawn around a pin, without a new
// review (Adobe US 9053553 / US 9454797 / US 10643365 / US 10964081). Pins are only
// ever placed by the user.

#include "ui/canvas_widget.hpp"
#include "ui/canvas_widget_shared.hpp"

#include "core/layer_metadata.hpp"
#include "core/puppet_warp.hpp"
#include "core/smart_object.hpp"
#include "core/vector_shape.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/image_document_io.hpp"
#include "ui/modifier_names.hpp"
#include "ui/qt_geometry.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <utility>

namespace patchy::ui {

namespace {

// Lattice cells at most this many pixels across: preview and commit render the same
// grid, so what the user sees is what Enter applies.
constexpr double kPuppetRenderCellPixels = 4.0;
constexpr std::size_t kPuppetHistoryLimit = 200;

std::vector<std::uint8_t> alpha_channel(const QImage& image) {
  std::vector<std::uint8_t> alpha(static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.height()));
  for (int y = 0; y < image.height(); ++y) {
    const auto* row = image.constScanLine(y);
    for (int x = 0; x < image.width(); ++x) {
      alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) + static_cast<std::size_t>(x)] =
          row[static_cast<std::size_t>(x) * 4U + 3U];
    }
  }
  return alpha;
}

}  // namespace

bool CanvasWidget::begin_puppet_warp(const PuppetWarpOptions& options) {
  if (puppet_.active) {
    return true;
  }
  if (layer_edit_target_ == LayerEditTarget::SmartFilterMask) {
    report_status_error(tr("This tool is unavailable while editing a Smart Filter mask"));
    return false;
  }
  if (document_ == nullptr) {
    return false;
  }
  finish_free_transform();
  finish_warp_transform();
  auto* layer = active_pixel_layer();
  // Smart Objects are refused like Liquify (no native Puppet Warp Smart Filter is
  // authored); MainWindow offers Rasterize for text and shape layers first.
  if (layer == nullptr || !layer_has_movable_pixels(*layer) || layer_is_smart_object(*layer) ||
      layer_is_text(*layer) || layer_is_vector_shape(*layer)) {
    report_status_error(tr("Select an editable pixel layer to warp"));
    return false;
  }
  auto source = qimage_from_pixel_buffer(std::as_const(*layer).pixels()).convertToFormat(QImage::Format_RGBA8888);
  if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
    report_status_error(tr("Layer has no pixels to warp"));
    return false;
  }
  puppet_ = PuppetWarpSession{};
  puppet_.layer_id = layer->id();
  puppet_.source = std::move(source);
  puppet_.source_bounds = layer->bounds();
  puppet_.options = options;
  rebuild_puppet_warp_mesh();
  if (puppet_warp_mesh_is_empty(puppet_.mesh)) {
    puppet_ = PuppetWarpSession{};
    report_status_error(tr("Layer has no pixels to warp"));
    return false;
  }
  puppet_.active = true;
  set_move_transform_controls_layer(std::nullopt);
  warp_base_display_mip_cache_.clear();
  warp_base_display_mip_source_key_ = 0;
  build_warp_base_cache(puppet_.layer_id);
  refresh_puppet_warp_preview();
  setCursor(Qt::CrossCursor);
  update();
  notify_transform_controls_changed();
  if (status_callback_) {
    status_callback_(resolve_modifier_names(
        tr("Click the mesh to add pins, then drag them. %ALT%-click a pin to remove it. Enter applies, Esc cancels.")));
  }
  return true;
}

void CanvasWidget::finish_puppet_warp() {
  if (puppet_.active) {
    commit_puppet_warp();
  }
}

void CanvasWidget::cancel_puppet_warp() {
  if (!puppet_.active) {
    return;
  }
  reset_puppet_warp_state();
  update_tool_cursor();
  update();
  if (status_callback_) {
    status_callback_(tr("Puppet Warp cancelled"));
  }
  notify_transform_controls_changed();
}

bool CanvasWidget::puppet_warp_active() const noexcept {
  return puppet_.active;
}

void CanvasWidget::set_puppet_warp_options(const PuppetWarpOptions& options) {
  if (!puppet_.active) {
    return;
  }
  const bool remesh =
      options.density != puppet_.options.density || options.expansion != puppet_.options.expansion;
  const bool rerender = remesh || options.mode != puppet_.options.mode;
  puppet_.options = options;
  if (remesh) {
    rebuild_puppet_warp_mesh();
  }
  if (rerender) {
    refresh_puppet_warp_preview();
  }
  update();
  notify_transform_controls_changed();
}

CanvasWidget::PuppetWarpOptions CanvasWidget::puppet_warp_options() const {
  return puppet_.options;
}

int CanvasWidget::add_puppet_pin(QPointF document_point) {
  if (!puppet_.active) {
    return -1;
  }
  // A pin lands on the deformed surface: its rest position is the exact inverse of
  // the front-most cell under the point, so adding it leaves the shape in place.
  const auto rest = puppet_warp_rest_point(puppet_.grid, document_point.x(), document_point.y(),
                                           puppet_.source_bounds.x, puppet_.source_bounds.y);
  if (!rest.has_value()) {
    return -1;
  }
  record_puppet_warp_step();
  PuppetPin pin;
  pin.rest_x = (*rest)[0];
  pin.rest_y = (*rest)[1];
  pin.x = document_point.x();
  pin.y = document_point.y();
  puppet_.pins.push_back(pin);
  const auto index = static_cast<int>(puppet_.pins.size()) - 1;
  puppet_.selected = {index};
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
  return index;
}

void CanvasWidget::set_puppet_pin_position(int index, QPointF document_point) {
  if (!puppet_.active || index < 0 || index >= static_cast<int>(puppet_.pins.size())) {
    return;
  }
  record_puppet_warp_step();
  auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
  pin.x = document_point.x();
  pin.y = document_point.y();
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

int CanvasWidget::puppet_pin_count() const noexcept {
  return puppet_.active ? static_cast<int>(puppet_.pins.size()) : 0;
}

QPointF CanvasWidget::puppet_pin_position(int index) const {
  if (!puppet_.active || index < 0 || index >= static_cast<int>(puppet_.pins.size())) {
    return {};
  }
  const auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
  return {pin.x, pin.y};
}

void CanvasWidget::select_puppet_pins(std::vector<int> indices) {
  if (!puppet_.active) {
    return;
  }
  const auto count = static_cast<int>(puppet_.pins.size());
  std::erase_if(indices, [count](int index) { return index < 0 || index >= count; });
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  puppet_.selected = std::move(indices);
  update();
  notify_transform_controls_changed();
}

std::vector<int> CanvasWidget::selected_puppet_pins() const {
  return puppet_.active ? puppet_.selected : std::vector<int>{};
}

void CanvasWidget::remove_selected_puppet_pins() {
  if (!puppet_.active || puppet_.selected.empty()) {
    return;
  }
  record_puppet_warp_step();
  auto selected = puppet_.selected;
  std::sort(selected.begin(), selected.end(), std::greater<>());
  for (const auto index : selected) {
    puppet_.pins.erase(puppet_.pins.begin() + index);
  }
  puppet_.selected.clear();
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

void CanvasWidget::remove_all_puppet_pins() {
  if (!puppet_.active || puppet_.pins.empty()) {
    return;
  }
  record_puppet_warp_step();
  puppet_.pins.clear();
  puppet_.selected.clear();
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

void CanvasWidget::set_selected_puppet_pins_rotation(bool fixed, double degrees) {
  if (!puppet_.active || puppet_.selected.empty()) {
    return;
  }
  const auto changes = std::any_of(puppet_.selected.begin(), puppet_.selected.end(), [&](int index) {
    const auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
    return pin.fixed_rotation != fixed || (fixed && pin.rotation_degrees != degrees);
  });
  if (!changes) {
    return;
  }
  record_puppet_warp_step();
  for (const auto index : puppet_.selected) {
    auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
    pin.fixed_rotation = fixed;
    pin.rotation_degrees = fixed ? degrees : 0.0;
  }
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

void CanvasWidget::shift_selected_puppet_pins_depth(int delta) {
  if (!puppet_.active || puppet_.selected.empty() || delta == 0) {
    return;
  }
  record_puppet_warp_step();
  for (const auto index : puppet_.selected) {
    auto& depth = puppet_.pins[static_cast<std::size_t>(index)].depth;
    depth = std::clamp(depth + delta, -1000, 1000);
  }
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

CanvasWidget::PuppetPinControls CanvasWidget::puppet_pin_controls() const {
  PuppetPinControls controls;
  if (!puppet_.active || puppet_.selected.empty()) {
    return controls;
  }
  const auto index = static_cast<std::size_t>(puppet_.selected.front());
  controls.has_selection = true;
  controls.fixed_rotation = puppet_.pins[index].fixed_rotation;
  controls.angle_degrees =
      puppet_warp_pin_angle(puppet_.pins, puppet_.options.mode, puppet_.mesh.handle_radius, index);
  return controls;
}

bool CanvasWidget::puppet_warp_can_undo() const noexcept {
  return puppet_.active && !puppet_.undo_steps.empty();
}

bool CanvasWidget::puppet_warp_can_redo() const noexcept {
  return puppet_.active && !puppet_.redo_steps.empty();
}

void CanvasWidget::undo_puppet_warp_step() {
  if (!puppet_warp_can_undo() || puppet_.dragging) {
    return;
  }
  puppet_.redo_steps.push_back(std::move(puppet_.pins));
  puppet_.pins = std::move(puppet_.undo_steps.back());
  puppet_.undo_steps.pop_back();
  std::erase_if(puppet_.selected, [this](int index) { return index >= static_cast<int>(puppet_.pins.size()); });
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

void CanvasWidget::redo_puppet_warp_step() {
  if (!puppet_warp_can_redo() || puppet_.dragging) {
    return;
  }
  puppet_.undo_steps.push_back(std::move(puppet_.pins));
  puppet_.pins = std::move(puppet_.redo_steps.back());
  puppet_.redo_steps.pop_back();
  std::erase_if(puppet_.selected, [this](int index) { return index >= static_cast<int>(puppet_.pins.size()); });
  refresh_puppet_warp_preview();
  update();
  notify_transform_controls_changed();
}

void CanvasWidget::record_puppet_warp_step() {
  puppet_.undo_steps.push_back(puppet_.pins);
  if (puppet_.undo_steps.size() > kPuppetHistoryLimit) {
    puppet_.undo_steps.erase(puppet_.undo_steps.begin());
  }
  puppet_.redo_steps.clear();
}

void CanvasWidget::rebuild_puppet_warp_mesh() {
  const auto& bounds = puppet_.source_bounds;
  puppet_.mesh = build_puppet_warp_mesh(alpha_channel(puppet_.source), bounds.x, bounds.y, puppet_.source.width(),
                                        puppet_.source.height(), puppet_.options.density,
                                        puppet_.options.expansion);
  puppet_.subdivisions =
      std::max(1, static_cast<int>(std::ceil(static_cast<double>(puppet_.mesh.spacing) / kPuppetRenderCellPixels)));
}

void CanvasWidget::refresh_puppet_warp_preview() {
  warp_preview_patches_.clear();
  puppet_.grid = deform_puppet_warp_mesh(puppet_.mesh, puppet_.pins, puppet_.options.mode, puppet_.subdivisions,
                                         puppet_.source_bounds.x, puppet_.source_bounds.y);
  if (puppet_.grid.cell_order.empty()) {
    return;
  }
  const auto warped = resample_warped_rgba8(puppet_.source, puppet_.grid, transform_interpolation_);
  set_warp_preview_patches(puppet_.layer_id, warped.image, warped.bounds);
}

int CanvasWidget::puppet_pin_at(QPoint widget_point) const {
  constexpr double kPinHitRadius = 7.0;
  // Last placed draws on top, so it wins the hit test.
  for (auto index = static_cast<int>(puppet_.pins.size()) - 1; index >= 0; --index) {
    const auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
    const auto center = widget_position_f(QPointF(pin.x, pin.y));
    if (std::hypot(center.x() - widget_point.x(), center.y() - widget_point.y()) <= kPinHitRadius) {
      return index;
    }
  }
  return -1;
}

bool CanvasWidget::handle_puppet_warp_press(QMouseEvent* event) {
  // Read modifiers folded into this event (docs/ui-conventions.md).
  const auto modifiers = event->modifiers();
  const auto document_point = document_position_f(event->position());
  const auto hit = puppet_pin_at(event->pos());
  if ((modifiers & Qt::AltModifier) != 0) {
    if (hit >= 0) {
      record_puppet_warp_step();
      puppet_.pins.erase(puppet_.pins.begin() + hit);
      std::erase(puppet_.selected, hit);
      for (auto& index : puppet_.selected) {
        index -= index > hit ? 1 : 0;
      }
      refresh_puppet_warp_preview();
      update();
      notify_transform_controls_changed();
    }
    return true;
  }
  const bool extend = (modifiers & Qt::ShiftModifier) != 0;
  if (hit >= 0) {
    const bool was_selected = std::find(puppet_.selected.begin(), puppet_.selected.end(), hit) != puppet_.selected.end();
    if (extend && was_selected) {
      std::erase(puppet_.selected, hit);
      update();
      notify_transform_controls_changed();
      return true;
    }
    if (extend) {
      puppet_.selected.push_back(hit);
    } else if (!was_selected) {
      puppet_.selected = {hit};
    }
  } else {
    auto previous = puppet_.selected;
    const auto added = add_puppet_pin(document_point);
    if (added < 0) {
      if (!extend) {
        puppet_.selected.clear();
      }
      update();
      notify_transform_controls_changed();
      return true;
    }
    if (extend) {
      previous.push_back(added);
      puppet_.selected = std::move(previous);
    }
  }
  // Drag every selected pin by the pointer's offset. The step is recorded on the
  // first move, so a plain click on a pin adds nothing to the session history.
  puppet_.dragging = true;
  puppet_.drag_recorded = false;
  puppet_.drag_start = document_point;
  puppet_.drag_origins.clear();
  for (const auto index : puppet_.selected) {
    const auto& pin = puppet_.pins[static_cast<std::size_t>(index)];
    puppet_.drag_origins.emplace_back(pin.x, pin.y);
  }
  update();
  notify_transform_controls_changed();
  return true;
}

void CanvasWidget::handle_puppet_warp_move(QMouseEvent* event) {
  if (!puppet_.dragging) {
    const auto alt = (event->modifiers() & Qt::AltModifier) != 0;
    setCursor(puppet_pin_at(event->pos()) >= 0 ? (alt ? Qt::ForbiddenCursor : Qt::SizeAllCursor) : Qt::CrossCursor);
    return;
  }
  const auto delta = document_position_f(event->position()) - puppet_.drag_start;
  if (!puppet_.drag_recorded) {
    if (delta.isNull()) {
      return;
    }
    record_puppet_warp_step();  // first real move of this drag
    puppet_.drag_recorded = true;
  }
  for (std::size_t i = 0; i < puppet_.selected.size() && i < puppet_.drag_origins.size(); ++i) {
    auto& pin = puppet_.pins[static_cast<std::size_t>(puppet_.selected[i])];
    pin.x = puppet_.drag_origins[i].x() + delta.x();
    pin.y = puppet_.drag_origins[i].y() + delta.y();
  }
  refresh_puppet_warp_preview();
  update();
}

void CanvasWidget::handle_puppet_warp_release() {
  if (!puppet_.dragging) {
    return;
  }
  puppet_.dragging = false;
  puppet_.drag_origins.clear();
  notify_transform_controls_changed();
}

void CanvasWidget::draw_puppet_warp(QPainter& painter) const {
  if (!puppet_.active) {
    return;
  }
  painter.save();
  const auto& grid = puppet_.grid;
  const auto& mesh = puppet_.mesh;
  if (puppet_.options.show_mesh && grid.columns >= 2 && mesh.columns >= 2) {
    // Mesh-cell edges through the deformed lattice nodes (every subdivisions-th
    // node is a mesh node), so the drawn mesh bends with the warp.
    const int split = puppet_.subdivisions;
    const int cells_x = mesh.columns - 1;
    const int cells_y = mesh.rows - 1;
    const auto active = [&](int cx, int cy) {
      return cx >= 0 && cy >= 0 && cx < cells_x && cy < cells_y &&
             mesh.active[static_cast<std::size_t>(cy) * static_cast<std::size_t>(cells_x) +
                         static_cast<std::size_t>(cx)] != 0;
    };
    const auto node = [&](int column, int row) {
      const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                         static_cast<std::size_t>(column);
      return widget_position_f(QPointF(grid.doc_xs[index], grid.doc_ys[index]));
    };
    QVector<QLineF> lines;
    const auto add_edge = [&](int column, int row, int step_x, int step_y) {
      auto previous = node(column, row);
      for (int i = 1; i <= split; ++i) {
        const auto next = node(column + step_x * i, row + step_y * i);
        lines.append(QLineF(previous, next));
        previous = next;
      }
    };
    for (int cy = 0; cy < cells_y; ++cy) {
      for (int cx = 0; cx < cells_x; ++cx) {
        if (!active(cx, cy)) {
          continue;
        }
        add_edge(cx * split, cy * split, 1, 0);
        add_edge(cx * split, cy * split, 0, 1);
        if (!active(cx + 1, cy)) {
          add_edge((cx + 1) * split, cy * split, 0, 1);
        }
        if (!active(cx, cy + 1)) {
          add_edge(cx * split, (cy + 1) * split, 1, 0);
        }
      }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(128, 128, 128, 200), 0.0));
    painter.drawLines(lines);
  }
  // Pins: a yellow disc, with a dark center while selected (Photoshop's look).
  painter.setRenderHint(QPainter::Antialiasing, true);
  constexpr double kPinRadius = 5.5;
  for (std::size_t i = 0; i < puppet_.pins.size(); ++i) {
    const auto center = widget_position_f(QPointF(puppet_.pins[i].x, puppet_.pins[i].y));
    painter.setPen(QPen(QColor(20, 20, 20), 1.0));
    painter.setBrush(QColor(255, 214, 10));
    painter.drawEllipse(center, kPinRadius, kPinRadius);
    if (std::find(puppet_.selected.begin(), puppet_.selected.end(), static_cast<int>(i)) != puppet_.selected.end()) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(20, 20, 20));
      painter.drawEllipse(center, 2.5, 2.5);
    }
  }
  painter.restore();
}

void CanvasWidget::commit_puppet_warp() {
  if (!puppet_.active) {
    return;
  }
  auto* layer = document_ != nullptr ? document_->find_layer(puppet_.layer_id) : nullptr;
  if (layer == nullptr) {
    cancel_puppet_warp();
    return;
  }
  // Untouched pins are an exact no-op; a negative Expansion still trims the layer
  // to the shrunken mesh, as the preview shows.
  const bool changed = !puppet_warp_is_identity(puppet_.pins) || puppet_.options.expansion < 0;
  const auto old_bounds = layer->bounds();
  auto new_bounds = old_bounds;
  if (changed) {
    if (puppet_.grid.cell_order.empty()) {
      report_status_error(tr("The mesh is empty. Raise the Expansion to cover the layer."));
      return;
    }
    const auto baked = resample_warped_rgba8(puppet_.source, puppet_.grid, transform_interpolation_);
    if (before_edit_callback_) {
      before_edit_callback_(tr("Puppet Warp"));
    }
    layer = document_->find_layer(puppet_.layer_id);
    if (layer != nullptr && !baked.image.isNull()) {
      layer->set_pixels(pixels_from_image_rgba(baked.image));
      layer->set_bounds(baked.bounds);
      new_bounds = baked.bounds;
    }
  }
  reset_puppet_warp_state();
  update_tool_cursor();
  document_changed(to_qrect(old_bounds).united(to_qrect(new_bounds)));
  if (status_callback_) {
    status_callback_(changed ? tr("Applied Puppet Warp") : tr("Puppet Warp cancelled"));
  }
  notify_transform_controls_changed();
}

void CanvasWidget::reset_puppet_warp_state() {
  puppet_ = PuppetWarpSession{};
  warp_base_cache_ = QImage();
  warp_base_cache_scale_level_ = 0;
  warp_base_display_mip_cache_.clear();
  warp_base_display_mip_source_key_ = 0;
  warp_preview_patches_.clear();
}

}  // namespace patchy::ui
