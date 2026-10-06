#include "ui/color_range_dialog.hpp"

#include "ui/app_settings.hpp"
#include "ui/coalesced_preview_emitter.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/icon_theme.hpp"
#include "ui/modifier_names.hpp"
#include "ui/theme_palette.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace patchy::ui {

namespace {

// Persisted dialog settings (new keys, October 2026; tokens are a compatibility contract).
const QString kColorRangeSelectKey = QStringLiteral("tools/colorRangeSelect");
const QString kColorRangeFuzzinessKey = QStringLiteral("tools/colorRangeFuzziness");
const QString kColorRangeInvertKey = QStringLiteral("tools/colorRangeInvert");
const QString kColorRangeCanvasPreviewKey = QStringLiteral("tools/colorRangeSelectionPreview");

constexpr int kPreviewBox = 240;

struct SelectEntry {
  ColorRangeSelect select;
  const char* token;
};

constexpr std::array kSelectEntries{
    SelectEntry{ColorRangeSelect::SampledColors, "sampledColors"},
    SelectEntry{ColorRangeSelect::Reds, "reds"},
    SelectEntry{ColorRangeSelect::Yellows, "yellows"},
    SelectEntry{ColorRangeSelect::Greens, "greens"},
    SelectEntry{ColorRangeSelect::Cyans, "cyans"},
    SelectEntry{ColorRangeSelect::Blues, "blues"},
    SelectEntry{ColorRangeSelect::Magentas, "magentas"},
    SelectEntry{ColorRangeSelect::Highlights, "highlights"},
    SelectEntry{ColorRangeSelect::Midtones, "midtones"},
    SelectEntry{ColorRangeSelect::Shadows, "shadows"},
};

QString select_label(ColorRangeSelect select) {
  switch (select) {
    case ColorRangeSelect::SampledColors:
      return QObject::tr("Sampled Colors");
    case ColorRangeSelect::Reds:
      return QObject::tr("Reds");
    case ColorRangeSelect::Yellows:
      return QObject::tr("Yellows");
    case ColorRangeSelect::Greens:
      return QObject::tr("Greens");
    case ColorRangeSelect::Cyans:
      return QObject::tr("Cyans");
    case ColorRangeSelect::Blues:
      return QObject::tr("Blues");
    case ColorRangeSelect::Magentas:
      return QObject::tr("Magentas");
    case ColorRangeSelect::Highlights:
      return QObject::tr("Highlights");
    case ColorRangeSelect::Midtones:
      return QObject::tr("Midtones");
    case ColorRangeSelect::Shadows:
      return QObject::tr("Shadows");
  }
  return {};
}

struct CanvasPreviewEntry {
  ColorRangeCanvasPreview mode;
  const char* token;
};

constexpr std::array kCanvasPreviewEntries{
    CanvasPreviewEntry{ColorRangeCanvasPreview::None, "none"},
    CanvasPreviewEntry{ColorRangeCanvasPreview::Grayscale, "grayscale"},
    CanvasPreviewEntry{ColorRangeCanvasPreview::BlackMatte, "blackMatte"},
    CanvasPreviewEntry{ColorRangeCanvasPreview::WhiteMatte, "whiteMatte"},
    CanvasPreviewEntry{ColorRangeCanvasPreview::QuickMask, "quickMask"},
};

QString canvas_preview_label(ColorRangeCanvasPreview mode) {
  switch (mode) {
    case ColorRangeCanvasPreview::None:
      return QObject::tr("None");
    case ColorRangeCanvasPreview::Grayscale:
      return QObject::tr("Grayscale");
    case ColorRangeCanvasPreview::BlackMatte:
      return QObject::tr("Black Matte");
    case ColorRangeCanvasPreview::WhiteMatte:
      return QObject::tr("White Matte");
    case ColorRangeCanvasPreview::QuickMask:
      return QObject::tr("Quick Mask");
  }
  return {};
}

QImage gray_image(const std::vector<std::uint8_t>& mask, int width, int height) {
  if (mask.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) || width <= 0) {
    return {};
  }
  return QImage(mask.data(), width, height, width, QImage::Format_Grayscale8).copy();
}

std::vector<std::uint8_t> gray_bytes(const QImage& image) {
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.height()));
  for (int y = 0; y < image.height(); ++y) {
    std::copy_n(image.constScanLine(y), image.width(),
                bytes.begin() + static_cast<std::ptrdiff_t>(y) * image.width());
  }
  return bytes;
}

// The dialog's preview box: the scaled selection or image, clickable for sampling
// like the canvas.
class ColorRangePreview final : public QWidget {
public:
  explicit ColorRangePreview(QWidget* parent) : QWidget(parent) {
    setFixedSize(kPreviewBox + 2, kPreviewBox + 2);
    setCursor(Qt::CrossCursor);
  }

  std::function<void(QPoint image_point, Qt::KeyboardModifiers modifiers)> clicked;

  void set_image(QImage image) {
    image_ = std::move(image);
    update();
  }
  [[nodiscard]] const QImage& image() const noexcept { return image_; }
  [[nodiscard]] QRect image_rect() const {
    const QSize size = image_.size();
    return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(rect(), theme().canvas_backdrop);
    if (!image_.isNull()) {
      painter.drawImage(image_rect(), image_);
    }
    painter.setPen(theme().layer_thumbnail_border);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton || !clicked) {
      return;
    }
    const auto local = event->position().toPoint() - image_rect().topLeft();
    if (QRect(QPoint(), image_.size()).contains(local)) {
      clicked(local, event->modifiers());
    }
    event->accept();
  }

private:
  QImage image_;
};

enum class SampleAction {
  Replace,
  Add,
  Subtract
};

}  // namespace

QImage color_range_canvas_preview_image(const QImage& composite, const std::vector<std::uint8_t>& selection,
                                        ColorRangeCanvasPreview mode) {
  const int width = composite.width();
  const int height = composite.height();
  if (mode == ColorRangeCanvasPreview::None || composite.format() != QImage::Format_RGBA8888 ||
      selection.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) || width <= 0) {
    return {};
  }
  QImage overlay(width, height, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < height; ++y) {
    const auto* source = composite.constScanLine(y);
    const auto* coverage = selection.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    auto* out = reinterpret_cast<QRgb*>(overlay.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const int value = coverage[x];
      switch (mode) {
        case ColorRangeCanvasPreview::None:
          break;
        case ColorRangeCanvasPreview::Grayscale:
          out[x] = qRgb(value, value, value);
          break;
        case ColorRangeCanvasPreview::BlackMatte:
        case ColorRangeCanvasPreview::WhiteMatte: {
          // The visible pixel's own coverage times the selection, over the matte.
          const auto* pixel = source + static_cast<std::ptrdiff_t>(x) * 4;
          const int weight = (value * pixel[3] + 127) / 255;
          const int matte = mode == ColorRangeCanvasPreview::WhiteMatte ? 255 * (255 - weight) : 0;
          out[x] = qRgb((pixel[0] * weight + matte + 127) / 255, (pixel[1] * weight + matte + 127) / 255,
                        (pixel[2] * weight + matte + 127) / 255);
          break;
        }
        case ColorRangeCanvasPreview::QuickMask: {
          // Photoshop's default Quick Mask: 50% red over what is not selected.
          const int alpha = (255 - value) / 2;
          out[x] = qRgba(alpha, 0, 0, alpha);
          break;
        }
      }
    }
  }
  return overlay;
}

std::optional<ColorRangeDialogResult> request_color_range(QWidget* parent, const ColorRangeDialogInput& input,
                                                          const ColorRangeDialogHooks& hooks) {
  const auto& composite = input.composite;
  if (composite.isNull() || composite.format() != QImage::Format_RGBA8888) {
    return std::nullopt;
  }
  const int document_width = composite.width();
  const int document_height = composite.height();
  const bool has_base = !input.base_selection.isNull();

  ColorRangeParams params;
  auto canvas_preview_mode = ColorRangeCanvasPreview::None;
  {
    auto settings = app_settings();
    const auto select_token = settings.value(kColorRangeSelectKey).toString();
    for (const auto& entry : kSelectEntries) {
      if (select_token == QLatin1String(entry.token)) {
        params.select = entry.select;
      }
    }
    params.fuzziness = std::clamp(settings.value(kColorRangeFuzzinessKey, kColorRangeDefaultFuzziness).toInt(), 0,
                                  kColorRangeMaxFuzziness);
    params.invert = settings.value(kColorRangeInvertKey, false).toBool();
    const auto preview_token = settings.value(kColorRangeCanvasPreviewKey).toString();
    for (const auto& entry : kCanvasPreviewEntries) {
      if (preview_token == QLatin1String(entry.token)) {
        canvas_preview_mode = entry.mode;
      }
    }
  }
  if (input.initial_sample.isValid()) {
    params.added.push_back(ColorRangeColor{static_cast<std::uint8_t>(input.initial_sample.red()),
                                           static_cast<std::uint8_t>(input.initial_sample.green()),
                                           static_cast<std::uint8_t>(input.initial_sample.blue())});
  }

  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("patchyColorRangeDialog"));
  dialog.setWindowTitle(QObject::tr("Color Range"));
  auto* root = new QHBoxLayout(&dialog);
  auto* left = new QVBoxLayout();
  root->addLayout(left, 1);

  auto* form = new QFormLayout();
  auto* select_combo = new QComboBox(&dialog);
  select_combo->setObjectName(QStringLiteral("colorRangeSelectCombo"));
  for (const auto& entry : kSelectEntries) {
    select_combo->addItem(select_label(entry.select), QString::fromLatin1(entry.token));
    if (entry.select == ColorRangeSelect::Magentas) {
      select_combo->insertSeparator(select_combo->count());
    }
  }
  select_combo->setCurrentIndex(std::max(0, select_combo->findData(QString::fromLatin1(
                                                kSelectEntries[static_cast<std::size_t>(params.select)].token))));
  form->addRow(QObject::tr("Select:"), select_combo);
  auto* fuzziness_spin = add_dialog_slider_spin_row(form, &dialog, QObject::tr("Fuzziness:"),
                                                    QStringLiteral("colorRangeFuzzinessSlider"),
                                                    QStringLiteral("colorRangeFuzzinessSpin"), 0,
                                                    kColorRangeMaxFuzziness, params.fuzziness);
  left->addLayout(form);

  auto* preview = new ColorRangePreview(&dialog);
  preview->setObjectName(QStringLiteral("colorRangePreview"));
  preview->setAccessibleName(QObject::tr("Color Range preview"));
  left->addWidget(preview, 0, Qt::AlignHCenter);

  auto* view_row = new QHBoxLayout();
  auto* show_selection = new QRadioButton(QObject::tr("Selection"), &dialog);
  show_selection->setObjectName(QStringLiteral("colorRangeShowSelectionRadio"));
  show_selection->setChecked(true);
  auto* show_image = new QRadioButton(QObject::tr("Image"), &dialog);
  show_image->setObjectName(QStringLiteral("colorRangeShowImageRadio"));
  view_row->addStretch(1);
  view_row->addWidget(show_selection);
  view_row->addWidget(show_image);
  view_row->addStretch(1);
  left->addLayout(view_row);

  auto* preview_form = new QFormLayout();
  auto* canvas_preview_combo = new QComboBox(&dialog);
  canvas_preview_combo->setObjectName(QStringLiteral("colorRangeSelectionPreviewCombo"));
  for (const auto& entry : kCanvasPreviewEntries) {
    canvas_preview_combo->addItem(canvas_preview_label(entry.mode), QString::fromLatin1(entry.token));
  }
  canvas_preview_combo->setCurrentIndex(static_cast<int>(canvas_preview_mode));
  preview_form->addRow(QObject::tr("Selection Preview:"), canvas_preview_combo);
  left->addLayout(preview_form);

  auto* right = new QVBoxLayout();
  root->addLayout(right);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Vertical, &dialog);
  right->addWidget(buttons);
  right->addSpacing(12);
  auto* sampler_row = new QHBoxLayout();
  const auto make_sampler_button = [&dialog, sampler_row](const QString& object_name, const QString& text,
                                                          const QString& tooltip) {
    auto* button = new QToolButton(&dialog);
    button->setObjectName(object_name);
    button->setCheckable(true);
    button->setIcon(themed_svg_icon(QStringLiteral("tool-eyedropper")));
    button->setText(text);
    button->setToolButtonStyle(text.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    sampler_row->addWidget(button);
    return button;
  };
  auto* sample_button = make_sampler_button(QStringLiteral("colorRangeEyedropperButton"), QString(),
                                            QObject::tr("Eyedropper: click the image to sample a color"));
  auto* add_button = make_sampler_button(QStringLiteral("colorRangeAddSampleButton"), QStringLiteral("+"),
                                         QObject::tr("Add to Sample (Shift-click)"));
  auto* subtract_button =
      make_sampler_button(QStringLiteral("colorRangeSubtractSampleButton"), QStringLiteral("-"),
                          resolve_modifier_names(QObject::tr("Subtract from Sample (%ALT%-click)")));
  sample_button->setChecked(true);
  auto* sampler_group = new QButtonGroup(&dialog);
  sampler_group->setExclusive(true);
  sampler_group->addButton(sample_button, static_cast<int>(SampleAction::Replace));
  sampler_group->addButton(add_button, static_cast<int>(SampleAction::Add));
  sampler_group->addButton(subtract_button, static_cast<int>(SampleAction::Subtract));
  right->addLayout(sampler_row);
  auto* invert_check = new QCheckBox(QObject::tr("Invert", "Color Range: invert the selection"), &dialog);
  invert_check->setObjectName(QStringLiteral("colorRangeInvertCheck"));
  invert_check->setChecked(params.invert);
  right->addWidget(invert_check);
  right->addStretch(1);

  // The preview box works on a copy fitted to it (small documents scale up with
  // whole pixels); OK and the canvas preview score the full image.
  const auto scale = static_cast<double>(kPreviewBox) / std::max(document_width, document_height);
  const auto scaling = scale > 1.0 ? Qt::FastTransformation : Qt::SmoothTransformation;
  const QSize thumb_size(std::clamp(static_cast<int>(document_width * scale + 0.5), 1, kPreviewBox),
                         std::clamp(static_cast<int>(document_height * scale + 0.5), 1, kPreviewBox));
  const auto thumb_composite =
      composite.scaled(thumb_size, Qt::IgnoreAspectRatio, scaling).convertToFormat(QImage::Format_RGBA8888);
  const auto thumb_base =
      has_base ? gray_bytes(input.base_selection.scaled(thumb_size, Qt::IgnoreAspectRatio, scaling)
                                .convertToFormat(QImage::Format_Grayscale8))
               : std::vector<std::uint8_t>{};
  const auto full_base = has_base ? gray_bytes(input.base_selection) : std::vector<std::uint8_t>{};

  const auto final_selection = [&](const QImage& image, const std::vector<std::uint8_t>& base) {
    auto candidate = color_range_mask(image.constBits(), image.width(), image.height(), image.bytesPerLine(), params);
    auto combined = has_base ? base : std::vector<std::uint8_t>(candidate.size(), 0U);
    combine_color_range_mask(combined, candidate, input.combine, !has_base);
    return combined;
  };

  CoalescedPreviewEmitter<ColorRangeCanvasPreview> canvas_emitter(dialog, [&](ColorRangeCanvasPreview mode) {
    if (!hooks.set_canvas_overlay) {
      return;
    }
    if (mode == ColorRangeCanvasPreview::None) {
      hooks.set_canvas_overlay(QImage());
      return;
    }
    hooks.set_canvas_overlay(color_range_canvas_preview_image(composite, final_selection(composite, full_base), mode));
  });

  const auto refresh = [&] {
    const bool sampled = params.select == ColorRangeSelect::SampledColors;
    fuzziness_spin->parentWidget()->setEnabled(sampled);
    if (auto* label = form->labelForField(fuzziness_spin->parentWidget()); label != nullptr) {
      label->setEnabled(sampled);
    }
    for (auto* button : {sample_button, add_button, subtract_button}) {
      button->setEnabled(sampled);
    }
    if (show_image->isChecked()) {
      preview->set_image(thumb_composite);
    } else {
      preview->set_image(gray_image(final_selection(thumb_composite, thumb_base), thumb_size.width(),
                                    thumb_size.height()));
    }
    canvas_emitter.schedule(canvas_preview_mode);
  };

  const auto sample_color = [&](QColor color, Qt::KeyboardModifiers modifiers) {
    if (params.select != ColorRangeSelect::SampledColors || !color.isValid() || color.alpha() == 0) {
      return;
    }
    // Shift adds and Alt subtracts with any of the three eyedroppers, as in Photoshop.
    auto action = static_cast<SampleAction>(std::max(0, sampler_group->checkedId()));
    if ((modifiers & Qt::ShiftModifier) != 0) {
      action = SampleAction::Add;
    } else if ((modifiers & Qt::AltModifier) != 0) {
      action = SampleAction::Subtract;
    }
    const ColorRangeColor sample{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                                 static_cast<std::uint8_t>(color.blue())};
    switch (action) {
      case SampleAction::Replace:
        params.added = {sample};
        params.subtracted.clear();
        break;
      case SampleAction::Add:
        params.added.push_back(sample);
        std::erase(params.subtracted, sample);
        break;
      case SampleAction::Subtract:
        params.subtracted.push_back(sample);
        std::erase(params.added, sample);
        break;
    }
    refresh();
  };

  // Samples are single clicks: each press reads one pixel and the selection is scored
  // once afterwards. Drags do not sample (see docs/color-range.md, US 8050498).
  const auto canvas_sampler = [&](const CanvasReadGesture& gesture) {
    if (gesture.phase == CanvasReadPhase::Dismiss) {
      dialog.reject();
      return;
    }
    if (gesture.phase != CanvasReadPhase::Press ||
        !QRect(0, 0, document_width, document_height).contains(gesture.document_position)) {
      return;
    }
    sample_color(composite.pixelColor(gesture.document_position), gesture.modifiers);
  };
  const auto sync_canvas_sampler = [&] {
    if (!hooks.set_canvas_sampler) {
      return;
    }
    if (params.select == ColorRangeSelect::SampledColors) {
      hooks.set_canvas_sampler(canvas_sampler);
    } else {
      hooks.set_canvas_sampler({});
    }
  };

  preview->clicked = [&](QPoint image_point, Qt::KeyboardModifiers modifiers) {
    const QPoint document_point(
        std::clamp(static_cast<int>(image_point.x() / scale), 0, document_width - 1),
        std::clamp(static_cast<int>(image_point.y() / scale), 0, document_height - 1));
    sample_color(composite.pixelColor(document_point), modifiers);
  };
  QObject::connect(select_combo, &QComboBox::currentIndexChanged, &dialog, [&](int) {
    const auto token = select_combo->currentData().toString();
    for (const auto& entry : kSelectEntries) {
      if (token == QLatin1String(entry.token)) {
        params.select = entry.select;
      }
    }
    sync_canvas_sampler();
    refresh();
  });
  QObject::connect(fuzziness_spin, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int value) {
    params.fuzziness = value;
    refresh();
  });
  QObject::connect(invert_check, &QCheckBox::toggled, &dialog, [&](bool checked) {
    params.invert = checked;
    refresh();
  });
  QObject::connect(show_image, &QRadioButton::toggled, &dialog, [&](bool) { refresh(); });
  QObject::connect(canvas_preview_combo, &QComboBox::currentIndexChanged, &dialog, [&](int index) {
    const auto entry = std::clamp(index, 0, static_cast<int>(kCanvasPreviewEntries.size()) - 1);
    canvas_preview_mode = kCanvasPreviewEntries[static_cast<std::size_t>(entry)].mode;
    canvas_emitter.flush(canvas_preview_mode);
  });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  const auto clear_canvas = [&hooks] {
    if (hooks.set_canvas_sampler) {
      hooks.set_canvas_sampler({});
    }
    if (hooks.set_canvas_overlay) {
      hooks.set_canvas_overlay(QImage());
    }
  };
  QObject::connect(&dialog, &QDialog::finished, &dialog, clear_canvas);

  sync_canvas_sampler();
  refresh();
  const auto result = run_non_modal_dialog(dialog);
  clear_canvas();
  if (result != QDialog::Accepted) {
    return std::nullopt;
  }

  {
    auto settings = app_settings();
    settings.setValue(kColorRangeSelectKey,
                      QString::fromLatin1(kSelectEntries[static_cast<std::size_t>(params.select)].token));
    settings.setValue(kColorRangeFuzzinessKey, params.fuzziness);
    settings.setValue(kColorRangeInvertKey, params.invert);
    settings.setValue(kColorRangeCanvasPreviewKey,
                      QString::fromLatin1(kCanvasPreviewEntries[static_cast<std::size_t>(canvas_preview_mode)].token));
  }
  ColorRangeDialogResult chosen;
  chosen.selection = final_selection(composite, full_base);
  return chosen;
}

}  // namespace patchy::ui
