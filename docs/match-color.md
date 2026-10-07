# Match Color

Image > Adjustments > Match Color... (`imageAdjustMatchColorAction`, hotkey id `image.match_color`, no default shortcut) sits directly after Desaturate, as in Photoshop. It is destructive only (Photoshop has no Match Color adjustment layer) and uses the same gates as the other destructive dialogs: 8-bit RGB pixel layer, Smart Objects refused, text and shape layers through `prompt_rasterize_procedural_layer`, locked pixels refused, disabled in channel view. Flow: `MainWindow::match_color_dialog` (`main_window_destructive_adjustments.cpp`), widgets in `ui/match_color_dialog.cpp`, math in `core/match_color.{hpp,cpp}`, pixels through `apply_match_color_to_pixels` (`filter_workflows.cpp`). Coverage: `match_color_statistics_transfer_and_controls` (core), `ui_match_color_matches_another_document_inside_selection` (UI).

## Dialog

- Destination Image: `Target: <document> (<layer>, RGB/8)` and Ignore Selection when Applying Adjustment (enabled only with a target selection; applies to the whole layer).
- Image Options: Luminance 1..200 (100), Color Intensity 1..200 (100), Fade 0..100 (0), Neutralize.
- Image Statistics: Use Selection in Source to Calculate Colors (enabled only when the chosen source document has a selection), Use Selection in Target to Calculate Adjustment (enabled only with a target selection), Source (None, then every open document, the target included so two layers of one image can be matched), Layer (the source's color-bearing layers top first, groups and adjustment layers skipped, then Merged; defaults to the source's active layer, else Merged). Checkboxes default unchecked.
- Load Statistics / Save Statistics (Photoshop's `.sta` files) are not implemented.
- Live preview through the shared async launcher (`make_destructive_adjustment_preview_state`); OK commits one undo step named Match Color, applied inside the target selection unless Ignore Selection is checked.
- Statistics are computed once per Image Statistics choice on the UI thread and cached for the dialog. The target layer always contributes its original pixels, never the preview; Merged on the target document flattens a copy with the original pixels restored. A source closed while the dialog is open counts as None.

## Math

All constants live in `core/match_color.cpp`.

1. sRGB 8-bit to linear (IEC 61966-2-1), to XYZ with the sRGB D65 matrix, to CIE L\*a\*b\* with the D65 white.
2. Statistics per channel: weighted mean and population standard deviation. Pixel weight = alpha/255 x coverage/255, where coverage is the selection mask (0 or 255) or, for Merged, the compositor's merged alpha. Rows fan out across threads and sums reduce in row order, so results do not depend on thread count.
3. Per channel: `k = max(sd_s, 2) / max(sd_t, 2)`, `c' = mean_s + k (c - mean_t)`. The 2-unit floor keeps a near-flat channel from amplifying noise and keeps a self-match at exactly k = 1. Source None, an empty source, or an empty target uses the target's own statistics.
4. Neutralize replaces the matched a\* and b\* means with 0, removing an overall cast while keeping the matched spread.
5. Luminance multiplies L\* by Luminance/100; Color Intensity multiplies a\* and b\* by Intensity/100 (1 is near gray). L\* clamps to 0..100; conversion back to sRGB clips each channel.
6. Fade blends in 8-bit RGB: `out = (adjusted x (100 - Fade) + original x Fade + 50) / 100`.

Fade 100, or a self-match with default sliders and Neutralize off, is an exact identity and skips the pixel pass. Alpha is never changed.

## Photoshop parity

Names, ranges, defaults, groups and menu placement follow Adobe's published Match Color help. Nothing is calibrated against Photoshop pixels: the working color space, every formula above, the deviation floor, Neutralize's exact meaning, the Fade blend space, the Layer list order and default, and the checkbox defaults are approximations that need a Photoshop capture before any parity claim.

## Patent note (claim check, October 2026)

Read on the public claim text (Google Patents, full PDFs):

- Apple [US 8842911](https://patents.google.com/patent/US8842911B2/en) (luma-based color matching, active to 2033-04-02): independent claims require luma transforms that equalize the percentage of pixels in luma ranges (claim 1), per-luma-value average saturation matching (3), percentile brightness ranges plus sequential hue and saturation operation sets (11), or at least two brightness ranges with a transform per range (19). Continuation [US 9374504](https://patents.google.com/patent/US9374504B2/en) (to 2031-05-27): initial luma range sets split or combined, then per-range transforms.
- Adobe [US 9857953](https://patents.google.com/patent/US9857953B2/en) (image color and tone style transfer, to 2036-04-08): every independent claim (1, 10, 17) pairs a chrominance transfer driven by the input color covariance with a luminance tone-mapping curve fitted by non-linear optimization (1, 10) or matching luminance histograms (17).
- Xerox [US 8031202](https://patents.google.com/patent/US8031202B2/en) (to 2030-07-13): palette mixture models adapted by MAP estimation.
- Expired or lapsed: Apple US 8611655 (hue ranges) and US 8594426 (segmentation), Autodesk US 7003178, Google US 7796812 (iterated 1-D histogram matching). Adobe US 7664322 (to 2028) covers redeye and feature probabilities, not color matching.

The implementation stays outside every active claim above: one global affine map per Lab channel from mean and standard deviation (Reinhard et al. 2001, and Photoshop's own 2003 Match Color, are prior art), with no luma or hue ranges, no percentiles or histograms, no tone curve, no cross-channel covariance or rotation, no segmentation or regions, no learned models, and no face or skin handling. The binding rule is in [legal-constraints.md](legal-constraints.md).
