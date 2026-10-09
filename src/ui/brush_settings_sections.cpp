#include "ui/brush_settings_sections.hpp"

#include "ui/current_brush.hpp"
#include "ui/tool_traits.hpp"

#include <QCoreApplication>

namespace patchy::ui {

const char* brush_section_title_source(BrushSection section) {
  switch (section) {
    case BrushSection::TipShape:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Brush Tip Shape");
    case BrushSection::ShapeDynamics:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Shape Dynamics");
    case BrushSection::Scattering:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Scattering");
    case BrushSection::Texture:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Texture");
    case BrushSection::DualBrush:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Dual Brush");
    case BrushSection::ColorDynamics:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Color Dynamics");
    case BrushSection::Transfer:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Transfer");
    case BrushSection::WetEdges:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Wet Edges");
    case BrushSection::BuildUp:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Build-up");
    case BrushSection::Smoothing:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Smoothing");
    case BrushSection::Noise:
      return QT_TRANSLATE_NOOP("patchy::ui::BrushSettingsPanel", "Noise");
  }
  return "";
}

QString brush_section_key(BrushSection section) {
  switch (section) {
    case BrushSection::TipShape:
      return QStringLiteral("tipShape");
    case BrushSection::ShapeDynamics:
      return QStringLiteral("shapeDynamics");
    case BrushSection::Scattering:
      return QStringLiteral("scattering");
    case BrushSection::Texture:
      return QStringLiteral("texture");
    case BrushSection::DualBrush:
      return QStringLiteral("dualBrush");
    case BrushSection::ColorDynamics:
      return QStringLiteral("colorDynamics");
    case BrushSection::Transfer:
      return QStringLiteral("transfer");
    case BrushSection::WetEdges:
      return QStringLiteral("wetEdges");
    case BrushSection::BuildUp:
      return QStringLiteral("buildUp");
    case BrushSection::Smoothing:
      return QStringLiteral("smoothing");
    case BrushSection::Noise:
      return QStringLiteral("noise");
  }
  return {};
}

std::optional<BrushSection> brush_section_from_key(const QString& key) {
  for (const auto section : kBrushSections) {
    if (brush_section_key(section) == key) {
      return section;
    }
  }
  return std::nullopt;
}

bool brush_section_lockable(BrushSection section) {
  return section != BrushSection::TipShape;
}

void copy_brush_section(BrushSection section, const WorkingBrush& from, WorkingBrush& to) {
  const auto& a = from.dynamics;
  auto& b = to.dynamics;
  switch (section) {
    case BrushSection::TipShape:
      to.angle = from.angle;
      to.roundness = from.roundness;
      to.spacing = from.spacing;
      b.tip_flip_x = a.tip_flip_x;
      b.tip_flip_y = a.tip_flip_y;
      return;
    case BrushSection::ShapeDynamics:
      b.size_jitter = a.size_jitter;
      b.minimum_diameter = a.minimum_diameter;
      b.size_control = a.size_control;
      b.size_fade_steps = a.size_fade_steps;
      b.angle_jitter = a.angle_jitter;
      b.angle_control = a.angle_control;
      b.angle_fade_steps = a.angle_fade_steps;
      b.roundness_jitter = a.roundness_jitter;
      b.minimum_roundness = a.minimum_roundness;
      b.roundness_control = a.roundness_control;
      b.roundness_fade_steps = a.roundness_fade_steps;
      b.flip_x_jitter = a.flip_x_jitter;
      b.flip_y_jitter = a.flip_y_jitter;
      return;
    case BrushSection::Scattering:
      b.scatter = a.scatter;
      b.scatter_both_axes = a.scatter_both_axes;
      b.scatter_control = a.scatter_control;
      b.scatter_fade_steps = a.scatter_fade_steps;
      b.count = a.count;
      b.count_jitter = a.count_jitter;
      b.count_control = a.count_control;
      b.count_fade_steps = a.count_fade_steps;
      return;
    case BrushSection::Texture:
      b.texture_enabled = a.texture_enabled;
      b.texture_style = a.texture_style;
      b.texture_scale = a.texture_scale;
      b.texture_depth = a.texture_depth;
      b.texture_invert = a.texture_invert;
      b.texture_seed = a.texture_seed;
      b.texture_mode = a.texture_mode;
      b.texture_brightness = a.texture_brightness;
      b.texture_contrast = a.texture_contrast;
      b.texture_pattern_id = a.texture_pattern_id;
      b.texture_pattern_name = a.texture_pattern_name;
      return;
    case BrushSection::DualBrush:
      b.dual_brush_enabled = a.dual_brush_enabled;
      b.dual_brush_size = a.dual_brush_size;
      b.dual_brush_hardness = a.dual_brush_hardness;
      b.dual_brush_spacing = a.dual_brush_spacing;
      return;
    case BrushSection::ColorDynamics:
      b.color_dynamics_enabled = a.color_dynamics_enabled;
      b.foreground_background_jitter = a.foreground_background_jitter;
      b.color_control = a.color_control;
      b.color_fade_steps = a.color_fade_steps;
      b.hue_jitter = a.hue_jitter;
      b.saturation_jitter = a.saturation_jitter;
      b.brightness_jitter = a.brightness_jitter;
      b.purity = a.purity;
      b.color_per_tip = a.color_per_tip;
      return;
    case BrushSection::Transfer:
      b.opacity_jitter = a.opacity_jitter;
      b.minimum_opacity = a.minimum_opacity;
      b.opacity_control = a.opacity_control;
      b.opacity_fade_steps = a.opacity_fade_steps;
      b.flow_jitter = a.flow_jitter;
      b.minimum_flow = a.minimum_flow;
      b.flow_control = a.flow_control;
      b.flow_fade_steps = a.flow_fade_steps;
      return;
    case BrushSection::Noise:
      b.noise = a.noise;
      return;
    case BrushSection::WetEdges:
      b.wet_edges = a.wet_edges;
      return;
    case BrushSection::BuildUp:
      to.airbrush = from.airbrush;
      return;
    case BrushSection::Smoothing:
      return;
  }
}

bool brush_section_is_default(BrushSection section, const WorkingBrush& brush) {
  auto probe = brush;
  copy_brush_section(section, WorkingBrush{}, probe);
  return same_brush_settings(probe, brush);
}

std::optional<bool> brush_section_enabled_flag(BrushSection section, const WorkingBrush& brush) {
  switch (section) {
    case BrushSection::Texture:
      return brush.dynamics.texture_enabled;
    case BrushSection::DualBrush:
      return brush.dynamics.dual_brush_enabled;
    case BrushSection::ColorDynamics:
      return brush.dynamics.color_dynamics_enabled;
    case BrushSection::Noise:
      return brush.dynamics.noise;
    case BrushSection::WetEdges:
      return brush.dynamics.wet_edges;
    case BrushSection::BuildUp:
      return brush.airbrush;
    default:
      return std::nullopt;
  }
}

void set_brush_section_enabled_flag(BrushSection section, WorkingBrush& brush, bool enabled) {
  switch (section) {
    case BrushSection::Texture:
      brush.dynamics.texture_enabled = enabled;
      return;
    case BrushSection::DualBrush:
      brush.dynamics.dual_brush_enabled = enabled;
      return;
    case BrushSection::ColorDynamics:
      brush.dynamics.color_dynamics_enabled = enabled;
      return;
    case BrushSection::Noise:
      brush.dynamics.noise = enabled;
      return;
    case BrushSection::WetEdges:
      brush.dynamics.wet_edges = enabled;
      return;
    case BrushSection::BuildUp:
      brush.airbrush = enabled;
      return;
    default:
      return;
  }
}

WorkingBrush picked_over(const WorkingBrush& current, WorkingBrush picked, BrushSectionMask locks) {
  for (const auto section : kBrushSections) {
    if ((locks & brush_section_bit(section)) != 0U && brush_section_lockable(section)) {
      copy_brush_section(section, current, picked);
    }
  }
  return picked;
}

BrushSectionMask brush_sections_for_tool(CanvasTool tool) {
  if (tool_has(tool, kToolBrushDynamics)) {
    return kAllBrushSections;
  }
  BrushSectionMask sections = 0U;
  if (tool_has(tool, kToolBrushTip)) {
    sections |= brush_section_bit(BrushSection::TipShape);
  }
  if (tool_has(tool, kToolSmoothing)) {
    sections |= brush_section_bit(BrushSection::Smoothing);
  }
  return sections;
}

std::vector<CanvasTool> tools_honoring(BrushSection section) {
  auto tools = tools_with(kToolBrushDynamics | kToolBrushTip | kToolSmoothing);
  std::erase_if(tools, [section](CanvasTool tool) {
    return (brush_sections_for_tool(tool) & brush_section_bit(section)) == 0U;
  });
  return tools;
}

bool tool_uses_brush_dynamics(CanvasTool tool) {
  return (brush_sections_for_tool(tool) & brush_section_bit(BrushSection::ShapeDynamics)) != 0U;
}

}  // namespace patchy::ui
