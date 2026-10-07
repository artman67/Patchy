# Select > Color Range

Photoshop's Color Range command: a soft selection scored from color alone. The binding legal summary is the Color Range bullet in [legal-constraints.md](legal-constraints.md); the claim record is at the end of this page. Other selection engines are in [selection-tools.md](selection-tools.md).

## Where it lives

- Menu: Select > Color Range... (`selectColorRangeAction`, hotkey id `select.color_range`, no default key), its own group after Deselect Layers as in Photoshop. Blocked in Quick Mask like the other selection commands.
- `MainWindow::color_range_dialog` (main_window_layer_ops.cpp) renders the visible composite once with `qimage_from_document`, copies the current selection with `selection_as_grayscale`, holds the preview-dialog edit lock, and commits through `replace_selection_from_grayscale` with the history label "Color Range" (one undo entry; Cancel records nothing).
- `request_color_range` (`src/ui/color_range_dialog.{hpp,cpp}`) is the non-modal dialog. The scoring is Qt-free in `src/core/color_range.{hpp,cpp}`. Its sample editing (`apply_color_range_sample`), preview box, preview fit and eyedropper buttons are shared with Image > Adjustments > Replace Color ([replace-color.md](replace-color.md)).

## Behavior

- **Select** (`colorRangeSelectCombo`): Sampled Colors, Reds, Yellows, Greens, Cyans, Blues, Magentas, Highlights, Midtones, Shadows. Skin Tones and Out of Gamut are not offered.
- **Sampled Colors**: the sample list starts with the foreground color. The eyedropper replaces the list with the clicked color, Add (+) appends, Subtract (-) adds a removal sample; Shift and Alt switch any eyedropper to Add and Subtract for one click. Clicks work on the canvas (a `set_transient_read_interaction` reader that works under the edit lock) and in the dialog preview, which maps back to document pixels. Each click reads one pixel of the composite (Photoshop's Point Sample); dragging does not sample.
- **Score** (`color_range_alpha`): weighted RGB distance ("redmean", integer weights, one sqrt) scaled so black to white is about 255. A sample selects fully within Fuzziness / 2 and falls off linearly to 0 at Fuzziness; Fuzziness 0 selects the exact color only. The strongest add sample wins; removal samples multiply the result by `255 - strongest removal`. Fuzziness is 0..200, default 40, and only applies to Sampled Colors.
- **Color families** reuse the Hue/Saturation hextants (`kHueSaturationDefaultBandRanges`: full between the inner stops, linear to the outer ones) times a chroma weight that reaches 1 at chroma 128, so gray never qualifies.
- **Tonal presets** score Rec. 601 integer luma against the Lab lightness windows Photoshop's classic dialog documented (Shadows full to L 40, out by 55; Midtones full 55..75, fading over 40..55 and 75..85; Highlights full from 80, fading in from 75), converted to sRGB gray levels: Shadows 0..94 then 0 at 132, Midtones 132..185 with shoulders 94..132 and 185..212, Highlights from 198 with a shoulder from 185. Shoulders are linear in luma.
- **Invert** flips the color score. A pixel's score is then multiplied by its composite alpha, so fully transparent pixels are never selected, inverted or not.
- **Combining** follows the active selection tool's options-bar mode (`selection_mode()` when the active tool is a selection tool, otherwise New): Add takes the max, Subtract multiplies by the complement, Intersect takes the min. New keeps Photoshop's rule that Color Range works within an existing selection (Adobe's help wording, recalled; the page was not reachable to re-check), so with a selection New behaves like Intersect; deselect first for a whole-image range. With no selection every mode except Subtract yields the color range itself (`combine_color_range_mask`).
- **Preview box** (`colorRangePreview`, 240 px): Selection shows the final combined selection in grayscale, Image shows the composite. It is scored on a fitted copy (whole-pixel upscaling for small documents); OK scores the full image.
- **Selection Preview** (`colorRangeSelectionPreviewCombo`): None (default), Grayscale, Black Matte, White Matte, Quick Mask (50% red over unselected). The overlay is a document-size image from `color_range_canvas_preview_image`, recomputed at full resolution through a `CoalescedPreviewEmitter` and drawn by `CanvasWidget::set_selection_preview_overlay` after the composite, the same way the Quick Mask overlay is drawn (no mip chain).
- Persisted: `tools/colorRangeSelect` (tokens `sampledColors`, `reds`, `yellows`, `greens`, `cyans`, `blues`, `magentas`, `highlights`, `midtones`, `shadows`), `tools/colorRangeFuzziness`, `tools/colorRangeInvert`, `tools/colorRangeSelectionPreview` (`none`, `grayscale`, `blackMatte`, `whiteMatte`, `quickMask`); written on OK only (compatibility contract). Samples do not persist.

## Calibration status

No Photoshop was available when this was built (October 2026), so nothing is calibrated against it. Approximations to check with Photoshop COM before claiming parity: the distance metric and plateau-plus-ramp falloff (Photoshop's exact Fuzziness curve is unpublished), whether multiple samples combine per sample or as one color box, the family chroma weight, the tonal windows (from the Photoshop CS3-era documentation quoted in Adobe's forums, applied to luma rather than Lab L, so colored pixels can differ; newer Photoshop adds Range sliders whose defaults were not checked), and whether Photoshop scores transparent composite pixels. Sampling the visible composite rather than the active layer is an unverified assumption (it matches Select > Similar).

## Left out on purpose

Localized Color Clusters and its Range slider, Skin Tones and Detect Faces (patent and policy area; never add without a new review), Out of Gamut (no CMYK gamut model), the Highlights/Midtones/Shadows Fuzziness and Range sliders of newer Photoshop (the presets use the fixed windows above, like Photoshop's classic dialog), Load/Save of `.axt` settings, drag sampling, the Eyedropper Sample Size option (always one pixel), and the Ctrl preview toggle.

## Tests

Core: `color_range_fuzziness_plateau_then_linear_falloff`, `color_range_add_and_subtract_samples`, `color_range_color_families_and_tonal_ranges`, `color_range_mask_weights_alpha_and_combines_with_selection`. UI: `ui_color_range_samples_canvas_and_preview_into_undoable_soft_selection` (canvas and preview sampling, the Grayscale canvas overlay, edit lock, partial alpha, Undo; artifacts `ui_color_range_dialog`, `ui_color_range_canvas_grayscale_preview`, `ui_color_range_selection`) and `ui_color_range_presets_follow_selection_combine_mode`.

## Patent record (claim check 2026-10-06)

Engineering research, not legal advice; same caveats as [patent-research.md](patent-research.md). Classic Color Range (global per-pixel color distance with a fuzziness falloff, fixed hue and tone presets, a grayscale preview updated when a control changes) shipped in Photoshop in the mid 1990s and is prior art. Claims read on Google Patents:

- **US 8050498** (Adobe, live coherent selection, to 2029-11-03). Claim 1 classifies pixels *coherently* as foreground or background *while* the sample-defining input is being received and updates the display as the sample is defined. Patchy's score is per pixel and independent of neighbours (not coherent), and a sample is one click whose score runs after the press; drags never sample. Keep it that way: no drag or brush sampling with live scoring, and no spatial smoothing or graph cut in the score.
- **US 8004536** (Adobe, coherent selection, to 2029-04-16). Claim 1 assigns each pixel an energy that is a function of its distance from the selection location and combines it with neighbour differences. Patchy never uses the sample's position or neighbour differences. This is also why Localized Color Clusters (which weights by distance from the sample point) is left out.
- **US 11223744** (Adobe, local color range selection, to 2037-12-02). Claim 1 requires a user-drawn spatial region, a range mask, and a parameter adjustment weighted by their combination. Color Range only produces a selection; it applies no adjustment.
- **US 8264500** (local color models for mask boundaries, to 2026-12-01) and **US 8879835** (RBF color similarity combined with a texture locality mask, to 2033-02-28): no boundary color models, radial basis functions, or locality masks are used.

Any change that scores a pixel from anything other than its own color and the user's samples (position, neighbours, clusters, learned skin or face models) needs a new check.
