#pragma once

// One row per CanvasTool holding what the rest of the UI asks about a tool:
// its names, hotkey id, default key, icon, and the capability flags that used
// to be hand-maintained switch statements and tool lists across the canvas
// and MainWindow translation units. Adding a tool means appending its enum
// value, bumping kCanvasToolCount, adding its row in tool_traits.cpp, and
// placing it in the tool palette (build_tool_palette). Per-tool behavior (the
// code that paints, hit-tests, or handles events) stays with that code.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace patchy::ui {

enum class CanvasTool;

using ToolTraitFlags = std::uint32_t;

// Paint/retouch tools that stroke a round footprint: the Size/Soft footprint
// cursor (past the ~155px display cap a crosshair plus the canvas-overlay
// outline), Shift+click straight-line connect from the last stroke end,
// strokes that may start off the canvas, and the pen's eraser end (which
// switches them to Eraser).
inline constexpr ToolTraitFlags kToolPaintsStrokes = 1U << 0U;
// The options bar's Size and Soft spins, and the Alt+Right-drag size gesture.
inline constexpr ToolTraitFlags kToolBrushSize = 1U << 1U;
// The Size and Soft sliders beside those spins.
inline constexpr ToolTraitFlags kToolBrushSizeSliders = 1U << 2U;
// The options bar's brush Opacity spin.
inline constexpr ToolTraitFlags kToolBrushOpacity = 1U << 3U;
// The Opacity slider beside that spin.
inline constexpr ToolTraitFlags kToolBrushOpacitySlider = 1U << 4U;
// Bare digit keys set the tool's opacity (handle_opacity_digit_key).
inline constexpr ToolTraitFlags kToolOpacityDigitKeys = 1U << 5U;
// Flow: the options bar spin, Shift+digit keys, spaced dabs below 100%, and
// the Properties tool line.
inline constexpr ToolTraitFlags kToolFlow = 1U << 6U;
// Stroke Smoothing: the options bar controls, the stroke stabilizer, and the
// Properties tool line. Photoshop scopes it to Brush, Mixer Brush and Eraser.
inline constexpr ToolTraitFlags kToolSmoothing = 1U << 7U;
// The options bar's brush Preset combo.
inline constexpr ToolTraitFlags kToolBrushPresets = 1U << 8U;
// Strokes stamp the active bitmap brush tip: the Tip picker, and a cursor,
// hover outline and size-drag preview that trace the tip shape. Every other
// footprint tool strokes procedurally and keeps the round outline.
inline constexpr ToolTraitFlags kToolBrushTip = 1U << 9U;
// Dodge/Burn/Sponge/Blur/Sharpen style local adjustment brushes.
inline constexpr ToolTraitFlags kToolLocalAdjustment = 1U << 10U;
// Alt+click temporarily picks a color. Rectangle/Ellipse are deliberately
// without it: Alt means draw-from-center there (Photoshop parity).
inline constexpr ToolTraitFlags kToolAltClickPicksColor = 1U << 11U;
// A press is refused in Quick Mask mode and while editing a Smart Filter mask.
inline constexpr ToolTraitFlags kToolBlockedOnMasks = 1U << 12U;
// A press is refused while viewing a single document channel.
inline constexpr ToolTraitFlags kToolBlockedInChannelView = 1U << 13U;
// The Properties panel's tool line lists Size/Opacity/Softness for it.
inline constexpr ToolTraitFlags kToolBrushInfo = 1U << 14U;
// Changes only the view, so its action stays enabled while a preview dialog
// locks editing.
inline constexpr ToolTraitFlags kToolViewOnly = 1U << 15U;
// Strokes apply the working brush's dynamics, effects and Build-up, so every Brush Settings
// section applies; the options bar's Dynamics button and Airbrush. Other kToolBrushTip tools
// honor Brush Tip Shape only, plus Smoothing with kToolSmoothing (brush_sections_for_tool).
// Keep it in step with what canvas_widget_brush.cpp strips.
inline constexpr ToolTraitFlags kToolBrushDynamics = 1U << 16U;

struct ToolTraits {
  CanvasTool tool;
  // Tool palette text (QT_TRANSLATE_NOOP in the patchy::ui::MainWindow
  // context); also the source of the action's object name.
  const char* name{nullptr};
  // Status bar and Properties name (QT_TRANSLATE_NOOP in the QObject context).
  const char* display_name{nullptr};
  // Persisted hotkey command id; never change one.
  const char* hotkey_id{nullptr};
  // Default shortcut as a Qt::Key; 0 for none.
  int default_key{0};
  // Icon resource name, src/ui/icons/<icon>.svg.
  const char* icon{nullptr};
  ToolTraitFlags flags{0};
  // Slot in the per-selection-tool mode/feather arrays, or -1.
  int selection_slot{-1};
  // Second tooltip line for gestures the name does not explain, or nullptr.
  const char* tooltip_detail{nullptr};
  // Status bar sentence shown when the tool is picked, or nullptr (the
  // display name is shown).
  const char* activation_hint{nullptr};
};

// The row for a tool. A value outside the enum gets a neutral fallback row
// ("Tool", no flags).
[[nodiscard]] const ToolTraits& tool_traits(CanvasTool tool) noexcept;

[[nodiscard]] inline bool tool_has(CanvasTool tool, ToolTraitFlags flag) noexcept {
  return (tool_traits(tool).flags & flag) != 0;
}

// Every tool with the flag, in enum order (options bar registrations).
[[nodiscard]] std::vector<CanvasTool> tools_with(ToolTraitFlags flag);

}  // namespace patchy::ui
