// Image > Adjustments > Match Color dialog: Photoshop's Destination Image,
// Image Options, and Image Statistics groups (Load/Save Statistics omitted;
// see docs/match-color.md). The caller owns the statistics and the preview.

#include "ui/match_color_dialog.hpp"

#include "ui/coalesced_preview_emitter.hpp"
#include "ui/dialog_utils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace patchy::ui {

namespace {

struct MatchColorPreviewRequest {
  bool enabled{true};
  MatchColorDialogSettings settings{};
};

}  // namespace

std::optional<MatchColorDialogSettings> request_match_color_settings(
    QWidget* parent, const MatchColorDialogSpec& spec,
    std::function<void(bool, const MatchColorDialogSettings&)> preview_changed) {
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("patchyMatchColorDialog"));
  dialog.setWindowTitle(QCoreApplication::translate("MatchColorDialog", "Match Color"));
  auto* layout = new QVBoxLayout(&dialog);

  auto* destination_group =
      new QGroupBox(QCoreApplication::translate("MatchColorDialog", "Destination Image"), &dialog);
  auto* destination_layout = new QVBoxLayout(destination_group);
  auto* target_label = new QLabel(spec.target_description, destination_group);
  target_label->setObjectName(QStringLiteral("matchColorTargetLabel"));
  destination_layout->addWidget(target_label);
  auto* ignore_selection = new QCheckBox(
      QCoreApplication::translate("MatchColorDialog", "Ignore Selection when Applying Adjustment"),
      destination_group);
  ignore_selection->setObjectName(QStringLiteral("matchColorIgnoreSelectionCheck"));
  ignore_selection->setEnabled(spec.target_has_selection);
  destination_layout->addWidget(ignore_selection);
  layout->addWidget(destination_group);

  auto* options_group = new QGroupBox(QCoreApplication::translate("MatchColorDialog", "Image Options"), &dialog);
  auto* options_form = new QFormLayout(options_group);
  auto* luminance = add_dialog_slider_spin_row(
      options_form, &dialog, QCoreApplication::translate("MatchColorDialog", "Luminance"),
      QStringLiteral("matchColorLuminanceSlider"), QStringLiteral("matchColorLuminanceSpin"),
      kMatchColorLuminanceMin, kMatchColorLuminanceMax, 100);
  auto* intensity = add_dialog_slider_spin_row(
      options_form, &dialog, QCoreApplication::translate("MatchColorDialog", "Color Intensity"),
      QStringLiteral("matchColorIntensitySlider"), QStringLiteral("matchColorIntensitySpin"),
      kMatchColorIntensityMin, kMatchColorIntensityMax, 100);
  auto* fade = add_dialog_slider_spin_row(options_form, &dialog,
                                          QCoreApplication::translate("MatchColorDialog", "Fade"),
                                          QStringLiteral("matchColorFadeSlider"),
                                          QStringLiteral("matchColorFadeSpin"), 0, kMatchColorFadeMax, 0);
  auto* neutralize = new QCheckBox(QCoreApplication::translate("MatchColorDialog", "Neutralize"), options_group);
  neutralize->setObjectName(QStringLiteral("matchColorNeutralizeCheck"));
  options_form->addRow(QString(), neutralize);
  layout->addWidget(options_group);

  auto* statistics_group =
      new QGroupBox(QCoreApplication::translate("MatchColorDialog", "Image Statistics"), &dialog);
  auto* statistics_form = new QFormLayout(statistics_group);
  auto* use_source_selection = new QCheckBox(
      QCoreApplication::translate("MatchColorDialog", "Use Selection in Source to Calculate Colors"),
      statistics_group);
  use_source_selection->setObjectName(QStringLiteral("matchColorUseSourceSelectionCheck"));
  statistics_form->addRow(use_source_selection);
  auto* use_target_selection = new QCheckBox(
      QCoreApplication::translate("MatchColorDialog", "Use Selection in Target to Calculate Adjustment"),
      statistics_group);
  use_target_selection->setObjectName(QStringLiteral("matchColorUseTargetSelectionCheck"));
  use_target_selection->setEnabled(spec.target_has_selection);
  statistics_form->addRow(use_target_selection);
  auto* source_combo = new QComboBox(statistics_group);
  source_combo->setObjectName(QStringLiteral("matchColorSourceCombo"));
  source_combo->addItem(QCoreApplication::translate("MatchColorDialog", "None"), -1);
  for (std::size_t index = 0; index < spec.sources.size(); ++index) {
    source_combo->addItem(spec.sources[index].title, static_cast<int>(index));
  }
  statistics_form->addRow(QCoreApplication::translate("MatchColorDialog", "Source:"), source_combo);
  auto* layer_combo = new QComboBox(statistics_group);
  layer_combo->setObjectName(QStringLiteral("matchColorLayerCombo"));
  statistics_form->addRow(QCoreApplication::translate("MatchColorDialog", "Layer:"), layer_combo);
  layout->addWidget(statistics_group);

  auto* preview = new QCheckBox(QCoreApplication::translate("MatchColorDialog", "Preview"), &dialog);
  preview->setObjectName(QStringLiteral("matchColorPreviewCheck"));
  preview->setChecked(true);
  layout->addWidget(preview);

  const auto current_source = [&]() -> const MatchColorSourceDocument* {
    const auto index = source_combo->currentData().toInt();
    return index >= 0 && static_cast<std::size_t>(index) < spec.sources.size()
               ? &spec.sources[static_cast<std::size_t>(index)]
               : nullptr;
  };
  // Refills the Layer list for the chosen source (layers top first, then Merged)
  // and enables only the controls that source can use.
  const auto refresh_source_controls = [&] {
    const QSignalBlocker block(layer_combo);
    layer_combo->clear();
    const auto* source = current_source();
    if (source != nullptr) {
      for (int index = 0; index < source->layer_names.size(); ++index) {
        layer_combo->addItem(source->layer_names[index], index);
      }
      layer_combo->addItem(QCoreApplication::translate("MatchColorDialog", "Merged"), -1);
      const auto default_index = layer_combo->findData(source->default_layer);
      layer_combo->setCurrentIndex(default_index >= 0 ? default_index : layer_combo->count() - 1);
    }
    layer_combo->setEnabled(source != nullptr);
    use_source_selection->setEnabled(source != nullptr && source->has_selection);
  };
  refresh_source_controls();

  const auto build_settings = [&] {
    MatchColorDialogSettings settings;
    settings.options.luminance = luminance->value();
    settings.options.color_intensity = intensity->value();
    settings.options.fade = fade->value();
    settings.options.neutralize = neutralize->isChecked();
    settings.source = source_combo->currentData().toInt();
    settings.source_layer = layer_combo->count() > 0 ? layer_combo->currentData().toInt() : -1;
    settings.use_source_selection = use_source_selection->isEnabled() && use_source_selection->isChecked();
    settings.use_target_selection = use_target_selection->isEnabled() && use_target_selection->isChecked();
    settings.ignore_selection = ignore_selection->isEnabled() && ignore_selection->isChecked();
    return settings;
  };

  CoalescedPreviewEmitter<MatchColorPreviewRequest> preview_emitter(
      dialog, [&](const MatchColorPreviewRequest& request) {
        if (preview_changed) {
          preview_changed(request.enabled, request.settings);
        }
      });
  const auto preview_request = [&] { return MatchColorPreviewRequest{preview->isChecked(), build_settings()}; };
  const auto schedule_preview = [&] { preview_emitter.schedule(preview_request()); };
  const auto flush_preview = [&] { preview_emitter.flush(preview_request()); };
  for (auto* spin : {luminance, intensity, fade}) {
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&schedule_preview](int) {
      schedule_preview();
    });
  }
  for (auto* check : {neutralize, use_source_selection, use_target_selection, ignore_selection, preview}) {
    QObject::connect(check, &QCheckBox::toggled, &dialog, [&flush_preview](bool) { flush_preview(); });
  }
  QObject::connect(source_combo, &QComboBox::currentIndexChanged, &dialog, [&](int) {
    refresh_source_controls();
    flush_preview();
  });
  QObject::connect(layer_combo, &QComboBox::currentIndexChanged, &dialog, [&flush_preview](int) {
    flush_preview();
  });

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  QTimer::singleShot(0, &dialog, [&dialog, &flush_preview] {
    if (dialog.isVisible()) {
      flush_preview();
    }
  });
  if (run_non_modal_dialog(dialog) != QDialog::Accepted) {
    return std::nullopt;
  }
  return build_settings();
}

}  // namespace patchy::ui
