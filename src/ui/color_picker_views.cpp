#include "ui/color_picker_views.hpp"

#include <QConicalGradient>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace patchy::ui {

namespace {

constexpr int kColorPlaneSize = 188;
constexpr int kHueSliderWidth = 16;
constexpr int kColorWheelSize = 188;
constexpr int kColorWheelMinSize = 150;
constexpr int kColorWheelRing = 20;
constexpr double kColorWheelMinRing = 12.0;
// Gap between the hue ring's inner edge and the triangle's corners.
constexpr double kTriangleRingGap = 3.0;
constexpr int kChannelSliderHeight = 20;
constexpr int kChannelSliderWidth = 184;
constexpr double kPi = 3.14159265358979323846;

int bounded_channel(int value) {
  return std::clamp(value, 0, 255);
}

int rounded_scaled_channel(int position, int maximum_position, int maximum_value) {
  if (maximum_position <= 0) {
    return 0;
  }
  return std::clamp(static_cast<int>(std::lround(static_cast<double>(position) * maximum_value / maximum_position)), 0,
                    maximum_value);
}

// Renders the saturation (x) / value (y) gradient for a fixed hue into an image,
// shared by the square-mode plane and the colour wheel's inner square.
QImage render_saturation_value_image(int hue, QSize size) {
  if (size.isEmpty()) {
    return {};
  }
  QImage image(size, QImage::Format_RGB32);
  const auto max_x = std::max(1, size.width() - 1);
  const auto max_y = std::max(1, size.height() - 1);
  for (int y = 0; y < size.height(); ++y) {
    auto* scanline = reinterpret_cast<QRgb*>(image.scanLine(y));
    const int value = 255 - rounded_scaled_channel(y, max_y, 255);
    for (int x = 0; x < size.width(); ++x) {
      const int saturation = rounded_scaled_channel(x, max_x, 255);
      scanline[x] = QColor::fromHsv(hue, saturation, value).rgb();
    }
  }
  return image;
}

// A two-tone crosshair (dark halo + light core) so the marker stays visible over
// any colour underneath. Used by the SV plane and the wheel's inner square.
void draw_crosshair_marker(QPainter& painter, QPointF center) {
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(10, 10, 10), 3.0));
  painter.drawLine(center + QPointF(-8.0, 0.0), center + QPointF(8.0, 0.0));
  painter.drawLine(center + QPointF(0.0, -8.0), center + QPointF(0.0, 8.0));
  painter.setPen(QPen(QColor(245, 245, 245), 1.0));
  painter.drawLine(center + QPointF(-8.0, 0.0), center + QPointF(8.0, 0.0));
  painter.drawLine(center + QPointF(0.0, -8.0), center + QPointF(0.0, 8.0));
}

// The same two-tone treatment as a small ring, for the hue and triangle markers.
void draw_ring_marker(QPainter& painter, QPointF center, double radius) {
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor(20, 20, 20), 3.0));
  painter.drawEllipse(center, radius, radius);
  painter.setPen(QPen(QColor(245, 245, 245), 1.5));
  painter.drawEllipse(center, radius, radius);
}

double dot(QPointF a, QPointF b) {
  return a.x() * b.x() + a.y() * b.y();
}

QPointF closest_point_on_segment(QPointF point, QPointF a, QPointF b) {
  const auto ab = b - a;
  const auto length_squared = dot(ab, ab);
  if (length_squared <= 0.0) {
    return a;
  }
  const auto t = std::clamp(dot(point - a, ab) / length_squared, 0.0, 1.0);
  return a + ab * t;
}

// Barycentric weights of a point against the triangle's (hue, white, black)
// corners; negative outside the triangle.
std::array<double, 3> triangle_weights(QPointF point, QPointF hue_corner, QPointF white_corner,
                                       QPointF black_corner) {
  const auto v0 = white_corner - hue_corner;
  const auto v1 = black_corner - hue_corner;
  const auto v2 = point - hue_corner;
  const auto d00 = dot(v0, v0);
  const auto d01 = dot(v0, v1);
  const auto d11 = dot(v1, v1);
  const auto denominator = d00 * d11 - d01 * d01;
  if (std::abs(denominator) < 1e-9) {
    return {1.0, 0.0, 0.0};
  }
  const auto d20 = dot(v2, v0);
  const auto d21 = dot(v2, v1);
  const auto white = (d11 * d20 - d01 * d21) / denominator;
  const auto black = (d00 * d21 - d01 * d20) / denominator;
  return {1.0 - white - black, white, black};
}

// Weights clamped into the triangle, renormalized to sum to one.
std::array<double, 3> clamped_triangle_weights(std::array<double, 3> weights) {
  double sum = 0.0;
  for (auto& weight : weights) {
    weight = std::max(0.0, weight);
    sum += weight;
  }
  if (sum <= 0.0) {
    return {1.0, 0.0, 0.0};
  }
  for (auto& weight : weights) {
    weight /= sum;
  }
  return weights;
}

// Triangle weights to saturation/value in 0..1: value is the distance from the
// black corner, saturation the share of pure hue in the non-black part.
std::pair<double, double> saturation_value_from_weights(const std::array<double, 3>& weights,
                                                        double fallback_saturation) {
  const auto value = std::clamp(weights[0] + weights[1], 0.0, 1.0);
  const auto saturation = value > 1e-9 ? std::clamp(weights[0] / value, 0.0, 1.0) : fallback_saturation;
  return {saturation, value};
}

}  // namespace

QColor normalized_rgb_color(QColor color) {
  if (!color.isValid()) {
    return color;
  }
  color = color.toRgb();
  color.setAlpha(255);
  return color;
}

QColor parse_panel_color(QString text) {
  text = text.trimmed();
  if (!text.startsWith(QLatin1Char('#'))) {
    const QColor named(text);
    if (named.isValid()) {
      return named;
    }
    text.prepend(QLatin1Char('#'));
  }
  if (text.size() == 5) {
    QString expanded = QStringLiteral("#");
    for (qsizetype i = 1; i < text.size(); ++i) {
      expanded += QString(2, text[i]);
    }
    text = std::move(expanded);
  }
  if (text.size() == 9) {
    bool valid = false;
    const auto rgba = text.mid(1).toUInt(&valid, 16);
    return valid ? QColor::fromRgba((rgba >> 8U) | ((rgba & 255U) << 24U)) : QColor{};
  }
  return QColor(text);
}

// --- HsvColorModel ---

int HsvColorModel::channel_value(ColorChannel channel) const {
  switch (channel) {
    case ColorChannel::Red:
      return color_.red();
    case ColorChannel::Green:
      return color_.green();
    case ColorChannel::Blue:
      return color_.blue();
    case ColorChannel::Hue:
      return hue_;
    case ColorChannel::Saturation:
      return saturation_;
    case ColorChannel::Value:
      return value_;
  }
  return 0;
}

void HsvColorModel::set_color(QColor color, ColorChangeNotification notification) {
  if (!color.isValid()) {
    return;
  }

  color = normalized_rgb_color(color);
  const auto previous = color_;

  int color_hue = 0;
  int color_saturation = 0;
  int color_value = 0;
  color.getHsv(&color_hue, &color_saturation, &color_value);
  if (color_hue >= 0) {
    hue_ = std::clamp(color_hue, 0, 359);
  }
  saturation_ = bounded_channel(color_saturation);
  value_ = bounded_channel(color_value);
  color_ = color;
  color_model_changed(previous, notification);
}

void HsvColorModel::set_hsv(int hue, int saturation, int value, ColorChangeNotification notification) {
  hue_ = std::clamp(hue, 0, 359);
  saturation_ = bounded_channel(saturation);
  value_ = bounded_channel(value);
  const auto previous = color_;
  color_ = normalized_rgb_color(QColor::fromHsv(hue_, saturation_, value_));
  color_model_changed(previous, notification);
}

void HsvColorModel::set_saturation_value_from_point(QPoint point, QSize size) {
  const auto max_x = std::max(1, size.width() - 1);
  const auto max_y = std::max(1, size.height() - 1);
  const int x = std::clamp(point.x(), 0, max_x);
  const int y = std::clamp(point.y(), 0, max_y);
  const int saturation = x <= 2 ? 0 : (x >= max_x - 2 ? 255 : rounded_scaled_channel(x, max_x, 255));
  const int value = y <= 2 ? 255 : (y >= max_y - 2 ? 0 : 255 - rounded_scaled_channel(y, max_y, 255));
  set_hsv(hue_, saturation, value, ColorChangeNotification::Yes);
}

void HsvColorModel::set_hue_from_point(QPoint point, QSize size) {
  const auto max_y = std::max(1, size.height() - 1);
  const int y = std::clamp(point.y(), 0, max_y);
  set_hsv(rounded_scaled_channel(y, max_y, 359), saturation_, value_, ColorChangeNotification::Yes);
}

void HsvColorModel::set_hue(int hue) {
  set_hsv(hue, saturation_, value_, ColorChangeNotification::Yes);
}

void HsvColorModel::set_channel(ColorChannel channel, int value) {
  switch (channel) {
    case ColorChannel::Red:
      set_color(QColor(bounded_channel(value), color_.green(), color_.blue()), ColorChangeNotification::Yes);
      return;
    case ColorChannel::Green:
      set_color(QColor(color_.red(), bounded_channel(value), color_.blue()), ColorChangeNotification::Yes);
      return;
    case ColorChannel::Blue:
      set_color(QColor(color_.red(), color_.green(), bounded_channel(value)), ColorChangeNotification::Yes);
      return;
    case ColorChannel::Hue:
      set_hsv(value, saturation_, value_, ColorChangeNotification::Yes);
      return;
    case ColorChannel::Saturation:
      set_hsv(hue_, value, value_, ColorChangeNotification::Yes);
      return;
    case ColorChannel::Value:
      set_hsv(hue_, saturation_, value, ColorChangeNotification::Yes);
      return;
  }
}

// --- ColorPlaneWidget ---

ColorPlaneWidget::ColorPlaneWidget(HsvColorModel& model, QWidget* parent) : QWidget(parent), model_(model) {
  setObjectName(QStringLiteral("patchyColorPlane"));
  setCursor(Qt::CrossCursor);
  setMinimumSize(kColorPlaneSize, kColorPlaneSize);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFocusPolicy(Qt::ClickFocus);  // Edit > Copy/Paste route to the picker
}

QSize ColorPlaneWidget::sizeHint() const {
  return QSize(kColorPlaneSize, kColorPlaneSize);
}

void ColorPlaneWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  const auto paint_size = size();
  if (paint_size.isEmpty()) {
    return;
  }

  const auto max_x = std::max(1, paint_size.width() - 1);
  const auto max_y = std::max(1, paint_size.height() - 1);

  QPainter painter(this);
  painter.drawImage(QPoint(0, 0), render_saturation_value_image(model_.hue(), paint_size));
  painter.setPen(QPen(QColor(26, 26, 26), 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  const double cursor_x = static_cast<double>(model_.saturation()) / 255.0 * max_x;
  const double cursor_y = static_cast<double>(255 - model_.value()) / 255.0 * max_y;
  draw_crosshair_marker(painter, QPointF(cursor_x, cursor_y));
}

void ColorPlaneWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    model_.set_saturation_value_from_point(event->position().toPoint(), size());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ColorPlaneWidget::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) != 0) {
    model_.set_saturation_value_from_point(event->position().toPoint(), size());
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

// --- HueSliderWidget ---

HueSliderWidget::HueSliderWidget(HsvColorModel& model, QWidget* parent) : QWidget(parent), model_(model) {
  setObjectName(QStringLiteral("patchyHueSlider"));
  setCursor(Qt::PointingHandCursor);
  setMinimumSize(kHueSliderWidth, kColorPlaneSize);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFocusPolicy(Qt::ClickFocus);
}

QSize HueSliderWidget::sizeHint() const {
  return QSize(kHueSliderWidth, kColorPlaneSize);
}

void HueSliderWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  const auto max_y = std::max(1, height() - 1);
  for (int y = 0; y < height(); ++y) {
    const int hue = rounded_scaled_channel(y, max_y, 359);
    painter.setPen(QColor::fromHsv(hue, 255, 255));
    painter.drawLine(0, y, width() - 1, y);
  }

  painter.setPen(QPen(QColor(26, 26, 26), 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  const int marker_y = rounded_scaled_channel(model_.hue(), 359, max_y);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(0, 0, 0), 3.0));
  painter.drawLine(QPointF(0.0, marker_y), QPointF(width() - 1.0, marker_y));
  painter.setPen(QPen(QColor(245, 245, 245), 1.0));
  painter.drawLine(QPointF(0.0, marker_y), QPointF(width() - 1.0, marker_y));
}

void HueSliderWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    model_.set_hue_from_point(event->position().toPoint(), size());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void HueSliderWidget::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) != 0) {
    model_.set_hue_from_point(event->position().toPoint(), size());
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

// --- ColorWheelWidget ---

ColorWheelWidget::ColorWheelWidget(HsvColorModel& model, ColorWheelInner inner, QWidget* parent)
    : QWidget(parent), model_(model), inner_(inner) {
  setObjectName(inner == ColorWheelInner::Square ? QStringLiteral("patchyColorWheel")
                                                 : QStringLiteral("patchyColorTriangleWheel"));
  setCursor(Qt::CrossCursor);
  setMinimumSize(kColorWheelMinSize, kColorWheelMinSize);
  // Expand to fill the page so there is no wasted space around the ring.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::ClickFocus);
}

QSize ColorWheelWidget::sizeHint() const {
  return QSize(kColorWheelSize, kColorWheelSize);
}

ColorWheelWidget::WheelGeometry ColorWheelWidget::wheel_geometry() const {
  const double side = std::min(width(), height());
  const QPointF center(width() / 2.0, height() / 2.0);
  const double outer = side / 2.0 - 2.0;
  // The ring thins on small wheels (a narrow dock) so the inner area keeps room.
  const double ring = std::clamp(side * 0.11, kColorWheelMinRing, static_cast<double>(kColorWheelRing));
  const double inner = std::max(0.0, outer - ring);
  const double square_side = std::max(8.0, inner * std::sqrt(2.0) - 2.0);
  const QRectF square(center.x() - square_side / 2.0, center.y() - square_side / 2.0, square_side, square_side);
  // The triangle's hue corner points at the current hue on the ring; white
  // follows 120 degrees counter-clockwise, black 120 degrees after that.
  const double radius = std::max(4.0, inner - kTriangleRingGap);
  const auto corner = [&center, radius](double degrees) {
    const double radians = degrees * kPi / 180.0;
    return QPointF(center.x() + radius * std::cos(radians), center.y() - radius * std::sin(radians));
  };
  const double hue = model_.hue();
  return {center, outer, inner, square, corner(hue), corner(hue + 120.0), corner(hue + 240.0)};
}

QPointF ColorWheelWidget::inner_point_for(int saturation, int value) const {
  const auto geometry = wheel_geometry();
  if (inner_ == ColorWheelInner::Square) {
    const QRect square = geometry.square.toRect();
    return {square.left() + saturation / 255.0 * std::max(1, square.width() - 1),
            square.top() + (255 - value) / 255.0 * std::max(1, square.height() - 1)};
  }
  const double s = saturation / 255.0;
  const double v = value / 255.0;
  return geometry.hue_corner * (v * s) + geometry.white_corner * (v * (1.0 - s)) + geometry.black_corner * (1.0 - v);
}

void ColorWheelWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  const auto geometry = wheel_geometry();
  if (geometry.outer <= 0.0) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Hue ring: a conical sweep clipped to the annulus. Qt's conical gradient runs
  // counter-clockwise from 3 o'clock, which is the same convention the click
  // hit-test uses (atan2 of the inverted y), so marker and gradient stay aligned.
  QConicalGradient ring(geometry.center, 0.0);
  for (int stop = 0; stop <= 6; ++stop) {
    ring.setColorAt(stop / 6.0, QColor::fromHsv((stop * 60) % 360, 255, 255));
  }
  QPainterPath ring_path;
  ring_path.addEllipse(geometry.center, geometry.outer, geometry.outer);
  QPainterPath hole;
  hole.addEllipse(geometry.center, geometry.inner, geometry.inner);
  painter.fillPath(ring_path.subtracted(hole), QBrush(ring));
  painter.setPen(QPen(QColor(26, 26, 26), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(geometry.center, geometry.outer, geometry.outer);
  painter.drawEllipse(geometry.center, geometry.inner, geometry.inner);

  if (inner_ == ColorWheelInner::Square) {
    // Inner saturation/value square for the current hue.
    const QRect square = geometry.square.toRect();
    painter.drawImage(square.topLeft(), render_saturation_value_image(model_.hue(), square.size()));
    painter.setPen(QPen(QColor(26, 26, 26), 1));
    painter.drawRect(square.adjusted(0, 0, -1, -1));
  } else {
    paint_triangle(painter, geometry);
  }

  // Hue marker on the ring.
  const double angle = model_.hue() * kPi / 180.0;
  const double mid_radius = (geometry.outer + geometry.inner) / 2.0;
  const QPointF hue_marker(geometry.center.x() + mid_radius * std::cos(angle),
                           geometry.center.y() - mid_radius * std::sin(angle));
  draw_ring_marker(painter, hue_marker, 6.0);

  // Saturation/value marker inside the inner area.
  const auto marker = inner_point_for(model_.saturation(), model_.value());
  if (inner_ == ColorWheelInner::Square) {
    draw_crosshair_marker(painter, marker);
  } else {
    draw_ring_marker(painter, marker, 5.0);
  }
}

void ColorWheelWidget::paint_triangle(QPainter& painter, const WheelGeometry& geometry) const {
  QPainterPath path;
  path.moveTo(geometry.hue_corner);
  path.lineTo(geometry.white_corner);
  path.lineTo(geometry.black_corner);
  path.closeSubpath();
  const QRect box = path.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
  if (box.isEmpty()) {
    return;
  }
  // Pixels just outside the edges take the nearest edge color, so the
  // antialiased fill below never blends toward an undefined color.
  QImage image(box.size(), QImage::Format_RGB32);
  const int hue = model_.hue();
  for (int y = 0; y < box.height(); ++y) {
    auto* scanline = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < box.width(); ++x) {
      const QPointF point(box.left() + x + 0.5, box.top() + y + 0.5);
      const auto weights = clamped_triangle_weights(
          triangle_weights(point, geometry.hue_corner, geometry.white_corner, geometry.black_corner));
      const auto [saturation, value] = saturation_value_from_weights(weights, 0.0);
      scanline[x] = QColor::fromHsv(hue, static_cast<int>(std::lround(saturation * 255.0)),
                                    static_cast<int>(std::lround(value * 255.0)))
                        .rgb();
    }
  }
  QBrush brush(image);
  brush.setTransform(QTransform::fromTranslate(box.left(), box.top()));
  painter.fillPath(path, brush);
  painter.setPen(QPen(QColor(26, 26, 26), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);
}

void ColorWheelWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    active_ = hit_test(event->position().toPoint());
    apply(event->position().toPoint());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) != 0 && active_ != Region::None) {
    apply(event->position().toPoint());
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent* event) {
  active_ = Region::None;
  QWidget::mouseReleaseEvent(event);
}

ColorWheelWidget::Region ColorWheelWidget::hit_test(QPoint pos) const {
  const auto geometry = wheel_geometry();
  const double distance = std::hypot(pos.x() - geometry.center.x(), pos.y() - geometry.center.y());
  if (distance >= geometry.inner - 2.0 && distance <= geometry.outer + 6.0) {
    return Region::Ring;
  }
  if (distance < geometry.inner) {
    return Region::Inner;
  }
  return Region::None;
}

void ColorWheelWidget::apply(QPoint pos) {
  const auto geometry = wheel_geometry();
  if (active_ == Region::Ring) {
    double degrees = std::atan2(geometry.center.y() - pos.y(), pos.x() - geometry.center.x()) * 180.0 / kPi;
    if (degrees < 0.0) {
      degrees += 360.0;
    }
    model_.set_hue(static_cast<int>(std::lround(degrees)) % 360);
    return;
  }
  if (active_ != Region::Inner) {
    return;
  }
  if (inner_ == ColorWheelInner::Triangle) {
    apply_triangle(geometry, QPointF(pos));
    return;
  }
  const QRect square = geometry.square.toRect();
  const QPoint local(std::clamp(pos.x() - square.left(), 0, std::max(1, square.width() - 1)),
                     std::clamp(pos.y() - square.top(), 0, std::max(1, square.height() - 1)));
  model_.set_saturation_value_from_point(local, square.size());
}

void ColorWheelWidget::apply_triangle(const WheelGeometry& geometry, QPointF pos) {
  auto weights = triangle_weights(pos, geometry.hue_corner, geometry.white_corner, geometry.black_corner);
  if (std::min({weights[0], weights[1], weights[2]}) < 0.0) {
    // Outside: snap to the nearest point on the triangle's edges.
    const std::array<QPointF, 3> candidates{
        closest_point_on_segment(pos, geometry.hue_corner, geometry.white_corner),
        closest_point_on_segment(pos, geometry.white_corner, geometry.black_corner),
        closest_point_on_segment(pos, geometry.black_corner, geometry.hue_corner)};
    auto nearest = candidates[0];
    for (const auto& candidate : candidates) {
      if (dot(candidate - pos, candidate - pos) < dot(nearest - pos, nearest - pos)) {
        nearest = candidate;
      }
    }
    weights = triangle_weights(nearest, geometry.hue_corner, geometry.white_corner, geometry.black_corner);
  }
  const auto [saturation, value] =
      saturation_value_from_weights(clamped_triangle_weights(weights), model_.saturation() / 255.0);
  model_.set_hsv(model_.hue(), static_cast<int>(std::lround(saturation * 255.0)),
                 static_cast<int>(std::lround(value * 255.0)), ColorChangeNotification::Yes);
}

// --- ColorChannelSlider ---

ColorChannelSlider::ColorChannelSlider(HsvColorModel& model, ColorChannel channel, const QString& object_name,
                                       QWidget* parent)
    : QWidget(parent), model_(model), channel_(channel) {
  setObjectName(object_name);
  setCursor(Qt::PointingHandCursor);
  setMinimumSize(120, kChannelSliderHeight);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFocusPolicy(Qt::ClickFocus);
}

QSize ColorChannelSlider::sizeHint() const {
  return QSize(kChannelSliderWidth, kChannelSliderHeight);
}

void ColorChannelSlider::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  const int track_width = std::max(1, width());
  const int max_value = HsvColorModel::channel_maximum(channel_);

  QImage track(track_width, 1, QImage::Format_RGB32);
  auto* scanline = reinterpret_cast<QRgb*>(track.scanLine(0));
  for (int x = 0; x < track_width; ++x) {
    scanline[x] = channel_color(rounded_scaled_channel(x, track_width - 1, max_value)).rgb();
  }

  QPainter painter(this);
  painter.drawImage(rect(), track);
  painter.setPen(QPen(QColor(26, 26, 26), 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  const double thumb_x =
      static_cast<double>(model_.channel_value(channel_)) / std::max(1, max_value) * (track_width - 1);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF handle(thumb_x - 4.0, 0.5, 8.0, height() - 1.0);
  painter.setPen(QPen(QColor(20, 20, 20), 2.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(handle, 2.0, 2.0);
  painter.setPen(QPen(QColor(245, 245, 245), 1.0));
  painter.drawRoundedRect(handle.adjusted(1.0, 1.0, -1.0, -1.0), 1.5, 1.5);
}

void ColorChannelSlider::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    set_from_x(event->position().toPoint().x());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ColorChannelSlider::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) != 0) {
    set_from_x(event->position().toPoint().x());
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

QColor ColorChannelSlider::channel_color(int channel_value) const {
  const QColor color = model_.current_color();
  switch (channel_) {
    case ColorChannel::Red:
      return QColor(channel_value, color.green(), color.blue());
    case ColorChannel::Green:
      return QColor(color.red(), channel_value, color.blue());
    case ColorChannel::Blue:
      return QColor(color.red(), color.green(), channel_value);
    case ColorChannel::Hue:
      return QColor::fromHsv(std::clamp(channel_value, 0, 359), 255, 255);
    case ColorChannel::Saturation:
      return QColor::fromHsv(model_.hue(), channel_value, model_.value());
    case ColorChannel::Value:
      return QColor::fromHsv(model_.hue(), model_.saturation(), channel_value);
  }
  return color;
}

void ColorChannelSlider::set_from_x(int x) {
  const int track_width = std::max(1, width());
  const int clamped = std::clamp(x, 0, track_width - 1);
  model_.set_channel(channel_, rounded_scaled_channel(clamped, track_width - 1, HsvColorModel::channel_maximum(channel_)));
}

}  // namespace patchy::ui
