#include "ui/color_dock_panel.hpp"

#include "ui/app_settings.hpp"
#include "ui/color_panel.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/theme_palette.hpp"

#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace patchy::ui {

namespace {

constexpr int kSwatchSide = 30;
constexpr int kSwatchOffset = 16;
constexpr int kDockWheelMinimumSize = 110;
constexpr int kDockPlaneMinimumSize = 60;

struct ModeEntry {
  ColorDockPanel::Mode mode;
  const char* token;
  const char* label;
  int page;  // index into pages_; the three slider modes share one page
  bool hsv_rows;
  bool rgb_rows;
};

// Combo order and persisted tokens. Tokens are permanent settings values.
constexpr std::array<ModeEntry, 6> kModeEntries{{
    {ColorDockPanel::Mode::HsvSquare, "square", QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "HSV Square"), 0,
     false, false},
    {ColorDockPanel::Mode::WheelSquare, "wheel", QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "Wheel + Square"), 1,
     false, false},
    {ColorDockPanel::Mode::WheelTriangle, "triangle",
     QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "Wheel + Triangle"), 2, false, false},
    {ColorDockPanel::Mode::HsvSliders, "hsv_sliders", QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "HSV Sliders"),
     3, true, false},
    {ColorDockPanel::Mode::RgbSliders, "rgb_sliders", QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "RGB Sliders"),
     3, false, true},
    {ColorDockPanel::Mode::Sliders, "sliders", QT_TRANSLATE_NOOP("patchy::ui::ColorDockPanel", "HSV + RGB Sliders"),
     3, true, true},
}};

int mode_index(ColorDockPanel::Mode mode) {
  for (std::size_t index = 0; index < kModeEntries.size(); ++index) {
    if (kModeEntries[index].mode == mode) {
      return static_cast<int>(index);
    }
  }
  return 0;
}

}  // namespace

// Photoshop-style overlapping foreground/background squares. The one the panel
// edits is drawn on top with the accent outline; clicking the other switches.
class ColorDockSwatches final : public QWidget {
public:
  ColorDockSwatches(ColorDockPanel& panel, std::function<void(bool)> choose_background, QWidget* parent)
      : QWidget(parent), panel_(panel), choose_background_(std::move(choose_background)) {
    setObjectName(QStringLiteral("colorDockSwatches"));
    setFixedSize(kSwatchSide + kSwatchOffset, kSwatchSide + kSwatchOffset);
    setCursor(Qt::PointingHandCursor);
  }

  [[nodiscard]] static QRect foreground_rect() { return {0, 0, kSwatchSide, kSwatchSide}; }
  [[nodiscard]] static QRect background_rect() {
    return {kSwatchOffset, kSwatchOffset, kSwatchSide, kSwatchSide};
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    const bool background_active = panel_.editing_background();
    const auto paint_swatch = [&painter](QRect rect, QColor color, bool active) {
      if (active) {
        // Accent ring, then a background gap, so the ring reads even when
        // the color itself is close to the accent.
        painter.fillRect(rect, theme().accent_bright);
        painter.fillRect(rect.adjusted(2, 2, -2, -2), theme().window_bg);
        painter.fillRect(rect.adjusted(3, 3, -3, -3), color);
      } else {
        painter.fillRect(rect, color);
        painter.setPen(QPen(theme().swatch_border, 1));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
      }
    };
    if (background_active) {
      paint_swatch(foreground_rect(), panel_.foreground(), false);
      paint_swatch(background_rect(), panel_.background(), true);
    } else {
      paint_swatch(background_rect(), panel_.background(), false);
      paint_swatch(foreground_rect(), panel_.foreground(), true);
    }
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    const auto position = event->position().toPoint();
    // The active swatch is on top, so it wins where the two overlap.
    const bool background_active = panel_.editing_background();
    const auto top = background_active ? background_rect() : foreground_rect();
    const auto under = background_active ? foreground_rect() : background_rect();
    if (!top.contains(position) && under.contains(position)) {
      choose_background_(!background_active);
    }
    event->accept();
  }

private:
  ColorDockPanel& panel_;
  std::function<void(bool)> choose_background_;
};

ColorDockPanel::ColorDockPanel(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("colorDockPanel"));
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  auto* top_row = new QHBoxLayout();
  top_row->setContentsMargins(0, 0, 0, 0);
  top_row->setSpacing(8);
  swatches_ = new ColorDockSwatches(*this, [this](bool background) { set_editing_background(background); }, this);
  top_row->addWidget(swatches_, 0, Qt::AlignTop);

  auto* controls = new QVBoxLayout();
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(4);
  mode_combo_ = new QComboBox(this);
  mode_combo_->setObjectName(QStringLiteral("colorDockModeCombo"));
  for (const auto& entry : kModeEntries) {
    mode_combo_->addItem(QString(), QLatin1String(entry.token));
  }
  controls->addWidget(mode_combo_);
  hex_edit_ = new QLineEdit(this);
  hex_edit_->setObjectName(QStringLiteral("colorDockHexEdit"));
  select_all_on_focus(*hex_edit_);
  controls->addWidget(hex_edit_);
  top_row->addLayout(controls, 1);
  root->addLayout(top_row);

  // One page per mode; only the current page is visible, so hidden pages never
  // add to the panel's minimum height.
  auto* square_page = new QWidget(this);
  auto* square_layout = new QHBoxLayout(square_page);
  square_layout->setContentsMargins(0, 0, 0, 0);
  square_layout->setSpacing(6);
  auto* plane = new ColorPlaneWidget(*this, square_page);
  plane->setObjectName(QStringLiteral("colorDockPlane"));
  plane->setMinimumSize(kDockPlaneMinimumSize, kDockPlaneMinimumSize);
  plane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* hue_bar = new HueSliderWidget(*this, square_page);
  hue_bar->setObjectName(QStringLiteral("colorDockHueSlider"));
  hue_bar->setMinimumHeight(kDockPlaneMinimumSize);
  hue_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  square_layout->addWidget(plane, 1);
  square_layout->addWidget(hue_bar);

  auto* wheel = new ColorWheelWidget(*this, ColorWheelInner::Square, this);
  wheel->setObjectName(QStringLiteral("colorDockWheel"));
  wheel->setMinimumSize(kDockWheelMinimumSize, kDockWheelMinimumSize);
  auto* triangle = new ColorWheelWidget(*this, ColorWheelInner::Triangle, this);
  triangle->setObjectName(QStringLiteral("colorDockTriangleWheel"));
  triangle->setMinimumSize(kDockWheelMinimumSize, kDockWheelMinimumSize);

  auto* sliders_page = new QWidget(this);
  auto* sliders_layout = new QGridLayout(sliders_page);
  sliders_layout->setContentsMargins(0, 2, 0, 2);
  sliders_layout->setHorizontalSpacing(8);
  sliders_layout->setVerticalSpacing(6);
  sliders_layout->setColumnStretch(1, 1);
  struct ChannelRow {
    ColorChannel channel;
    const char* object_name;
  };
  constexpr std::array<ChannelRow, 6> rows{{
      {ColorChannel::Hue, "colorDockSliderHue"},
      {ColorChannel::Saturation, "colorDockSliderSat"},
      {ColorChannel::Value, "colorDockSliderVal"},
      {ColorChannel::Red, "colorDockSliderRed"},
      {ColorChannel::Green, "colorDockSliderGreen"},
      {ColorChannel::Blue, "colorDockSliderBlue"},
  }};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto* label = new QLabel(sliders_page);
    auto* slider = new ColorChannelSlider(*this, rows[index].channel, QLatin1String(rows[index].object_name),
                                          sliders_page);
    slider->setMinimumWidth(80);
    slider_labels_[index] = label;
    sliders_[index] = slider;
    sliders_layout->addWidget(label, static_cast<int>(index), 0);
    sliders_layout->addWidget(slider, static_cast<int>(index), 1);
  }
  sliders_layout->setRowStretch(static_cast<int>(rows.size()), 1);

  pages_ = {square_page, wheel, triangle, sliders_page};
  for (auto* page : pages_) {
    page->hide();
    root->addWidget(page, 1);
  }

  QObject::connect(mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index >= 0 && index < static_cast<int>(kModeEntries.size())) {
      set_mode(kModeEntries[static_cast<std::size_t>(index)].mode);
    }
  });
  QObject::connect(hex_edit_, &QLineEdit::editingFinished, this, [this] { apply_hex_text(); });

  auto mode = Mode::HsvSquare;
  {
    auto settings = app_settings();
    const auto token = settings.value(QLatin1String(kColorDockModeKey)).toString();
    for (const auto& entry : kModeEntries) {
      if (token == QLatin1String(entry.token)) {
        mode = entry.mode;
      }
    }
  }
  mode_ = mode;
  {
    const QSignalBlocker blocker(mode_combo_);
    mode_combo_->setCurrentIndex(mode_index(mode_));
  }
  show_mode_widgets(mode_index(mode_));
  retranslate();
  set_color(foreground_, ColorChangeNotification::No);
}

void ColorDockPanel::set_colors(QColor foreground, QColor background) {
  foreground_ = normalized_rgb_color(foreground);
  background_ = normalized_rgb_color(background);
  const auto target = editing_background_ ? background_ : foreground_;
  // While the panel's own edit is being applied, the views keep tracking the
  // pointer: a palette-mode snap must not yank the markers mid-drag. The
  // swatch still shows the color the canvas actually took.
  if (target != current_color() && !reporting_edit_) {
    set_color(target, ColorChangeNotification::No);
  } else {
    swatches_->update();
  }
}

void ColorDockPanel::set_editing_background(bool background) {
  if (editing_background_ == background) {
    return;
  }
  editing_background_ = background;
  set_color(background ? background_ : foreground_, ColorChangeNotification::No);
}

void ColorDockPanel::show_mode_widgets(int index) {
  const auto& entry = kModeEntries[static_cast<std::size_t>(index)];
  for (std::size_t row = 0; row < sliders_.size(); ++row) {
    const bool visible = row < 3 ? entry.hsv_rows : entry.rgb_rows;
    slider_labels_[row]->setVisible(visible);
    sliders_[row]->setVisible(visible);
  }
  for (std::size_t page = 0; page < pages_.size(); ++page) {
    pages_[page]->setVisible(static_cast<int>(page) == entry.page);
  }
}

void ColorDockPanel::set_mode(Mode mode) {
  const auto index = mode_index(mode);
  if (mode_combo_->currentIndex() != index) {
    const QSignalBlocker blocker(mode_combo_);
    mode_combo_->setCurrentIndex(index);
  }
  show_mode_widgets(index);
  if (mode_ == mode) {
    return;
  }
  mode_ = mode;
  auto settings = app_settings();
  settings.setValue(QLatin1String(kColorDockModeKey), QLatin1String(kModeEntries[static_cast<std::size_t>(index)].token));
}

void ColorDockPanel::color_model_changed(QColor previous, ColorChangeNotification notification) {
  (editing_background_ ? background_ : foreground_) = current_color();
  refresh_views();
  if (notification == ColorChangeNotification::Yes && current_color() != previous) {
    reporting_edit_ = true;
    emit color_edited(editing_background_, current_color());
    reporting_edit_ = false;
  }
}

void ColorDockPanel::refresh_views() {
  {
    const QSignalBlocker blocker(hex_edit_);
    hex_edit_->setText(current_color().name(QColor::HexRgb).toUpper());
  }
  swatches_->update();
  for (auto* page : pages_) {
    page->update();
  }
  for (auto* slider : sliders_) {
    slider->update();
  }
}

void ColorDockPanel::apply_hex_text() {
  const auto parsed = parse_panel_color(hex_edit_->text());
  if (parsed.isValid()) {
    set_color(parsed, ColorChangeNotification::Yes);
  } else {
    refresh_views();
  }
}

void ColorDockPanel::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    retranslate();
  }
}

void ColorDockPanel::retranslate() {
  {
    const QSignalBlocker blocker(mode_combo_);
    for (std::size_t index = 0; index < kModeEntries.size(); ++index) {
      mode_combo_->setItemText(static_cast<int>(index), tr(kModeEntries[index].label));
    }
  }
  mode_combo_->setToolTip(tr("Choose how the Color panel shows the color"));
  hex_edit_->setToolTip(tr("Hex color: type or paste #RRGGBB"));
  swatches_->setToolTip(tr("Click the foreground or background swatch to choose which color the panel edits"));
  // The popup picker's slider labels, shared so both read the same.
  const std::array<QString, 6> labels{PatchyColorPicker::tr("Hue"),   PatchyColorPicker::tr("Sat"),
                                      PatchyColorPicker::tr("Val"),   PatchyColorPicker::tr("Red"),
                                      PatchyColorPicker::tr("Green"), PatchyColorPicker::tr("Blue")};
  for (std::size_t index = 0; index < labels.size(); ++index) {
    slider_labels_[index]->setText(labels[index]);
  }
}

}  // namespace patchy::ui
