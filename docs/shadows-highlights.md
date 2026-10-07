# Shadows/Highlights

Image > Adjustments > Shadows/Highlights... (`imageAdjustShadowsHighlightsAction`, persisted command id `image.shadows_highlights`, no default shortcut, like Photoshop). Destructive only, as in Photoshop, which has no Shadows/Highlights adjustment layer. Kernel: `src/filters/shadows_highlights.{hpp,cpp}` (Qt-free, `apply_shadows_highlights`). Dialog: `request_shadows_highlights_settings` in `src/ui/adjustment_dialogs.cpp`. Flow: `MainWindow::shadows_highlights_dialog` in `src/ui/main_window_destructive_adjustments.cpp` (shared async destructive preview, progress dialog with Cancel, one undo step, no new layer).

## Behavior

- Targets editable 8-bit RGB pixel layers. Text and shape layers go through the rasterize prompt (docs/filters.md "Text and shape layer gate"); locked pixels and Smart Objects are refused with the standard status messages.
- Selection: neighborhoods read the whole layer; only selected pixels change (`apply_shadows_highlights_to_pixels` restores the rest). Hard-edged like the other destructive adjustments.
- Alpha never changes and fully transparent pixels are skipped. When the layer has any transparency the neighborhood blur is alpha-weighted, so transparent pixels do not darken a neighborhood.
- Both Amounts (or both Tones) at 0 with Midtone 0 is an exact no-op (`shadows_highlights_has_effect`); the clips are ignored then.

## Controls

Photoshop's dialog, defaults, and ranges:

| Control | Object name root | Default | Range |
|---|---|---|---|
| Shadows Amount / Tone / Radius | `shadowsHighlightsShadows{Amount,Tone,Radius}` | 35 % / 50 % / 30 px | 0..100 % / 0..100 % / 0..2500 px |
| Highlights Amount / Tone / Radius | `shadowsHighlightsHighlights{...}` | 0 % / 50 % / 30 px | same |
| Color | `shadowsHighlightsColor` | +20 | -100..100 |
| Midtone | `shadowsHighlightsMidtone` | 0 | -100..100 |
| Black Clip / White Clip | `shadowsHighlights{Black,White}ClipSpin` | 0.01 % | 0..50 %, stored in hundredths |

The basic view shows only the two Amounts. Show More Options (`shadowsHighlightsMoreOptionsCheck`) reveals Tone, Radius, the Adjustments group, and Save Defaults; its state persists as `shadowsHighlights/showMoreOptions`. Save Defaults writes `shadowsHighlights/{shadowsAmount,shadowsTone,shadowsRadius,highlightsAmount,highlightsTone,highlightsRadius,color,midtone,blackClip,whiteClip}` (clips in hundredths of a percent); the dialog opens at those values, else the factory defaults. These keys are compatibility contracts. Not implemented: Photoshop's Shift+Save Defaults factory reset and the Load/Save settings files.

## Formulas

Patchy's own design; NOT calibrated against Photoshop (no Photoshop on the development machine). Values on 0..1.

- `Y` = Rec. 601 luma (`299R + 587G + 114B`). `Bs`, `Bh` = `Y` blurred by a Gaussian of sigma = Radius px, using the three-box approximation of the mask feather (`mask_feather_blur`, edge clamped). Equal radii share one plane.
- Shadows, amount `as`, tone `ts`: where `Bs < ts`, `f = 1 - Bs/ts` and `Y <- Y^(1 / (1 + 2 as f^2))`.
- Highlights, with `D = 1 - Bh`: where `D < th`, `f = 1 - D/th` and `Y <- 1 - (1 - Y)^(1 / (1 + 2 ah f^2))`.
- Midtone `m` = Midtone/100: `Y <- Y + 2m (Y - 0.5) Y (1 - Y)` (monotonic for |m| <= 1, zero at black and white).
- Color `k` = Color/100: each channel `c' = Y' + (c - Y) (Y'/Y)^k`, the ratio floored at 1/1024. 0 keeps absolute chroma, +100 keeps channel ratios, negative values mute. Neutral pixels stay neutral; a pixel whose tone did not change keeps its exact bytes.
- Black Clip applies only while the Shadows range is active, White Clip only while Highlights is: one merged R+G+B 256-bin histogram of the result over visible pixels; the black point is the first level whose running count exceeds Clip % of the samples (white point likewise from 255), then a linear stretch maps them to 0 and 255. A clip of 0 disables that end.

Unverified against Photoshop: curve strength (full Amount over a black neighborhood is gamma 1/3), the Tone falloff, Radius as Gaussian sigma, the Color scaling, the Midtone curve, and the clip semantics, in particular whether Photoshop clips the whole image as Patchy does. Consequence of the clip design: a flat area that is the darkest tone in its layer is pinned back to black by the default 0.01 % Black Clip.

## Smart Filter omission

Photoshop also applies Shadows/Highlights as a Smart Filter. Patchy does not: its native Smart Filters are the catalog filters with a native descriptor mapping (`native_smart_filter_kind_for`), Image > Adjustments commands keep the Smart Object refusal (docs/smart-objects.md), and authoring Photoshop's Shadows/Highlights Smart Filter descriptor without Photoshop to verify it risks files Photoshop warns about. Rasterize first.

## Performance

The box blur is O(pixels) at any radius and single-threaded. The per-pixel pass splits rows across workers for buffers of 1 Mpx and up (sequential on wasm, where previews already run on a pooled worker). Memory: one 16-bit plane per distinct radius, plus an alpha plane and blur scratch for layers with transparency. Previews render at full resolution on the async destructive preview worker.

## Patent record (October 2026)

Claim check against the public claim text on Google Patents:

- Adobe US 7412105 B2 "Tone selective adjustment of images" (filed 2003-10-03): claim 1 is shadow and highlight neighborhoods sized by parameters, neighborhood intensities, tone functions with shape parameters, and shadow/highlight local weights driving the adjustment, the classic Shadows/Highlights design. Expired 2025-09-19. Continuations US 7853096 B1 (expired 2024-04-14) and US 8260080 B1 (expired 2023-10-28; its claims add neighborhoods inside saturation-determined tonal regions).
- Adobe US 7466868 B2 "Determining parameters for adjusting images" (automatic parameters by minimizing a distribution objective): expired 2025-03-31, continuation US 7783127 B1 expired. Patchy computes no parameters anyway.
- Active nearby: Apple US 8958658 B1 and continuation US 9369684 B2 "Image tone adjustment using local tone curve computation" (to 2033): per-block luminance histograms, target luminance values, and an optimization solving per-block tone adjustment values. Patchy has no blocks, no local histograms, no targets, and no optimization.
- Prior art for the design: N. Moroney, "Local color correction using non-linear masking", IS&T/SID Color Imaging Conference 2000 (a blurred luminance mask driving a per-pixel gamma).
- Not searched claim by claim: edge-aware local tone mapping (local Laplacian pyramids, bilateral or guided filtering). Patchy uses none of those techniques.

The binding boundary is the Shadows/Highlights bullet in [legal-constraints.md](legal-constraints.md).

## Tests

Core `shadows_highlights_defaults_lift_shadows_and_zero_amounts_are_identity`, `shadows_highlights_controls_are_monotonic`; UI `ui_shadows_highlights_lifts_selected_shadows_as_one_undo_step` (basic and expanded views, preview, selection, one undo step, Save Defaults, Cancel).
