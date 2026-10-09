// MainWindow's working-brush glue, split out of main_window_tool_options.cpp: the CurrentBrush
// accessor and its push to the active canvas, folding canvas gestures back, options-bar size
// group edits, the tip pick policy, the Eraser's own size group, brush-tip import/define, and
// the remembered working brush (docs/brushes.md, "Current brush").
// Pure function moves; behavior must stay identical.

#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "ui/brush_automation.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/current_brush.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/image_document_io.hpp"

#include <QComboBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>

#include <cmath>
#include <exception>
#include <functional>
#include <utility>

namespace patchy::ui {

CurrentBrush& MainWindow::current_brush() {
  if (current_brush_ == nullptr) {
    current_brush_ = new CurrentBrush(this);
    connect(current_brush_, &CurrentBrush::changed, this, &MainWindow::on_current_brush_changed);
  }
  return *current_brush_;
}

void MainWindow::push_current_brush_to_canvas(CanvasWidget* canvas, unsigned changes) {
  if (canvas == nullptr) {
    return;
  }
  const auto& brush = current_brush().brush();
  if ((changes & CurrentBrush::Tip) != 0U) {
    // Tips are application-wide; an incoming canvas (new tab or tab switch) may hold a stale or
    // empty one. Re-setting an identical tip would only rebuild its mip chain.
    auto tip = brush.snapshot_tip;
    if (tip == nullptr && !is_builtin_brush_tip_id(brush.tip_id)) {
      tip = brush_tip_library().tip(brush.tip_id);
    }
    if (tip != nullptr && tip->empty()) {
      tip = nullptr;
    }
    const auto tip_id = tip != nullptr ? brush.tip_id : QString();
    if (canvas->brush_tip() != tip || canvas->brush_tip_id() != tip_id) {
      canvas->set_brush_tip(tip, tip_id);
    }
    canvas->set_brush_shape(tip == nullptr && brush.tip_id == builtin_square_brush_tip_id()
                                ? patchy::BrushShape::Square
                                : patchy::BrushShape::Round);
    changes |= CurrentBrush::Spacing;  // set_brush_tip clears the override
  }
  if ((changes & CurrentBrush::Spacing) != 0U) {
    canvas->set_brush_spacing_override(brush.spacing);
  }
  if ((changes & CurrentBrush::TipShape) != 0U) {
    canvas->set_brush_base_shape(brush.angle, static_cast<int>(std::lround(brush.roundness)));
  }
  if ((changes & CurrentBrush::Dynamics) != 0U) {
    // Brush strokes only: erase strokes strip them, and a dynamics-active Round or Square
    // stamps through a synthesized disc or square tip inside CanvasWidget.
    canvas->set_brush_dynamics(brush.dynamics);
  }
  if (!eraser_brush_settings_active_) {
    if ((changes & CurrentBrush::Size) != 0U) canvas->set_brush_size(brush.size);
    if ((changes & CurrentBrush::Opacity) != 0U) canvas->set_brush_opacity(brush.opacity);
    if ((changes & CurrentBrush::Flow) != 0U) canvas->set_brush_flow(brush.flow);
    if ((changes & CurrentBrush::Softness) != 0U) canvas->set_brush_softness(brush.softness);
    if ((changes & CurrentBrush::Airbrush) != 0U) canvas->set_brush_build_up(brush.airbrush);
  }
  if ((changes & (CurrentBrush::Tip | CurrentBrush::TipShape)) != 0U) {
    canvas->refresh_tool_cursor();
  }
}

void MainWindow::on_current_brush_changed(unsigned changes) {
  push_current_brush_to_canvas(canvas_, changes);
  const auto& brush = current_brush().brush();
  const auto& base = current_brush().base();
  if (brush_tip_picker_ != nullptr && (changes & CurrentBrush::Tip) != 0U) {
    brush_tip_picker_->set_current_tip_id(brush.snapshot_tip != nullptr || brush.tip_id.isEmpty()
                                              ? builtin_round_brush_tip_id()
                                              : brush.tip_id);
    if (brush.snapshot_tip != nullptr) {
      QString label;
      if (base.kind == BrushBase::Kind::Preset && brush_automation_library_ != nullptr) {
        try {
          label = brush_automation_library_->preset(base.id)[QStringLiteral("name")].toString();
        } catch (const std::exception&) {
        }
      }
      brush_tip_picker_->set_working_preview(label.isEmpty() ? tr("Working brush") : label,
                                             brush_tip_thumbnail(*brush.snapshot_tip, 32));
    }
  }
  if (brush_dynamics_button_ != nullptr &&
      (changes & (CurrentBrush::Tip | CurrentBrush::TipShape | CurrentBrush::Dynamics)) != 0U) {
    brush_dynamics_button_->set_working_brush(working_brush_tip_key(brush), brush.dynamics,
                                              brush.angle, brush.roundness);
  }
  if (brush_preset_combo_ != nullptr && (changes & CurrentBrush::Base) != 0U &&
      base.kind == BrushBase::Kind::Preset) {
    const QSignalBlocker block(brush_preset_combo_);
    brush_preset_combo_->setProperty("lastBrushPresetId", base.id);
    brush_preset_combo_->setCurrentIndex(brush_preset_combo_->findData(base.id));
  }
  if (canvas_ != nullptr) {
    sync_brush_controls_from_canvas();
    schedule_save_tool_settings();
    refresh_document_info();
  }
}

void MainWindow::fold_canvas_brush_values() {
  if (canvas_ == nullptr) {
    return;
  }
  if (eraser_brush_settings_active_) {
    stored_eraser_brush_settings_ =
        BrushToolSettings{canvas_->brush_size(), canvas_->brush_opacity(), canvas_->brush_flow(),
                          canvas_->brush_softness(), canvas_->brush_build_up()};
    return;
  }
  BrushEdit edit;
  edit.size = canvas_->brush_size();
  edit.opacity = canvas_->brush_opacity();
  edit.flow = canvas_->brush_flow();
  edit.softness = canvas_->brush_softness();
  edit.airbrush = canvas_->brush_build_up();
  current_brush().edit(edit);
}

void MainWindow::edit_brush_option(const BrushEdit& edit) {
  if (canvas_ == nullptr) {
    return;
  }
  if (!eraser_brush_settings_active_) {
    current_brush().edit(edit);
    return;
  }
  // The Eraser's group lives on the canvas while it is active.
  if (edit.size) canvas_->set_brush_size(*edit.size);
  if (edit.opacity) canvas_->set_brush_opacity(*edit.opacity);
  if (edit.flow) canvas_->set_brush_flow(*edit.flow);
  if (edit.softness) canvas_->set_brush_softness(*edit.softness);
  if (edit.airbrush) canvas_->set_brush_build_up(*edit.airbrush);
  schedule_save_tool_settings();
  refresh_document_info();
}

void MainWindow::set_active_brush_tip(const QString& tip_id, bool announce,
                                      bool apply_tool_settings) {
  // A tip pick ends any saved preset's pen mapping.
  preset_pen_override_.reset();
  if (canvas_) apply_pen_input_settings(canvas_);
  auto effective = tip_id.isEmpty() ? builtin_round_brush_tip_id() : tip_id;
  const auto* entry = brush_tip_library().find_entry(effective);
  if (!is_builtin_brush_tip_id(effective) &&
      (entry == nullptr || brush_tip_library().tip(effective) == nullptr)) {
    effective = builtin_round_brush_tip_id();
    entry = nullptr;
  }
  current_brush().pick_tip(effective, entry, apply_tool_settings);
  if (announce) {
    statusBar()->showMessage(entry != nullptr                                  ? tr("Brush tip: %1").arg(entry->name)
                             : effective == builtin_square_brush_tip_id() ? tr("Brush tip: Square")
                                                                          : tr("Brush tip: Round"));
  }
}

void MainWindow::import_brush_tips_from_abr() {
  const auto path = get_open_file_name(this, tr("Import Photoshop Brushes"), QString(),
                                       tr("Photoshop Brushes (*.abr)"), nullptr,
                                       QStringLiteral("brushTipImportFileDialog"));
  if (path.isEmpty()) {
    return;
  }
  const auto before = brush_tip_library().entries().size();
  QString error;
  QStringList warnings;
  const auto first_id = brush_tip_library().import_abr(path, error, warnings);
  if (first_id.isEmpty()) {
    QMessageBox::warning(this, tr("Import Brushes"), error);
    return;
  }
  const auto imported = static_cast<int>(brush_tip_library().entries().size() - before);
  set_active_brush_tip(first_id, false);
  QMessageBox message(QMessageBox::Information, tr("Import Brushes"),
                      abr_import_summary(imported, warnings), QMessageBox::Ok, this);
  if (!warnings.isEmpty()) {
    message.setDetailedText(warnings.join(QStringLiteral("\n")));
  }
  message.exec();
}

void MainWindow::open_brush_tip_manager() {
  std::function<QImage()> capture;
  if (canvas_ != nullptr) {
    capture = [this] { return capture_brush_tip_define_source(); };
  }
  request_brush_tip_manager(this, brush_tip_library(), current_brush().brush().tip_id, capture,
                            [this](const QString& id) { set_active_brush_tip(id, true); });
}

QImage MainWindow::capture_brush_tip_define_source() const {
  if (canvas_ == nullptr) {
    return {};
  }
  const auto& doc = document();
  const auto canvas_rect = QRect(0, 0, doc.width(), doc.height());
  auto capture_rect = canvas_rect;
  const auto selected = canvas_->selected_document_rect();
  if (selected.has_value()) {
    capture_rect = selected->intersected(canvas_rect);
  }
  if (capture_rect.isEmpty() || capture_rect.width() > 4096 || capture_rect.height() > 4096) {
    return {};
  }

  const auto composited =
      qimage_from_document_rect(doc, capture_rect, true).convertToFormat(QImage::Format_ARGB32);
  if (composited.isNull()) {
    return {};
  }
  // Photoshop semantics: dark pixels paint, light pixels stay clear, transparency masks out.
  // A soft or non-rectangular selection additionally shapes the tip.
  return brush_coverage_from_image(composited, selected.has_value()
      ? std::function<int(int,int)>([&](int x,int y) { return canvas_->selection_alpha_at(capture_rect.topLeft()+QPoint(x,y)); })
      : std::function<int(int,int)>());
}

void MainWindow::define_brush_tip_from_selection() {
  if (canvas_ == nullptr) {
    return;
  }
  const auto mask = capture_brush_tip_define_source();
  if (mask.isNull()) {
    show_status_error(tr("The selection is empty or too large to use as a brush tip (max 4096px)"));
    return;
  }
  bool accepted = false;
  const auto name = QInputDialog::getText(this, tr("Define Brush Tip"), tr("Name:"), QLineEdit::Normal,
                                          tr("Brush %1").arg(brush_tip_library().entries().size() + 1),
                                          &accepted);
  if (!accepted || name.trimmed().isEmpty()) {
    return;
  }
  const auto id = brush_tip_library().add_tip(name.trimmed(), mask, 0.25);
  if (id.isEmpty()) {
    show_status_error(tr("The selection is empty or too large to use as a brush tip (max 4096px)"));
    return;
  }
  set_active_brush_tip(id, false);
  statusBar()->showMessage(tr("Defined brush tip: %1").arg(name.trimmed()));
}

void MainWindow::apply_active_brush_settings_to_canvas() {
  if (canvas_ == nullptr) {
    return;
  }
  if (eraser_brush_settings_active_) {
    const auto& values = stored_eraser_brush_settings_;
    canvas_->set_brush_size(values.size);
    canvas_->set_brush_opacity(values.opacity);
    canvas_->set_brush_flow(values.flow);
    canvas_->set_brush_softness(values.softness);
    canvas_->set_brush_build_up(values.airbrush);
  }
  // The working brush is application-wide; an incoming canvas (new tab or tab switch) may hold
  // stale values.
  push_current_brush_to_canvas(canvas_, CurrentBrush::All);
  if (preset_pen_override_) {
    canvas_->set_pen_input_settings(*preset_pen_override_);
  }
}

void MainWindow::set_eraser_brush_settings_active(bool active) {
  if (eraser_brush_settings_active_ == active) {
    return;
  }
  stash_active_brush_settings();
  eraser_brush_settings_active_ = active;
  apply_active_brush_settings_to_canvas();
  sync_brush_controls_from_canvas();
  save_tool_settings();
}

bool MainWindow::restore_working_brush(const QSettings& settings) {
  const auto state =
      QJsonDocument::fromJson(settings.value(QStringLiteral("tools/workingBrush")).toString().toUtf8())
          .object();
  if (state.isEmpty()) {
    return false;
  }
  auto brush = working_brush_from_json(state["brush"].toObject());
  const auto base_object = state["base"].toObject();
  BrushBase base{base_object["kind"].toString() == QStringLiteral("tip") ? BrushBase::Kind::Tip
                                                                          : BrushBase::Kind::Preset,
                 base_object["id"].toString(), working_brush_from_json(base_object["brush"].toObject())};
  const auto resolve_tip = [this, &base](WorkingBrush& candidate) {
    if (is_builtin_brush_tip_id(candidate.tip_id) ||
        (brush_tip_library().find_entry(candidate.tip_id) != nullptr &&
         brush_tip_library().tip(candidate.tip_id) != nullptr)) {
      return true;
    }
    // A saved preset's embedded tip comes back from that preset.
    if (base.kind == BrushBase::Kind::Preset && find_brush_preset(base.id) == nullptr) {
      try {
        candidate.snapshot_tip =
            brush_automation_library().resolve(QJsonObject{{"presetId", base.id}}).tip;
      } catch (const std::exception&) {
      }
    }
    return candidate.snapshot_tip != nullptr;
  };
  if (!resolve_tip(brush)) {
    return false;
  }
  if (!resolve_tip(base.settings)) {
    base.settings = brush;
  }
  current_brush().restore(brush, base);
  return true;
}

void MainWindow::save_working_brush(QSettings& settings) const {
  if (current_brush_ != nullptr) {
    const auto& base = current_brush_->base();
    const QJsonObject state{
        {"brush", working_brush_to_json(current_brush_->brush())},
        {"base", QJsonObject{{"kind", base.kind == BrushBase::Kind::Tip ? "tip" : "preset"},
                             {"id", base.id},
                             {"brush", working_brush_to_json(base.settings)}}}};
    settings.setValue(QStringLiteral("tools/workingBrush"),
                      QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));
  }
}

}  // namespace patchy::ui
