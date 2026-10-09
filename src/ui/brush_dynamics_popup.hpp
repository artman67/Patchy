#pragma once

#include "core/brush_dynamics.hpp"
#include "ui/brush_settings_sections.hpp"

#include <QString>
#include <QToolButton>
#include <QWidget>

#include <map>

class QCheckBox;
class QComboBox;
class QSlider;
class QSpinBox;

namespace patchy::ui {

// Photoshop's angle/roundness preview: a rotated ellipse with an arrow along the angle. Dragging
// rotates it; dragging one of the two roundness handles flattens or rounds it.
class AngleRoundnessWidget : public QWidget {
  Q_OBJECT

public:
  explicit AngleRoundnessWidget(QWidget* parent = nullptr);
  void set_values(double angle_degrees, double roundness);
  [[nodiscard]] QSize sizeHint() const override;

signals:
  // Whole degrees (-180..180) and percent (1..100), like the spin boxes beside it.
  void edited(int angle_degrees, int roundness);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  [[nodiscard]] double radius() const;
  void drag_to(QPointF position);

  double angle_{0.0};
  double roundness_{100.0};
  enum class Drag { None, Angle, Roundness } drag_{Drag::None};
};

// The dynamics editing form, one page per Brush Settings section (Brush Tip Shape angle and
// roundness, Shape Dynamics, Scattering, Texture, Dual Brush, Color Dynamics, Transfer, Wet
// Edges). AllSections stacks the pages in titled groups with a Reset button (the Brush Tips
// manager's "Edit Dynamics..." dialog); Pages leaves them for the Brush Settings panel to
// place, whose section list carries the enable flags. Emits edited() on every user change;
// hosts read the values back through the getters.
class BrushDynamicsPanel : public QWidget {
  Q_OBJECT

public:
  enum class Presentation { AllSections, Pages };

  explicit BrushDynamicsPanel(QWidget* parent = nullptr,
                              Presentation presentation = Presentation::AllSections);

  void set_values(const patchy::BrushDynamics& dynamics, double base_angle_degrees,
                  double base_roundness);
  [[nodiscard]] patchy::BrushDynamics dynamics() const;
  [[nodiscard]] double base_angle_degrees() const;
  [[nodiscard]] double base_roundness() const;
  // The section's page, or nullptr for sections this form does not edit (Build-up, Smoothing).
  // With Presentation::Pages the host reparents it into its own layout.
  [[nodiscard]] QWidget* section_page(BrushSection section) const;

signals:
  void edited();

protected:
  void changeEvent(QEvent* event) override;

private:
  void reset_to_defaults();
  void retranslate_combos();
  // Fade-steps spins show only while their combo says Fade; minimum Transfer rows are live
  // only while their matching control has a real source; a disabled effect greys its rows.
  void refresh_control_dependent_widgets();

  Presentation presentation_;
  std::map<BrushSection, QWidget*> pages_;
  AngleRoundnessWidget* angle_roundness_widget_{nullptr};
  QSpinBox* base_angle_spin_{nullptr};
  QSpinBox* base_roundness_spin_{nullptr};
  QSpinBox* size_jitter_spin_{nullptr};
  QSpinBox* minimum_diameter_spin_{nullptr};
  QComboBox* size_control_combo_{nullptr};
  QSpinBox* size_fade_steps_spin_{nullptr};
  QSpinBox* angle_jitter_spin_{nullptr};
  QComboBox* angle_control_combo_{nullptr};
  QSpinBox* fade_steps_spin_{nullptr};
  QSpinBox* roundness_jitter_spin_{nullptr};
  QSpinBox* minimum_roundness_spin_{nullptr};
  QComboBox* roundness_control_combo_{nullptr};
  QSpinBox* roundness_fade_steps_spin_{nullptr};
  QCheckBox* flip_x_check_{nullptr};
  QCheckBox* flip_y_check_{nullptr};
  QSpinBox* scatter_spin_{nullptr};
  QComboBox* scatter_control_combo_{nullptr};
  QSpinBox* scatter_fade_steps_spin_{nullptr};
  QCheckBox* both_axes_check_{nullptr};
  QSpinBox* count_spin_{nullptr};
  QSpinBox* count_jitter_spin_{nullptr};
  QComboBox* count_control_combo_{nullptr};
  QSpinBox* count_fade_steps_spin_{nullptr};
  QSpinBox* opacity_jitter_spin_{nullptr};
  QSpinBox* minimum_opacity_spin_{nullptr};
  QSlider* minimum_opacity_slider_{nullptr};
  QComboBox* opacity_control_combo_{nullptr};
  QSpinBox* opacity_fade_steps_spin_{nullptr};
  QSpinBox* flow_jitter_spin_{nullptr};
  QSpinBox* minimum_flow_spin_{nullptr};
  QSlider* minimum_flow_slider_{nullptr};
  QComboBox* flow_control_combo_{nullptr};
  QSpinBox* flow_fade_steps_spin_{nullptr};
  QCheckBox* texture_enabled_check_{nullptr};
  QComboBox* texture_style_combo_{nullptr};
  QSpinBox* texture_scale_spin_{nullptr};
  QSpinBox* texture_depth_spin_{nullptr};
  QCheckBox* texture_invert_check_{nullptr};
  // The struct last loaded: dynamics() starts from it, so fields the form does not show (the
  // imported textureSeed, fields added to the engine later) survive an edit.
  patchy::BrushDynamics loaded_{};
  QCheckBox* dual_brush_enabled_check_{nullptr};
  QSpinBox* dual_brush_size_spin_{nullptr};
  QSpinBox* dual_brush_hardness_spin_{nullptr};
  QSpinBox* dual_brush_spacing_spin_{nullptr};
  QCheckBox* color_dynamics_enabled_check_{nullptr};
  QSpinBox* foreground_background_jitter_spin_{nullptr};
  QComboBox* color_control_combo_{nullptr};
  QSpinBox* color_fade_steps_spin_{nullptr};
  QSpinBox* hue_jitter_spin_{nullptr};
  QSpinBox* saturation_jitter_spin_{nullptr};
  QSpinBox* brightness_jitter_spin_{nullptr};
  QSpinBox* purity_spin_{nullptr};
  QCheckBox* color_per_tip_check_{nullptr};
  QCheckBox* wet_edges_check_{nullptr};
  bool loading_{false};
};

// Options-bar "Dynamics" button for the Brush tool. A click shows the Brush Settings panel
// (show_settings_requested); the button lights up while the working brush carries dynamics or
// a non-default tip shape.
class BrushDynamicsButton : public QToolButton {
  Q_OBJECT

public:
  explicit BrushDynamicsButton(QWidget* parent = nullptr);

  // Refreshes the indicator from the working brush; tip_key is working_brush_tip_key().
  void set_working_brush(const QString& tip_key, const patchy::BrushDynamics& dynamics,
                         double base_angle_degrees, double base_roundness);
  // True once a brush is loaded; MainWindow::refresh_options_bar combines this with the
  // document-editability flag instead of blanket-enabling the button.
  [[nodiscard]] bool has_active_tip() const noexcept { return !tip_id_.isEmpty(); }
  void retranslate();

signals:
  void show_settings_requested();

private:
  QString tip_id_;
};

}  // namespace patchy::ui
