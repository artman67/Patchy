#pragma once

#include "core/brush_dynamics.hpp"

#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QToolButton>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QSpinBox;

namespace patchy::ui {

// The dynamics editing form (Tip Shape / Shape Dynamics / Scattering / Transfer / texture /
// dual brush / color / effects + Reset), shared by the options-bar Dynamics popup and the
// Brush Tips manager's editor. Emits edited() on every user change; hosts read the values back
// via the getters.
class BrushDynamicsPanel : public QWidget {
  Q_OBJECT

public:
  explicit BrushDynamicsPanel(QWidget* parent = nullptr);

  void set_values(const patchy::BrushDynamics& dynamics, double base_angle_degrees,
                  double base_roundness);
  [[nodiscard]] patchy::BrushDynamics dynamics() const;
  [[nodiscard]] double base_angle_degrees() const;
  [[nodiscard]] double base_roundness() const;

signals:
  void edited();

private:
  void reset_to_defaults();
  // Fade-steps spins show only while their combo says Fade; minimum Transfer rows are live
  // only while their matching control has a real source.
  void refresh_control_dependent_widgets();

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
  QComboBox* opacity_control_combo_{nullptr};
  QSpinBox* opacity_fade_steps_spin_{nullptr};
  QSpinBox* flow_jitter_spin_{nullptr};
  QSpinBox* minimum_flow_spin_{nullptr};
  QComboBox* flow_control_combo_{nullptr};
  QSpinBox* flow_fade_steps_spin_{nullptr};
  QCheckBox* texture_enabled_check_{nullptr};
  QComboBox* texture_style_combo_{nullptr};
  QSpinBox* texture_scale_spin_{nullptr};
  QSpinBox* texture_depth_spin_{nullptr};
  QCheckBox* texture_invert_check_{nullptr};
  std::uint32_t texture_seed_{0x5A17C9E3U};  // imported/persisted, intentionally not user-edited
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

// Options-bar "Dynamics" button for the Brush tool: opens a popup hosting a BrushDynamicsPanel
// for the working brush (ui/current_brush). Edits are debounced and emitted via
// dynamics_edited; they change the working brush only, never a tip's stored defaults.
class BrushDynamicsButton : public QToolButton {
  Q_OBJECT

public:
  explicit BrushDynamicsButton(QWidget* parent = nullptr);

  // Loads the button's model from the working brush; tip_key is working_brush_tip_key(). While
  // the popup is open for the same tip the reload is skipped, so the echo of our own edit does
  // not fight the open controls.
  void set_working_brush(const QString& tip_key, const patchy::BrushDynamics& dynamics,
                         double base_angle_degrees, double base_roundness);
  // True once a model is loaded; MainWindow::refresh_options_bar combines this with the
  // document-editability flag instead of blanket-enabling the button.
  [[nodiscard]] bool has_active_tip() const noexcept { return !tip_id_.isEmpty(); }
  void retranslate();

signals:
  void dynamics_edited(const QString& tip_id, const patchy::BrushDynamics& dynamics,
                       double base_angle_degrees, double base_roundness);

private:
  void show_popup();
  void schedule_emit();
  void refresh_active_indicator();

  QString tip_id_;
  patchy::BrushDynamics dynamics_{};
  double base_angle_degrees_{0.0};
  double base_roundness_{100.0};
  QPointer<QFrame> popup_;
  // Toggle guard: the click that dismisses the Qt::Popup is replayed onto the button (or lands
  // while the popup is still closing), which would instantly reopen it. See show_popup().
  QElapsedTimer popup_clock_;
  qint64 popup_dismissed_ms_{-1};
};

}  // namespace patchy::ui
