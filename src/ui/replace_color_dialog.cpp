#include "ui/replace_color_dialog.hpp"

#include "ui/app_settings.hpp"
#include "ui/coalesced_preview_emitter.hpp"
#include "ui/color_range_dialog.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/theme_palette.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace patchy::ui {

namespace {

// Persisted dialog setting (new key, October 2026; a compatibility contract).
const QString kReplaceColorFuzzinessKey = QStringLiteral("tools/replaceColorFuzziness");

// The Color and Result swatches: a document color, so painted as is; an empty swatch
// shows the canvas backdrop.
class ColorSwatch final : public QWidget {
public:
  explicit ColorSwatch(QWidget* parent) : QWidget(parent) { setFixedSize(40, 24); }

  void set_color(QColor color) {
    color_ = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(rect(), color_.isValid() ? color_ : theme().canvas_backdrop);
    painter.setPen(theme().layer_thumbnail_border);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }

private:
  QColor color_;
};

struct ReplaceColorPreviewRequest {
  bool enabled{true};
  ReplaceColorSettings settings;
};

QColor to_qcolor(RgbColor color) {
  return QColor(color.red, color.green, color.blue);
}

}  // namespace

std::optional<ReplaceColorSettings> request_replace_color(QWidget* parent, const ReplaceColorDialogInput& input,
                                                          const ReplaceColorDialogHooks& hooks) {
  const auto& layer = input.layer;
  if (layer.isNull() || layer.format() != QImage::Format_RGBA8888 || input.document_size.isEmpty()) {
    return std::nullopt;
  }

  ReplaceColorSettings settings;
  settings.range.fuzziness = std::clamp(
      app_settings().value(kReplaceColorFuzzinessKey, kColorRangeDefaultFuzziness).toInt(), 0, kColorRangeMaxFuzziness);
  if (input.initial_sample.isValid()) {
    settings.range.added.push_back(ColorRangeColor{static_cast<std::uint8_t>(input.initial_sample.red()),
                                                   static_cast<std::uint8_t>(input.initial_sample.green()),
                                                   static_cast<std::uint8_t>(input.initial_sample.blue())});
  }

  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("patchyReplaceColorDialog"));
  dialog.setWindowTitle(QObject::tr("Replace Color"));
  auto* root = new QHBoxLayout(&dialog);
  auto* left = new QVBoxLayout();
  root->addLayout(left, 1);

  // Selection: Photoshop's eyedroppers, Color swatch, Fuzziness and preview box.
  auto* selection_group = new QGroupBox(QObject::tr("Selection"), &dialog);
  auto* selection_layout = new QVBoxLayout(selection_group);
  auto* sampler_row = new QHBoxLayout();
  const auto samplers = add_color_range_sampler_buttons(dialog, sampler_row, QStringLiteral("replaceColor"));
  sampler_row->addStretch(1);
  sampler_row->addWidget(new QLabel(QObject::tr("Color:"), &dialog));
  auto* color_swatch = new ColorSwatch(&dialog);
  color_swatch->setObjectName(QStringLiteral("replaceColorSampleSwatch"));
  color_swatch->setAccessibleName(QObject::tr("Sampled color"));
  sampler_row->addWidget(color_swatch);
  selection_layout->addLayout(sampler_row);
  auto* selection_form = new QFormLayout();
  auto* fuzziness_spin = add_dialog_slider_spin_row(selection_form, &dialog, QObject::tr("Fuzziness:"),
                                                    QStringLiteral("replaceColorFuzzinessSlider"),
                                                    QStringLiteral("replaceColorFuzzinessSpin"), 0,
                                                    kColorRangeMaxFuzziness, settings.range.fuzziness);
  selection_layout->addLayout(selection_form);
  auto* preview = new ColorRangePreview(&dialog);
  preview->setObjectName(QStringLiteral("replaceColorPreview"));
  preview->setAccessibleName(QObject::tr("Replace Color preview"));
  selection_layout->addWidget(preview, 0, Qt::AlignHCenter);
  auto* view_row = new QHBoxLayout();
  auto* show_selection = new QRadioButton(QObject::tr("Selection"), &dialog);
  show_selection->setObjectName(QStringLiteral("replaceColorShowSelectionRadio"));
  show_selection->setChecked(true);
  auto* show_image = new QRadioButton(QObject::tr("Image"), &dialog);
  show_image->setObjectName(QStringLiteral("replaceColorShowImageRadio"));
  view_row->addStretch(1);
  view_row->addWidget(show_selection);
  view_row->addWidget(show_image);
  view_row->addStretch(1);
  selection_layout->addLayout(view_row);
  left->addWidget(selection_group);

  // Replacement: Hue/Saturation's master sliders and the Result swatch.
  auto* replacement_group = new QGroupBox(QObject::tr("Replacement"), &dialog);
  auto* replacement_form = new QFormLayout(replacement_group);
  auto* hue_spin = add_dialog_slider_spin_row(replacement_form, &dialog, QObject::tr("Hue"),
                                              QStringLiteral("replaceColorHueSlider"),
                                              QStringLiteral("replaceColorHueSpin"), -kReplaceColorHueRange,
                                              kReplaceColorHueRange, 0);
  auto* saturation_spin = add_dialog_slider_spin_row(
      replacement_form, &dialog, QObject::tr("Saturation"), QStringLiteral("replaceColorSaturationSlider"),
      QStringLiteral("replaceColorSaturationSpin"), -kReplaceColorSaturationRange, kReplaceColorSaturationRange, 0);
  auto* lightness_spin = add_dialog_slider_spin_row(
      replacement_form, &dialog, QObject::tr("Lightness"), QStringLiteral("replaceColorLightnessSlider"),
      QStringLiteral("replaceColorLightnessSpin"), -kReplaceColorLightnessRange, kReplaceColorLightnessRange, 0);
  auto* result_swatch = new ColorSwatch(&dialog);
  result_swatch->setObjectName(QStringLiteral("replaceColorResultSwatch"));
  result_swatch->setAccessibleName(QObject::tr("Result color"));
  replacement_form->addRow(QObject::tr("Result:"), result_swatch);
  left->addWidget(replacement_group);

  auto* right = new QVBoxLayout();
  root->addLayout(right);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Vertical, &dialog);
  right->addWidget(buttons);
  auto* preview_check = new QCheckBox(QObject::tr("Preview"), &dialog);
  preview_check->setObjectName(QStringLiteral("replaceColorPreviewCheck"));
  preview_check->setChecked(true);
  right->addWidget(preview_check);
  right->addStretch(1);

  // The preview box shows the layer at its document position, fitted to the box, and
  // the selection it would affect: the color score, black outside a selection.
  const auto fit = fit_color_range_preview(input.document_size);
  QImage thumb_layer(fit.size, QImage::Format_RGBA8888);
  thumb_layer.fill(Qt::transparent);
  {
    const QSize scaled_size(std::max(1, static_cast<int>(layer.width() * fit.scale + 0.5)),
                            std::max(1, static_cast<int>(layer.height() * fit.scale + 0.5)));
    QPainter painter(&thumb_layer);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QPoint(static_cast<int>(input.layer_origin.x() * fit.scale + 0.5),
                             static_cast<int>(input.layer_origin.y() * fit.scale + 0.5)),
                      layer.scaled(scaled_size, Qt::IgnoreAspectRatio, fit.mode));
  }
  std::vector<std::uint8_t> thumb_selection;
  if (!input.selection.isEmpty()) {
    QImage selection_image(fit.size, QImage::Format_Grayscale8);
    selection_image.fill(0);
    QPainter painter(&selection_image);
    painter.scale(fit.scale, fit.scale);
    for (const auto& rect : input.selection) {
      painter.fillRect(rect, Qt::white);
    }
    painter.end();
    thumb_selection.resize(static_cast<std::size_t>(fit.size.width()) * static_cast<std::size_t>(fit.size.height()));
    for (int y = 0; y < fit.size.height(); ++y) {
      std::copy_n(selection_image.constScanLine(y), fit.size.width(),
                  thumb_selection.begin() + static_cast<std::ptrdiff_t>(y) * fit.size.width());
    }
  }

  const auto current_settings = [&] {
    auto current = settings;
    current.hue = hue_spin->value();
    current.saturation = saturation_spin->value();
    current.lightness = lightness_spin->value();
    return current;
  };
  CoalescedPreviewEmitter<ReplaceColorPreviewRequest> preview_emitter(
      dialog, [&hooks](const ReplaceColorPreviewRequest& request) {
        if (hooks.preview_changed) {
          hooks.preview_changed(request.enabled, request.settings);
        }
      });
  const auto schedule_preview = [&] {
    preview_emitter.schedule(ReplaceColorPreviewRequest{preview_check->isChecked(), current_settings()});
  };
  const auto refresh_result = [&] {
    const auto current = current_settings();
    if (current.range.added.empty()) {
      color_swatch->set_color(QColor());
      result_swatch->set_color(QColor());
      return;
    }
    const auto sample = current.range.added.front();
    const RgbColor color{sample.r, sample.g, sample.b};
    color_swatch->set_color(to_qcolor(color));
    result_swatch->set_color(to_qcolor(replace_color_full_shift(color, current)));
  };
  const auto refresh_selection = [&] {
    if (show_image->isChecked()) {
      preview->set_image(thumb_layer);
      return;
    }
    auto mask = color_range_mask(thumb_layer.constBits(), thumb_layer.width(), thumb_layer.height(),
                                 thumb_layer.bytesPerLine(), settings.range);
    if (!thumb_selection.empty() && mask.size() == thumb_selection.size()) {
      for (std::size_t index = 0; index < mask.size(); ++index) {
        mask[index] = static_cast<std::uint8_t>((mask[index] * thumb_selection[index] + 127) / 255);
      }
    }
    preview->set_image(color_range_gray_image(mask, fit.size.width(), fit.size.height()));
  };

  // Samples are single clicks on the layer's own pixels: each press reads one pixel
  // and the score runs once afterwards. Drags do not sample (docs/replace-color.md).
  const auto sample_at = [&](QPoint document_point, Qt::KeyboardModifiers modifiers) {
    const auto local = document_point - input.layer_origin;
    if (!layer.rect().contains(local)) {
      return;
    }
    const auto* pixel = layer.constScanLine(local.y()) + static_cast<std::ptrdiff_t>(local.x()) * 4;
    if (pixel[3] == 0U) {
      return;
    }
    apply_color_range_sample(settings.range, ColorRangeColor{pixel[0], pixel[1], pixel[2]},
                             samplers.action(modifiers));
    refresh_selection();
    refresh_result();
    schedule_preview();
  };
  const auto canvas_sampler = [&](const CanvasReadGesture& gesture) {
    if (gesture.phase == CanvasReadPhase::Dismiss) {
      dialog.reject();
      return;
    }
    if (gesture.phase == CanvasReadPhase::Press) {
      sample_at(gesture.document_position, gesture.modifiers);
    }
  };
  preview->clicked = [&](QPoint image_point, Qt::KeyboardModifiers modifiers) {
    sample_at(fit.document_point(image_point, input.document_size), modifiers);
  };

  QObject::connect(fuzziness_spin, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int value) {
    settings.range.fuzziness = value;
    refresh_selection();
    schedule_preview();
  });
  for (auto* spin : {hue_spin, saturation_spin, lightness_spin}) {
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int) {
      refresh_result();
      schedule_preview();
    });
  }
  QObject::connect(preview_check, &QCheckBox::toggled, &dialog, [&](bool checked) {
    preview_emitter.flush(ReplaceColorPreviewRequest{checked, current_settings()});
  });
  QObject::connect(show_image, &QRadioButton::toggled, &dialog, [&](bool) { refresh_selection(); });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  const auto clear_sampler = [&hooks] {
    if (hooks.set_canvas_sampler) {
      hooks.set_canvas_sampler({});
    }
  };
  QObject::connect(&dialog, &QDialog::finished, &dialog, clear_sampler);

  if (hooks.set_canvas_sampler) {
    hooks.set_canvas_sampler(canvas_sampler);
  }
  refresh_selection();
  refresh_result();
  const auto result = run_non_modal_dialog(dialog);
  clear_sampler();
  if (result != QDialog::Accepted) {
    return std::nullopt;
  }
  app_settings().setValue(kReplaceColorFuzzinessKey, settings.range.fuzziness);
  return current_settings();
}

}  // namespace patchy::ui
