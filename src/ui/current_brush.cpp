#include "ui/current_brush.hpp"

#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/canvas_widget.hpp"

#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace patchy::ui {

bool same_brush_settings(const WorkingBrush& a, const WorkingBrush& b) {
  return a.tip_id == b.tip_id && a.snapshot_tip == b.snapshot_tip && a.size == b.size &&
         a.opacity == b.opacity && a.flow == b.flow && a.softness == b.softness &&
         a.airbrush == b.airbrush && a.angle == b.angle && a.roundness == b.roundness &&
         a.spacing == b.spacing &&
         brush_dynamics_to_json(a.dynamics) == brush_dynamics_to_json(b.dynamics);
}

WorkingBrush working_brush_from_preset(const BrushPreset& preset) {
  WorkingBrush brush;
  brush.tip_id = preset.tip_id.isEmpty() ? builtin_round_brush_tip_id() : preset.tip_id;
  brush.size = preset.size;
  brush.opacity = preset.opacity;
  brush.flow = preset.flow;
  brush.softness = preset.softness;
  brush.airbrush = preset.build_up;
  return brush;
}

QString working_brush_tip_key(const WorkingBrush& brush) {
  return brush.snapshot_tip != nullptr ? QStringLiteral("snapshot:") + brush.tip_id : brush.tip_id;
}

CurrentBrush::CurrentBrush(QObject* parent) : QObject(parent) {
  brush_.tip_id = builtin_round_brush_tip_id();
  base_ = BrushBase{BrushBase::Kind::Preset, QStringLiteral("round"), brush_};
}

bool CurrentBrush::modified() const {
  return !same_brush_settings(brush_, base_.settings);
}

bool CurrentBrush::procedural() const {
  return brush_.snapshot_tip == nullptr && is_builtin_brush_tip_id(brush_.tip_id);
}

void CurrentBrush::edit(const BrushEdit& edit) {
  auto next = brush_;
  unsigned changes = 0;
  const auto set = [&changes](auto& field, const auto& value, Change change) {
    if (field != value) {
      field = value;
      changes |= change;
    }
  };
  if (edit.size) set(next.size, std::clamp(*edit.size, 1, kMaxBrushSize), Size);
  if (edit.opacity) set(next.opacity, std::clamp(*edit.opacity, 1, 100), Opacity);
  if (edit.flow) set(next.flow, std::clamp(*edit.flow, 1, 100), Flow);
  if (edit.softness) set(next.softness, std::clamp(*edit.softness, 0, 100), Softness);
  if (edit.airbrush) set(next.airbrush, *edit.airbrush, Airbrush);
  if (edit.angle) set(next.angle, std::clamp(*edit.angle, -180.0, 360.0), TipShape);
  if (edit.roundness) set(next.roundness, std::clamp(*edit.roundness, 1.0, 100.0), TipShape);
  if (edit.spacing) set(next.spacing, *edit.spacing, Spacing);
  if (edit.dynamics &&
      brush_dynamics_to_json(*edit.dynamics) != brush_dynamics_to_json(next.dynamics)) {
    next.dynamics = *edit.dynamics;
    changes |= Dynamics;
  }
  if (changes != 0) {
    replace(std::move(next), changes);
  }
}

void CurrentBrush::pick(const WorkingBrush& brush, BrushBase::Kind kind, const QString& id) {
  auto next = brush;
  if (next.tip_id.isEmpty() && next.snapshot_tip == nullptr) {
    next.tip_id = builtin_round_brush_tip_id();
  }
  base_ = BrushBase{kind, id, next};
  // A pick always pushes everything, so a canvas that drifted (direct test setters) resyncs.
  replace(std::move(next), All);
}

void CurrentBrush::pick_tip(const QString& tip_id, const BrushTipEntry* entry,
                            bool apply_tool_settings) {
  auto next = brush_;
  unsigned changes = Tip | TipShape | Dynamics | Spacing | Base;
  next.tip_id = tip_id;
  next.snapshot_tip.reset();
  next.spacing.reset();
  if (entry != nullptr) {
    next.angle = entry->base_angle_degrees;
    next.roundness = entry->base_roundness;
    next.dynamics = entry->dynamics;
    if (apply_tool_settings && entry->tool_flow_percent.has_value()) {
      next.flow = std::clamp(*entry->tool_flow_percent, 1, 100);
      changes |= Flow;
    }
    if (apply_tool_settings && entry->tool_airbrush.has_value()) {
      next.airbrush = *entry->tool_airbrush;
      changes |= Airbrush;
    }
  } else {
    // replace() keeps procedural_ in step with every procedural brush; Round and Square share it.
    next.angle = procedural_.angle;
    next.roundness = procedural_.roundness;
    next.dynamics = procedural_.dynamics;
  }
  base_ = BrushBase{BrushBase::Kind::Tip, tip_id, next};
  replace(std::move(next), changes);
}

void CurrentBrush::rebase(BrushBase::Kind kind, const QString& id) {
  base_ = BrushBase{kind, id, brush_};
  emit changed(Base);
}

void CurrentBrush::replace(WorkingBrush next, unsigned changes) {
  brush_ = std::move(next);
  if (procedural()) {
    procedural_ = ProceduralShape{brush_.angle, brush_.roundness, brush_.dynamics};
  }
  emit changed(changes);
}

}  // namespace patchy::ui
