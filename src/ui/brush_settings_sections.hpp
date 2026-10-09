#pragma once

#include <QString>

#include <array>
#include <optional>
#include <vector>

namespace patchy::ui {

enum class CanvasTool;
struct WorkingBrush;

// The Brush Settings panel's sections, in Photoshop's order. Lock state persists by
// brush_section_key(), so keys never change; append new sections at the end of the enum and
// place them in kBrushSections where Photoshop lists them. See docs/brush-panels.md.
enum class BrushSection {
  TipShape,
  ShapeDynamics,
  Scattering,
  Texture,
  DualBrush,
  ColorDynamics,
  Transfer,
  WetEdges,
  BuildUp,
  Smoothing,
  Noise,
};

inline constexpr std::array<BrushSection, 11> kBrushSections{
    BrushSection::TipShape,      BrushSection::ShapeDynamics, BrushSection::Scattering,
    BrushSection::Texture,       BrushSection::DualBrush,     BrushSection::ColorDynamics,
    BrushSection::Transfer,      BrushSection::Noise,         BrushSection::WetEdges,
    BrushSection::BuildUp,       BrushSection::Smoothing};

// A set of sections (locks, sections a tool honors).
using BrushSectionMask = unsigned;
[[nodiscard]] constexpr BrushSectionMask brush_section_bit(BrushSection section) noexcept {
  return 1U << static_cast<unsigned>(section);
}
inline constexpr BrushSectionMask kAllBrushSections = (1U << kBrushSections.size()) - 1U;

// Untranslated title (context "patchy::ui::BrushSettingsPanel").
[[nodiscard]] const char* brush_section_title_source(BrushSection section);
// Persisted token (tools/brushSectionLocks).
[[nodiscard]] QString brush_section_key(BrushSection section);
[[nodiscard]] std::optional<BrushSection> brush_section_from_key(const QString& key);
// Every section except Brush Tip Shape can be locked (Photoshop has no lock there either).
[[nodiscard]] bool brush_section_lockable(BrushSection section);

// Copies one section's settings from `from` into `to`. Brush Tip Shape carries the static
// angle, roundness, flips and spacing (the tip, size and softness belong to the pick itself), Build-up
// the Airbrush flag; Smoothing lives outside the working brush, so it copies nothing. Every
// BrushDynamics field belongs to exactly one section: a new field must be added here
// (ui_brush_section_locks_merge_picked_brush checks the mapping covers the persisted form).
void copy_brush_section(BrushSection section, const WorkingBrush& from, WorkingBrush& to);
// True when the section's settings equal a default-constructed brush's.
[[nodiscard]] bool brush_section_is_default(BrushSection section, const WorkingBrush& brush);
// The enable flag a section carries in the brush itself (Texture, Dual Brush, Color Dynamics,
// Noise, Wet Edges, Build-up); empty for sections whose checkbox is derived from their values.
[[nodiscard]] std::optional<bool> brush_section_enabled_flag(BrushSection section,
                                                             const WorkingBrush& brush);
void set_brush_section_enabled_flag(BrushSection section, WorkingBrush& brush, bool enabled);

// The brush a pick produces with locks on: `picked`, except that every locked section keeps
// the current brush's values. The one place lock semantics live.
[[nodiscard]] WorkingBrush picked_over(const WorkingBrush& current, WorkingBrush picked,
                                       BrushSectionMask locks);

// The sections the tool's strokes honor, read from its tool traits (ui/tool_traits): every
// section with kToolBrushDynamics (the Brush); Brush Tip Shape for the other tip-stamping tools
// (kToolBrushTip: Mixer Brush, Pattern Stamp, Eraser, History Brush), which stamp the tip without
// dynamics, plus Smoothing with kToolSmoothing; nothing for tools that ignore the working brush.
[[nodiscard]] BrushSectionMask brush_sections_for_tool(CanvasTool tool);
// True for the tools whose strokes apply brush dynamics and effects (the Brush only).
[[nodiscard]] bool tool_uses_brush_dynamics(CanvasTool tool);
// The tools that honor a section, for options-bar visibility (the Dynamics button, Airbrush).
[[nodiscard]] std::vector<CanvasTool> tools_honoring(BrushSection section);

}  // namespace patchy::ui
