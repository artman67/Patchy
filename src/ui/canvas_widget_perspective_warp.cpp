// CanvasWidget's Perspective Warp session (Edit > Perspective Warp): Layout-mode
// quads, Warp-mode corner drags, the one-shot straighten, the preview, and the
// commit. The geometry and its patent boundary live in core/perspective_warp
// (see docs/perspective-warp.md); the preview shares the Warp Transform cage's
// base cache and patches, since the two sessions never run together.

#include "ui/canvas_widget.hpp"
#include "ui/canvas_widget_shared.hpp"

#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/vector_shape.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/image_document_io.hpp"
#include "ui/qt_geometry.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <utility>

namespace patchy::ui {

namespace {

// Preview and commit share one lattice, so the preview is exactly what Enter
// bakes: nodes 4 px apart, at most 256 cells per axis.
constexpr double kLatticeCellPixels = 4.0;
constexpr int kLatticeMaxCells = 256;
constexpr double kCornerHitPixels = 14.0;

QPointF quad_corner(const PerspectiveQuad& quad, int corner) {
  const auto index = static_cast<std::size_t>(corner * 2);
  return {quad[index], quad[index + 1U]};
}

QPolygonF quad_polygon(const PerspectiveQuad& quad) {
  return QPolygonF{quad_corner(quad, 0), quad_corner(quad, 1), quad_corner(quad, 2), quad_corner(quad, 3)};
}

}  // namespace

bool CanvasWidget::begin_perspective_warp() {
  if (perspective_warp_.has_value()) {
    return true;
  }
  if (document_ == nullptr) {
    return false;
  }
  if (layer_edit_target_ != LayerEditTarget::Content && layer_edit_target_ != LayerEditTarget::Mask) {
    report_status_error(tr("Select an editable pixel layer to warp"));
    return false;
  }
  // An open session commits first, as a tool switch does.
  commit_modal_session();
  const auto* layer = active_pixel_layer();
  if (layer != nullptr && layer_is_smart_object(*layer)) {
    // No calibrated Photoshop smart-filter descriptor exists for it (Liquify rule).
    report_status_error(tr("Rasterize the Smart Object before using Perspective Warp"));
    return false;
  }
  if (layer == nullptr || layer_is_text(*layer) || layer_is_vector_shape(*layer) ||
      layer->pixels().format().bit_depth != BitDepth::UInt8 || layer->pixels().format().channels < 3) {
    report_status_error(tr("Select an editable pixel layer to warp"));
    return false;
  }
  if (layer_effectively_locks_image_pixels(*layer)) {
    show_layer_pixels_locked_message();
    return false;
  }
  if (layer_effectively_locks_position(*layer)) {
    show_layer_position_locked_message();
    return false;
  }
  auto source = qimage_from_pixel_buffer(layer->pixels()).convertToFormat(QImage::Format_RGBA8888);
  if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
    report_status_error(tr("Layer has no pixels to warp"));
    return false;
  }
  perspective_warp_.emplace();
  perspective_warp_->layer_id = layer->id();
  perspective_warp_->source = std::move(source);
  set_move_transform_controls_layer(std::nullopt);
  setCursor(Qt::CrossCursor);
  update();
  notify_transform_controls_changed();
  if (status_callback_) {
    status_callback_(tr("Drag to draw a quad over each plane, then choose Warp. Enter applies, Esc cancels."));
  }
  return true;
}

void CanvasWidget::finish_perspective_warp() {
  if (!perspective_warp_.has_value()) {
    return;
  }
  const auto& session = *perspective_warp_;
  auto* layer = document_ != nullptr ? document_->find_layer(session.layer_id) : nullptr;
  const bool changed = std::any_of(session.planes.begin(), session.planes.end(),
                                   [](const PerspectivePlane& plane) { return plane.layout != plane.warped; });
  if (layer == nullptr || !changed) {
    cancel_perspective_warp();
    return;
  }
  // One resample of the ORIGINAL pixels through the same lattice the preview used.
  const auto old_bounds = layer->bounds();
  const auto grid = build_perspective_warp_grid(session.planes, old_bounds.x, old_bounds.y, session.source.width(),
                                                session.source.height(), kLatticeCellPixels, kLatticeMaxCells);
  const auto baked = grid.has_value() ? resample_warped_rgba8(session.source, *grid, transform_interpolation_)
                                      : TransformedImage{};
  if (baked.image.isNull()) {
    cancel_perspective_warp();
    return;
  }
  if (before_edit_callback_) {
    before_edit_callback_(tr("Perspective Warp"));
  }
  layer->set_pixels(pixels_from_image_rgba(baked.image));
  layer->set_bounds(baked.bounds);
  reset_perspective_warp_state();
  update_tool_cursor();
  document_changed(to_qrect(old_bounds).united(to_qrect(baked.bounds)));
  if (status_callback_) {
    status_callback_(tr("Applied Perspective Warp"));
  }
  notify_transform_controls_changed();
}

void CanvasWidget::cancel_perspective_warp() {
  if (!perspective_warp_.has_value()) {
    return;
  }
  reset_perspective_warp_state();
  update_tool_cursor();
  update();
  if (status_callback_) {
    status_callback_(tr("Perspective Warp cancelled"));
  }
  notify_transform_controls_changed();
}

bool CanvasWidget::perspective_warp_active() const noexcept {
  return perspective_warp_.has_value();
}

CanvasWidget::PerspectiveWarpMode CanvasWidget::perspective_warp_mode() const noexcept {
  return perspective_warp_.has_value() ? perspective_warp_->mode : PerspectiveWarpMode::Layout;
}

bool CanvasWidget::set_perspective_warp_mode(PerspectiveWarpMode mode) {
  if (!perspective_warp_.has_value()) {
    return false;
  }
  auto& session = *perspective_warp_;
  if (session.mode == mode) {
    return true;
  }
  if (mode == PerspectiveWarpMode::Warp && session.planes.empty()) {
    report_status_error(tr("Draw a quad before switching to Warp"));
    notify_transform_controls_changed();  // re-syncs the mode buttons
    return false;
  }
  session.mode = mode;
  session.drag_quad = -1;
  session.drag_corner = -1;
  session.drawing_quad = false;
  if (mode == PerspectiveWarpMode::Warp) {
    ensure_warp_base_cache(session.layer_id);
    refresh_perspective_warp_preview();
    setCursor(Qt::ArrowCursor);
  } else {
    // Layout shows the untouched layer under the layout quads.
    warp_preview_patches_.clear();
    setCursor(Qt::CrossCursor);
  }
  update();
  notify_transform_controls_changed();
  if (status_callback_) {
    status_callback_(mode == PerspectiveWarpMode::Warp
                         ? tr("Drag the quad corners. Enter applies, Esc cancels.")
                         : tr("Drag to draw a quad over each plane, then choose Warp. Enter applies, Esc cancels."));
  }
  return true;
}

bool CanvasWidget::add_perspective_warp_quad(const PerspectiveQuad& corners) {
  if (!perspective_warp_.has_value() || perspective_warp_->mode != PerspectiveWarpMode::Layout) {
    return false;
  }
  auto planes = perspective_warp_->planes;
  planes.push_back(PerspectivePlane{corners, corners});
  if (!perspective_warp_planes_allowed(planes)) {
    return false;
  }
  remember_perspective_warp_step();
  perspective_warp_->planes = std::move(planes);
  perspective_warp_->selected_quad = static_cast<int>(perspective_warp_->planes.size()) - 1;
  update();
  notify_transform_controls_changed();
  return true;
}

int CanvasWidget::perspective_warp_quad_count() const noexcept {
  return perspective_warp_.has_value() ? static_cast<int>(perspective_warp_->planes.size()) : 0;
}

int CanvasWidget::perspective_warp_selected_quad() const noexcept {
  return perspective_warp_.has_value() ? perspective_warp_->selected_quad : -1;
}

void CanvasWidget::select_perspective_warp_quad(int index) {
  if (!perspective_warp_.has_value()) {
    return;
  }
  const int selected = index >= 0 && index < perspective_warp_quad_count() ? index : -1;
  if (selected != perspective_warp_->selected_quad) {
    perspective_warp_->selected_quad = selected;
    update();
    notify_transform_controls_changed();
  }
}

QPointF CanvasWidget::perspective_warp_corner(int quad, int corner) const {
  if (!perspective_warp_.has_value() || quad < 0 || quad >= perspective_warp_quad_count() || corner < 0 ||
      corner > 3) {
    return {};
  }
  const auto& plane = perspective_warp_->planes[static_cast<std::size_t>(quad)];
  return quad_corner(perspective_warp_->mode == PerspectiveWarpMode::Warp ? plane.warped : plane.layout, corner);
}

bool CanvasWidget::set_perspective_warp_corner(int quad, int corner, QPointF document_point) {
  if (!perspective_warp_.has_value() || quad < 0 || quad >= perspective_warp_quad_count() || corner < 0 ||
      corner > 3) {
    return false;
  }
  auto& session = *perspective_warp_;
  auto planes = session.planes;
  auto& plane = planes[static_cast<std::size_t>(quad)];
  const auto x = static_cast<std::size_t>(corner * 2);
  if (session.mode == PerspectiveWarpMode::Layout) {
    // Moving a layout corner carries its warped twin along, keeping its offset.
    plane.warped[x] += document_point.x() - plane.layout[x];
    plane.warped[x + 1U] += document_point.y() - plane.layout[x + 1U];
    plane.layout[x] = document_point.x();
    plane.layout[x + 1U] = document_point.y();
  } else {
    // Only the dragged corner moves; no other corner is ever repositioned.
    plane.warped[x] = document_point.x();
    plane.warped[x + 1U] = document_point.y();
  }
  if (!perspective_warp_planes_allowed(planes)) {
    return false;  // the corner stays at its last allowed position
  }
  session.planes = std::move(planes);
  if (session.mode == PerspectiveWarpMode::Warp) {
    queue_warp_preview_refresh();
  }
  update();
  return true;
}

bool CanvasWidget::straighten_perspective_warp_quad(bool vertical, bool horizontal) {
  if (!perspective_warp_.has_value() || perspective_warp_->mode != PerspectiveWarpMode::Warp) {
    return false;
  }
  auto& session = *perspective_warp_;
  if (session.selected_quad < 0 || session.selected_quad >= perspective_warp_quad_count()) {
    report_status_error(tr("Select a quad to straighten"));
    return false;
  }
  auto planes = session.planes;
  auto& plane = planes[static_cast<std::size_t>(session.selected_quad)];
  plane.warped = straighten_perspective_quad(plane.warped, vertical, horizontal);
  if (!perspective_warp_planes_allowed(planes)) {
    report_status_error(tr("Straightening would fold this quad or make it touch another quad"));
    return false;
  }
  remember_perspective_warp_step();
  session.planes = std::move(planes);
  refresh_perspective_warp_preview();
  update();
  return true;
}

void CanvasWidget::remove_all_perspective_warp_quads() {
  if (!perspective_warp_.has_value()) {
    return;
  }
  if (!perspective_warp_->planes.empty()) {
    remember_perspective_warp_step();
  }
  perspective_warp_->planes.clear();
  perspective_warp_->selected_quad = -1;
  perspective_warp_->drag_quad = -1;
  perspective_warp_->drag_corner = -1;
  if (perspective_warp_->mode == PerspectiveWarpMode::Warp) {
    set_perspective_warp_mode(PerspectiveWarpMode::Layout);
  }
  update();
  notify_transform_controls_changed();
}

bool CanvasWidget::undo_perspective_warp_step() {
  if (!perspective_warp_.has_value() || perspective_warp_->undo_steps.empty() ||
      perspective_warp_->drag_quad >= 0 || perspective_warp_->drawing_quad) {
    return false;
  }
  auto& session = *perspective_warp_;
  session.planes = std::move(session.undo_steps.back());
  session.undo_steps.pop_back();
  if (session.selected_quad >= perspective_warp_quad_count()) {
    session.selected_quad = -1;
  }
  if (session.planes.empty() && session.mode == PerspectiveWarpMode::Warp) {
    set_perspective_warp_mode(PerspectiveWarpMode::Layout);
  } else {
    refresh_perspective_warp_preview();
  }
  update();
  notify_transform_controls_changed();
  return true;
}

void CanvasWidget::remember_perspective_warp_step() {
  constexpr std::size_t kMaxUndoSteps = 100;
  auto& steps = perspective_warp_->undo_steps;
  if (steps.size() >= kMaxUndoSteps) {
    steps.erase(steps.begin());
  }
  steps.push_back(perspective_warp_->planes);
}

bool CanvasWidget::perspective_warp_planes_allowed(const std::vector<PerspectivePlane>& planes) const {
  // Quads stay separate in both modes: never a shared edge or corner.
  std::vector<PerspectiveQuad> layouts;
  std::vector<PerspectiveQuad> warps;
  for (const auto& plane : planes) {
    layouts.push_back(plane.layout);
    warps.push_back(plane.warped);
  }
  return perspective_quads_are_valid(layouts) && perspective_quads_are_valid(warps);
}

void CanvasWidget::refresh_perspective_warp_preview() {
  warp_preview_patches_.clear();
  if (!perspective_warp_.has_value() || perspective_warp_->mode != PerspectiveWarpMode::Warp ||
      document_ == nullptr) {
    return;
  }
  const auto& session = *perspective_warp_;
  const auto* layer = std::as_const(*document_).find_layer(session.layer_id);
  if (layer == nullptr) {
    return;
  }
  const auto bounds = layer->bounds();
  const auto grid = build_perspective_warp_grid(session.planes, bounds.x, bounds.y, session.source.width(),
                                                session.source.height(), kLatticeCellPixels, kLatticeMaxCells);
  if (!grid.has_value()) {
    return;
  }
  const auto warped = resample_warped_rgba8(session.source, *grid, transform_interpolation_);
  set_warp_preview_patches(session.layer_id, warped.image, warped.bounds);
}

void CanvasWidget::reset_perspective_warp_state() {
  perspective_warp_.reset();
  clear_warp_preview();
}

std::pair<int, int> CanvasWidget::perspective_warp_corner_at(QPoint widget_point) const {
  if (!perspective_warp_.has_value()) {
    return {-1, -1};
  }
  // The selected quad wins where corners of two quads sit under the pointer.
  const int count = perspective_warp_quad_count();
  const int selected = perspective_warp_->selected_quad;
  for (int step = -1; step < count; ++step) {
    const int quad = step < 0 ? selected : step;
    if (quad < 0 || (step >= 0 && quad == selected)) {
      continue;
    }
    for (int corner = 0; corner < 4; ++corner) {
      const auto point = widget_position_f(perspective_warp_corner(quad, corner));
      const QRectF hit_rect(point.x() - kCornerHitPixels / 2.0, point.y() - kCornerHitPixels / 2.0,
                            kCornerHitPixels, kCornerHitPixels);
      if (hit_rect.contains(widget_point)) {
        return {quad, corner};
      }
    }
  }
  return {-1, -1};
}

int CanvasWidget::perspective_warp_quad_at(QPointF document_point) const {
  if (!perspective_warp_.has_value()) {
    return -1;
  }
  const bool warp_mode = perspective_warp_->mode == PerspectiveWarpMode::Warp;
  for (int quad = 0; quad < perspective_warp_quad_count(); ++quad) {
    const auto& plane = perspective_warp_->planes[static_cast<std::size_t>(quad)];
    if (quad_polygon(warp_mode ? plane.warped : plane.layout).containsPoint(document_point, Qt::OddEvenFill)) {
      return quad;
    }
  }
  return -1;
}

bool CanvasWidget::handle_perspective_warp_press(QMouseEvent* event) {
  if (!perspective_warp_.has_value()) {
    return false;
  }
  event->accept();
  if (event->button() != Qt::LeftButton) {
    return true;  // the session owns the canvas; other buttons do nothing
  }
  auto& session = *perspective_warp_;
  const int previous_selection = session.selected_quad;
  const auto point = document_position_f(event->position());
  if (const auto [quad, corner] = perspective_warp_corner_at(event->pos()); quad >= 0) {
    remember_perspective_warp_step();
    session.drag_quad = quad;
    session.drag_corner = corner;
    session.selected_quad = quad;
  } else if (const auto hit = perspective_warp_quad_at(point); hit >= 0) {
    session.selected_quad = hit;
  } else if (session.mode == PerspectiveWarpMode::Layout) {
    session.drawing_quad = true;
    session.draw_start = point;
    session.draw_current = point;
    session.selected_quad = -1;
  } else {
    session.selected_quad = -1;
  }
  update();
  if (session.selected_quad != previous_selection) {
    notify_transform_controls_changed();
  }
  return true;
}

bool CanvasWidget::handle_perspective_warp_move(QMouseEvent* event) {
  if (!perspective_warp_.has_value()) {
    return false;
  }
  auto& session = *perspective_warp_;
  const auto point = document_position_f(event->position());
  if (session.drag_quad >= 0) {
    set_perspective_warp_corner(session.drag_quad, session.drag_corner, point);
  } else if (session.drawing_quad) {
    session.draw_current = point;
    update();
  } else if (perspective_warp_corner_at(event->pos()).first >= 0) {
    setCursor(Qt::SizeAllCursor);
  } else {
    setCursor(session.mode == PerspectiveWarpMode::Layout ? Qt::CrossCursor : Qt::ArrowCursor);
  }
  last_mouse_position_ = event->pos();
  event->accept();
  return true;
}

bool CanvasWidget::handle_perspective_warp_release(QMouseEvent* event) {
  if (!perspective_warp_.has_value() || dragging_guide_) {
    return false;  // a ruler guide drag finishes on its own path
  }
  event->accept();
  auto& session = *perspective_warp_;
  const auto point = document_position_f(event->position());
  if (session.drag_quad >= 0) {
    set_perspective_warp_corner(session.drag_quad, session.drag_corner, point);
    session.drag_quad = -1;
    session.drag_corner = -1;
    if (!session.undo_steps.empty() && session.undo_steps.back() == session.planes) {
      session.undo_steps.pop_back();  // a click on a corner changed nothing
    }
    update();
  } else if (session.drawing_quad) {
    session.drawing_quad = false;
    const auto rect = QRectF(session.draw_start, point).normalized();
    // A click or a sliver lays out nothing.
    if (rect.width() >= kPerspectiveQuadMinimumSide && rect.height() >= kPerspectiveQuadMinimumSide &&
        !add_perspective_warp_quad({rect.left(), rect.top(), rect.right(), rect.top(), rect.right(), rect.bottom(),
                                    rect.left(), rect.bottom()})) {
      report_status_error(tr("Quads can't touch or overlap. Leave a gap between planes."));
    }
    update();
  }
  return true;
}

bool CanvasWidget::handle_perspective_warp_key(QKeyEvent* event) {
  if (!perspective_warp_.has_value()) {
    return false;
  }
  switch (event->key()) {
    case Qt::Key_Escape:
      cancel_perspective_warp();
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      finish_perspective_warp();
      return true;
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
      // Layout mode removes the selected quad; the keys never reach the layer.
      auto& session = *perspective_warp_;
      if (session.mode == PerspectiveWarpMode::Layout && session.selected_quad >= 0 && session.drag_quad < 0 &&
          session.selected_quad < perspective_warp_quad_count()) {
        remember_perspective_warp_step();
        session.planes.erase(session.planes.begin() + session.selected_quad);
        session.selected_quad = -1;
        update();
        notify_transform_controls_changed();
      }
      return true;
    }
    default:
      return false;
  }
}

void CanvasWidget::draw_perspective_warp(QPainter& painter) const {
  if (!perspective_warp_.has_value()) {
    return;
  }
  const auto& session = *perspective_warp_;
  const bool warp_mode = session.mode == PerspectiveWarpMode::Warp;
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QColor accent(95, 170, 255);  // the Warp Transform cage colors
  for (int quad = 0; quad < perspective_warp_quad_count(); ++quad) {
    const auto& plane = session.planes[static_cast<std::size_t>(quad)];
    const auto& corners = warp_mode ? plane.warped : plane.layout;
    const bool selected = quad == session.selected_quad;
    // A 3x3 guide grid through the quad's own perspective, then the outline.
    if (const auto unit = homography_from_rect_to_quad(0.0, 0.0, 1.0, 1.0, corners); unit.has_value()) {
      QColor grid_color = accent;
      grid_color.setAlpha(150);
      painter.setPen(QPen(grid_color, 1.0));
      const auto widget_point = [this, &unit](double u, double v) {
        const auto mapped = apply_homography(*unit, u, v);
        return widget_position_f(QPointF(mapped[0], mapped[1]));
      };
      for (const double t : {1.0 / 3.0, 2.0 / 3.0}) {
        painter.drawLine(widget_point(t, 0.0), widget_point(t, 1.0));
        painter.drawLine(widget_point(0.0, t), widget_point(1.0, t));
      }
    }
    QPolygonF outline;
    for (int corner = 0; corner < 4; ++corner) {
      outline << widget_position_f(quad_corner(corners, corner));
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, selected ? 2.0 : 1.0));
    painter.drawPolygon(outline);
    painter.setPen(QPen(QColor(10, 14, 20), 1.0));
    for (int corner = 0; corner < 4; ++corner) {
      const bool dragged = quad == session.drag_quad && corner == session.drag_corner;
      painter.setBrush(dragged ? accent : QColor(245, 248, 252));
      painter.drawRect(QRectF(outline[corner].x() - 4.0, outline[corner].y() - 4.0, 8.0, 8.0));
    }
  }
  if (session.drawing_quad) {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, 1.0, Qt::DashLine));
    painter.drawRect(QRectF(widget_position_f(session.draw_start), widget_position_f(session.draw_current)).normalized());
  }
  painter.restore();
}

}  // namespace patchy::ui
