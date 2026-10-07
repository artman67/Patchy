# Adjustment calibration vs Photoshop

Calibration record for adjustment-layer and auto-adjustment math: Brightness/Contrast (legacy and modern), Curves, the auto adjustments, Hue/Saturation, Exposure, and the still-uncalibrated Gradient Map, Selective Color, Black & White and Channel Mixer. Read this before touching src/core/adjustment_layer.* or src/filters/auto_levels_math.*.
Conventions: "PS" = Adobe Photoshop 2026/27.8, the installed ground truth; every rule is pinned by PS COM captures unless noted. Fixtures named `photoshop-*` live in `test-fixtures/psd/`; `local-test-fixtures/` is machine-local. The COM workflow lives in [ps-compat.md](ps-compat.md).

## Brightness/Contrast legacy calibration (July 2026)

Nine 256-gray-ramp captures pin `brightness_contrast_channel_value` (core/adjustment_layer.cpp):

- c = 0: `clamp(v + b)`.
- 0 < c < 100: brightness folds into the INPUT: `clamp(lround((v + b - 127.5) * 100/(100-c) + 127.5))`. The slope is `100/(100-c)`, NOT the destructive filter's `(100+c)/100`.
- c = 100: hard threshold `(v + b) >= 127 ? 255 : 0`.
- c < 0: contrast compresses FIRST, brightness adds to the OUTPUT: `clamp(lround((v - 127.5) * (100+c)/100 + 127.5) + b)`.

The hybrid order (brightness inside for positive contrast, outside for negative) is required by the interaction captures; residual +/-1 (PS's own LUT has sub-LSB noise). Do not "unify" this with the destructive `patchy.filters.brightness_contrast` in either direction: the destructive output is byte-pinned, and the adjustment layer must render PS's math because it round-trips as a native legacy `brit` record.

## Modern Brightness/Contrast (July 2026)

The default (useLegacy=false) algorithm is fully modeled, recovered from 300 16-bit ramp captures (PS computes in its 15-bit 0..32768 space), 150 contrast captures, and a 451-curve 8-bit sweep (captures/scripts in `local-test-fixtures/ps-bc-captures/`; implementation with exact constants in core/adjustment_layer.cpp):

- `result = contrast(brightness(v))` on the unit interval, one final rounding per depth (compose the real-valued curves, not byte LUTs).
- 0 < b <= 100: gain ray `sigma = 2^(b/110)` until output 0.5, then one cubic Hermite to (1, 1) with end slope `tau = max(0.1, 1/(1 + 12*(sigma-1)))`, clamped to 1 (legitimate overshoot for b > 88, where the tau floor engages).
- b > 100: `f_b = f_{b-100} o f_100`. b < 0: the exact functional inverse of `f_{-b}`, evaluated by fixed 64-step bisection (deterministic; NOT a mirrored construction).
- Contrast: `beta = 1 - 0.0076*c`; lower half `y = (2-2*beta)*u^2 + beta*u`, upper half mirrored through (0.5, 0.5). Exact for c in -50..100.
- The stored `means` value is inert at render time (dialog Auto metadata); Patchy preserves imported values, writes 127 otherwise. `sigma` multiplies out the literal 110th root of 2 (1.006321233202252) per step instead of calling `exp2`, for determinism.
- Validation: all 450 15-bit captures within 1 LSB15; 449 of 452 8-bit curves byte-exact, the rest within 1/255.

## Curves 2.0 engine and editor

- `CurvesAdjustment`: independent ordered control points for Composite RGB, R, G, B; 2-19 points, values 0-255, duplicate inputs resolve deterministically, first/last input may move inward; outside them the LUT clamps to that point's output.
- `build_curve_lut` is the ONLY point-curve interpolation path: natural cubic through the points, zero second derivative at both endpoints, clamped outside movable endpoints, rounded to nearest byte. `build_curves_lut` applies component tables first, then Composite. All Curves consumers (destructive, adjustment render, thumbnails, editor graph) use these LUTs; ramp calibration matched all 3,072 measured bytes exactly.
- Old Patchy 3-value data loads as Composite anchors at 0/128/255; `plAD` v4 may carry the `CRV2` v1 tail (bounded to 324 bytes; malformed tails fall back to the anchors). Editable saves migrate to native `curv` and OMIT `plAD` (PS warns on a second private adjustment block); malformed native `curv` stays on the opaque Pixel path retaining both raw blocks. Levels and Hue/Saturation followed as the last `plAD` writers (native `levl`/`hue2` carry every modeled value); the Levels dialog's channel tab persists only when importing legacy files (fresh saves reset to composite, matching PS).
- Native `curv` shape: zero map-mode byte, version 1, big-endian u32 changed-channel bitmap (despite Adobe's table saying two bytes), implicit point records, indexed `Crv ` version 4 extension, padded to four bytes. Channel ids/bits: Composite=0, R=1, G=2, B=3; points are output then input; identity = bitmap/count zero, 20-byte payload. Patchy writes that shape, keeps untouched payloads byte-for-byte, regenerates only after a modeled edit; native blocks are authoritative over `plAD`. COM probes round-trip Patchy files as editable `LayerKind.CURVES`.
- Editor: four-channel histogram, one box-averaged sample per 2x2 block over the full image (integer means over opaque pixels only, all-transparent blocks skipped). The light 2x2 kernel fills codes missing from quantized sources like PS's cache-level-2 histograms; a larger kernel over-concentrates noisy channels into a few towering bins that crush the rest of the linear display (observed on the blue channel of a warm-lit 24MP photo). The composite is the sum of the per-channel counts, and the display is linear with a ceiling: `height = min(1, count / ceiling)` with `ceiling = 4 * total_samples / non_empty_bins` (`histogram_display_ceiling` / `histogram_display_fraction` in ui/curves_editor.hpp, shared by Levels and Curves). PS's Histogram panel is linear to the tallest bin; its Levels dialog is not. Pinned September 2026 (GitHub issue 32) by native captures of PS's Levels dialog over three synthetic 8-bit RGB images with known per-bin counts, measured column by column on the 256x100 plot: (a) 65,536 samples with spikes at 1/8..1/1024 of the total and every other bin at 188 filled the plot from 1/64 up and drew 1/80, 1/96, 1/128, 1/192, 1/256 at 80, 67, 50, 34, 25 px; (b) the same spikes with the remainder in one bin (12 non-empty bins) drew 1/8 at 38 px and 1/16 at 19 px (ceiling 21,845 = 4 * 65,536 / 12); (c) 40,000 samples over 122 non-empty bins, 20 of them holding one sample, drew 1/48, 1/64, 1/128, 1/256 at 64, 48, 24, 12 px (ceiling 1,311 = 4 * 40,000 / 122; a single-sample bin counts as non-empty). Refuted, do not re-derive: linear to the tallest bin, `sqrt(count / max)` (the August 2026 fit, which only compared against linear and log), log, power 1/3, a fixed share of the total (1/64 matched (a) only), and any median or percentile base ((b) has median 0). PS also resamples the bar image with a smoothing filter, so a one-bin dip between taller neighbours renders dimmer rather than shorter; Patchy draws exact columns. Non-empty bins never draw under 1 px. Histogram columns draw unantialiased and inset 1px from the graph frame so a clipping spike in bin 0 or 255 stays visible instead of vanishing under the border. Probe generator and captures: `local-test-fixtures/histogram-scale-probe/` (machine-local). Dialog Auto consumes the same averaged histograms, and the Levels dialog shares the sampler over the active layer. Re-editing an unclipped layer samples the layer-tree prefix below it (Auto sees the input); Auto is disabled for clipped adjustments. `.acv` Load accepts version 4 counted RGB, legacy version 1 bitmaps, and the indexed `Crv ` extension; Save writes PS's five-curve RGB shape with the trailing identity compatibility curve.

## Levels stage order and Posterize (October 2026)

Fitted against Photoshop's renders of psd-tools' `adjustments/levels_rgb.psd` and `posterize_rgb.psd` (GitHub issue 65; fetch with `testy/fetch_psd_tools_corpus.py`).

- **Levels:** the component channel record runs FIRST, then Composite RGB (the Curves order), and the channel result reaches the composite stage unrounded. Within 2/255 everywhere on the render; composite-first is off by up to 52, and a byte-rounded intermediate has ten times the 2/255 misses. `apply_levels` (core) and `build_levels_luts` (ui/filter_workflows.cpp, the destructive command) both follow it. A master-only adjustment is unchanged.
- **Posterize:** `floor(floor(value * levels / 256) * 255 / (levels - 1))`, byte-exact at 3, 7, 13 and 21 levels (3 levels give 0, 127, 255). Refuted: nearest-step rounding (the pre-October formula), and rounding the output step instead of truncating it. `posterize_channel_value` serves the adjustment layer and `patchy.filters.posterize` alike.

## Exposure (October 2026)

`AdjustmentKind::Exposure`, Photoshop's `expA` block (16 bytes: u16 version 1, float32 exposure, offset, gamma, 2 pad bytes). Settings are integers at Photoshop's field precision: hundredths of a stop (-20.00..20.00), ten-thousandths of offset (-0.5000..0.5000), hundredths of gamma (0.01..9.99). An unedited imported block is written back byte for byte, which keeps Photoshop's exact floats.

- Math (`exposure_channel_value`), per channel on 8-bit values: `linear = (v/255)^2.2`, `exposed = linear * 2^exposure + offset`, `corrected = max(0, exposed)^(1/gamma)`, `out = 255 * clamp(corrected)^(1/2.2)`. Within 1/255 on every pixel of Photoshop's render of psd-tools' `adjustments/exposure_rgb.psd` for all four of its setting triples.
- Refuted: the piecewise sRGB curve in place of the plain 2.2 power (up to 7/255 off at +2 stops).
- Unprobed: Grayscale and CMYK documents (Photoshop adjusts in the document's space; Patchy converts to sRGB on open first) and 16/32-bit sources.

## Vibrance (October 2026)

`AdjustmentKind::Vibrance`, Photoshop's `vibA` block: u32 descriptor version 16, then a `null` descriptor holding `vibrance` (stringID) and `Strt` (charID, the Saturation slider), both `long`, -100..100. Adobe's [PSD specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) names only the key and the descriptor wrapper; the item keys are Photoshop's Action Manager names. A missing item reads as 0, a `doub` item rounds; Patchy writes both items. An unedited imported block is written back byte for byte.

- Math (`apply_vibrance`), on HSL saturation `s = (max-min) / (255 - |max+min-255|)` at fixed `L = (max+min)/2`: `s1 = 1 - (1-s)^(2^(vibrance/100))`, then `s2 = min(1, s1 * (1 + saturation/100))`, each channel `L + (c-L) * s2/s`, rounded. Hue and HSL lightness are kept, neutrals never change, no channel clips. Vibrance +100 about doubles a muted color and leaves full saturation alone; -100 about halves a muted color and barely touches vivid ones. Saturation -100 is gray at L, +100 doubles s up to 1.
- Uncalibrated. Written from Adobe's published behavior (Vibrance raises less-saturated colors more and limits clipping near full saturation; Saturation applies the same amount to every color) with no Photoshop render to fit; the curve shape, the stage order and the Saturation endpoints are Patchy's choices. psd-tools' adjustment corpus (`testy/fetch_psd_tools_corpus.py`) is the calibration route, as for Exposure. Photoshop has not yet opened a Patchy-written `vibA` (warning-free open, editable Vibrance layer): owed. Today it round-trips through Patchy's reader only.
- Deliberate deviation: no skin-tone protection (patent note below). Photoshop damps warm, skin-like hues; Patchy's strength depends only on saturation, so skin saturates more than in Photoshop.
- Image > Adjustments > Vibrance... rewrites the active pixel layer through the same function (`apply_vibrance_to_pixels`), equal to a Vibrance layer with the same settings. CMYK and grayscale documents keep the RGB math, like Hue/Saturation.
- Patent note (2026-10-06, claim text on Google Patents). Apple US 8638338 (to 2032-03-22): claims 1, 8, 12, 16 and 21 need one control that adjusts colors inside a color sub-region (skin) by their deviation from a reference color and colors outside it UNIFORMLY; claim 25 needs a region selector and one control that adjusts one pixel set uniformly and another non-uniformly. Its continuation US 9639965 (to 2028-09-29): claims 1 and 8 adjust a pixel inside a color sub-region by its deviation from the NEAREST of several reference colors there and other pixels to a different degree; claim 14 the same with one or more selected reference colors. Adobe US 8406482 (to 2031-05-03): a skin mask from a trained model refined by image-specific skin and non-skin models. Patchy's Vibrance is one formula of each pixel's own saturation for every color: no color sub-region, no reference or skin color, no hue term, no image statistics, so no pixel set is adjusted uniformly and nothing is measured against a reference color. The Saturation slider is the long-standing uniform scale with the usual gamut cap, the same as Hue/Saturation's master slider. Kodak US 6771311 and Jasc US 6868179 (automatic saturation from image statistics or hue/lightness tables) expired in 2023. Binding rule: [legal-constraints.md](legal-constraints.md).

## Selective Color (NOT calibrated; open item)

Added without Photoshop access (October 2026); nothing below is pinned by a PS capture. `AdjustmentKind::SelectiveColor`, Photoshop's `selc` block, laid out per the [Adobe Photoshop File Formats Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) ("Selective Color"): u16 version 1, u16 method (0 Relative, 1 Absolute), then ten 8-byte records of i16 cyan, magenta, yellow, black percentages (-100..100); the first record is reserved (written zero), the rest run reds, yellows, greens, cyans, blues, magentas, whites, neutrals, blacks. 84 bytes. An unedited import re-emits byte for byte, reserved record included; an edit regenerates. New layers are Relative with all corrections 0; the dialog opens on Reds. PS's Preset menu is not modeled.

- Weights (`selective_color_weights`, 8-bit inputs, always summing to 1): Reds `max(0, R - max(G, B)) / 255` (Greens, Blues alike); Yellows `max(0, min(R, G) - B) / 255` (Cyans lack R, Magentas lack G); Whites `max(0, 2 min - 255) / 255`; Blacks `max(0, 255 - 2 max) / 255`; Neutrals `(510 - |2 max - 255| - |2 min - 255|) / 510`. Equal corrections on all nine colors therefore act globally.
- Correction (`apply_selective_color`): a channel's ink is `1 - v` (cyan for R, magenta for G, yellow for B). Per color, `gain = (1 + plate) (1 + black) - 1`; the ink change is `weight * gain * ink` (Relative) or `weight * gain` (Absolute), summed over the nine colors against the original pixel, then `out = 255 * clamp(v - change)`, rounded. Relative cannot move pure white, as Adobe's help states.
- Basis: Adobe's documented Relative/Absolute definitions (percentage of the existing ink vs of full ink). The chroma/tone weight split and the compounding black term are the shape open reimplementations use (FFmpeg's `selectivecolor` filter documents the same model); Photoshop's real weights and gain are unmeasured.
- No CMYK ink space: a `selc` layer from a CMYK document renders on the RGB math. Photoshop corrects that document's four plates (Black edits K); unmodeled.
- The Image > Adjustments command runs the same function (`apply_selective_color_to_pixels`), so it equals a Selective Color layer over the same pixels.
- Probes owed (PS COM): hue sweeps at full and half saturation and a gray ramp under +100 cyan and +100 black in each method (weights, gain, rounding); the cyan -100 / black +100 cross term; summed vs chained colors; a CMYK document; and warning-free opening of a Patchy-written `selc` (unverified).
- Patent check (2026-10-06, Google Patents claim text): selective color correction of separations is expired prior art (Hell US 4649423; Agfa US 6058207, priority 1995). Nearest active Adobe filings: US 11223744 (to 2037; claim 1 weights an adjustment by a user-drawn region mask combined with a generated range mask) and US 7586499 (to 2027; a 2D color-spectrum control with a superimposed gain control). Patchy's correction is a global point operation from each pixel's own RGB with fixed family weights and plain sliders; the only spatial input is the generic adjustment-layer mask, the 1990s architecture this codebase already ships for Hue/Saturation bands. Binding line in [legal-constraints.md](legal-constraints.md).

## Black & White (NOT calibrated; open item)

Added without Photoshop access (October 2026); nothing below is pinned by a PS capture. `AdjustmentKind::BlackWhite`, Photoshop's `blwh` block: u32 descriptor version 16, then a `null` descriptor. Adobe's [PSD specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) names only the key and the wrapper; the items are Photoshop's Action Manager names: integer weights `Rd  `, `Yllw`, `Grn `, `Cyn `, `Bl  `, `Mgnt` (percent, -200..300), `useTint` (stringID bool), `tintColor` (stringID, an `RGBC` object of `Rd  `/`Grn `/`Bl  ` doubles), plus Photoshop's preset items (`bwPresetKind`, `blackAndWhitePresetFileName`). A missing weight reads as Photoshop's default (40, 60, 40, 60, 20, 80), a `doub` weight rounds, a non-RGBC tint color keeps the default tint. An unedited import re-emits byte for byte; an edit patches the imported descriptor in place (item order, preset items and an unedited tint color kept); a fresh layer writes the six weights, `useTint` and `tintColor` only (no preset items: what Photoshop shows for an absent preset kind is unverified).

- Gray (`black_white_gray`, 8-bit integer math): with `max >= mid >= min` channels, `gray = min + (mid - min) * secondary% + (max - mid) * primary%`, rounded half up, clamped to 0..255. Primary = the largest channel's color (Reds, Greens, Blues), secondary = the pair of the two largest (Yellows R+G, Cyans G+B, Magentas R+B); ties fall on a zero-width share, so the choice cannot matter. Grays keep their value; defaults give pure red 102, yellow 153, blue 51. This is the decomposition open reimplementations of Photoshop's dialog use; Photoshop's actual rounding, working space and curve shape are unmeasured.
- Tint (`apply_black_white`): the tint color laid over the gray in Color blend mode (`blend_rgb`, the PDF set_lum path), so luminosity stays the gray's and black and white stay put. Patchy's Hue/Saturation map to the color as HSB with brightness 100% (`black_white_tint_color`); read back, hue is the hexcone hue and Saturation the channel spread in percent (`black_white_tint_from_color`), the only two things the Color blend uses. Defaults: Tint off, Hue 35, Saturation 25 (Photoshop's dialog defaults as commonly documented; not checked against Photoshop here). Photoshop's own Hue/Saturation to `tintColor` mapping and its tint math are unmeasured, so a Photoshop file's tint shows different dialog numbers in Patchy and may render a different strength.
- Not modeled: Auto, the Preset menu (Photoshop's presets are Adobe data, not reproduced), and click-and-drag on the image to move a slider; see the patent note.
- No ink space: B&W mixes channels, so CMYK and grayscale documents keep the RGB math (a gray document's pixels are already gray and only the tint changes them).
- Image > Adjustments > Black & White... (Alt+Shift+Ctrl+B) rewrites the active pixel layer through the same function (`apply_black_white_to_pixels`), equal to a B&W layer with the same settings. Layer > New Adjustment Layer > Black & White... adds the layer.
- Probes owed (PS COM): a hue wheel at full and half saturation and a gray ramp at the defaults and at single-slider extremes (-200, 300); the tint at several Hue/Saturation pairs over a gray ramp, with the `tintColor` Photoshop stores for each; warning-free opening of a Patchy-written `blwh` and the preset label Photoshop shows for it (unverified).
- Patent check (2026-10-06, claim text from the USPTO full-text PDFs; Google Patents was rate-limited). Adobe US 7706606 (filed 2006-11-01, 825 days term adjustment, to about 2029-02): claim 1 determines color clusters from the image's pixels and gives each color-space point a gray from a weighted average of target differences to the clusters, the image-adaptive conversion an Auto button would be; omitted. Adobe US 7920739 (filed 2006-12-13, 1119 days adjustment, to about 2030-01): claims 1 and 12 select a color adjuster from the color of an area the user picks on the (grayscale) image; the on-image drag is omitted. Apple US 9092893 (continuation of US 8971617, priority 2012-03-06): a single control value mapped along a parameterized color-space path to the weights; Patchy has six independent sliders, the arrangement that patent describes as prior art. Binding rule: [legal-constraints.md](legal-constraints.md).

## Channel Mixer (NOT calibrated; open item)

Added without Photoshop access (October 2026); nothing below is pinned by a PS capture. `AdjustmentKind::ChannelMixer`, Photoshop's `mixr` block. The [Adobe Photoshop File Formats Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) ("Channel Mixer") gives u16 version 1, u16 monochrome, then records of four i16 color percentages plus an i16 constant, without a record count. Patchy reads the first three as the red, green and blue outputs (sources red/cyan, green/magenta, blue/yellow, black, constant; the black source is ignored) and writes four (the fourth a zero black output): 44 bytes. Monochrome keeps the Gray mix in the first record; Patchy also repeats it in the next two. The record count, the fourth record's RGB contents and where Photoshop keeps the gray are inferred from open readers' documentation, not from a Photoshop file. An unedited import re-emits byte for byte (CMYK black data and any tail kept); an edit regenerates.

- Math (`apply_channel_mixer`), per output on encoded 8-bit values: `out = clamp((r% R + g% G + b% B + c% 255) / 100)`, integer, rounded half up. Basis: Adobe's documented behavior (each source slider adds that percentage of the channel; Constant adds white or black). Uncalibrated: the constant's scale (255 per 100%, so +-200% forces black or white for any source total within +-100%, as Adobe's help describes) and the rounding.
- UI: Output Channel (Red, Green, Blue; only Gray while Monochrome), Red, Green and Blue sources and Constant at -200..200%, Total (the three sources, not the constant) with a warning icon above 100%. New layers are the identity, opening on Red. Ticking Monochrome first shows 40/40/20 (Photoshop's widely reported default, unverified); clearing it leaves every output on the gray mix, as Adobe documents for hand tinting, which is also the canonical stored form (`clamp_channel_mixer`). PS's Preset menu is not modeled.
- No CMYK ink space: a CMYK document's mixer (four ink outputs, a Black source) renders its C/M/Y records as R/G/B on the RGB math, so its constant's sign and the black source are wrong; unmodeled, like Hue/Saturation.
- Image > Adjustments > Channel Mixer runs `apply_channel_mixer_to_pixels`, equal to a layer with the same settings.
- Probes owed (PS COM): a Photoshop-saved `mixr` (RGB and CMYK, color and monochrome) for the record count and contents; source and constant sweeps on a gray ramp; the mono default; warning-free opening of a Patchy-written block.
- Patent check (2026-10-07, Google Patents claim text). The channel mixer itself is 1990s prior art (Photoshop 5.0, 1998; GIMP's Channel Mixer). Nearest active claims: Adobe US 8086029 (to 2030-10-24; every independent claim derives the mixer weights by principal component analysis of the image) and Apple US 9092893 (to 2032-09-27; one control value mapped through a chroma-space path to RGB weights for grayscale). Apple US 8462384 (weights from image statistics) has lapsed. Patchy's weights are only the user's sliders and fixed constants: no image statistics and no single-control weight path. Binding rule: [legal-constraints.md](legal-constraints.md).

## Photo Filter (October 2026)

`AdjustmentKind::PhotoFilter`, Photoshop's `phfl` block, laid out per Adobe's [PSD specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) ("Photo Filter"): u16 version (2 or 3), the color, u32 density percent, u8 Preserve Luminosity. Defaults: Warming Filter (85), Density 25 (1..100), Preserve Luminosity on. Image > Adjustments > Photo Filter... (after Color Balance, no shortcut) and Layer > New Adjustment Layer share one dialog (Filter menu or Color swatch, Density, Preserve Luminosity); the destructive command runs `apply_photo_filter_to_pixels`, equal to a layer with the same settings.

- Writer: version 2 with an RGB color structure (space 0, components x257, fourth 0), then 3 zero pad bytes (20 bytes). Every field is spec-defined; ag-psd writes the same shape. An unedited imported block is written back byte for byte.
- Reader: version 2 color spaces RGB, CMYK (naive ink mix) and Lab; other spaces leave the layer unparsed. Version 3's color is three i32 the spec calls "XYZ"; Patchy reads them as Lab x100, as other PSD readers do, which no Photoshop file has confirmed (an out-of-range triple leaves the layer unparsed). A version 3 Photoshop file may therefore show a wrong filter color until it is checked; an unedited save keeps its bytes.
- Math (`apply_photo_filter`): each channel `c * (1 - d + d * f/255)` for filter channel f and density d, rounded; with Preserve Luminosity the result then takes the source's luminosity through the Luminosity blend mode's set-luminosity and gamut clip (`blend_rgb`). Without it the adjustment is per channel and builds a LUT; with it, per pixel. A white filter changes nothing.
- Uncalibrated: written from Adobe's description (a colored lens filter; Density sets the amount; Preserve Luminosity keeps the image from darkening) with no Photoshop render to fit. The multiply model, the blend toward it and the luminosity weights (0.3/0.59/0.11) are Patchy's choices; Photoshop may work in Lab or another space.
- Presets: Warming Filter (85) is (236, 138, 0), the value other PSD tools pair with Photoshop's default. The other 19 colors in `photo_filter_preset_colors()` are commonly published sRGB readings of Photoshop's presets, approximated and unverified. The block stores only the color; the dialog shows Filter when the color EXACTLY equals a preset, otherwise Color.
- Owed: Photoshop has not opened a Patchy-written `phfl` (warning-free open, editable layer, same color) and no Photoshop file has pinned the version 3 color or the math. psd-tools' adjustment corpus is the calibration route, as for Exposure.
- Patent note (2026-10-07, claim text read). Wertheim US 9378563 (filed 2014-02-18, 110 days term adjustment, to about 2034-06 if maintained) multiplies each pixel's RGB by a filter's transmission, but its only independent claim needs a diagnostic image for a color vision disorder, trying filters until the image is acceptable to that person, and correlating the change to the CLOSEST filter in a database. Patchy has no diagnostic image or person and matches a preset only by exact equality for display. The multiply-and-luminosity design is the Multiply and Luminosity blend modes (PDF 1.4, 2001) and Photoshop CS (2003) behavior; Adobe's adjustment-layer patent US 5974198 and the luminance-filter patent US 6731797 have expired. Binding rule: [legal-constraints.md](legal-constraints.md).

## Color Lookup (October 2026)

3DLUT File mode with .cube tables, tetrahedral interpolation and a position-hash Dither; uncalibrated. Math, `clrL` layout, caps and the patent note: [color-lookup.md](color-lookup.md).

## Adjustment layers of CMYK documents (October 2026)

Patchy converts a CMYK file's pixels to RGB when it reads it, but an adjustment layer is
not pixels: Photoshop evaluates it on the ink channels. The same Levels numbers run on RGB
matched Photoshop on 28 percent of the pixels of psd-tools' `levels_cmyk.psd`; run on the
inks they match on 99.9 percent (worst channel miss 7/255 at the 16 pinned probes).

- `InkSpace` (`core/ink_space.hpp`) is the document's CMYK profile sampled both ways:
  sRGB to inks on a 33-node grid, inks to sRGB on a 17-node grid, 16-bit samples,
  integer trilinear and quadrilinear interpolation (deterministic across toolchains).
  `build_cmyk_ink_space` (color module, lcms2, relative colorimetric with black point
  compensation, like the pixel conversion) builds it; the PSD reader registers it under
  an id hashed from the profile bytes and stamps it on every adjustment layer it reads
  (`kLayerMetadataAdjustmentInkSpace`).
- `adjustment_runs_in_ink_space`: Levels, Curves, Invert, Posterize, Brightness/Contrast
  and Exposure. `apply_adjustment_to_color` then takes the color to the inks, maps each
  ink through a 256-entry table built from the ordinary per-channel math, and returns to
  sRGB. Cyan, magenta and yellow read the records an RGB document calls red, green and
  blue; the black ink has its own (`LevelsAdjustment::black_ink`, the `levl` block's
  fifth record, and `CurvesAdjustment::black_ink`, curve index 4), which Patchy used to
  drop. Ink values are the stored ones (0 = full ink), the domain Photoshop's CMYK
  Levels reads. `build_adjustment_lut` returns nullopt for these, so every compositor
  takes the per-pixel path.
- Hue/Saturation, Color Balance, Vibrance, Selective Color, Black & White, Channel Mixer, Photo Filter, Color Lookup and Threshold stay on RGB math in CMYK documents.
- Grayscale documents get the one-channel form (`InkSpace::is_gray`, `build_gray_ink_space`):
  the 256 stored gray values through the gray profile and the nearest-value inverse.
  Their Levels record and curve sit in the slot RGB calls red (index 1; the composite
  stays at the identity), so the reader copies it to green and blue; on red alone it
  tinted the picture. Threshold is channel-wise here too. Photoshop's flatten of
  psd-tools' `levels_grayscale.psd` and `curves_grayscale.psd` is matched within 3/255 and
  1/255 (22/255 without the gray space, a visible tint before the copy). Pinned by
  `psd_tools_grayscale_adjustments_apply_to_the_gray_channel_if_available`.
- No profile, or one lcms2 cannot use: no ink space, RGB math as before.
- The black ink's record is never written: Photoshop 2026 silently turns a Levels layer
  of an RGB document into a plain empty layer, mask gone, when the `levl` block's fifth
  record is not the identity (found by Testy the night the record was first written).
  A `curv` payload that carries a fifth curve is regenerated without it for the same
  reason instead of being passed through.
- Gap: Patchy saves RGB. The layer is written as an ordinary RGB adjustment, so
  Photoshop, and Patchy after a reopen in another run (the id is then unregistered),
  evaluate it on RGB again. Pinned by `psd_tools_cmyk_levels_run_on_the_inks_if_available`.

## Auto adjustments calibration (August 2026)

Six self-authored ramp/outlier fixtures through PS COM (`autoLevels()`, `autoContrast()`, the `Lvls` event) vs Patchy's `auto_levels_math`:

- **Auto Tone (per-channel) byte-exact on five of six fixtures**; PS leaves a constant channel untouched, matching Patchy's degenerate-scan identity rule.
- **Auto Contrast is merged-histogram composite semantics** (color relationships preserved), within maxAbs 2 of Patchy's prediction.
- **PS's effective clip thresholds exceed 0.1% on small images** (a 0.18%-per-end probe still clipped; bounded below 0.39%); not chased. Patchy keeps `max(1, samples/1000)` so menu commands and the Levels dialog Auto agree internally.
- **Auto Color is not flag-invocable**: `Lvls` with `AuCo` reproduces monochromatic-contrast output; ScriptListener records menu Auto Color as explicit computed values. Patchy's midtone snap follows PS's documented Auto Options defaults (midtones 128 neutral, shadows 0, highlights 255); the snap formulation is Patchy's own design. Photoshop-like, not pixel identity.

Capture scripts are session-scratch; conclusions are pinned by the core auto-adjustment tests.

## Hue/Saturation calibration (July 2026)

Calibrated with byte-patched `hue2` probes (patch the header triple, PS flatten to BMP, fit each stage by exact interval arithmetic). Shared helpers and the measured tables (`photoshop_lightness_value`, the 1530-step wheel, `photoshop_hsl_reconstruct`, `kColorizeSaturationScale`, `kColorizeHueInterp`, `kMasterSaturationScale`) live in src/core/adjustment_layer.cpp.

### Colorize

- hue2 header = version(=2) u16, colorize u8, pad u8, colorize h/s/l i16 x3, master h/s/l i16 x3; hue stored -180..180 (raw 203 also accepted); colorize-off files still carry (0, 25, 0). Fresh layers append six default hextant band records plus an undocumented 36-byte trailer of (k*60, 100, 50) triples; Patchy writes that exact template (`kPhotoshopHueSaturationDefaultTail`) and patch-in-place preserves imported bands/trailer byte-identically.
- Pipeline (`apply_colorize`): `li = (max+min)>>1`; lightness blends toward white/black and ROUNDS; `delta = min(li, 255-li) * kColorizeSaturationScale[s]` (PS's percent conversion sits ~0.05-0.2% BELOW s/100, hence a per-percent table); `q = li + floor(delta + 0.5)` but `p = li - floor(delta)` (the round/truncate asymmetry reproduces PS's alternating 2l/2l+1 channel sums); `mid = p + floor((q-p) * kColorizeHueInterp[hue]/255 + 0.5)`. `kColorizeHueInterp` is a measured per-degree table (adjacent degrees quantize together, values pin near sector boundaries, sinusoidal deviation up to ~5/255, sectors NOT symmetric; no closed form, all 360 degrees probed).
- Accuracy: 99.62% of 1.04M probed pixels byte-exact, residuals +/-1 (worst +/-2 on 0.16%). Master sliders are ignored by PS while colorize is on. The 'HStr' action path renders identically to the file path. Fixture `photoshop-hue-saturation-colorize.psd`.

### Hue/Saturation master (colorize off)

Calibrated from a 256x284 probe canvas (gray ramp, the full 32,640-entry lightness/chroma grid, three hue strips) under 763 byte-patched slider triples; harness `local-test-fixtures/hue-sat-master-probe/`.

- Stage order (`apply_hue_saturation`): LIGHTNESS runs FIRST and PER CHANNEL, then integer HSL read (`light = (max+min)>>1`), saturation scales the half-chroma, hue rotates the wheel position, shared reconstruction rebuilds the triple.
- Lightness is BYTE-EXACT: PS quantizes the percent to a byte first (`step = |lightness| * 255 / 100` truncated), then rounds the scale toward 0 or 255. The plain `|lightness|/100` form drifts by 1 on 188 of 201 percents (they agree on multiples of 20, which is why colorize probes at 0/+-40 never saw it). Colorize shares this helper.
- Saturation multiplies the half-chroma via `kMasterSaturationScale` (201 entries, indexed delta + 100), fitted by maximum-agreement interval overlap. NEUTRALS ARE NEVER TINTED (`max == min` returns early). No closed form reproduces the table: -100 is exactly 0, 0 exactly 1, -50 exactly 0.5 and +50 exactly 2.0, but +40/+60 sit measurably BELOW `1/(1 - s/100)`, and +100 is 128, not unbounded (PS leaves a chroma-1 midtone at half saturation). Refuted candidates, do not re-derive: `1 + s/100`, `1/(1 - s/100)`, `1/(1 - kColorizeSaturationScale[s])`, `(100+s)/(100-s)`, any HSV-saturation variant.
- The saturation limit clamps the RECONSTRUCTED BYTES, never the incoming chroma: `half_chroma = min(half * ratio, max(min(light, 255-light), half))`. Because `light` floors, normalized saturation legitimately exceeds 1 for `{min == 0, max odd}` and `{max == 255, min even}`; clamping at 1 there breaks the identity, and dropping the `max(base, ...)` limit overshoots saturated dark pixels by up to 17/255.
- Hue rotates by a WHOLE number of wheel steps, `floor(degrees * 4.25 + 0.5)`, not continuous `4.25 * degrees`. Do NOT reuse `kColorizeHueInterp` here: degree-indexed, deliberately non-monotone; inverting it breaks the shift-zero identity.
- Identity: all sliders zero is a BYTE-EXACT identity over the whole RGB cube (bought by the asymmetric q/p rounding); PS's own zero-slider render wobbles by 1 on about half the wheel. Patchy is deliberately the cleaner.
- Accuracy: no probed pixel off by more than 2/255 (61% byte-exact, 97% within 1). Fixture `photoshop-hue-saturation-master.psd` + `.bmp`.
- Destructive Image > Adjustments > Hue/Saturation routes through the same core math (`apply_hue_saturation_to_pixels`, filter_workflows.cpp), bit-identical; Affinity `HsRA` import maps onto this model.

### Per-hue-range bands

Each of the six band records = four i16 range stops in wheel order + an i16 h/s/l triple.

- Weight: a trapezoid over the four stops (0 outside, ramps between outer and inner stops, 1 between the inner pair), linear in degrees, wrapping past 360 (reds default 315/345/15/45); `outer == inner` = hard step.
- Selection is by the pixel's ORIGINAL hue; a master hue rotation does not move it; neutral pixels are never touched.
- Hue: rotations ADD, converted to wheel steps PER BAND (`floor(weight * degrees * 4.25 + 0.5)`), then added to the master rotation.
- Saturation: each band contributes `weight * (kMasterSaturationScale[percent] - 1)`; contributions SUM; `1 + sum` multiplies the master ratio. A full-weight band equals the master slider exactly. Do not multiply per-band ratios or sum percents into one lookup (weight 0.5 at +50 measures ratio 1.5, not `table[+25]`).
- Lightness is NOT the master blend: positive collapses chroma toward the max channel (`min' = min + (max-min) * L/100`), negative toward the min channel (`max' = max + (min-max) * |L|/100`). Overlapping bands SUM weighted percents and apply once. Band lightness runs BEFORE master lightness.
- Accuracy: plateaus within 2/255; feather RAMPS carry up to 7/255 (the reds ramp-in runs a constant 6 wheel steps behind PS, unexplained; ramp-out within 1). Fixture `photoshop-hue-saturation-bands.psd`/`.bmp`: max 7, mean 0.13, 98% within 2, pinned at those bounds.
- Round trip: the six band records are written from the model (header plus 84 bytes); only the undocumented 36-byte trailer stays patch-in-place, so an unedited layer resaves byte-identically.

## Gradient Map (NOT calibrated; open item)

Added without Photoshop access (September 2026). Nothing below is pinned by a PS capture; every rule is a documented choice to confirm or replace by COM probes. Model: `GradientMapAdjustment` (core/adjustment_layer); render: `gradient_map_luminance` + `build_gradient_map_lut`.

- Luminance: `blend_if_gray_value`, integer `(299R + 590G + 111B + 500) / 1000`, the one RGB-to-gray conversion already pinned against PS (Blend If composite gray, [layer-effects-render.md](layer-effects-render.md)). Grays map to themselves. Adobe documents only that the map follows the image's grayscale range; the weights and rounding PS uses here are unmeasured.
- Lookup: a 256-entry table sampled at `index / 255` (reversed as `1 - index / 255`) through the shared `gradient_color`, Classic with the GdFl endpoint smoothing (a 2-stop Classic ramp eases unless smoothness is 0). Perceptual (OKLab) and Linear follow [gradients.md](gradients.md). New maps default to Perceptual, PS's default since 2022 (Adobe community forum reports; unverified here).
- Opacity stops are kept for the file but ignored: the output is the opaque gradient color, blended by the layer's opacity, mask, Fill, and Blend If like every adjustment. Pixel alpha is untouched.
- Dither: the gradient effects' `dither_gradient_color` (-1..+2 per channel from a splitmix64 hash of the document coordinate), so tiles and full renders agree. PS's dither pattern is unknown.
- Probes owed: a gray ramp and the primaries under a black-to-white map in each method and smoothness 0/100 (weights, rounding, Classic easing), a map with opacity stops, and dither on/off statistics.

### `grdm` block

Layout from the [Adobe Photoshop File Formats Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) ("Gradient settings"), plus two details the table omits that the open-source psd-tools reader takes from real files: version 3 adds a 4-byte method key (`Gcls`, `Perc`, `Lnr `, `Smoo`) after the reverse and dither bytes, and each color stop ends with a u16 color type (0 user, 1 foreground, 2 background), 20 bytes instead of 18. The reader accepts both stop sizes; the expansion count (2) and noise-block length (32) must land where expected. Unverified reads: location 0..4096, midpoint percent, opacity 0..255; stop colors RGB (u16), HSB, or gray (0..10000) (CMYK/Lab stops fail the parse and keep the pre-model path: a plain layer re-emitting the raw block); `Smoo` reads as Perceptual (matching the descriptor reader), other method keys as Classic; mode 1 with no color stops reads as Noise, everything else as Solid; noise color model 1 HSB, 2 or 7 Lab, else RGB; channel ranges above 100 are read as 16-bit.

Writes: an unedited import re-emits byte-for-byte; a Reverse, Dither, or Method change patches only those bytes (a version-1 block gains the method key and becomes version 3); a gradient change regenerates a version-3 Solid block (name as a NUL-terminated Unicode string, RGB stops at byte*257, color types kept, padded to 4 bytes). Noise definitions are never authored: the writer and the dialog's preset picker flatten them to 65 sampled stops (`sampled_solid_gradient`). Photoshop has not opened a Patchy-written `grdm` yet: warning-free opening is unverified.
