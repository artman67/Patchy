// CanvasWidget's view implementation, split out of canvas_widget.cpp: the
// zoom accessors and zoom/fit/center commands, spacebar and pen panning,
// pan constraining and view-changed notification, the wheel-zoom setting,
// widget/document coordinate mapping, the Zoom tool's drag preview and pen
// zoom drag, and the Rotate View state with its view/widget mapping (see
// docs/rotate-view.md).

#include "ui/canvas_widget.hpp"
#include "ui/canvas_widget_shared.hpp"

#include "core/adjustment_layer.hpp"
#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/smart_filter.hpp"
#include "core/layer_render_utils.hpp"
#include "core/layer_tree.hpp"
#include "core/pixel_tools.hpp"
#include "core/quick_select.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/image_document_io.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/tool_cursors.hpp"

#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QEventLoop>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QInputDevice>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointingDevice>
#include <QPolygon>
#include <QPolygonF>
#include <QPointer>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QTabletEvent>
#include <QTimerEvent>
#include <QTransform>
#include <QWheelEvent>
#include <QRandomGenerator>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace patchy::ui {

namespace {

constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 128.0;
// One pixel of drag-zoom travel (the pen ZoomCanvas button vertically, Scrubby
// Zoom horizontally) multiplies the zoom by this: about 70 px doubles it.
constexpr double kZoomDragFactorPerPixel = 1.01;
constexpr double kMinimumVisibleDocumentFraction = 0.10;
constexpr int kScrollBarSingleStep = 20;

// The allowed pan range per axis: at least 10% of the document (or of the
// viewport, whichever is smaller) must stay visible. The scroll bars derive
// their range from the same rule so bar scrolling and hand-tool panning can
// never disagree.
struct PanAxisRange {
  double minimum;
  double maximum;
};

// Photoshop's Rotation Angle field runs -180..180; Shift-drag steps by 15.
constexpr double kRotateViewSnapDegrees = 15.0;

double normalized_view_rotation(double degrees) noexcept {
  if (!std::isfinite(degrees)) {
    return 0.0;
  }
  auto angle = std::fmod(degrees, 360.0);
  if (angle <= -180.0) {
    angle += 360.0;
  } else if (angle > 180.0) {
    angle -= 360.0;
  }
  // Snap float noise (and -0) onto zero so an unrotated view takes the exact
  // unrotated code paths.
  return std::abs(angle) < 1e-9 ? 0.0 : angle;
}

// Width and height of the bounding box of a size turned by `degrees`; the
// fit commands size a rotated document by what it covers on screen.
QSizeF rotated_extent(QSizeF size, double degrees) noexcept {
  if (degrees == 0.0) {
    return size;
  }
  const auto radians = qDegreesToRadians(degrees);
  const auto cosine = std::abs(std::cos(radians));
  const auto sine = std::abs(std::sin(radians));
  return {std::max(1e-6, size.width() * cosine + size.height() * sine),
          std::max(1e-6, size.width() * sine + size.height() * cosine)};
}

QTransform rotation_about(QPointF center, double degrees) {
  QTransform transform;
  transform.translate(center.x(), center.y());
  transform.rotate(degrees);
  transform.translate(-center.x(), -center.y());
  return transform;
}

PanAxisRange pan_axis_range(double viewport_span, double document_span) noexcept {
  const auto minimum_visible =
      std::max(1.0, std::min(viewport_span, document_span) * kMinimumVisibleDocumentFraction);
  return {minimum_visible - document_span, viewport_span - minimum_visible};
}

double constrained_document_axis(double pan, double viewport_span, double document_span) noexcept {
  if (!std::isfinite(pan) || viewport_span <= 0.0 || document_span <= 0.0) {
    return pan;
  }

  const auto range = pan_axis_range(viewport_span, document_span);
  return std::clamp(pan, range.minimum, range.maximum);
}

}  // namespace

double CanvasWidget::zoom() const noexcept {
  return zoom_;
}

double CanvasWidget::view_zoom() const noexcept {
  return zoom_ * devicePixelRatioF();
}

void CanvasWidget::set_view_zoom(double view_zoom) {
  set_zoom(view_zoom / std::max(0.01, devicePixelRatioF()));
}

void CanvasWidget::set_view_zoom_centered(double view_zoom) {
  set_zoom_centered(view_zoom / std::max(0.01, devicePixelRatioF()));
}

double CanvasWidget::clamp_logical_zoom(double logical_zoom) const noexcept {
  const auto ratio = std::max(0.01, devicePixelRatioF());
  return std::clamp(logical_zoom * ratio, kMinZoom, kMaxZoom) / ratio;
}

void CanvasWidget::set_zoom(double zoom) {
  const auto clamped = clamp_logical_zoom(zoom);
  if (std::abs(clamped - zoom_) < 0.0001) {
    return;
  }
  zoom_ = clamped;
  constrain_pan();
  update_tool_cursor();
  update();
  notify_view_changed();
}

void CanvasWidget::set_zoom_centered(double zoom) {
  const auto clamped = clamp_logical_zoom(zoom);
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0) {
    set_zoom(clamped);
    return;
  }
  ZoomTraceScope trace("zoom_step", zoom_);
  const QPointF viewport_center(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0);
  // Anchor the document point under the viewport center, clamped to the
  // document bounds: when a geometry change left the view off-center, a raw
  // center anchor can sit in the grey margin and pin empty space, pushing the
  // document almost entirely off screen at the new zoom.
  const QPointF document_anchor(
      std::clamp((viewport_center.x() - pan_.x()) / zoom_, 0.0, static_cast<double>(document_->width())),
      std::clamp((viewport_center.y() - pan_.y()) / zoom_, 0.0, static_cast<double>(document_->height())));
  const auto old_zoom = zoom_;
  const auto old_pan = pan_;
  zoom_ = clamped;
  pan_ = QPointF(viewport_center.x() - document_anchor.x() * zoom_,
                 viewport_center.y() - document_anchor.y() * zoom_);
  // Photoshop's scroll model, applied per axis at preset time: a document
  // that fits the window is centered, and one that overflows never shows grey
  // past its edges (hand-tool overscroll stays free afterwards).
  const auto settle_axis = [](double start, double span, double viewport) {
    return span <= viewport ? (viewport - span) / 2.0 : std::clamp(start, viewport - span, 0.0);
  };
  if (!view_rotated()) {
    pan_.setX(settle_axis(pan_.x(), static_cast<double>(document_->width()) * zoom_, static_cast<double>(width())));
    pan_.setY(settle_axis(pan_.y(), static_cast<double>(document_->height()) * zoom_, static_cast<double>(height())));
  } else {
    // A rotated view applies the same rule to the document's on-screen bounds.
    const auto bounds = document_widget_bounds();
    pan_by_widget_delta(QPointF(settle_axis(bounds.x(), bounds.width(), static_cast<double>(width())) - bounds.x(),
                                settle_axis(bounds.y(), bounds.height(), static_cast<double>(height())) - bounds.y()));
  }
  constrain_pan();
  if (std::abs(old_zoom - zoom_) < 0.0001 && (pan_ - old_pan).manhattanLength() < 0.01) {
    return;
  }
  update_tool_cursor();
  update();
  notify_view_changed();
}

void CanvasWidget::zoom_at_widget_point(QPointF widget_position, double factor) {
  zoom_at_view_point(view_point_from_widget(widget_position), factor);
}

void CanvasWidget::zoom_at_view_point(QPointF widget_position, double factor) {
  ZoomTraceScope trace("zoom_step", zoom_);
  if (factor <= 0.0 || !std::isfinite(factor)) {
    return;
  }
  const QPointF document_anchor((widget_position.x() - pan_.x()) / zoom_,
                                (widget_position.y() - pan_.y()) / zoom_);
  const auto old_zoom = zoom_;
  zoom_ = clamp_logical_zoom(zoom_ * factor);
  if (std::abs(old_zoom - zoom_) < 0.0001) {
    return;
  }
  pan_ = QPointF(widget_position.x() - document_anchor.x() * zoom_,
                 widget_position.y() - document_anchor.y() * zoom_);
  constrain_pan();
  update_tool_cursor();
  update();
  notify_view_changed();
}

void CanvasWidget::fit_to_view() {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0 || width() <= 0 || height() <= 0) {
    return;
  }

  const auto available_width = std::max(1.0, static_cast<double>(width() - 80));
  const auto available_height = std::max(1.0, static_cast<double>(height() - 80));
  const auto fitted = rotated_extent(QSizeF(document_->width(), document_->height()), shown_view_rotation());
  zoom_ = clamp_logical_zoom(std::min(available_width / fitted.width(), available_height / fitted.height()));
  pan_ = QPointF((static_cast<double>(width()) - static_cast<double>(document_->width()) * zoom_) / 2.0,
                 (static_cast<double>(height()) - static_cast<double>(document_->height()) * zoom_) / 2.0);
  constrain_pan();
  update();
  notify_view_changed();
}

void CanvasWidget::fill_to_view() {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0 || width() <= 0 || height() <= 0) {
    return;
  }

  const auto fitted = rotated_extent(QSizeF(document_->width(), document_->height()), shown_view_rotation());
  zoom_ = clamp_logical_zoom(std::max(static_cast<double>(width()) / fitted.width(),
                                      static_cast<double>(height()) / fitted.height()));
  pan_ = QPointF((static_cast<double>(width()) - static_cast<double>(document_->width()) * zoom_) / 2.0,
                 (static_cast<double>(height()) - static_cast<double>(document_->height()) * zoom_) / 2.0);
  constrain_pan();
  update();
  notify_view_changed();
}

void CanvasWidget::center_document_in_view() {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0 || width() <= 0 || height() <= 0) {
    return;
  }

  pan_ = QPointF((static_cast<double>(width()) - static_cast<double>(document_->width()) * zoom_) / 2.0,
                 (static_cast<double>(height()) - static_cast<double>(document_->height()) * zoom_) / 2.0);
  constrain_pan();
  update();
  notify_view_changed();
}

void CanvasWidget::zoom_to_document_rect(QRect document_rect) {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0 || width() <= 0 || height() <= 0) {
    return;
  }

  document_rect = document_rect.normalized().intersected(QRect(0, 0, document_->width(), document_->height()));
  // Reject only a degenerate point; a thin strip still zooms to fit its longer
  // axis (the divisions below stay safe since a non-empty rect is at least 1px).
  if (document_rect.width() <= 1 && document_rect.height() <= 1) {
    return;
  }

  const auto available_width = std::max(1.0, static_cast<double>(width() - 80));
  const auto available_height = std::max(1.0, static_cast<double>(height() - 80));
  const auto fitted = rotated_extent(QSizeF(std::max(1.0, static_cast<double>(document_rect.width())),
                                            std::max(1.0, static_cast<double>(document_rect.height()))),
                                     shown_view_rotation());
  zoom_ = clamp_logical_zoom(std::min(available_width / fitted.width(), available_height / fitted.height()));
  pan_ = QPointF((static_cast<double>(width()) - static_cast<double>(document_rect.width()) * zoom_) / 2.0 -
                     static_cast<double>(document_rect.x()) * zoom_,
                 (static_cast<double>(height()) - static_cast<double>(document_rect.height()) * zoom_) / 2.0 -
                     static_cast<double>(document_rect.y()) * zoom_);
  constrain_pan();
  update_tool_cursor();
  update();
  notify_view_changed();
}

void CanvasWidget::set_spacebar_panning(bool enabled) {
  if (spacebar_panning_ == enabled) {
    return;
  }
  spacebar_panning_ = enabled;
  if (!panning_) {
    update_tool_cursor();
  }
}

bool CanvasWidget::begin_pan_at_global_position(QPoint global_position) {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0) {
    return false;
  }
  clear_move_hover_outline();
  last_mouse_position_ = view_point_from_widget(QPointF(mapFromGlobal(global_position))).toPoint();
  panning_ = true;
  setCursor(Qt::ClosedHandCursor);
  return true;
}

bool CanvasWidget::pan_to_global_position(QPoint global_position) {
  if (!panning_) {
    return false;
  }
  clear_move_hover_outline();
  const auto position = view_point_from_widget(QPointF(mapFromGlobal(global_position))).toPoint();
  const auto delta = position - last_mouse_position_;
  const auto old_pan = pan_;
  pan_ += QPointF(delta);
  constrain_pan();
  last_mouse_position_ = position;
  if (pan_ != old_pan) {
    update();
    notify_view_changed();
  }
  return true;
}

bool CanvasWidget::end_pan() {
  if (!panning_) {
    return false;
  }
  panning_ = false;
  update_tool_cursor();
  return true;
}

bool CanvasWidget::constrain_pan() noexcept {
  if (document_ == nullptr || document_->width() <= 0 || document_->height() <= 0 || width() <= 0 || height() <= 0) {
    return false;
  }

  // The clamp works on the document's on-screen bounds, which are pan_ and the
  // zoomed size exactly while the view is unrotated.
  const auto bounds = document_widget_bounds();
  const QPointF constrained(constrained_document_axis(bounds.x(), static_cast<double>(width()), bounds.width()),
                            constrained_document_axis(bounds.y(), static_cast<double>(height()), bounds.height()));
  if (constrained == bounds.topLeft()) {
    return false;
  }

  if (view_rotated()) {
    pan_by_widget_delta(constrained - bounds.topLeft());
  } else {
    pan_ = constrained;
  }
  return true;
}

double CanvasWidget::view_rotation() const noexcept {
  return view_rotation_;
}

bool CanvasWidget::view_rotated() const noexcept {
  return shown_view_rotation() != 0.0;
}

double CanvasWidget::shown_view_rotation() const noexcept {
  return view_rotation_suspended_ ? 0.0 : view_rotation_;
}

void CanvasWidget::set_view_rotation_suspended(bool suspended) {
  if (view_rotation_suspended_ == suspended) {
    return;
  }
  view_rotation_suspended_ = suspended;
  if (view_rotation_ == 0.0) {
    return;
  }
  constrain_pan();
  update_tool_cursor();
  update();
  notify_view_changed();
}

void CanvasWidget::set_view_rotation(double degrees) {
  const auto normalized = normalized_view_rotation(degrees);
  if (normalized == view_rotation_) {
    return;
  }
  // The view turns about the viewport center, which maps to itself, so the
  // document point under the center stays put and pan_ needs no change.
  view_rotation_ = normalized;
  if (view_rotation_suspended_) {
    notify_view_changed();  // the angle field still follows
    return;
  }
  constrain_pan();
  update_tool_cursor();
  update();
  notify_view_changed();
}

QTransform CanvasWidget::view_to_widget_transform() const {
  if (!view_rotated()) {
    return {};
  }
  return rotation_about(QPointF(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0),
                        shown_view_rotation());
}

QPointF CanvasWidget::view_point_from_widget(QPointF widget_point) const {
  if (!view_rotated()) {
    return widget_point;
  }
  return rotation_about(QPointF(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0),
                        -shown_view_rotation())
      .map(widget_point);
}

QPointF CanvasWidget::widget_point_from_view(QPointF view_point) const {
  return view_rotated() ? view_to_widget_transform().map(view_point) : view_point;
}

QPoint CanvasWidget::global_point_for_view_point(QPoint view_point) const {
  return mapToGlobal(widget_point_from_view(QPointF(view_point)).toPoint());
}

QPointF CanvasWidget::view_delta_from_widget_delta(QPointF widget_delta) const {
  return view_rotated() ? QTransform().rotate(-shown_view_rotation()).map(widget_delta) : widget_delta;
}

QRect CanvasWidget::widget_rect_for_view_rect(const QRectF& view_rect) const {
  if (!view_rotated()) {
    return view_rect.toAlignedRect();
  }
  // One pixel of slack covers antialiased edges that the rotation smears
  // across a pixel boundary.
  return view_to_widget_transform().mapRect(view_rect).toAlignedRect().adjusted(-1, -1, 1, 1);
}

QRect CanvasWidget::view_rect_for_widget_rect(const QRectF& widget_rect) const {
  if (!view_rotated()) {
    return widget_rect.toAlignedRect();
  }
  return rotation_about(QPointF(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0),
                        -shown_view_rotation())
      .mapRect(widget_rect)
      .toAlignedRect()
      .adjusted(-1, -1, 1, 1);
}

QRect CanvasWidget::visible_view_rect() const {
  return view_rotated() ? view_rect_for_widget_rect(QRectF(rect())) : rect();
}

void CanvasWidget::update_view_rect(const QRect& view_rect) {
  if (!view_rotated()) {
    update(view_rect);
    return;
  }
  if (!view_rect.isEmpty()) {
    update(widget_rect_for_view_rect(QRectF(view_rect)));
  }
}

void CanvasWidget::update_view_region(const QRegion& view_region) {
  if (!view_rotated()) {
    update(view_region);
    return;
  }
  // Rotated rects cover more than their bounds, so a many-rect region is
  // cheaper as one bounding rect than as a union of inflated bounds.
  if (!view_region.isEmpty()) {
    update(widget_rect_for_view_rect(QRectF(view_region.boundingRect())));
  }
}

QRectF CanvasWidget::document_widget_bounds() const {
  if (document_ == nullptr) {
    return {};
  }
  const QRectF view_bounds(pan_, QSizeF(static_cast<double>(document_->width()) * zoom_,
                                        static_cast<double>(document_->height()) * zoom_));
  return view_rotated() ? view_to_widget_transform().mapRect(view_bounds) : view_bounds;
}

void CanvasWidget::pan_by_widget_delta(QPointF widget_delta) {
  pan_ += view_delta_from_widget_delta(widget_delta);
}

bool CanvasWidget::pixel_snapped_view() const noexcept {
  return uses_pixel_aligned_view(zoom_) && !view_rotated();
}

bool CanvasWidget::deep_zoom_pixel_view() const noexcept {
  return uses_deep_zoom_pixel_renderer(view_zoom()) && !view_rotated();
}

void CanvasWidget::begin_rotate_view_drag(QPointF widget_point) {
  const QPointF center(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0);
  rotating_view_ = true;
  rotate_view_start_rotation_ = view_rotation_;
  rotate_view_start_pointer_angle_ =
      qRadiansToDegrees(std::atan2(widget_point.y() - center.y(), widget_point.x() - center.x()));
}

void CanvasWidget::update_rotate_view_drag(QPointF widget_point, Qt::KeyboardModifiers modifiers) {
  if (!rotating_view_) {
    return;
  }
  // The canvas follows the pointer's angle around the viewport center, like
  // turning a sheet of paper; y points down, so the angle grows clockwise.
  const QPointF center(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0);
  const auto pointer_angle =
      qRadiansToDegrees(std::atan2(widget_point.y() - center.y(), widget_point.x() - center.x()));
  auto rotation = rotate_view_start_rotation_ + (pointer_angle - rotate_view_start_pointer_angle_);
  if ((modifiers & Qt::ShiftModifier) != 0) {
    rotation = std::round(rotation / kRotateViewSnapDegrees) * kRotateViewSnapDegrees;
  }
  set_view_rotation(rotation);
}

void CanvasWidget::end_rotate_view_drag() {
  rotating_view_ = false;
}

bool CanvasWidget::dispatch_view_mapped_event(QEvent* event, bool& result) {
  switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
      auto* source = static_cast<QMouseEvent*>(event);
      QMouseEvent mapped(source->type(), view_point_from_widget(source->position()), source->scenePosition(),
                         source->globalPosition(), source->button(), source->buttons(), source->modifiers(),
                         source->pointingDevice());
      mapped.setTimestamp(source->timestamp());
      result = QWidget::event(&mapped);
      event->setAccepted(mapped.isAccepted());
      return true;
    }
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
    case QEvent::TabletMove: {
      auto* source = static_cast<QTabletEvent*>(event);
      QTabletEvent mapped(source->type(), source->pointingDevice(), view_point_from_widget(source->position()),
                          source->globalPosition(), source->pressure(), source->xTilt(), source->yTilt(),
                          source->tangentialPressure(), source->rotation(), source->z(), source->modifiers(),
                          source->button(), source->buttons());
      mapped.setTimestamp(source->timestamp());
      mapped.setAccepted(source->isAccepted());
      result = QWidget::event(&mapped);
      event->setAccepted(mapped.isAccepted());
      return true;
    }
    case QEvent::Wheel: {
      auto* source = static_cast<QWheelEvent*>(event);
      QWheelEvent mapped(view_point_from_widget(source->position()), source->globalPosition(), source->pixelDelta(),
                         source->angleDelta(), source->buttons(), source->modifiers(), source->phase(),
                         source->inverted(), source->source(), source->pointingDevice());
      mapped.setTimestamp(source->timestamp());
      result = QWidget::event(&mapped);
      event->setAccepted(mapped.isAccepted());
      return true;
    }
    case QEvent::Enter: {
      auto* source = static_cast<QEnterEvent*>(event);
      QEnterEvent mapped(view_point_from_widget(source->position()), source->scenePosition(),
                         source->globalPosition(), source->pointingDevice());
      result = QWidget::event(&mapped);
      event->setAccepted(mapped.isAccepted());
      return true;
    }
    default:
      return false;
  }
}

void CanvasWidget::notify_view_changed() {
  ZoomTraceScope trace("view_changed", zoom_);
  sync_scroll_bars();  // before the callback so observers see consistent bars
  if (view_changed_callback_) {
    view_changed_callback_();
  }
}

void CanvasWidget::sync_scroll_bars() {
  if (horizontal_scroll_bar_ == nullptr || vertical_scroll_bar_ == nullptr) {
    return;
  }
  syncing_scroll_bars_ = true;

  // Right and bottom edges, each shortened so the corner square stays free
  // (Photoshop layout). setGeometry no-ops when unchanged.
  const auto bar_thickness_v = std::max(1, vertical_scroll_bar_->sizeHint().width());
  const auto bar_thickness_h = std::max(1, horizontal_scroll_bar_->sizeHint().height());
  horizontal_scroll_bar_->setGeometry(0, height() - bar_thickness_h,
                                      std::max(0, width() - bar_thickness_v), bar_thickness_h);
  vertical_scroll_bar_->setGeometry(width() - bar_thickness_v, 0, bar_thickness_v,
                                    std::max(0, height() - bar_thickness_h));

  const bool bars_visible = document_ != nullptr && document_->width() > 0 && document_->height() > 0 &&
                            width() > 0 && height() > 0;
  horizontal_scroll_bar_->setVisible(bars_visible);
  vertical_scroll_bar_->setVisible(bars_visible);
  if (bars_visible) {
    const auto sync_axis = [](QScrollBar& bar, double pan, double viewport_span, double document_span) {
      const auto range = pan_axis_range(viewport_span, document_span);
      // Worst case fits int: a 300000 px document side at 128x zoom plus the
      // viewport is ~38.4M, and Qt's slider pixel math is qint64.
      const auto maximum = std::max(0, qRound(range.maximum - range.minimum));
      bar.setRange(0, maximum);
      bar.setPageStep(qRound(viewport_span));  // proportional thumb, like Photoshop
      bar.setSingleStep(kScrollBarSingleStep);
      // maximum and value are rounded independently, hence the explicit clamp.
      bar.setValue(std::clamp(qRound(range.maximum - pan), 0, maximum));
    };
    // On-screen bounds: pan_ and the zoomed size while unrotated, the rotated
    // document's bounding box otherwise, so the bars always scroll the screen.
    const auto bounds = document_widget_bounds();
    sync_axis(*horizontal_scroll_bar_, bounds.x(), static_cast<double>(width()), bounds.width());
    sync_axis(*vertical_scroll_bar_, bounds.y(), static_cast<double>(height()), bounds.height());
    horizontal_scroll_bar_->raise();  // stay above later-created children (inline text editor)
    vertical_scroll_bar_->raise();
  }
  syncing_scroll_bars_ = false;
}

void CanvasWidget::handle_scroll_bar_value_changed(Qt::Orientation orientation, int value) {
  if (syncing_scroll_bars_ || document_ == nullptr || document_->width() <= 0 || document_->height() <= 0) {
    return;
  }
  const bool horizontal = orientation == Qt::Horizontal;
  const auto viewport_span = static_cast<double>(horizontal ? width() : height());
  const auto bounds = document_widget_bounds();
  const auto document_span = horizontal ? bounds.width() : bounds.height();
  if (viewport_span <= 0.0 || document_span <= 0.0) {
    return;
  }
  const auto target = pan_axis_range(viewport_span, document_span).maximum - static_cast<double>(value);
  const auto current = horizontal ? bounds.x() : bounds.y();
  if (current == target) {
    return;
  }
  if (view_rotated()) {
    pan_by_widget_delta(horizontal ? QPointF(target - current, 0.0) : QPointF(0.0, target - current));
  } else {
    (horizontal ? pan_.rx() : pan_.ry()) = target;
  }
  constrain_pan();  // no-op by construction (target is inside the clamp range); safety net
  update();
  notify_view_changed();
}

QPoint CanvasWidget::widget_position_for_document_point(QPoint document_position) const {
  if (!view_rotated()) {
    return widget_position(document_position);
  }
  return widget_point_for_document_point(QPointF(document_position)).toPoint();
}

QPointF CanvasWidget::widget_point_for_document_point(QPointF document_position) const {
  return widget_point_from_view(widget_position_f(document_position));
}

void CanvasWidget::set_wheel_zooms(bool enabled) noexcept {
  wheel_zooms_ = enabled;
}

bool CanvasWidget::wheel_zooms() const noexcept {
  return wheel_zooms_;
}

void CanvasWidget::set_zoom_scrubby(bool enabled) noexcept {
  zoom_scrubby_ = enabled;
}

bool CanvasWidget::zoom_scrubby() const noexcept {
  return zoom_scrubby_;
}

void CanvasWidget::set_zoom_tool_zooms_out(bool enabled) {
  if (zoom_tool_zooms_out_ == enabled) {
    return;
  }
  zoom_tool_zooms_out_ = enabled;
  if (tool_ == CanvasTool::Zoom) {
    update_tool_cursor();
    update();
  }
}

bool CanvasWidget::zoom_tool_zooms_out() const noexcept {
  return zoom_tool_zooms_out_;
}

bool CanvasWidget::zoom_tool_zoom_out_active(Qt::KeyboardModifiers modifiers) const noexcept {
  return zoom_tool_zooms_out_ != ((modifiers & Qt::AltModifier) != 0);
}

QPointF CanvasWidget::zoom_click_anchor(QPointF widget_pos) const {
  // A press in the grey margin zooms toward the nearest point on the document
  // frame rather than toward the empty space under the cursor.
  if (document_ == nullptr) {
    return widget_pos;
  }
  const QRectF frame(widget_position_f(QPointF(0.0, 0.0)),
                     widget_position_f(QPointF(document_->width(), document_->height())));
  return QPointF(std::clamp(widget_pos.x(), frame.left(), frame.right()),
                 std::clamp(widget_pos.y(), frame.top(), frame.bottom()));
}

void CanvasWidget::draw_zoom_preview(QPainter& painter) const {
  // No marquee while the click would zoom out (Alt, or the Zoom Out mode: a
  // point zoom, not a rectangle) or while a Scrubby Zoom drag is zooming live.
  if (!zooming_ || zoom_scrubbing_ || zoom_tool_zoom_out_active(QApplication::keyboardModifiers())) {
    return;
  }

  const auto preview_rect = QRect(widget_position(zoom_start_), widget_position(zoom_current_)).normalized();
  if (preview_rect.width() < 2 && preview_rect.height() < 2) {
    return;
  }

  painter.save();
  painter.setBrush(QColor(65, 135, 220, 35));
  painter.setPen(Qt::NoPen);
  painter.drawRect(preview_rect);

  QPen dark(QColor(15, 18, 22), 1.0);
  dark.setDashPattern({4.0, 4.0});
  dark.setDashOffset(selection_dash_offset_ + 4);
  dark.setCosmetic(true);
  QPen light(QColor(248, 250, 253), 1.0);
  light.setDashPattern({4.0, 4.0});
  light.setDashOffset(selection_dash_offset_);
  light.setCosmetic(true);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(dark);
  painter.drawRect(preview_rect);
  painter.setPen(light);
  painter.drawRect(preview_rect);
  painter.restore();
}

QPoint CanvasWidget::document_position(const QPoint& widget_position) const {
  const auto coordinate_from_widget = [this](int widget_coordinate, double pan, int limit) {
    auto coordinate = static_cast<int>(std::floor((static_cast<double>(widget_coordinate) - pan) / zoom_));
    if (document_ == nullptr || !deep_zoom_pixel_view()) {
      return coordinate;
    }
    const auto edge = [pan, this](int document_coordinate) {
      return static_cast<int>(std::round(pan + static_cast<double>(document_coordinate) * zoom_));
    };
    while (coordinate > 0 && edge(coordinate) > widget_coordinate) {
      --coordinate;
    }
    while (coordinate < limit && edge(coordinate + 1) <= widget_coordinate) {
      ++coordinate;
    }
    return coordinate;
  };
  const auto x = coordinate_from_widget(widget_position.x(), pan_.x(), document_ != nullptr ? document_->width() : 0);
  const auto y = coordinate_from_widget(widget_position.y(), pan_.y(), document_ != nullptr ? document_->height() : 0);
  return QPoint(x, y);
}

QPointF CanvasWidget::document_position_f(QPointF widget_position) const {
  return QPointF((widget_position.x() - pan_.x()) / zoom_, (widget_position.y() - pan_.y()) / zoom_);
}

QPoint CanvasWidget::widget_position(const QPoint& document_position) const {
  return QPoint(static_cast<int>(std::round(pan_.x() + static_cast<double>(document_position.x()) * zoom_)),
                static_cast<int>(std::round(pan_.y() + static_cast<double>(document_position.y()) * zoom_)));
}

QPointF CanvasWidget::widget_position_f(QPointF document_position) const {
  return QPointF(pan_.x() + document_position.x() * zoom_, pan_.y() + document_position.y() * zoom_);
}

void CanvasWidget::begin_zoom_drag(QPointF widget_position) {
  pen_zoom_dragging_ = true;
  zoom_drag_anchor_widget_ = widget_position;
  zoom_drag_last_pos_ = widget_position;
  setCursor(Qt::SizeVerCursor);
}

void CanvasWidget::update_zoom_drag(QPointF widget_position) {
  if (!pen_zoom_dragging_) {
    return;
  }
  // Dragging up zooms in, dragging down zooms out, anchored on the press point.
  // "Up" is on screen, so a rotated view compares real widget heights.
  const auto delta =
      widget_point_from_view(zoom_drag_last_pos_).y() - widget_point_from_view(widget_position).y();
  zoom_drag_last_pos_ = widget_position;
  apply_zoom_drag_step(delta);
}

void CanvasWidget::apply_zoom_drag_step(double delta_pixels) {
  if (std::abs(delta_pixels) < 0.001) {
    return;
  }
  zoom_at_view_point(zoom_drag_anchor_widget_, std::pow(kZoomDragFactorPerPixel, delta_pixels));
}

void CanvasWidget::end_zoom_drag() {
  if (!pen_zoom_dragging_) {
    return;
  }
  pen_zoom_dragging_ = false;
  update_tool_cursor();
}

}  // namespace patchy::ui
