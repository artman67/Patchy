#pragma once

// The HSV color state and the drawing widgets shared by the popup color picker
// (color_panel.cpp) and the docked Color panel (color_dock_panel.cpp). See
// docs/color-picker.md.

#include <QColor>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace patchy::ui {

enum class ColorChangeNotification {
  No,
  Yes,
};

// One scalar component of the current colour. Drives the gradient sliders and the
// generic getter/setter on the model so a single slider class covers all six.
enum class ColorChannel { Red, Green, Blue, Hue, Saturation, Value };

[[nodiscard]] QColor normalized_rgb_color(QColor color);

// A color typed or carried as text: a name, "#RGB", "#RRGGBB", or CSS
// "#RGBA"/"#RRGGBBAA". Invalid QColor when unparseable.
[[nodiscard]] QColor parse_panel_color(QString text);

// The color every picker view edits. Hue, saturation and value are kept beside
// the RGB color so a grey keeps its hue and the markers do not jump. Views call
// the edit helpers (which always notify); the owner reacts in
// color_model_changed.
class HsvColorModel {
public:
  HsvColorModel() = default;
  virtual ~HsvColorModel() = default;
  HsvColorModel(const HsvColorModel&) = delete;
  HsvColorModel& operator=(const HsvColorModel&) = delete;

  [[nodiscard]] QColor current_color() const { return color_; }
  [[nodiscard]] int hue() const { return hue_; }
  [[nodiscard]] int saturation() const { return saturation_; }
  [[nodiscard]] int value() const { return value_; }
  [[nodiscard]] int channel_value(ColorChannel channel) const;
  [[nodiscard]] static int channel_maximum(ColorChannel channel) { return channel == ColorChannel::Hue ? 359 : 255; }

  // RGB in; the hue is kept when the color has none (greys).
  void set_color(QColor color, ColorChangeNotification notification);
  void set_hsv(int hue, int saturation, int value, ColorChangeNotification notification);

  // View edits. Every one notifies.
  void set_saturation_value_from_point(QPoint point, QSize size);
  void set_hue_from_point(QPoint point, QSize size);
  void set_hue(int hue);
  void set_channel(ColorChannel channel, int value);

protected:
  // Runs after every state change, with the color from before it: refresh the
  // controls, and report the edit when notification is Yes.
  virtual void color_model_changed(QColor previous, ColorChangeNotification notification) = 0;

private:
  QColor color_{Qt::black};
  int hue_{0};
  int saturation_{0};
  int value_{0};
};

// Saturation (x) / value (y) plane for the current hue.
class ColorPlaneWidget final : public QWidget {
public:
  ColorPlaneWidget(HsvColorModel& model, QWidget* parent);
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  HsvColorModel& model_;
};

// Vertical hue bar beside the plane.
class HueSliderWidget final : public QWidget {
public:
  HueSliderWidget(HsvColorModel& model, QWidget* parent);
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  HsvColorModel& model_;
};

// What sits inside the wheel's hue ring: a saturation/value square, or the
// painter's triangle (pure hue, white and black corners, rotating with the hue).
enum class ColorWheelInner { Square, Triangle };

// An outer hue ring (drag = hue) around an inner saturation/value area. Hue 0
// (red) is at 3 o'clock and hue runs counter-clockwise.
class ColorWheelWidget final : public QWidget {
public:
  ColorWheelWidget(HsvColorModel& model, ColorWheelInner inner, QWidget* parent);
  [[nodiscard]] QSize sizeHint() const override;

  // Where the given saturation/value sits in the inner area (widget
  // coordinates); exposed for tests.
  [[nodiscard]] QPointF inner_point_for(int saturation, int value) const;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  enum class Region { None, Ring, Inner };

  struct WheelGeometry {
    QPointF center;
    double outer{0.0};
    double inner{0.0};
    QRectF square;
    // Triangle corners: pure hue, white, black.
    QPointF hue_corner;
    QPointF white_corner;
    QPointF black_corner;
  };

  [[nodiscard]] WheelGeometry wheel_geometry() const;
  [[nodiscard]] Region hit_test(QPoint pos) const;
  void apply(QPoint pos);
  void apply_triangle(const WheelGeometry& geometry, QPointF pos);
  void paint_triangle(QPainter& painter, const WheelGeometry& geometry) const;

  HsvColorModel& model_;
  ColorWheelInner inner_;
  Region active_{Region::None};
};

// One gradient track per colour channel. The track previews the colour across
// that channel's range with the other channels held at their current values.
class ColorChannelSlider final : public QWidget {
public:
  ColorChannelSlider(HsvColorModel& model, ColorChannel channel, const QString& object_name, QWidget* parent);
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  [[nodiscard]] QColor channel_color(int channel_value) const;
  void set_from_x(int x);

  HsvColorModel& model_;
  ColorChannel channel_;
};

}  // namespace patchy::ui
