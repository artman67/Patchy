#pragma once

// The docked Color panel: foreground/background swatches choosing which color
// it edits, a mode dropdown, and the shared picker views from
// color_picker_views.hpp. MainWindow owns the routing into the canvas; see
// docs/color-picker.md.

#include "ui/color_picker_views.hpp"

#include <QColor>
#include <QWidget>

#include <array>

class QComboBox;
class QEvent;
class QLineEdit;
class QLabel;

namespace patchy::ui {

class ColorDockSwatches;

class ColorDockPanel final : public QWidget, private HsvColorModel {
  Q_OBJECT

public:
  // Persisted by token under kColorDockModeKey; append only.
  enum class Mode { HsvSquare, WheelSquare, WheelTriangle, Sliders, HsvSliders, RgbSliders };

  explicit ColorDockPanel(QWidget* parent = nullptr);

  // External foreground/background changes (eyedropper, swatch popup, Palette
  // panel, swap, defaults). Never emits color_edited.
  void set_colors(QColor foreground, QColor background);
  [[nodiscard]] QColor foreground() const { return foreground_; }
  [[nodiscard]] QColor background() const { return background_; }

  // Which color the views edit: the foreground (default) or the background.
  void set_editing_background(bool background);
  [[nodiscard]] bool editing_background() const noexcept { return editing_background_; }

  void set_mode(Mode mode);
  [[nodiscard]] Mode mode() const noexcept { return mode_; }

  using HsvColorModel::current_color;

signals:
  // A user edit in the panel; background tells which color it targets.
  void color_edited(bool background, QColor color);

protected:
  void changeEvent(QEvent* event) override;

private:
  void color_model_changed(QColor previous, ColorChangeNotification notification) override;
  void retranslate();
  void apply_hex_text();
  void refresh_views();
  void show_mode_widgets(int index);

  QColor foreground_{Qt::black};
  QColor background_{Qt::white};
  bool editing_background_{false};
  bool reporting_edit_{false};
  Mode mode_{Mode::HsvSquare};
  ColorDockSwatches* swatches_{nullptr};
  QComboBox* mode_combo_{nullptr};
  QLineEdit* hex_edit_{nullptr};
  std::array<QWidget*, 4> pages_{};
  std::array<QLabel*, 6> slider_labels_{};
  std::array<QWidget*, 6> sliders_{};
};

// QSettings key holding the panel's mode token ("square", "wheel", "triangle",
// "hsv_sliders", "rgb_sliders", "sliders"). Persisted identifiers: never rename the key or a token.
inline constexpr const char* kColorDockModeKey = "colorDock/mode";

}  // namespace patchy::ui
