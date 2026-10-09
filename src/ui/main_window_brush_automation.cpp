#include <QCoreApplication>
#include "ui/main_window.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brushes_panel.hpp"
#include "ui/current_brush.hpp"
#include "ui/script_engine.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/localization.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace patchy::ui {
BrushAutomationLibrary& MainWindow::brush_automation_library() {
  if (!brush_automation_library_) {
    brush_automation_library_ = new BrushAutomationLibrary(brush_tip_library(), this);
    connect(brush_automation_library_, &BrushAutomationLibrary::changed, this, &MainWindow::refresh_automation_brush_presets);
    auto* timer = new QTimer(this); timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] {
      if ((!script_engine_host_ || !script_engine_host_->run_active()) && (!canvas_ || !canvas_->pointer_gesture_active()))
        brush_automation_library_->refresh();
    });
    timer->start();
  }
  return *brush_automation_library_;
}
void MainWindow::refresh_automation_brush_presets() {
  if (!brush_preset_combo_ || !brush_automation_library_) return;
  const auto selected = brush_preset_combo_->currentData();
  const QSignalBlocker block(brush_preset_combo_);
  brush_preset_combo_->clear();
  for (const auto& value : brush_automation_library_->presets()) {
    const auto p = value.toObject(); brush_preset_combo_->addItem(p["name"].toString(), p["id"].toString());
  }
  brush_preset_combo_->insertSeparator(brush_preset_combo_->count());
  brush_preset_combo_->addItem(tr("Save Current Brush..."), "__saveBrush");
  brush_preset_combo_->addItem(tr("Manage Saved Brushes..."), "__manageBrushes");
  int popup_width = 0;
  const QFontMetrics metrics(brush_preset_combo_->font());
  for (int i=0;i<brush_preset_combo_->count();++i)
    popup_width = std::max(popup_width, metrics.horizontalAdvance(brush_preset_combo_->itemText(i)));
  brush_preset_combo_->view()->setMinimumWidth(popup_width+36);
  const auto index = brush_preset_combo_->findData(selected);
  brush_preset_combo_->setCurrentIndex(index >= 0 ? index : 0);
}
void MainWindow::pick_brush_preset(const QString& preset_id) {
  if (!canvas_) return;
  const auto* preset = find_brush_preset(preset_id);
  if (preset == nullptr) {
    try {
      auto& library = brush_automation_library(); library.refresh();
      auto s = library.resolve(QJsonObject{{"presetId", preset_id}});
      const auto entry = library.preset(preset_id);
      if (!entry["includeColors"].toBool()) {
        s.color = canvas_->primary_color(); s.background = canvas_->secondary_color();
      }
      // A preset saved without its size or tool settings keeps the artist's current ones.
      const auto& working = current_brush().brush();
      if (!entry["captureSize"].toBool(true)) s.size = working.size;
      const auto tool_settings = entry["includeToolSettings"].toBool(true);
      if (!tool_settings) {
        s.opacity = s.mixer ? 100 : working.opacity; s.flow = s.mixer ? current_mixer_flow_ : working.flow;
        s.smoothing = current_brush_smoothing_; s.pulled_string = current_brush_smoothing_pulled_string_;
        s.catch_up = current_brush_smoothing_catch_up_; s.catch_up_end = current_brush_smoothing_catch_up_end_;
        s.zoom_adjust = current_brush_smoothing_zoom_adjust_;
        s.wet = current_mixer_wet_; s.load = current_mixer_load_; s.mix = current_mixer_mix_;
      }
      activate_automation_brush(s);
      if (!tool_settings && preset_pen_override_) {
        // No saved pen mapping: the global pen preferences stay in charge.
        preset_pen_override_.reset(); apply_pen_input_settings(canvas_);
      }
    } catch (const std::exception& e) { show_status_error(tr("Brush preset operation failed: %1").arg(translate_data_text(e.what()))); }
    return;
  }
  // A built-in preset is a complete brush: its procedural tip with no dynamics (so it never
  // leaves a stale bitmap tip or a surprising dynamics setup behind) and no pen mapping.
  if (preset_pen_override_) { preset_pen_override_.reset(); apply_pen_input_settings(canvas_); }
  auto brush = working_brush_from_preset(*preset);
  if (eraser_brush_settings_active_) {
    // The Eraser keeps its own size group; the preset fills it and the working brush keeps its.
    stored_eraser_brush_settings_ = {preset->size, preset->opacity, preset->flow, preset->softness, preset->build_up};
    canvas_->set_brush_build_up(preset->build_up); canvas_->set_brush_size(preset->size);
    canvas_->set_brush_opacity(preset->opacity); canvas_->set_brush_flow(preset->flow);
    canvas_->set_brush_softness(preset->softness);
    const auto& working = current_brush().brush();
    brush.size = working.size; brush.opacity = working.opacity; brush.flow = working.flow;
    brush.softness = working.softness; brush.airbrush = working.airbrush;
  }
  current_brush().pick(brush, BrushBase::Kind::Preset, preset->id);
  save_tool_settings();
  statusBar()->showMessage(tr("Brush preset: %1").arg(brush_preset_display_name(*preset)));
}
void MainWindow::activate_automation_brush(const ScriptStroke& input) {
  if (!canvas_) brush_input::invalid(QCoreApplication::translate("patchy::ui::BrushAutomationLibrary", "active document"));
  auto s = input;
  if ((current_brush().locks() & brush_section_bit(BrushSection::Smoothing)) != 0U) {
    // A locked Smoothing section keeps the artist's values (the other sections lock in pick()).
    s.smoothing = current_brush_smoothing_; s.pulled_string = current_brush_smoothing_pulled_string_;
    s.catch_up = current_brush_smoothing_catch_up_; s.catch_up_end = current_brush_smoothing_catch_up_end_;
    s.zoom_adjust = current_brush_smoothing_zoom_adjust_;
  }
  activate_tool(s.mixer ? CanvasTool::MixerBrush : s.erase ? CanvasTool::Eraser : CanvasTool::Brush);
  // The canvas takes the complete brush, including its pen mapping, Mixer and Smoothing values.
  canvas_->apply_script_brush(s);
  preset_pen_override_ = canvas_->pen_input_settings();
  current_mixer_wet_ = s.wet; current_mixer_load_ = s.load; current_mixer_mix_ = s.mix;
  current_mixer_flow_ = s.flow;
  current_brush_smoothing_ = s.smoothing; current_brush_smoothing_pulled_string_ = s.pulled_string;
  current_brush_smoothing_catch_up_ = s.catch_up; current_brush_smoothing_catch_up_end_ = s.catch_up_end;
  current_brush_smoothing_zoom_adjust_ = s.zoom_adjust;
  if (mixer_sample_all_layers_check_) {
    const QSignalBlocker block(mixer_sample_all_layers_check_);
    mixer_sample_all_layers_check_->setChecked(s.sample_all_layers);
  }
  WorkingBrush brush;
  brush.tip_id = s.tip_id.isEmpty() && !s.tip ? builtin_round_brush_tip_id() : s.tip_id;
  // A tip outside the library (a saved preset's embedded tip, a captured working tip) travels
  // with the brush.
  if (s.tip && !is_builtin_brush_tip_id(s.tip_id) && !brush_tip_library().find_entry(s.tip_id)) brush.snapshot_tip = s.tip;
  brush.spacing = s.spacing; brush.angle = s.angle; brush.roundness = s.roundness; brush.dynamics = s.dynamics;
  if (s.erase) {
    // The Eraser keeps its own size group.
    stored_eraser_brush_settings_ = {s.size, s.opacity, s.flow, s.softness, s.airbrush};
    const auto& working = current_brush().brush();
    brush.size = working.size; brush.opacity = working.opacity; brush.flow = working.flow;
    brush.softness = working.softness; brush.airbrush = working.airbrush;
  } else {
    brush.size = s.size; brush.opacity = s.opacity; brush.flow = s.flow;
    brush.softness = s.softness; brush.airbrush = s.airbrush;
  }
  current_brush().pick(brush, s.preset_id.isEmpty() ? BrushBase::Kind::Tip : BrushBase::Kind::Preset,
                       s.preset_id.isEmpty() ? brush.tip_id : s.preset_id);
  sync_brush_controls_from_canvas();
  canvas_->refresh_tool_cursor(); refresh_document_info();
}
QString MainWindow::save_working_brush_as_preset(const QString& name) {
  return save_working_brush_as_preset(name, BrushPresetSaveOptions{});
}
QString MainWindow::save_working_brush_as_preset(const QString& name, const BrushPresetSaveOptions& options) {
  if (!canvas_) brush_input::invalid(QCoreApplication::translate("patchy::ui::BrushAutomationLibrary", "active document"));
  fold_canvas_brush_values();
  QJsonObject flags;
  if (!options.capture_size) flags["captureSize"] = false;
  if (!options.include_tool_settings) flags["includeToolSettings"] = false;
  const auto id = brush_automation_library().save(name, canvas_->current_script_brush(), options.include_color, {},
                                                  options.folder, flags);
  if (!eraser_brush_settings_active_) current_brush().rebase(BrushBase::Kind::Preset, id);
  return id;
}
void MainWindow::save_current_automation_brush() {
  // The options-bar combo's entry opens the same New Brush Preset dialog as the panels.
  new_brush_preset_from_panel(brushes_panel_ != nullptr ? brushes_panel_->selected_folder() : QString());
}
void MainWindow::manage_automation_brush_presets() {
  auto& library = brush_automation_library(); library.refresh();
  QDialog dialog(this); dialog.setObjectName("brushPresetManager"); dialog.setWindowTitle(tr("Saved Brushes"));
  auto* layout = new QVBoxLayout(&dialog);
  auto* hint = new QLabel(tr("Select a saved brush. Update replaces it with the current brush settings. Document Undo does not change saved brushes."), &dialog);
  hint->setWordWrap(true); layout->addWidget(hint);
  auto* list = new QListWidget(&dialog); list->setObjectName("savedBrushList"); layout->addWidget(list);
  auto* row = new QHBoxLayout; layout->addLayout(row);
  auto reload = [&] {
    list->clear(); for (const auto& value : library.presets()) { const auto p = value.toObject();
      if (p["source"] != "user") continue;
      auto* item = new QListWidgetItem(p["name"].toString(), list); item->setData(Qt::UserRole, p["id"]); }
  };
  const auto action = [&](const QString& label, const std::function<void(const QString&)>& fn) {
    auto* button = new QPushButton(label, &dialog); row->addWidget(button);
    connect(button, &QPushButton::clicked, &dialog, [&, fn] {
      if (!list->currentItem()) return;
      try { fn(list->currentItem()->data(Qt::UserRole).toString()); reload(); }
      catch (const std::exception& e) { show_status_error(tr("Brush preset operation failed: %1").arg(translate_data_text(e.what()))); }
    });
  };
  action(tr("Use"), [&](const QString& id) {
    auto s = library.resolve(QJsonObject{{"presetId", id}});
    if (canvas_ && !library.preset(id)["includeColors"].toBool()) { s.color=canvas_->primary_color();s.background=canvas_->secondary_color(); }
    activate_automation_brush(s);
  });
  action(tr("Update"), [&](const QString& id) {
    if (!canvas_) return;
    const auto p=library.preset(id);
    QJsonObject flags;  // keep the record's capture choices
    for (const auto* key : {"captureSize", "includeToolSettings"}) if (p.contains(key)) flags[key] = p[key];
    (void)library.save(p["name"].toString(),canvas_->current_script_brush(),p["includeColors"].toBool(),id,p["folder"].toString(),flags);
  });
  const auto rename = [&](const QString& id, bool duplicate) {
    const auto p=library.preset(id); bool accepted=false;
    const auto name=QInputDialog::getText(&dialog,duplicate?tr("Duplicate Brush"):tr("Rename Brush"),tr("Name:"),QLineEdit::Normal,p["name"].toString(),&accepted);
    if(!accepted || name.trimmed().isEmpty()) return;
    if (duplicate) (void)library.duplicate(id, name); else library.update_entry(id, name, p["folder"].toString());
  };
  action(tr("Duplicate"),[&](const QString& id){rename(id,true);});
  action(tr("Rename"),[&](const QString& id){rename(id,false);});
  action(tr("Delete"),[&](const QString& id){library.remove(id);});
  auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);
  connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  reload();dialog.resize(620,400);run_non_modal_dialog(dialog);
}
}  // namespace patchy::ui
