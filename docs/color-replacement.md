# Color Replacement

The Color Replacement brush (`CanvasTool::ColorReplacement`, hotkey id `tools.color_replacement`, object name `toolColorReplacementAction`, unbound) repaints pixels under the brush that match a sampled color with the foreground color's hue, saturation, color, or luminosity, keeping the rest of each pixel. It shares the Brush Tools flyout (`brushToolButton`) with the Brush (B, the default); Shift+B is "Cycle Brush Tools" (`tools.cycle.brush`, `toolCycleBrushAction`). Photoshop's brush group also holds Pencil and Mixer Brush: Patchy has no Pencil, and Mixer Brush stays in the Detail flyout (Shift+R), so moving it would change an existing slot.

## Options

| Control | Values (default) | Settings key and tokens |
|---|---|---|
| Size / Soft | the shared procedural footprint (round, Soft feather; bitmap tips do not apply) | the Brush's session mirrors |
| Mode | Hue, Saturation, Color (default), Luminosity | `tools/colorReplacementMode`: `hue` `saturation` `color` `luminosity` |
| Sampling | Continuous (default), Once, Background Swatch | `tools/colorReplacementSampling`: `continuous` `once` `backgroundSwatch` |
| Limits | Discontiguous, Contiguous (default), Find Edges | `tools/colorReplacementLimits`: `discontiguous` `contiguous` `findEdges` |
| Tol | 1..100%, 30 | `tools/colorReplacementTolerance` |
| Anti-alias | on | `tools/colorReplacementAntiAlias` |

Settings are application-wide like the other tool options (one `ColorReplacementSettings` value, `MainWindow::current_color_replacement_`, pushed to every canvas). The tokens are persisted identifiers; never store enum values.

## Engine

- Math is Qt-free in `core/color_replacement.{hpp,cpp}`; the canvas side is `CanvasWidget::color_replacement_brush_segment` / `color_replacement_dab` (canvas_widget_brush.cpp). The stroke rides the local adjustment flow (`is_local_adjustment_tool`): one undo step labeled "Color Replacement", a stroke-start layer snapshot, refusal on grayscale targets, channel views, Quick Mask, and Smart Filter masks, Shift-click stroke connect, the pen's eraser end erasing.
- Dabs fall at 25% of the brush size along the path (document distance, not event density). Each dab reads only the snapshot: Continuous samples the snapshot pixel under the dab center (a transparent center skips the dab), Once keeps the first sample of the stroke, Background Swatch uses the background color.
- Match: the largest per-channel RGB difference against `tolerance% x 255 / 100` (rounded). Within it the match is 1; Anti-alias ramps the last quarter of the tolerance down to 0, and nothing past the tolerance ever changes.
- Limits per dab footprint: Discontiguous keeps every match; Contiguous keeps matches 4-connected to the dab center through matching footprint cells (if the center does not match, the dab does nothing); Find Edges also refuses a flood step between neighbours whose colors differ by more than half the tolerance, so the stroke stays on its side of a sharp edge even when both sides are within tolerance.
- Write: weight = footprint coverage x match x selection coverage, capped per stroke at the strongest weight any dab reached (`capped_stroke_coverage`), blended toward `blend_rgb(foreground, snapshot pixel, mode)` (the compositor's non-separable Hue/Saturation/Color/Luminosity). Alpha is never written, so Lock Transparent Pixels holds trivially and layer bounds never grow. Palette mode snaps like the other retouch brushes. 8-bit RGB layers only.

## Photoshop parity

Approximated, not calibrated (no Photoshop on the development machine): the tolerance metric and percent scale, the anti-alias ramp width, the Find Edges edge test, the 25% dab spacing, and Continuous sampling from the stroke-start pixels. Matching names, defaults, flyout placement, Shift+B, and Alt+click foreground pick follow Photoshop. Omitted: bitmap tips and dynamics, pen pressure for tolerance, the sampling icon buttons (a combo box replaces them), and 16-bit layers.

## Patent note (claim check, October 2026)

Photoshop CS (2003) shipped the tool. The closest teaching is Adobe's Smart Erasure Brush (the Background Eraser), US 7627168 (priority 1999-04-26, expired 2019): per-tip sampling once or continuously or from a fixed color, a tolerance tent match, contiguity by paths from the tip center, and an edge-based "same side of an edge" rule. That is free prior art, and this design uses only those elements; luminance-preserving localized recoloring is also old (Kodak US 5130789, 1989). Checked on Google Patents and found not to read: Adobe US 8175409 (to 2030; claims a selection mask built during input plus a displayed selectable icon; this tool builds no selection), Adobe US 8218860 (to 2031; claims a hue histogram and a prominent hue range; nothing here builds a histogram), Apple US 8687015 (to 2032; claims an adjustment previewed on the whole image, then brushed in with edge detection and removed from the rest), Behr US 10416790 family (to 2036; claims bilateral smoothing plus Sobel and Canny maps feeding a flood segmentation, or room-image masking-tool UIs), Adobe US 9665930 (to 2035; stores tool settings in the file). Expired: Corel US 7782338 / 8081196 / 8115782 (assisted tool impression), Adobe US 8280198 family, US 7817159, US 9230344. Binding rule in [legal-constraints.md](legal-constraints.md).

Coverage: core `tool_color_replacement_matches_within_tolerance_and_keeps_luminosity`, `tool_color_replacement_limits_follow_connectivity_and_edges` (tests/core/pixel_tools_tests.cpp); UI `ui_color_replacement_recolors_sampled_color_in_one_undo_step` (Continuous vs Once, luminosity, selection, one undo step); the flyout and keys ride `ui_tool_cycle_hotkeys_walk_each_flyout`, `ui_photoshop_shortcuts_are_registered`, `ui_hotkey_defaults_have_no_conflicts`, and `ui_tool_palette_icons_render_sheet`.
