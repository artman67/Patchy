// CanvasWidget's Paint Symmetry (docs/paint-symmetry.md): the per-document
// center and rotation, the copies handed to the brush engine, the on-canvas
// guide, and the Transform Symmetry drag (move the center, rotate the axes).
// The engine itself lives in core (paint_symmetry.hpp, EditOptions::symmetry).

#include "ui/canvas_widget.hpp"

#include "ui/theme_palette.hpp"
#include "ui/tool_cursors.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace patchy::ui {

namespace {

// Screen radius around the center that grabs it instead of rotating.
constexpr double kSymmetryCenterGrabRadius = 12.0;
constexpr double kSymmetryCenterHandleRadius = 5.0;
constexpr double kSymmetryRotationSnapDegrees = 15.0;

// The direction (dx, dy) turned by `degrees`, clockwise on screen like
// patchy::paint_symmetry_transforms.
QPointF turned(QPointF direction, double degrees) {
  const auto radians = degrees * std::numbers::pi / 180.0;
  const auto c = std::cos(radians);
  const auto s = std::sin(radians);
  return QPointF(c * direction.x() - s * direction.y(), s * direction.x() + c * direction.y());
}

// Parameter range [t0, t1] of center + t * direction inside `bounds`
// (slab clipping); empty when t0 > t1.
std::pair<double, double> clip_ray(QPointF center, QPointF direction, const QRectF& bounds) {
  auto t0 = std::numeric_limits<double>::lowest();
  auto t1 = std::numeric_limits<double>::max();
  const auto slab = [&](double origin, double delta, double low, double high) {
    if (std::abs(delta) < 1e-12) {
      if (origin < low || origin > high) {
        t0 = 1.0;
        t1 = 0.0;
      }
      return;
    }
    auto a = (low - origin) / delta;
    auto b = (high - origin) / delta;
    if (a > b) {
      std::swap(a, b);
    }
    t0 = std::max(t0, a);
    t1 = std::min(t1, b);
  };
  slab(center.x(), direction.x(), bounds.left(), bounds.right());
  slab(center.y(), direction.y(), bounds.top(), bounds.bottom());
  return {t0, t1};
}

double normalized_degrees(double degrees) {
  auto value = std::remainder(degrees, 360.0);
  if (value <= -180.0) {
    value += 360.0;
  }
  return value;
}

}  // namespace

bool CanvasWidget::tool_uses_paint_symmetry(CanvasTool tool) noexcept {
  // Photoshop offers Paint Symmetry on the Brush, Pencil, Mixer Brush, and
  // Eraser; Patchy's hard Brush stands in for the Pencil.
  return tool == CanvasTool::Brush || tool == CanvasTool::MixerBrush || tool == CanvasTool::Eraser;
}

void CanvasWidget::set_paint_symmetry(patchy::PaintSymmetryMode mode, int segments) {
  paint_symmetry_mode_ = mode;
  paint_symmetry_segments_ = patchy::clamp_paint_symmetry_segments(
      patchy::paint_symmetry_uses_segments(mode) ? mode : patchy::PaintSymmetryMode::Radial, segments);
  if (mode == patchy::PaintSymmetryMode::Off && paint_symmetry_transforming_) {
    end_paint_symmetry_transform(true);
  }
  update();
}

patchy::PaintSymmetryMode CanvasWidget::paint_symmetry_mode() const noexcept {
  return paint_symmetry_mode_;
}

int CanvasWidget::paint_symmetry_segments() const noexcept {
  return paint_symmetry_segments_;
}

void CanvasWidget::set_paint_symmetry_visible(bool visible) {
  paint_symmetry_visible_ = visible;
  update();
}

bool CanvasWidget::paint_symmetry_visible() const noexcept {
  return paint_symmetry_visible_;
}

QPointF CanvasWidget::paint_symmetry_center() const {
  if (document_ == nullptr) {
    return {};
  }
  // Snapped to half pixels: a mirror about a pixel edge or pixel center maps
  // whole pixels onto whole pixels, so mirrored copies stay pixel exact.
  const auto snap = [](double value) { return std::round(value * 2.0) / 2.0; };
  return QPointF(snap(paint_symmetry_center_fraction_.x() * document_->width()),
                 snap(paint_symmetry_center_fraction_.y() * document_->height()));
}

void CanvasWidget::set_paint_symmetry_center(QPointF document_point) {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0) {
    return;
  }
  const auto width = static_cast<double>(document_->width());
  const auto height = static_cast<double>(document_->height());
  const auto x = std::clamp(std::round(document_point.x() * 2.0) / 2.0, 0.0, width);
  const auto y = std::clamp(std::round(document_point.y() * 2.0) / 2.0, 0.0, height);
  paint_symmetry_center_fraction_ = QPointF(x / width, y / height);
  update();
}

double CanvasWidget::paint_symmetry_angle_degrees() const noexcept {
  return paint_symmetry_angle_degrees_;
}

void CanvasWidget::set_paint_symmetry_angle_degrees(double degrees) {
  paint_symmetry_angle_degrees_ = normalized_degrees(degrees);
  update();
}

void CanvasWidget::reset_paint_symmetry_placement() {
  paint_symmetry_center_fraction_ = QPointF(0.5, 0.5);
  paint_symmetry_angle_degrees_ = 0.0;
  update();
}

std::vector<patchy::SymmetryTransform> CanvasWidget::paint_symmetry_copies() const {
  if (paint_symmetry_mode_ == patchy::PaintSymmetryMode::Off || document_ == nullptr ||
      !tool_uses_paint_symmetry(tool_)) {
    return {};
  }
  // The guide sits on pixel edges; the engine centers a dab at x on pixel x,
  // so the engine's center is half a pixel up and left of the guide's.
  const auto center = paint_symmetry_center();
  return patchy::paint_symmetry_transforms(paint_symmetry_mode_, paint_symmetry_segments_,
                                           center.x() - 0.5, center.y() - 0.5,
                                           paint_symmetry_angle_degrees_);
}

patchy::MixerBrushState& CanvasWidget::mixer_brush_state_for_copy(std::size_t copy_index) {
  if (copy_index == 0) {
    return mixer_brush_state_;
  }
  // Each symmetry copy keeps its own pickup, sampled where that copy paints,
  // so mirrored strokes smear their own side of the canvas.
  while (mixer_symmetry_states_.size() < copy_index) {
    patchy::MixerBrushState state;
    patchy::begin_mixer_brush_stroke(state);
    mixer_symmetry_states_.push_back(state);
  }
  return mixer_symmetry_states_[copy_index - 1];
}

bool CanvasWidget::paint_symmetry_guide_shown() const noexcept {
  return paint_symmetry_mode_ != patchy::PaintSymmetryMode::Off && document_ != nullptr &&
         (paint_symmetry_transforming_ || (paint_symmetry_visible_ && tool_uses_paint_symmetry(tool_)));
}

void CanvasWidget::draw_paint_symmetry_guide(QPainter& painter) const {
  if (!paint_symmetry_guide_shown()) {
    return;
  }
  const auto bounds = QRectF(0.0, 0.0, document_->width(), document_->height());
  const auto center = paint_symmetry_center();
  const auto angle = paint_symmetry_angle_degrees_;
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(theme().paint_symmetry_guide, 1.0));
  // Full lines through the center for the mirror modes, spokes from it for the
  // radial ones. Endpoints are clipped in document space and mapped one by one,
  // so the guide follows any view transform.
  const auto draw_ray = [&](QPointF direction, bool both_ways) {
    const auto direction_turned = turned(direction, angle);
    auto [t0, t1] = clip_ray(center, direction_turned, bounds);
    if (!both_ways) {
      t0 = std::max(t0, 0.0);
    }
    if (t0 > t1) {
      return;
    }
    painter.drawLine(widget_position_f(center + direction_turned * t0),
                     widget_position_f(center + direction_turned * t1));
  };
  switch (paint_symmetry_mode_) {
    case patchy::PaintSymmetryMode::Off:
      break;
    case patchy::PaintSymmetryMode::Vertical:
      draw_ray(QPointF(0.0, 1.0), true);
      break;
    case patchy::PaintSymmetryMode::Horizontal:
      draw_ray(QPointF(1.0, 0.0), true);
      break;
    case patchy::PaintSymmetryMode::DualAxis:
      draw_ray(QPointF(0.0, 1.0), true);
      draw_ray(QPointF(1.0, 0.0), true);
      break;
    case patchy::PaintSymmetryMode::Diagonal:
      draw_ray(QPointF(1.0, -1.0) / std::numbers::sqrt2, true);
      break;
    case patchy::PaintSymmetryMode::Radial:
    case patchy::PaintSymmetryMode::Mandala: {
      // One spoke per segment boundary, the first pointing up.
      const auto step = 360.0 / static_cast<double>(paint_symmetry_segments_);
      for (int index = 0; index < paint_symmetry_segments_; ++index) {
        draw_ray(turned(QPointF(0.0, -1.0), step * static_cast<double>(index)), false);
      }
      break;
    }
  }
  if (paint_symmetry_transforming_) {
    painter.drawEllipse(widget_position_f(center), kSymmetryCenterHandleRadius, kSymmetryCenterHandleRadius);
  }
  painter.restore();
}

void CanvasWidget::begin_paint_symmetry_transform() {
  if (paint_symmetry_mode_ == patchy::PaintSymmetryMode::Off || document_ == nullptr ||
      paint_symmetry_transforming_) {
    return;
  }
  paint_symmetry_transforming_ = true;
  paint_symmetry_drag_ = PaintSymmetryDrag::None;
  paint_symmetry_saved_center_fraction_ = paint_symmetry_center_fraction_;
  paint_symmetry_saved_angle_degrees_ = paint_symmetry_angle_degrees_;
  update_paint_symmetry_transform_cursor(last_mouse_position_);
  update();
}

void CanvasWidget::end_paint_symmetry_transform(bool commit) {
  if (!paint_symmetry_transforming_) {
    return;
  }
  if (!commit) {
    paint_symmetry_center_fraction_ = paint_symmetry_saved_center_fraction_;
    paint_symmetry_angle_degrees_ = paint_symmetry_saved_angle_degrees_;
  }
  paint_symmetry_transforming_ = false;
  paint_symmetry_drag_ = PaintSymmetryDrag::None;
  update_tool_cursor();
  update();
}

bool CanvasWidget::paint_symmetry_transform_active() const noexcept {
  return paint_symmetry_transforming_;
}

void CanvasWidget::update_paint_symmetry_transform_cursor(QPoint widget_position) {
  if (!paint_symmetry_transforming_ || document_ == nullptr) {
    return;
  }
  const auto delta = QPointF(widget_position) - widget_position_f(paint_symmetry_center());
  setCursor(std::hypot(delta.x(), delta.y()) <= kSymmetryCenterGrabRadius ? QCursor(Qt::SizeAllCursor)
                                                                           : crop_rotate_cursor());
}

bool CanvasWidget::handle_paint_symmetry_transform_press(QMouseEvent* event) {
  if (!paint_symmetry_transforming_ || document_ == nullptr || event->button() != Qt::LeftButton) {
    return false;
  }
  const auto center = paint_symmetry_center();
  const auto delta = event->position() - widget_position_f(center);
  const auto document_point = document_position_f(event->position());
  if (std::hypot(delta.x(), delta.y()) <= kSymmetryCenterGrabRadius) {
    paint_symmetry_drag_ = PaintSymmetryDrag::Move;
    paint_symmetry_drag_offset_ = center - document_point;
  } else {
    paint_symmetry_drag_ = PaintSymmetryDrag::Rotate;
    paint_symmetry_drag_start_pointer_degrees_ =
        std::atan2(document_point.y() - center.y(), document_point.x() - center.x()) * 180.0 / std::numbers::pi;
    paint_symmetry_drag_start_angle_degrees_ = paint_symmetry_angle_degrees_;
  }
  event->accept();
  return true;
}

bool CanvasWidget::handle_paint_symmetry_transform_move(QMouseEvent* event) {
  if (!paint_symmetry_transforming_) {
    return false;
  }
  if (paint_symmetry_drag_ == PaintSymmetryDrag::None || (event->buttons() & Qt::LeftButton) == 0) {
    paint_symmetry_drag_ = PaintSymmetryDrag::None;
    if (event->buttons() != Qt::NoButton) {
      return false;  // a middle-button pan or similar keeps its own handling
    }
    update_paint_symmetry_transform_cursor(event->pos());
    event->accept();
    return true;
  }
  const auto document_point = document_position_f(event->position());
  if (paint_symmetry_drag_ == PaintSymmetryDrag::Move) {
    set_paint_symmetry_center(document_point + paint_symmetry_drag_offset_);
  } else {
    const auto center = paint_symmetry_center();
    const auto pointer_degrees =
        std::atan2(document_point.y() - center.y(), document_point.x() - center.x()) * 180.0 / std::numbers::pi;
    auto angle = paint_symmetry_drag_start_angle_degrees_ + pointer_degrees - paint_symmetry_drag_start_pointer_degrees_;
    if ((event->modifiers() & Qt::ShiftModifier) != 0) {
      angle = std::round(angle / kSymmetryRotationSnapDegrees) * kSymmetryRotationSnapDegrees;
    }
    set_paint_symmetry_angle_degrees(angle);
  }
  event->accept();
  return true;
}

bool CanvasWidget::handle_paint_symmetry_transform_release(QMouseEvent* event) {
  if (!paint_symmetry_transforming_ || event->button() != Qt::LeftButton) {
    return false;
  }
  paint_symmetry_drag_ = PaintSymmetryDrag::None;
  event->accept();
  return true;
}

bool CanvasWidget::handle_paint_symmetry_transform_key(QKeyEvent* event) {
  if (!paint_symmetry_transforming_) {
    return false;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    end_paint_symmetry_transform(true);
  } else if (event->key() == Qt::Key_Escape) {
    end_paint_symmetry_transform(false);
  } else {
    return false;
  }
  event->accept();
  return true;
}

}  // namespace patchy::ui
