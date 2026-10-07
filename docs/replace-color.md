# Image > Adjustments > Replace Color

Photoshop's Replace Color command: a Color Range "Sampled Colors" score weights a Hue/Saturation master shift of the active layer's pixels. The binding legal summary is the Replace Color bullet in [legal-constraints.md](legal-constraints.md); the claim record is at the end of this page. The scoring and sampling it shares are documented in [color-range.md](color-range.md).

## Where it lives

- Menu: Image > Adjustments > Replace Color... (`imageAdjustReplaceColorAction`, hotkey id `image.replace_color`, no default key), after Desaturate, where Photoshop groups it. Disabled in channel views like the other destructive adjustment dialogs.
- `MainWindow::replace_color_dialog` (main_window_destructive_adjustments.cpp) follows the destructive adjustment flow: the procedural-layer rasterize gate, smart-object and pixel-lock refusals, the async canvas preview (`make_destructive_adjustment_preview_state`), the preview-dialog edit lock, a progress dialog on OK, and one undo entry "Replace Color". Cancel and "made no changes" record nothing.
- `request_replace_color` (`src/ui/replace_color_dialog.{hpp,cpp}`) is the non-modal dialog. The math is Qt-free in `src/core/replace_color.{hpp,cpp}`; `apply_replace_color_to_pixels` (filter_workflows.cpp) runs it per selected row span, with the strip fan-out on previews.

## Behavior

- **Selection** group: eyedropper, Add (+) and Subtract (-) buttons, the Color swatch (the first added sample), Fuzziness 0..200 (default 40), the 240 px preview box and the Selection / Image radio (Selection is the default). The sample list starts with the foreground color, as in Color Range.
- **Sampling** is Color Range's model (`apply_color_range_sample`, `ColorRangeSamplerButtons`): the eyedropper replaces the samples, Add appends, Subtract adds a removal sample; Shift and Alt switch any eyedropper to Add and Subtract for one click. Clicks work on the canvas (`set_transient_read_interaction`, under the edit lock) and in the preview box. Each click reads one pixel of the active layer's own pixels, from the copy taken before any preview, so the live preview never feeds back into a sample; a click where the layer is transparent or absent is ignored. Drags do not sample.
- **Score**: `color_range_alpha` with Sampled Colors (same distance, plateau and linear falloff, strongest add sample, removal samples multiply by the complement). Alpha does not weaken the score.
- **Replacement**: Hue -180..180, Saturation -100..100, Lightness -100..100, all starting at 0. The shift is Hue/Saturation's calibrated master math (`apply_adjustment_to_color` with colorize off and no bands; [adjustments-calibration.md](adjustments-calibration.md)). Each pixel moves linearly toward its fully shifted color by score / 255, rounded; a fully scored pixel equals Hue/Saturation with the same sliders. The Result swatch shows the Color swatch fully shifted. All sliders at zero, or no sample, is a no-op.
- **Preview box**: Selection shows the score of the layer (placed at its document position, fitted with `fit_color_range_preview`, weighted by the layer's alpha) and black outside an existing selection; Image shows the layer thumbnail.
- **Existing selection** limits the pixels the command writes through the same `QRegion` row-span walk as Hue/Saturation and the other destructive adjustments, so a feathered selection acts at full strength wherever it is non-zero.
- **Preview** checkbox toggles the live canvas preview (coalesced, async).
- Persisted: `tools/replaceColorFuzziness`, written on OK only (compatibility contract). Samples and the sliders do not persist.

## Shared with Color Range

`apply_color_range_sample` (core), `ColorRangePreview`, `fit_color_range_preview`, `color_range_gray_image` and `add_color_range_sampler_buttons` (color_range_dialog.hpp) were moved out of Color Range's dialog so both commands share them. Color Range's behavior and object names are unchanged.

## Calibration status

No Photoshop was available (October 2026), so nothing is calibrated against it. Approximations to check with Photoshop COM: everything listed for Color Range's Sampled Colors score; that Replace Color's sliders equal the Hue/Saturation master sliders (Lightness in particular); the linear blend by score; whether Photoshop's eyedroppers read the active layer or the composite, and whether its Image view shows the layer or the composite; whether its initial Color is the foreground color; and whether it persists the sliders. Photoshop feathers by a soft selection; Patchy, like its other destructive adjustments, does not.

## Left out on purpose

Localized Color Clusters and its Range slider (patent area, see below; never add without a new review), clicking the Color or Result swatch to open a color picker, Load/Save of `.axt` settings, drag sampling, the Eyedropper Sample Size option (always one pixel), and an adjustment-layer form (Photoshop has none).

## Tests

Core: `replace_color_weights_hue_saturation_shift_by_sampled_score`, `replace_color_add_and_subtract_samples`. UI: `ui_replace_color_samples_previews_and_commits_one_undo_step` (canvas and preview sampling, Shift/Alt, live preview under the edit lock, Result swatch, one undo step, Undo, Cancel, an existing selection; artifacts `ui_replace_color_dialog`, `ui_replace_color_canvas_preview`, `ui_replace_color_in_selection`).

## Patent record (claim check 2026-10-07)

Engineering research, not legal advice; same caveats as [patent-research.md](patent-research.md). Replace Color (a fuzziness-masked hue, saturation and lightness shift of sampled colors, limited to the current selection) has shipped in Photoshop since the mid 1990s and is prior art. Every Color Range claim in [color-range.md](color-range.md) applies to the scoring and sampling. Claims read on Google Patents:

- **US 8218860** (Adobe, replacing color ranges, to 2031-05-11). Every independent claim (1, 12, 23) builds a histogram of hue values and determines a prominent hue range from it, then maps it to a replacement range. Patchy never builds a histogram or finds prominent colors: the range comes only from the user's clicked samples, and the shift is a relative Hue/Saturation offset.
- **US 8340406** (Adobe, location-weighted color masking, to 2031-10-26). Every independent claim (1, 12, 13, 23, 24, 34) builds a Gaussian color model whose soft mask depends on proximity to the selected pixel's location. Patchy's score ignores position and uses a plateau with a linear ramp; this is the Localized Color Clusters design and why it stays out.
- **US 11223744** (Adobe, local color range adjustment, to 2037-12-02). Claim 1 weights an adjustment by a combination of a range mask and a spatial mask generated from a user drawing around a region as part of the tuning; claims 7 and 12 add a Gaussian width or Lab point/area models. Replace Color draws no region: it uses whatever selection already exists, through the generic row-span limit every destructive adjustment (Hue/Saturation's hue bands included) applies, never a soft spatial weight, and builds no Lab or Gaussian model. That is the 1990s Replace Color behavior.
- Also checked: US 7009733 (Corel, tone curves, expired 2023).

Any change that scores from anything but a pixel's own color and the user's samples, derives the range from image statistics, weights the effect by a soft spatial mask, or adds a picker-driven target color mapping needs a new check.
