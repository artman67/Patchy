// The Gradient Map dialog shared by Layer > New Adjustment Layer > Gradient
// Map, editing an existing Gradient Map layer, and Image > Adjustments >
// Gradient Map. Declared in ui/filter_workflows.hpp beside the other
// adjustment dialogs. The gradient is edited in place with the shared stop
// editor and replaced from the gradient library's quick picker.

#include "ui/filter_workflows.hpp"

#include "core/blend_math.hpp"
#include "ui/coalesced_preview_emitter.hpp"
#include "ui/color_panel.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "ui/gradient_preset_popup.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/measurement_units.hpp"
#include "ui/theme_qss.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace patchy::ui {

namespace {

constexpr int kResultStripWidth = 256;
constexpr int kResultStripHeight = 18;

// The dialog keeps the model it was given and changes only the fields the
// user touches, so opening an imported layer and pressing OK returns the
// gradient bit-for-bit (the PSD writer then re-emits the original block).
struct GradientMapPreviewRequest {
  bool enabled{true};
  GradientMapSettings settings{};
};

struct GradientMapDialogState {
  GradientMapSettings value;
  int selected_stop{0};
};

GradientMapSettings current_settings(const GradientMapDialogState& state) {
  auto settings = state.value;
  std::stable_sort(settings.gradient.color_stops.begin(), settings.gradient.color_stops.end(),
                   [](const GradientColorStop& lhs, const GradientColorStop& rhs) {
                     return lhs.location < rhs.location;
                   });
  settings.lut.reset();
  return settings;
}

QPixmap gradient_map_result_strip(const GradientMapSettings& settings) {
  const auto lut = build_gradient_map_lut(settings);
  QImage image(kResultStripWidth, kResultStripHeight, QImage::Format_RGB888);
  for (int x = 0; x < kResultStripWidth; ++x) {
    const auto color = lut[static_cast<std::size_t>(x * 255 / (kResultStripWidth - 1))];
    for (int y = 0; y < kResultStripHeight; ++y) {
      auto* px = image.scanLine(y) + static_cast<std::size_t>(x) * 3U;
      px[0] = color.red;
      px[1] = color.green;
      px[2] = color.blue;
    }
  }
  return QPixmap::fromImage(image);
}

RgbColor rgb_from_qcolor(const QColor& color) {
  return RgbColor{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                  static_cast<std::uint8_t>(color.blue())};
}

}  // namespace

std::optional<GradientMapSettings> request_gradient_map_settings(
    QWidget* parent, std::function<void(bool, const GradientMapSettings&)> preview_changed,
    GradientMapSettings initial, GradientLibrary* library, RgbColor foreground, RgbColor background) {
  auto state = std::make_shared<GradientMapDialogState>();
  state->value = std::move(initial);
  state->value.lut.reset();

  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("patchyGradientMapDialog"));
  dialog.setWindowTitle(QObject::tr("Gradient Map"));
  auto* layout = new QVBoxLayout(&dialog);

  auto* heading_row = new QHBoxLayout();
  heading_row->addWidget(new QLabel(QObject::tr("Gradient Used for Grayscale Mapping"), &dialog), 1);
  auto* preset_button = new QPushButton(QObject::tr("Preset..."), &dialog);
  preset_button->setObjectName(QStringLiteral("gradientMapPresetButton"));
  preset_button->setEnabled(library != nullptr);
  heading_row->addWidget(preset_button);
  layout->addLayout(heading_row);

  auto* editor = new GradientStopsEditorWidget(&dialog);
  editor->setObjectName(QStringLiteral("gradientMapStopsEditor"));
  layout->addWidget(editor);

  auto* noise_note = new QLabel(
      QObject::tr("This is a Noise gradient. Choose a preset to replace it with editable color stops."), &dialog);
  noise_note->setObjectName(QStringLiteral("gradientMapNoiseNote"));
  noise_note->setWordWrap(true);
  layout->addWidget(noise_note);

  auto* stop_row = new QWidget(&dialog);
  auto* stop_layout = new QHBoxLayout(stop_row);
  stop_layout->setContentsMargins(0, 0, 0, 0);
  stop_layout->setSpacing(8);
  auto* location = new QSpinBox(stop_row);
  location->setObjectName(QStringLiteral("gradientMapStopLocationSpin"));
  location->setRange(0, 100);
  location->setSuffix(percent_suffix());
  configure_dialog_spinbox(location, 64);
  auto* swatch = new QPushButton(stop_row);
  swatch->setObjectName(QStringLiteral("gradientMapStopColorButton"));
  swatch->setFixedSize(34, 24);
  swatch->setToolTip(QObject::tr("Choose Color..."));
  auto* midpoint = new QSpinBox(stop_row);
  midpoint->setObjectName(QStringLiteral("gradientMapStopMidpointSpin"));
  midpoint->setRange(5, 95);
  midpoint->setSuffix(percent_suffix());
  configure_dialog_spinbox(midpoint, 64);
  stop_layout->addWidget(new QLabel(QObject::tr("Location"), stop_row));
  stop_layout->addWidget(location);
  stop_layout->addWidget(new QLabel(QObject::tr("Color"), stop_row));
  stop_layout->addWidget(swatch);
  stop_layout->addWidget(new QLabel(QObject::tr("Midpoint"), stop_row));
  stop_layout->addWidget(midpoint);
  stop_layout->addStretch(1);
  layout->addWidget(stop_row);

  auto* form = new QFormLayout();
  layout->addLayout(form);
  auto* smoothness = new QSpinBox(&dialog);
  smoothness->setObjectName(QStringLiteral("gradientMapSmoothnessSpin"));
  smoothness->setRange(0, 100);
  smoothness->setSuffix(percent_suffix());
  configure_dialog_spinbox(smoothness, 64);
  form->addRow(QObject::tr("Smoothness"), smoothness);

  auto* result_strip = new QLabel(&dialog);
  result_strip->setObjectName(QStringLiteral("gradientMapResultStrip"));
  result_strip->setFixedSize(kResultStripWidth, kResultStripHeight);
  form->addRow(QObject::tr("Result"), result_strip);

  auto* options_label = new QLabel(QObject::tr("Gradient Options"), &dialog);
  layout->addWidget(options_label);
  auto* options = new QFormLayout();
  layout->addLayout(options);
  auto* dither = new QCheckBox(QObject::tr("Dither"), &dialog);
  dither->setObjectName(QStringLiteral("gradientMapDitherCheck"));
  dither->setChecked(state->value.dither);
  auto* reverse = new QCheckBox(QObject::tr("Reverse"), &dialog);
  reverse->setObjectName(QStringLiteral("gradientMapReverseCheck"));
  reverse->setChecked(state->value.reverse);
  auto* toggles = new QHBoxLayout();
  toggles->addWidget(dither);
  toggles->addWidget(reverse);
  toggles->addStretch(1);
  options->addRow(toggles);
  // Photoshop's Gradient Map lists its methods in this order.
  auto* method = new QComboBox(&dialog);
  method->setObjectName(QStringLiteral("gradientMapMethodCombo"));
  method->addItem(QObject::tr("Perceptual"), static_cast<int>(GradientInterpolationMethod::Perceptual));
  method->addItem(QObject::tr("Linear", "gradient interpolation"), static_cast<int>(GradientInterpolationMethod::Linear));
  method->addItem(QObject::tr("Classic"), static_cast<int>(GradientInterpolationMethod::Classic));
  method->setCurrentIndex(std::max(0, method->findData(static_cast<int>(state->value.method))));
  options->addRow(QObject::tr("Method"), method);

  auto* preview = new QCheckBox(QObject::tr("Preview"), &dialog);
  preview->setObjectName(QStringLiteral("gradientMapPreviewCheck"));
  preview->setChecked(true);
  layout->addWidget(preview);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  CoalescedPreviewEmitter<GradientMapPreviewRequest> preview_emitter(
      dialog, [&](const GradientMapPreviewRequest& request) {
        if (preview_changed) {
          preview_changed(request.enabled, request.settings);
        }
      });
  const auto preview_request = [&] {
    return GradientMapPreviewRequest{preview->isChecked(), current_settings(*state)};
  };

  // Pushes the model into every widget without re-entering the handlers.
  const auto refresh = [&] {
    auto& gradient = state->value.gradient;
    const bool solid = gradient.form == GradientDefinitionForm::Solid;
    editor->setVisible(solid);
    stop_row->setVisible(solid);
    noise_note->setVisible(!solid);
    auto& stops = gradient.color_stops;
    if (solid && !stops.empty()) {
      state->selected_stop = std::clamp(state->selected_stop, 0, static_cast<int>(stops.size()) - 1);
      std::vector<GradientStop> editor_stops;
      std::vector<float> midpoints;
      for (const auto& stop : stops) {
        editor_stops.push_back(GradientStop{stop.location, EditColor{stop.color.red, stop.color.green,
                                                                     stop.color.blue, 255}});
        midpoints.push_back(stop.midpoint);
      }
      editor->set_stops(std::move(editor_stops));
      editor->set_color_midpoints(std::move(midpoints));
      editor->set_current_row(state->selected_stop);
      const auto& stop = stops[static_cast<std::size_t>(state->selected_stop)];
      const QSignalBlocker location_blocker(location);
      const QSignalBlocker midpoint_blocker(midpoint);
      location->setValue(static_cast<int>(std::lround(std::clamp(stop.location, 0.0F, 1.0F) * 100.0F)));
      midpoint->setValue(static_cast<int>(std::lround(std::clamp(stop.midpoint, 0.05F, 0.95F) * 100.0F)));
      // The first stop's midpoint has no segment to shape.
      const auto sorted = current_settings(*state).gradient.color_stops;
      midpoint->setEnabled(sorted.empty() || stop.location > sorted.front().location);
      set_themed_style(*swatch, QStringLiteral("QPushButton { background: rgb(%1, %2, %3); "
                                               "border: 1px solid @swatch_border; }")
                                    .arg(stop.color.red)
                                    .arg(stop.color.green)
                                    .arg(stop.color.blue));
    }
    const QSignalBlocker smoothness_blocker(smoothness);
    smoothness->setValue(static_cast<int>(std::lround(gradient.smoothness * 100.0 / 4096.0)));
    // Smoothness shapes only the Classic method's ramp.
    smoothness->setEnabled(solid && state->value.method == GradientInterpolationMethod::Classic);
    result_strip->setPixmap(gradient_map_result_strip(current_settings(*state)));
  };
  const auto changed = [&](bool immediate) {
    refresh();
    if (immediate) {
      preview_emitter.flush(preview_request());
    } else {
      preview_emitter.schedule(preview_request());
    }
  };

  const auto stop_at = [state](int row) -> GradientColorStop* {
    auto& stops = state->value.gradient.color_stops;
    return row >= 0 && row < static_cast<int>(stops.size()) ? &stops[static_cast<std::size_t>(row)] : nullptr;
  };
  editor->stop_selected = [&](int row) {
    state->selected_stop = row;
    refresh();
  };
  editor->stop_location_changed = [&](int row, int percent) {
    if (auto* stop = stop_at(row); stop != nullptr) {
      stop->location = static_cast<float>(std::clamp(percent, 0, 100)) / 100.0F;
      state->selected_stop = row;
      changed(false);
    }
  };
  editor->color_midpoint_changed = [&](int row, int percent) {
    if (auto* stop = stop_at(row); stop != nullptr) {
      stop->midpoint = static_cast<float>(std::clamp(percent, 5, 95)) / 100.0F;
      state->selected_stop = row;
      changed(false);
    }
  };
  editor->stop_color_picked = [&](int row, QColor color) {
    if (auto* stop = stop_at(row); stop != nullptr && color.isValid()) {
      stop->color = rgb_from_qcolor(color);
      stop->kind = GradientColorStop::Kind::User;
      changed(true);
    }
  };
  editor->stop_add_requested = [&](GradientStop added) {
    auto& stops = state->value.gradient.color_stops;
    stops.push_back(GradientColorStop{std::clamp(added.location, 0.0F, 1.0F),
                                      RgbColor{added.color.r, added.color.g, added.color.b}});
    state->selected_stop = static_cast<int>(stops.size()) - 1;
    changed(true);
    return state->selected_stop;
  };
  editor->stop_delete_requested = [&](int row) {
    auto& stops = state->value.gradient.color_stops;
    if (stops.size() <= 2U || stop_at(row) == nullptr) {
      return;
    }
    stops.erase(stops.begin() + row);
    state->selected_stop = std::min(row, static_cast<int>(stops.size()) - 1);
    changed(true);
  };
  const auto choose_stop_color = [&] {
    auto* stop = stop_at(state->selected_stop);
    if (stop == nullptr) {
      return;
    }
    const auto row = state->selected_stop;
    const auto original = *stop;
    const auto apply = [&, row](QColor color) {
      if (auto* target = stop_at(row); target != nullptr && color.isValid()) {
        target->color = rgb_from_qcolor(color);
        target->kind = GradientColorStop::Kind::User;
      }
    };
    const auto chosen = request_patchy_color(
        &dialog, QColor(original.color.red, original.color.green, original.color.blue),
        QObject::tr("Choose Gradient Stop Color"), [&](QColor color) {
          apply(color);
          changed(false);
        });
    if (chosen.has_value()) {
      apply(*chosen);
    } else if (auto* target = stop_at(row); target != nullptr) {
      *target = original;  // Cancel restores the stop exactly, color type included.
    }
    changed(true);
  };
  editor->choose_stop_color_requested = [&](int row) {
    state->selected_stop = row;
    refresh();
    choose_stop_color();
  };
  QObject::connect(swatch, &QPushButton::clicked, &dialog, [&] { choose_stop_color(); });
  QObject::connect(location, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int value) {
    if (auto* stop = stop_at(state->selected_stop); stop != nullptr) {
      stop->location = static_cast<float>(value) / 100.0F;
      changed(false);
    }
  });
  QObject::connect(midpoint, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int value) {
    if (auto* stop = stop_at(state->selected_stop); stop != nullptr) {
      stop->midpoint = static_cast<float>(value) / 100.0F;
      changed(false);
    }
  });
  QObject::connect(smoothness, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&](int value) {
    state->value.gradient.smoothness = static_cast<std::uint16_t>(value * 4096 / 100);
    changed(false);
  });
  QObject::connect(dither, &QCheckBox::toggled, &dialog, [&](bool checked) {
    state->value.dither = checked;
    changed(true);
  });
  QObject::connect(reverse, &QCheckBox::toggled, &dialog, [&](bool checked) {
    state->value.reverse = checked;
    changed(true);
  });
  QObject::connect(method, &QComboBox::currentIndexChanged, &dialog, [&](int) {
    state->value.method = static_cast<GradientInterpolationMethod>(method->currentData().toInt());
    changed(true);
  });
  QObject::connect(preview, &QCheckBox::toggled, &dialog, [&](bool) { preview_emitter.flush(preview_request()); });

  if (library != nullptr) {
    // Presets resolve their dynamic foreground/background stops now, like the
    // layer-style picker, and Noise presets flatten into editable stops, like
    // the Gradient tool.
    const auto use_preset = [&, foreground, background](const GradientLibraryEntry& entry) {
      auto gradient = sampled_solid_gradient(entry.definition);
      for (auto& stop : gradient.color_stops) {
        if (stop.kind == GradientColorStop::Kind::Foreground) {
          stop.color = foreground;
        } else if (stop.kind == GradientColorStop::Kind::Background) {
          stop.color = background;
        }
        stop.kind = GradientColorStop::Kind::User;
      }
      state->value.gradient = std::move(gradient);
      state->selected_stop = 0;
      changed(true);
    };
    QObject::connect(preset_button, &QPushButton::clicked, &dialog, [&, use_preset] {
      show_gradient_preset_popup(preset_button, *library, use_preset, [&, use_preset] {
        const auto selected = request_gradient_manager(&dialog, *library, {}, current_settings(*state).gradient);
        if (const auto* entry = library->find_entry(selected); entry != nullptr) {
          use_preset(*entry);
        }
      });
    });
  }

  refresh();
  QTimer::singleShot(0, &dialog, [&] {
    if (dialog.isVisible()) {
      preview_emitter.flush(preview_request());
    }
  });
  if (run_non_modal_dialog(dialog) != QDialog::Accepted) {
    return std::nullopt;
  }
  return current_settings(*state);
}

}  // namespace patchy::ui
