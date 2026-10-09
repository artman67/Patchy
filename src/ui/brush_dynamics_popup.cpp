#include "ui/brush_dynamics_popup.hpp"

#include "ui/brush_tip_library.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/theme_palette.hpp"
#include "ui/theme_qss.hpp"
#include "ui/unit_spin_box.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace patchy::ui {

namespace {

constexpr auto kPanelContext = "patchy::ui::BrushDynamicsPanel";
constexpr auto kSectionContext = "patchy::ui::BrushSettingsPanel";
constexpr double kPi = 3.14159265358979323846;

// Percent fraction <-> spin value helpers (BrushDynamics stores 0..1 fractions).
[[nodiscard]] int percent_from_fraction(double fraction) {
  return std::clamp(static_cast<int>(std::lround(fraction * 100.0)), 0, 1000);
}

[[nodiscard]] double fraction_from_percent(int percent) {
  return static_cast<double>(percent) / 100.0;
}

[[nodiscard]] int normalized_angle_value(double degrees) {
  auto normalized = std::fmod(degrees, 360.0);
  if (normalized > 180.0) {
    normalized -= 360.0;
  } else if (normalized < -180.0) {
    normalized += 360.0;
  }
  return static_cast<int>(std::lround(normalized));
}

// True for the control sources that actually read the pen / fade (mirrors the core's gating;
// Off and GlobalDefault only decide global-preference precedence).
[[nodiscard]] bool control_has_source(patchy::BrushDynamicControl control) {
  switch (control) {
    case patchy::BrushDynamicControl::Fade:
    case patchy::BrushDynamicControl::PenPressure:
    case patchy::BrushDynamicControl::PenTilt:
    case patchy::BrushDynamicControl::PenRotation:
    case patchy::BrushDynamicControl::StylusWheel:
      return true;
    default:
      return false;
  }
}

[[nodiscard]] patchy::BrushDynamicControl combo_control(const QComboBox& combo) {
  return static_cast<patchy::BrushDynamicControl>(combo.currentData().toInt());
}

void select_combo_control(QComboBox& combo, patchy::BrushDynamicControl control) {
  auto index = combo.findData(static_cast<int>(control));
  if (index < 0) {
    index = 0;  // sanitized upstream; first item is the slot's default
  }
  combo.setCurrentIndex(index);
}

// The data-mapped control combos' items: display text per enum value.
struct ControlItem {
  patchy::BrushDynamicControl control;
  const char* source;
};
constexpr ControlItem kControlItems[] = {
    {patchy::BrushDynamicControl::GlobalDefault, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Use Global Pen Setting")},
    {patchy::BrushDynamicControl::Off, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Off")},
    {patchy::BrushDynamicControl::Fade, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Fade")},
    {patchy::BrushDynamicControl::PenPressure, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Pressure")},
    {patchy::BrushDynamicControl::PenTilt, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Tilt")},
    {patchy::BrushDynamicControl::StylusWheel, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Stylus Wheel")},
    {patchy::BrushDynamicControl::PenRotation, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Rotation")},
};
// The angle combo predates the data-mapped ones: its item indices equal the enum values (tests
// and set_values rely on that), so new sources append in enum order.
constexpr const char* kAngleControlItems[] = {
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Off"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Fade"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Pressure"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Tilt"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Pen Rotation"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Initial Direction"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Direction"),
    QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Stylus Wheel"),
};
struct TextureItem {
  patchy::BrushTextureStyle style;
  const char* source;
};
constexpr TextureItem kTextureItems[] = {
    {patchy::BrushTextureStyle::FineGrain, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Fine Grain")},
    {patchy::BrushTextureStyle::Canvas, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Canvas")},
    {patchy::BrushTextureStyle::Speckle, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel", "Speckle")},
};

[[nodiscard]] QString panel_tr(const char* source) {
  return QCoreApplication::translate(kPanelContext, source);
}

// Binds a widget's text and tooltip so a language switch retranslates them (the MainWindow
// walk and this panel's own LanguageChange handling both re-apply bound properties).
template <typename Widget>
Widget* bound(Widget* widget, const char* text, const char* tooltip = nullptr,
              const char* context = kPanelContext) {
  if (text != nullptr) {
    bind_translated_text(widget, text, context);
  }
  if (tooltip != nullptr) {
    bind_translated_tooltip(widget, tooltip, context);
  }
  apply_bound_translation(widget);
  return widget;
}

// macOS: QMacStyle's Aqua layout spacings/margins are far roomier than the
// Windows metrics this dense panel was designed around, which pushed the popup
// past the screen height. Pin Windows-like metrics there; other platforms keep
// their style defaults (Windows rendering must not move).
void compact_group_grid(QGridLayout* grid) {
#ifdef Q_OS_MACOS
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setContentsMargins(9, 4, 9, 9);
#else
  Q_UNUSED(grid);
#endif
}

}  // namespace

AngleRoundnessWidget::AngleRoundnessWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("brushTipAngleRoundnessWidget"));
  setMinimumSize(64, 64);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setCursor(Qt::CrossCursor);
  bind_translated_tooltip(
      this, QT_TRANSLATE_NOOP("patchy::ui::BrushDynamicsPanel",
                              "Drag to set the tip angle; drag a dot to change its roundness"),
      kPanelContext);
  apply_bound_translation(this);
}

QSize AngleRoundnessWidget::sizeHint() const {
  return {72, 72};
}

void AngleRoundnessWidget::set_values(double angle_degrees, double roundness) {
  angle_ = angle_degrees;
  roundness_ = std::clamp(roundness, 1.0, 100.0);
  update();
}

double AngleRoundnessWidget::radius() const {
  return std::max(8.0, std::min(width(), height()) / 2.0 - 6.0);
}

void AngleRoundnessWidget::paintEvent(QPaintEvent* /*event*/) {
  const auto& palette = theme();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QPointF center(width() / 2.0, height() / 2.0);
  const auto r = radius();
  painter.setPen(QPen(palette.field_inset_border, 1.0));
  painter.setBrush(palette.field_bg);
  painter.drawEllipse(center, r + 3.0, r + 3.0);
  painter.translate(center);
  // Photoshop angles turn counter-clockwise; the widget's y axis points down.
  painter.rotate(-angle_);
  const auto minor = r * roundness_ / 100.0;
  auto fill = palette.accent;
  fill.setAlpha(70);
  painter.setPen(QPen(palette.accent, 1.5));
  painter.setBrush(fill);
  painter.drawEllipse(QPointF(0.0, 0.0), r, minor);
  painter.drawLine(QPointF(0.0, 0.0), QPointF(r, 0.0));
  painter.drawLine(QPointF(r, 0.0), QPointF(r - 5.0, -3.5));
  painter.drawLine(QPointF(r, 0.0), QPointF(r - 5.0, 3.5));
  painter.setPen(QPen(palette.field_bg, 1.0));
  painter.setBrush(palette.text_primary);
  painter.drawEllipse(QPointF(0.0, -minor), 3.5, 3.5);
  painter.drawEllipse(QPointF(0.0, minor), 3.5, 3.5);
}

void AngleRoundnessWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !isEnabled()) {
    QWidget::mousePressEvent(event);
    return;
  }
  const QPointF center(width() / 2.0, height() / 2.0);
  const auto minor = radius() * roundness_ / 100.0;
  const auto radians = angle_ * kPi / 180.0;
  // The minor axis is perpendicular to the angle; its ends carry the roundness dots.
  const QPointF up(-std::sin(radians) * minor, -std::cos(radians) * minor);
  const auto offset = event->position() - center;
  const auto near = [&offset](QPointF handle) {
    const auto d = offset - handle;
    return std::hypot(d.x(), d.y()) <= 7.0;
  };
  drag_ = near(up) || near(-up) ? Drag::Roundness : Drag::Angle;
  drag_to(event->position());
  event->accept();
}

void AngleRoundnessWidget::mouseMoveEvent(QMouseEvent* event) {
  if (drag_ == Drag::None) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  drag_to(event->position());
  event->accept();
}

void AngleRoundnessWidget::mouseReleaseEvent(QMouseEvent* event) {
  drag_ = Drag::None;
  QWidget::mouseReleaseEvent(event);
}

void AngleRoundnessWidget::drag_to(QPointF position) {
  const QPointF center(width() / 2.0, height() / 2.0);
  const auto dx = position.x() - center.x();
  const auto dy = center.y() - position.y();  // y up, counter-clockwise angles
  if (drag_ == Drag::Angle) {
    if (std::hypot(dx, dy) < 2.0) {
      return;
    }
    angle_ = std::round(std::atan2(dy, dx) * 180.0 / kPi);
  } else if (drag_ == Drag::Roundness) {
    // Distance from the center along the minor axis.
    const auto radians = angle_ * kPi / 180.0;
    const auto along = std::abs(-std::sin(radians) * dx + std::cos(radians) * dy);
    roundness_ = std::clamp(std::round(along / radius() * 100.0), 1.0, 100.0);
  }
  update();
  emit edited(static_cast<int>(angle_), static_cast<int>(roundness_));
}

BrushDynamicsPanel::BrushDynamicsPanel(QWidget* parent, Presentation presentation)
    : QWidget(parent), presentation_(presentation) {
  setObjectName(QStringLiteral("brushDynamicsPanel"));

  // One grid page per section; AllSections wraps them in titled groups below.
  const auto make_page = [this](BrushSection section) {
    auto* page = new QWidget(this);
    auto key = brush_section_key(section);
    key[0] = key[0].toUpper();
    page->setObjectName(QStringLiteral("brushSettingsPage") + key);
    auto* grid = new QGridLayout(page);
    compact_group_grid(grid);
    if (presentation_ == Presentation::Pages) {
      grid->setContentsMargins(0, 0, 0, 0);
    }
    pages_[section] = page;
    return grid;
  };

  const auto make_label = [this](const char* source) { return bound(new QLabel(this), source); };
  // Pages (the dock's narrow column) stack each row: label and value on one line, the slider or
  // combo on the next, like Photoshop's Brush Settings. AllSections keeps one line per row.
  const bool stacked = presentation_ == Presentation::Pages;
  const auto R = [stacked](int row) { return stacked ? row * 2 : row; };

  const auto add_percent_row = [this, make_label, stacked, R](QGridLayout* grid, int row, const char* label,
                                                              const QString& object_name, int maximum) -> QSpinBox* {
    auto* text = make_label(label);
    auto* slider = new QSlider(Qt::Horizontal, this);
    slider->setObjectName(object_name + QStringLiteral("Slider"));
    slider->setRange(0, maximum);
    slider->setMinimumWidth(presentation_ == Presentation::Pages ? 40 : 120);
    auto* spin = new UnitIntSpinBox(SpinUnit::Percent, this);
    spin->setObjectName(object_name);
    spin->setRange(0, maximum);
    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    if (stacked) {
      grid->addWidget(text, R(row), 0, 1, 2);
      grid->addWidget(spin, R(row), 2);
      grid->addWidget(slider, R(row) + 1, 0, 1, 3);
    } else {
      grid->addWidget(text, row, 0);
      grid->addWidget(slider, row, 1);
      grid->addWidget(spin, row, 2);
    }
    return spin;
  };

  // A "Control:" combo plus its fade-steps spin (shown only while the combo says Fade). The
  // items carry the enum in their data so display order stays decoupled from the enum values;
  // with_global lists "Use Global Pen Setting" first (size/roundness/opacity only).
  const auto add_control_row = [this, make_label, stacked, R](QGridLayout* grid, int row, const char* label,
                                                              const QString& combo_name, const QString& fade_name,
                                                              bool with_global) -> std::pair<QComboBox*, QSpinBox*> {
    grid->addWidget(make_label(label), R(row), 0, 1, stacked ? 3 : 1);
    auto* combo = new QComboBox(this);
    combo->setObjectName(combo_name);
    for (const auto& item : kControlItems) {
      if (item.control == patchy::BrushDynamicControl::GlobalDefault && !with_global) {
        continue;
      }
      combo->addItem(panel_tr(item.source), static_cast<int>(item.control));
    }
    auto* fade_spin = new QSpinBox(this);
    fade_spin->setObjectName(fade_name);
    fade_spin->setRange(1, 9999);
    fade_spin->setValue(25);
    bound(fade_spin, nullptr, QT_TR_NOOP("Spacing steps to fade over"));
    fade_spin->setVisible(false);
    auto* row_layout = new QHBoxLayout();
    row_layout->addWidget(combo, 1);
    row_layout->addWidget(fade_spin);
    if (stacked) {
      grid->addLayout(row_layout, R(row) + 1, 0, 1, 3);
    } else {
      grid->addLayout(row_layout, row, 1, 1, 2);
    }
    return {combo, fade_spin};
  };

  // Brush Tip Shape: the static Photoshop angle/roundness, with the draggable preview.
  {
    auto* grid = make_page(BrushSection::TipShape);
    angle_roundness_widget_ = new AngleRoundnessWidget(this);
    grid->addWidget(angle_roundness_widget_, 0, 0, 2, 1);
    grid->addWidget(make_label(QT_TR_NOOP("Angle:")), 0, 1);
    base_angle_spin_ = new UnitIntSpinBox(SpinUnit::Degrees, this);
    base_angle_spin_->setObjectName(QStringLiteral("dynamicsBaseAngleSpin"));
    base_angle_spin_->setRange(-180, 180);
    grid->addWidget(base_angle_spin_, 0, 2);
    grid->addWidget(make_label(QT_TR_NOOP("Roundness:")), 1, 1);
    base_roundness_spin_ = new UnitIntSpinBox(SpinUnit::Percent, this);
    base_roundness_spin_->setObjectName(QStringLiteral("dynamicsBaseRoundnessSpin"));
    base_roundness_spin_->setRange(1, 100);
    base_roundness_spin_->setValue(100);
    grid->addWidget(base_roundness_spin_, 1, 2);
    grid->setColumnStretch(3, 1);
    connect(angle_roundness_widget_, &AngleRoundnessWidget::edited, this, [this](int angle, int roundness) {
      base_angle_spin_->setValue(angle);
      base_roundness_spin_->setValue(roundness);
    });
  }

  // Shape Dynamics.
  {
    auto* grid = make_page(BrushSection::ShapeDynamics);
    size_jitter_spin_ =
        add_percent_row(grid, 0, QT_TR_NOOP("Size Jitter:"), QStringLiteral("dynamicsSizeJitterSpin"), 100);
    minimum_diameter_spin_ = add_percent_row(grid, 1, QT_TR_NOOP("Minimum Diameter:"),
                                             QStringLiteral("dynamicsMinimumDiameterSpin"), 100);
    std::tie(size_control_combo_, size_fade_steps_spin_) =
        add_control_row(grid, 2, QT_TR_NOOP("Size Control:"), QStringLiteral("dynamicsSizeControlCombo"),
                        QStringLiteral("dynamicsSizeFadeStepsSpin"), true);
    angle_jitter_spin_ =
        add_percent_row(grid, 3, QT_TR_NOOP("Angle Jitter:"), QStringLiteral("dynamicsAngleJitterSpin"), 100);
    grid->addWidget(make_label(QT_TR_NOOP("Angle Control:")), R(4), 0, 1, stacked ? 3 : 1);
    angle_control_combo_ = new QComboBox(this);
    angle_control_combo_->setObjectName(QStringLiteral("dynamicsAngleControlCombo"));
    for (const auto* source : kAngleControlItems) {
      angle_control_combo_->addItem(panel_tr(source));
    }
    fade_steps_spin_ = new QSpinBox(this);
    fade_steps_spin_->setObjectName(QStringLiteral("dynamicsFadeStepsSpin"));
    fade_steps_spin_->setRange(1, 9999);
    fade_steps_spin_->setValue(25);
    bound(fade_steps_spin_, nullptr, QT_TR_NOOP("Spacing steps to fade over"));
    fade_steps_spin_->setVisible(false);
    auto* control_row = new QHBoxLayout();
    control_row->addWidget(angle_control_combo_, 1);
    control_row->addWidget(fade_steps_spin_);
    if (stacked) {
      grid->addLayout(control_row, R(4) + 1, 0, 1, 3);
    } else {
      grid->addLayout(control_row, 4, 1, 1, 2);
    }
    roundness_jitter_spin_ = add_percent_row(grid, 5, QT_TR_NOOP("Roundness Jitter:"),
                                             QStringLiteral("dynamicsRoundnessJitterSpin"), 100);
    minimum_roundness_spin_ = add_percent_row(grid, 6, QT_TR_NOOP("Minimum Roundness:"),
                                              QStringLiteral("dynamicsMinimumRoundnessSpin"), 100);
    minimum_roundness_spin_->setValue(25);
    std::tie(roundness_control_combo_, roundness_fade_steps_spin_) = add_control_row(
        grid, 7, QT_TR_NOOP("Roundness Control:"), QStringLiteral("dynamicsRoundnessControlCombo"),
        QStringLiteral("dynamicsRoundnessFadeStepsSpin"), true);
    auto* flips_row = new QHBoxLayout();
    flip_x_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Flip X Jitter"));
    flip_x_check_->setObjectName(QStringLiteral("dynamicsFlipXCheck"));
    flip_y_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Flip Y Jitter"));
    flip_y_check_->setObjectName(QStringLiteral("dynamicsFlipYCheck"));
    flips_row->addWidget(flip_x_check_);
    flips_row->addWidget(flip_y_check_);
    flips_row->addStretch(1);
    grid->addLayout(flips_row, R(8), 0, 1, 3);
  }

  // Scattering.
  {
    auto* grid = make_page(BrushSection::Scattering);
    scatter_spin_ = add_percent_row(grid, 0, QT_TR_NOOP("Scatter:"), QStringLiteral("dynamicsScatterSpin"), 1000);
    std::tie(scatter_control_combo_, scatter_fade_steps_spin_) =
        add_control_row(grid, 1, QT_TR_NOOP("Scatter Control:"), QStringLiteral("dynamicsScatterControlCombo"),
                        QStringLiteral("dynamicsScatterFadeStepsSpin"), false);
    both_axes_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Both Axes"));
    both_axes_check_->setObjectName(QStringLiteral("dynamicsBothAxesCheck"));
    grid->addWidget(both_axes_check_, R(2), 0, 1, 2);
    grid->addWidget(make_label(QT_TR_NOOP("Count:")), R(3), 0);
    count_spin_ = new QSpinBox(this);
    count_spin_->setObjectName(QStringLiteral("dynamicsCountSpin"));
    count_spin_->setRange(1, 16);
    grid->addWidget(count_spin_, R(3), stacked ? 2 : 1, Qt::AlignLeft);
    count_jitter_spin_ =
        add_percent_row(grid, 4, QT_TR_NOOP("Count Jitter:"), QStringLiteral("dynamicsCountJitterSpin"), 100);
    std::tie(count_control_combo_, count_fade_steps_spin_) =
        add_control_row(grid, 5, QT_TR_NOOP("Count Control:"), QStringLiteral("dynamicsCountControlCombo"),
                        QStringLiteral("dynamicsCountFadeStepsSpin"), false);
  }

  // Texture. The generated grain is intentionally static: scale/depth/invert are saved brush
  // settings, never pen-input controls.
  {
    auto* grid = make_page(BrushSection::Texture);
    texture_enabled_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Enable Texture"));
    texture_enabled_check_->setObjectName(QStringLiteral("dynamicsTextureEnabledCheck"));
    grid->addWidget(texture_enabled_check_, R(0), 0, 1, 2);
    grid->addWidget(make_label(QT_TR_NOOP("Grain:")), R(1), 0);
    texture_style_combo_ = new QComboBox(this);
    texture_style_combo_->setObjectName(QStringLiteral("dynamicsTextureStyleCombo"));
    for (const auto& item : kTextureItems) {
      texture_style_combo_->addItem(panel_tr(item.source), static_cast<int>(item.style));
    }
    grid->addWidget(texture_style_combo_, R(1), 1, 1, 2);
    texture_scale_spin_ =
        add_percent_row(grid, 2, QT_TR_NOOP("Scale:"), QStringLiteral("dynamicsTextureScaleSpin"), 1000);
    texture_scale_spin_->setMinimum(1);
    texture_depth_spin_ =
        add_percent_row(grid, 3, QT_TR_NOOP("Depth:"), QStringLiteral("dynamicsTextureDepthSpin"), 100);
    texture_invert_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Invert Texture"));
    texture_invert_check_->setObjectName(QStringLiteral("dynamicsTextureInvertCheck"));
    grid->addWidget(texture_invert_check_, R(4), 0, 1, 2);
  }

  // Dual Brush: one fixed secondary computed mask.
  {
    auto* grid = make_page(BrushSection::DualBrush);
    dual_brush_enabled_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Enable Dual Brush"));
    dual_brush_enabled_check_->setObjectName(QStringLiteral("dynamicsDualBrushEnabledCheck"));
    grid->addWidget(dual_brush_enabled_check_, R(0), 0, 1, 2);
    dual_brush_size_spin_ = add_percent_row(grid, 1, QT_TR_NOOP("Secondary Size:"),
                                            QStringLiteral("dynamicsDualBrushSizeSpin"), 400);
    dual_brush_size_spin_->setMinimum(5);
    dual_brush_hardness_spin_ = add_percent_row(grid, 2, QT_TR_NOOP("Secondary Hardness:"),
                                                QStringLiteral("dynamicsDualBrushHardnessSpin"), 100);
    dual_brush_spacing_spin_ = add_percent_row(grid, 3, QT_TR_NOOP("Secondary Spacing:"),
                                               QStringLiteral("dynamicsDualBrushSpacingSpin"), 1000);
    dual_brush_spacing_spin_->setMinimum(10);
  }

  // Color Dynamics.
  {
    auto* grid = make_page(BrushSection::ColorDynamics);
    color_dynamics_enabled_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Enable Color Dynamics"));
    color_dynamics_enabled_check_->setObjectName(QStringLiteral("dynamicsColorEnabledCheck"));
    grid->addWidget(color_dynamics_enabled_check_, R(0), 0, 1, 2);
    foreground_background_jitter_spin_ =
        add_percent_row(grid, 1, QT_TR_NOOP("Foreground/Background Jitter:"),
                        QStringLiteral("dynamicsColorForegroundBackgroundJitterSpin"), 100);
    std::tie(color_control_combo_, color_fade_steps_spin_) =
        add_control_row(grid, 2, QT_TR_NOOP("Color Control:"), QStringLiteral("dynamicsColorControlCombo"),
                        QStringLiteral("dynamicsColorFadeStepsSpin"), false);
    hue_jitter_spin_ =
        add_percent_row(grid, 3, QT_TR_NOOP("Hue Jitter:"), QStringLiteral("dynamicsColorHueJitterSpin"), 100);
    saturation_jitter_spin_ = add_percent_row(grid, 4, QT_TR_NOOP("Saturation Jitter:"),
                                              QStringLiteral("dynamicsColorSaturationJitterSpin"), 100);
    brightness_jitter_spin_ = add_percent_row(grid, 5, QT_TR_NOOP("Brightness Jitter:"),
                                              QStringLiteral("dynamicsColorBrightnessJitterSpin"), 100);
    grid->addWidget(make_label(QT_TR_NOOP("Purity:")), R(6), 0);
    purity_spin_ = new UnitIntSpinBox(SpinUnit::Percent, this);
    purity_spin_->setObjectName(QStringLiteral("dynamicsColorPuritySpin"));
    purity_spin_->setRange(-100, 100);
    grid->addWidget(purity_spin_, R(6), stacked ? 2 : 1, Qt::AlignLeft);
    color_per_tip_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Apply Per Tip"));
    color_per_tip_check_->setObjectName(QStringLiteral("dynamicsColorPerTipCheck"));
    color_per_tip_check_->setChecked(true);
    grid->addWidget(color_per_tip_check_, R(7), 0, 1, 2);
  }

  // Transfer (opacity and flow).
  {
    auto* grid = make_page(BrushSection::Transfer);
    opacity_jitter_spin_ = add_percent_row(grid, 0, QT_TR_NOOP("Opacity Jitter:"),
                                           QStringLiteral("dynamicsOpacityJitterSpin"), 100);
    minimum_opacity_spin_ = add_percent_row(grid, 1, QT_TR_NOOP("Minimum Opacity:"),
                                            QStringLiteral("dynamicsMinimumOpacitySpin"), 100);
    minimum_opacity_slider_ = findChild<QSlider*>(QStringLiteral("dynamicsMinimumOpacitySpinSlider"));
    std::tie(opacity_control_combo_, opacity_fade_steps_spin_) = add_control_row(
        grid, 2, QT_TR_NOOP("Opacity Control:"), QStringLiteral("dynamicsOpacityControlCombo"),
        QStringLiteral("dynamicsOpacityFadeStepsSpin"), true);
    flow_jitter_spin_ =
        add_percent_row(grid, 3, QT_TR_NOOP("Flow Jitter:"), QStringLiteral("dynamicsFlowJitterSpin"), 100);
    minimum_flow_spin_ =
        add_percent_row(grid, 4, QT_TR_NOOP("Minimum Flow:"), QStringLiteral("dynamicsMinimumFlowSpin"), 100);
    minimum_flow_slider_ = findChild<QSlider*>(QStringLiteral("dynamicsMinimumFlowSpinSlider"));
    std::tie(flow_control_combo_, flow_fade_steps_spin_) =
        add_control_row(grid, 5, QT_TR_NOOP("Flow Control:"), QStringLiteral("dynamicsFlowControlCombo"),
                        QStringLiteral("dynamicsFlowFadeStepsSpin"), false);
  }

  // Noise: static grain on the soft parts of each dab.
  {
    auto* grid = make_page(BrushSection::Noise);
    noise_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Noise"),
                         QT_TR_NOOP("Adds grain to the soft edges of the brush tip. The grain stays put on the "
                                    "canvas; hard tips barely change."));
    noise_check_->setObjectName(QStringLiteral("dynamicsNoiseCheck"));
    grid->addWidget(noise_check_, 0, 0);
    auto* about = make_label(QT_TR_NOOP("Adds grain to the soft edges of the brush tip. The grain stays put on the "
                                        "canvas; hard tips barely change."));
    about->setObjectName(QStringLiteral("dynamicsNoiseHint"));
    about->setWordWrap(true);
    grid->addWidget(about, 1, 0);
    about->setVisible(presentation_ == Presentation::Pages);
  }

  // Wet Edges: the independent coverage-edge treatment.
  {
    auto* grid = make_page(BrushSection::WetEdges);
    wet_edges_check_ = bound(new QCheckBox(this), QT_TR_NOOP("Wet Edges"),
                             QT_TR_NOOP("Builds paint along stroke edges for a watercolor wash. It does not smear "
                                        "canvas colors; use Smudge for that."));
    wet_edges_check_->setObjectName(QStringLiteral("dynamicsWetEdgesCheck"));
    grid->addWidget(wet_edges_check_, 0, 0);
    auto* about = make_label(QT_TR_NOOP("Builds paint along stroke edges for a watercolor wash. It does not smear "
                                        "canvas colors; use Smudge for that."));
    about->setObjectName(QStringLiteral("dynamicsWetEdgesHint"));
    about->setWordWrap(true);
    grid->addWidget(about, 1, 0);
    about->setVisible(presentation_ == Presentation::Pages);
  }

  for (auto& [section, page] : pages_) {
    // Rows fill a page from the top; a taller host leaves the slack below them.
    auto* grid = static_cast<QGridLayout*>(page->layout());
    grid->setRowStretch(grid->rowCount(), 1);
    if (section != BrushSection::TipShape) {
      grid->setColumnStretch(1, 1);
    }
  }

  if (presentation_ == Presentation::Pages) {
    // The Brush Settings section list carries these enable flags; set_values keeps the hidden
    // boxes in step so dynamics() reads them back.
    for (auto* check : {texture_enabled_check_, dual_brush_enabled_check_, color_dynamics_enabled_check_,
                        noise_check_, wet_edges_check_}) {
      check->setVisible(false);
    }
    hide();
  } else {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    for (const auto section : kBrushSections) {
      auto found = pages_.find(section);
      if (found == pages_.end()) {
        continue;
      }
      auto* group = bound(new QGroupBox(this), brush_section_title_source(section), nullptr, kSectionContext);
      auto* group_layout = new QVBoxLayout(group);
      group_layout->setContentsMargins(0, 0, 0, 0);
      group_layout->addWidget(found->second);
      layout->addWidget(group);
    }
    auto* footer = new QHBoxLayout();
    footer->addStretch(1);
    auto* reset_button = bound(new QPushButton(this), QT_TR_NOOP("Reset"),
                               QT_TR_NOOP("Reset the tip shape and all dynamics to defaults"));
    reset_button->setObjectName(QStringLiteral("dynamicsResetButton"));
    footer->addWidget(reset_button);
    layout->addLayout(footer);
    connect(reset_button, &QPushButton::clicked, this, &BrushDynamicsPanel::reset_to_defaults);
  }

  const auto emit_edited = [this] {
    if (!loading_) {
      refresh_control_dependent_widgets();
      angle_roundness_widget_->set_values(base_angle_spin_->value(), base_roundness_spin_->value());
      emit edited();
    }
  };
  for (auto* spin :
       {base_angle_spin_, base_roundness_spin_, size_jitter_spin_, minimum_diameter_spin_,
        size_fade_steps_spin_, angle_jitter_spin_, fade_steps_spin_, roundness_jitter_spin_,
        minimum_roundness_spin_, roundness_fade_steps_spin_, scatter_spin_, scatter_fade_steps_spin_,
        count_spin_, count_jitter_spin_, count_fade_steps_spin_, opacity_jitter_spin_,
        minimum_opacity_spin_, opacity_fade_steps_spin_, flow_jitter_spin_, minimum_flow_spin_,
        flow_fade_steps_spin_, texture_scale_spin_, texture_depth_spin_, dual_brush_size_spin_,
        dual_brush_hardness_spin_, dual_brush_spacing_spin_,
        foreground_background_jitter_spin_, color_fade_steps_spin_, hue_jitter_spin_,
        saturation_jitter_spin_, brightness_jitter_spin_, purity_spin_}) {
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, emit_edited);
  }
  for (auto* combo : {angle_control_combo_, size_control_combo_, roundness_control_combo_,
                      scatter_control_combo_, count_control_combo_, opacity_control_combo_,
                      flow_control_combo_, texture_style_combo_, color_control_combo_}) {
    connect(combo, &QComboBox::currentIndexChanged, this, emit_edited);
  }
  for (auto* check : {flip_x_check_, flip_y_check_, both_axes_check_, texture_enabled_check_,
                      texture_invert_check_, dual_brush_enabled_check_,
                      color_dynamics_enabled_check_, color_per_tip_check_, noise_check_, wet_edges_check_}) {
    connect(check, &QCheckBox::toggled, this, emit_edited);
  }
  refresh_control_dependent_widgets();

  // Keep - / + buttons on the spin boxes (see the sub-control gotcha in dialog_utils), applied
  // after all children exist, and pair every "Label", slider, spin row for scrubbing (GitHub
  // issue 46): neither the popup host nor the dock runs the exec_dialog hook. Per page, since
  // Presentation::Pages moves them out of this widget.
  for (auto& [section, page] : pages_) {
    set_themed_style(*page, dialog_spinbox_button_style());
    install_scrub_labels_in(page);
  }
}

QWidget* BrushDynamicsPanel::section_page(BrushSection section) const {
  const auto found = pages_.find(section);
  return found != pages_.end() ? found->second : nullptr;
}

void BrushDynamicsPanel::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    // Pages placed elsewhere are not children any more; walk them explicitly.
    for (auto& [section, page] : pages_) {
      for (auto* child : page->findChildren<QObject*>()) {
        apply_bound_translation(child);
      }
    }
    for (auto* child : findChildren<QObject*>()) {
      apply_bound_translation(child);
    }
    retranslate_combos();
  }
}

void BrushDynamicsPanel::retranslate_combos() {
  for (auto* combo : {size_control_combo_, roundness_control_combo_, scatter_control_combo_,
                      count_control_combo_, opacity_control_combo_, flow_control_combo_,
                      color_control_combo_}) {
    const QSignalBlocker blocker(combo);
    for (int index = 0; index < combo->count(); ++index) {
      for (const auto& item : kControlItems) {
        if (combo->itemData(index).toInt() == static_cast<int>(item.control)) {
          combo->setItemText(index, panel_tr(item.source));
        }
      }
    }
  }
  {
    const QSignalBlocker blocker(angle_control_combo_);
    for (int index = 0; index < angle_control_combo_->count(); ++index) {
      angle_control_combo_->setItemText(index, panel_tr(kAngleControlItems[index]));
    }
  }
  const QSignalBlocker blocker(texture_style_combo_);
  for (int index = 0; index < texture_style_combo_->count(); ++index) {
    for (const auto& item : kTextureItems) {
      if (texture_style_combo_->itemData(index).toInt() == static_cast<int>(item.style)) {
        texture_style_combo_->setItemText(index, panel_tr(item.source));
      }
    }
  }
}

void BrushDynamicsPanel::set_values(const patchy::BrushDynamics& dynamics, double base_angle_degrees,
                                    double base_roundness) {
  loading_ = true;
  base_angle_spin_->setValue(normalized_angle_value(base_angle_degrees));
  base_roundness_spin_->setValue(
      std::clamp(static_cast<int>(std::lround(base_roundness)), 1, 100));
  angle_roundness_widget_->set_values(base_angle_spin_->value(), base_roundness_spin_->value());
  size_jitter_spin_->setValue(percent_from_fraction(dynamics.size_jitter));
  minimum_diameter_spin_->setValue(percent_from_fraction(dynamics.minimum_diameter));
  select_combo_control(*size_control_combo_, dynamics.size_control);
  size_fade_steps_spin_->setValue(std::clamp(dynamics.size_fade_steps, 1, 9999));
  angle_jitter_spin_->setValue(percent_from_fraction(dynamics.angle_jitter));
  angle_control_combo_->setCurrentIndex(static_cast<int>(dynamics.angle_control));
  fade_steps_spin_->setValue(std::clamp(dynamics.angle_fade_steps, 1, 9999));
  roundness_jitter_spin_->setValue(percent_from_fraction(dynamics.roundness_jitter));
  minimum_roundness_spin_->setValue(percent_from_fraction(dynamics.minimum_roundness));
  select_combo_control(*roundness_control_combo_, dynamics.roundness_control);
  roundness_fade_steps_spin_->setValue(std::clamp(dynamics.roundness_fade_steps, 1, 9999));
  flip_x_check_->setChecked(dynamics.flip_x_jitter);
  flip_y_check_->setChecked(dynamics.flip_y_jitter);
  scatter_spin_->setValue(percent_from_fraction(dynamics.scatter));
  select_combo_control(*scatter_control_combo_, dynamics.scatter_control);
  scatter_fade_steps_spin_->setValue(std::clamp(dynamics.scatter_fade_steps, 1, 9999));
  both_axes_check_->setChecked(dynamics.scatter_both_axes);
  count_spin_->setValue(std::clamp(dynamics.count, 1, 16));
  count_jitter_spin_->setValue(percent_from_fraction(dynamics.count_jitter));
  select_combo_control(*count_control_combo_, dynamics.count_control);
  count_fade_steps_spin_->setValue(std::clamp(dynamics.count_fade_steps, 1, 9999));
  opacity_jitter_spin_->setValue(percent_from_fraction(dynamics.opacity_jitter));
  minimum_opacity_spin_->setValue(percent_from_fraction(dynamics.minimum_opacity));
  select_combo_control(*opacity_control_combo_, dynamics.opacity_control);
  opacity_fade_steps_spin_->setValue(std::clamp(dynamics.opacity_fade_steps, 1, 9999));
  flow_jitter_spin_->setValue(percent_from_fraction(dynamics.flow_jitter));
  minimum_flow_spin_->setValue(percent_from_fraction(dynamics.minimum_flow));
  select_combo_control(*flow_control_combo_, dynamics.flow_control);
  flow_fade_steps_spin_->setValue(std::clamp(dynamics.flow_fade_steps, 1, 9999));
  texture_enabled_check_->setChecked(dynamics.texture_enabled);
  select_combo_control(*color_control_combo_, dynamics.color_control);
  texture_style_combo_->setCurrentIndex(std::max(
      0, texture_style_combo_->findData(static_cast<int>(dynamics.texture_style))));
  texture_scale_spin_->setValue(percent_from_fraction(dynamics.texture_scale));
  texture_depth_spin_->setValue(percent_from_fraction(dynamics.texture_depth));
  texture_invert_check_->setChecked(dynamics.texture_invert);
  loaded_ = dynamics;
  dual_brush_enabled_check_->setChecked(dynamics.dual_brush_enabled);
  dual_brush_size_spin_->setValue(percent_from_fraction(dynamics.dual_brush_size));
  dual_brush_hardness_spin_->setValue(percent_from_fraction(dynamics.dual_brush_hardness));
  dual_brush_spacing_spin_->setValue(percent_from_fraction(dynamics.dual_brush_spacing));
  color_dynamics_enabled_check_->setChecked(dynamics.color_dynamics_enabled);
  foreground_background_jitter_spin_->setValue(
      percent_from_fraction(dynamics.foreground_background_jitter));
  color_fade_steps_spin_->setValue(std::clamp(dynamics.color_fade_steps, 1, 9999));
  hue_jitter_spin_->setValue(percent_from_fraction(dynamics.hue_jitter));
  saturation_jitter_spin_->setValue(percent_from_fraction(dynamics.saturation_jitter));
  brightness_jitter_spin_->setValue(percent_from_fraction(dynamics.brightness_jitter));
  purity_spin_->setValue(static_cast<int>(std::lround(dynamics.purity * 100.0)));
  color_per_tip_check_->setChecked(dynamics.color_per_tip);
  noise_check_->setChecked(dynamics.noise);
  wet_edges_check_->setChecked(dynamics.wet_edges);
  refresh_control_dependent_widgets();
  loading_ = false;
}

patchy::BrushDynamics BrushDynamicsPanel::dynamics() const {
  auto dynamics = loaded_;  // only the fields this form edits are overwritten below
  dynamics.size_jitter = fraction_from_percent(size_jitter_spin_->value());
  dynamics.minimum_diameter = fraction_from_percent(minimum_diameter_spin_->value());
  dynamics.size_control = combo_control(*size_control_combo_);
  dynamics.size_fade_steps = size_fade_steps_spin_->value();
  dynamics.angle_jitter = fraction_from_percent(angle_jitter_spin_->value());
  dynamics.angle_control = static_cast<patchy::BrushDynamicControl>(
      std::clamp(angle_control_combo_->currentIndex(), 0,
                 static_cast<int>(patchy::BrushDynamicControl::StylusWheel)));
  dynamics.angle_fade_steps = fade_steps_spin_->value();
  dynamics.roundness_jitter = fraction_from_percent(roundness_jitter_spin_->value());
  dynamics.minimum_roundness = fraction_from_percent(minimum_roundness_spin_->value());
  dynamics.roundness_control = combo_control(*roundness_control_combo_);
  dynamics.roundness_fade_steps = roundness_fade_steps_spin_->value();
  dynamics.flip_x_jitter = flip_x_check_->isChecked();
  dynamics.flip_y_jitter = flip_y_check_->isChecked();
  dynamics.scatter = fraction_from_percent(scatter_spin_->value());
  dynamics.scatter_both_axes = both_axes_check_->isChecked();
  dynamics.scatter_control = combo_control(*scatter_control_combo_);
  dynamics.scatter_fade_steps = scatter_fade_steps_spin_->value();
  dynamics.count = count_spin_->value();
  dynamics.count_jitter = fraction_from_percent(count_jitter_spin_->value());
  dynamics.count_control = combo_control(*count_control_combo_);
  dynamics.count_fade_steps = count_fade_steps_spin_->value();
  dynamics.opacity_jitter = fraction_from_percent(opacity_jitter_spin_->value());
  dynamics.minimum_opacity = fraction_from_percent(minimum_opacity_spin_->value());
  dynamics.opacity_control = combo_control(*opacity_control_combo_);
  dynamics.opacity_fade_steps = opacity_fade_steps_spin_->value();
  dynamics.flow_jitter = fraction_from_percent(flow_jitter_spin_->value());
  dynamics.minimum_flow = fraction_from_percent(minimum_flow_spin_->value());
  dynamics.flow_control = combo_control(*flow_control_combo_);
  dynamics.flow_fade_steps = flow_fade_steps_spin_->value();
  dynamics.texture_enabled = texture_enabled_check_->isChecked();
  dynamics.texture_style = static_cast<patchy::BrushTextureStyle>(
      texture_style_combo_->currentData().toInt());
  dynamics.texture_scale = fraction_from_percent(texture_scale_spin_->value());
  dynamics.texture_depth = fraction_from_percent(texture_depth_spin_->value());
  dynamics.texture_invert = texture_invert_check_->isChecked();
  dynamics.dual_brush_enabled = dual_brush_enabled_check_->isChecked();
  dynamics.dual_brush_size = fraction_from_percent(dual_brush_size_spin_->value());
  dynamics.dual_brush_hardness = fraction_from_percent(dual_brush_hardness_spin_->value());
  dynamics.dual_brush_spacing = fraction_from_percent(dual_brush_spacing_spin_->value());
  dynamics.color_dynamics_enabled = color_dynamics_enabled_check_->isChecked();
  dynamics.foreground_background_jitter =
      fraction_from_percent(foreground_background_jitter_spin_->value());
  dynamics.color_control = combo_control(*color_control_combo_);
  dynamics.color_fade_steps = color_fade_steps_spin_->value();
  dynamics.hue_jitter = fraction_from_percent(hue_jitter_spin_->value());
  dynamics.saturation_jitter = fraction_from_percent(saturation_jitter_spin_->value());
  dynamics.brightness_jitter = fraction_from_percent(brightness_jitter_spin_->value());
  dynamics.purity = fraction_from_percent(purity_spin_->value());
  dynamics.color_per_tip = color_per_tip_check_->isChecked();
  dynamics.noise = noise_check_->isChecked();
  dynamics.wet_edges = wet_edges_check_->isChecked();
  return dynamics;
}

void BrushDynamicsPanel::refresh_control_dependent_widgets() {
  fade_steps_spin_->setVisible(angle_control_combo_->currentIndex() ==
                               static_cast<int>(patchy::BrushDynamicControl::Fade));
  const std::pair<QComboBox*, QSpinBox*> control_rows[] = {
      {size_control_combo_, size_fade_steps_spin_},
      {roundness_control_combo_, roundness_fade_steps_spin_},
      {scatter_control_combo_, scatter_fade_steps_spin_},
      {count_control_combo_, count_fade_steps_spin_},
      {opacity_control_combo_, opacity_fade_steps_spin_},
      {flow_control_combo_, flow_fade_steps_spin_},
      {color_control_combo_, color_fade_steps_spin_},
  };
  for (const auto& [combo, fade_spin] : control_rows) {
    fade_spin->setVisible(combo_control(*combo) == patchy::BrushDynamicControl::Fade);
  }
  // The Minimum Opacity floor only participates while the opacity control has a real source.
  const auto minimum_opacity_live = control_has_source(combo_control(*opacity_control_combo_));
  minimum_opacity_spin_->setEnabled(minimum_opacity_live);
  minimum_opacity_slider_->setEnabled(minimum_opacity_live);
  const auto minimum_flow_live = control_has_source(combo_control(*flow_control_combo_));
  minimum_flow_spin_->setEnabled(minimum_flow_live);
  minimum_flow_slider_->setEnabled(minimum_flow_live);
  // A disabled effect greys its rows; the page itself keeps the enable box live.
  const std::pair<BrushSection, QCheckBox*> effects[] = {
      {BrushSection::Texture, texture_enabled_check_},
      {BrushSection::DualBrush, dual_brush_enabled_check_},
      {BrushSection::ColorDynamics, color_dynamics_enabled_check_},
  };
  for (const auto& [section, check] : effects) {
    for (auto* child : pages_.at(section)->findChildren<QWidget*>()) {
      if (child != check) {
        child->setEnabled(check->isChecked());
      }
    }
  }
}

double BrushDynamicsPanel::base_angle_degrees() const {
  return base_angle_spin_->value();
}

double BrushDynamicsPanel::base_roundness() const {
  return base_roundness_spin_->value();
}

void BrushDynamicsPanel::reset_to_defaults() {
  set_values({}, 0.0, 100.0);
  emit edited();
}

BrushDynamicsButton::BrushDynamicsButton(QWidget* parent) : QToolButton(parent) {
  setObjectName(QStringLiteral("brushDynamicsButton"));
  setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(this, &QToolButton::clicked, this, &BrushDynamicsButton::show_settings_requested);
  retranslate();
  setEnabled(false);
}

void BrushDynamicsButton::retranslate() {
  setText(tr("Dynamics"));
  setToolTip(tr("Brush dynamics and effects for the current brush: shows the Brush Settings panel"));
}

void BrushDynamicsButton::set_working_brush(const QString& tip_key,
                                            const patchy::BrushDynamics& dynamics,
                                            double base_angle_degrees, double base_roundness) {
  tip_id_ = tip_key;
  setEnabled(!tip_id_.isEmpty());
  // Keyed on non-default (not active()): a brush whose only customization is a control of Off
  // (ignore the pen) never runs the per-dab path but is still a deliberate setup worth showing.
  const auto active = !tip_id_.isEmpty() &&
                      (!brush_dynamics_is_default(dynamics) || base_angle_degrees != 0.0 ||
                       base_roundness != 100.0);
  if (property("dynamicsActive").toBool() == active) {
    return;
  }
  setProperty("dynamicsActive", active);
  style()->unpolish(this);
  style()->polish(this);
}

}  // namespace patchy::ui
