# PR drafts for SethRobinson/Patchy

Fork-only file (lives on `jt`). One section per candidate PR. JT files these himself, one or two
at a time. Nothing here has been posted.

## How the branches work

- Each PR is a `pr/<topic>` branch on JT's fork: the feature as one commit on top of upstream
  `main` (v1.06, 4816d450), without the GCC 16 fix. Its content equals the tested
  `feature/<topic>` branch minus that fix (checked with `git patch-id`).
- `pr/replace-color` is stacked on `pr/color-range` (it reuses Color Range's sampling code). File
  Color Range first; once Seth merges it, rebase Replace Color onto his `main`.
- The adjustment-layer PRs (vibrance, selective-color, black-white, channel-mixer, photo-filter,
  color-lookup, gradient-map) each add to the same enum, switches, menu list and docs lines. Each
  applies cleanly to today's `main` alone, but after Seth merges one, the next needs a small
  rebase. File them one at a time.
- Before pushing a branch: `git push origin pr/<topic>`, then open the PR from the GitHub page.
- Every PR says Photoshop verification is still owed. Nothing here was checked against a real
  Photoshop; JT's copy is on the unbooted Windows install.
- Seth's README says he may not want large outside features. Asking him first (one issue listing
  these) is still the cheaper route.

Tested on Linux (Arch, GCC 16, Qt 6.8.3, offscreen): every branch builds with zero warnings,
passes the full core suite, and the full UI suite fails only the six tests that already fail on
an untouched upstream checkout on this machine (missing Windows fonts such as Arial):
ui_brush_tip_picker_keeps_options_bar_height, ui_animation_preview_fits_its_text_at_larger_fonts,
ui_drag_size_readout_shows_dimensions, ui_text_ime_composition_previews_and_commits,
ui_typing_uncovered_characters_switches_their_font,
ui_options_bar_transform_session_replaces_tool_controls.

---

## fix/gcc16-build

Title: Fix the build with GCC 16

Patchy didn't compile with GCC 16 on Arch Linux. Three small fixes: a missing `<algorithm>`
include in a test, the new `QPdfDocument::Error::Unknown` case in `pdf_import.cpp`, and a GCC 16
array-bounds false positive in a curves test (the test now builds its buffer in one allocation).
No behavior changes.

---

## pr/rotate-view

Title: Add the Rotate View tool (R) so the canvas can be turned while painting

Painting on a tablet means turning the canvas the way you'd turn paper, and Patchy had no way to
do that. This adds Photoshop's Rotate View tool. It shares a View Tools flyout with the Hand (H;
Shift+H cycles the flyout). Drag to turn the view, Shift snaps to 15 degrees, the options bar has
Rotation Angle and Reset View, and Esc or double-clicking the tool resets it. Each document keeps
its own angle; the angle is never saved or put in history.

R moves from Smudge to Rotate View, as in Photoshop; Smudge has no default key now and Shift+R
still cycles the Detail flyout. The rotation is applied only at input (pointer events are mapped
back into the old unrotated view space) and at paint (one painter transform over the existing
caches), so a zero angle is an exact no-op. Fit on Screen and Zoom to selection use the rotated
bounding box. Design notes are in docs/rotate-view.md.

Known limits: the canvas shows upright while a text box is open, rulers hide their numbers while
rotated, and transform-handle cursors don't follow the rotation. AGENTS.md is untouched; the
pointer to docs/rotate-view.md sits at the top of docs/view-navigation.md, which AGENTS.md already
links.

---

## pr/gradient-map

Title: Add the Gradient Map adjustment (layer and Image > Adjustments)

Gradient Map is one of the most used adjustments for color grading and thumbnails, and Patchy
didn't have it. This adds it as an adjustment layer and as a destructive Image > Adjustments
command, with the same gradient editor the layer styles use, Dither and Reverse. PSDs read and
write Photoshop's native `grdm` block, laid out from Adobe's published spec; unedited imported
blocks save back byte for byte. Luminance uses the Blend If gray weights. Some `grdm` fields are
unverified without Photoshop and are listed in docs/adjustments-calibration.md.

---

## pr/vibrance

Title: Add the Vibrance adjustment (layer and Image > Adjustments)

Vibrance is the gentler saturation boost that leaves already-saturated colors alone, and Patchy
had only Hue/Saturation. This adds a Vibrance adjustment layer and an Image > Adjustments command
with Photoshop's Vibrance and Saturation sliders, and native `vibA` PSD data. The math works on
saturation at fixed lightness, so grays never change and nothing clips.

One deliberate difference: no skin-tone protection. The way Photoshop holds back skin tones is
close to an active Apple patent (US 8638338), so the strength here depends only on how saturated
a pixel already is. The claim notes are in docs/adjustments-calibration.md and a binding rule is
in docs/legal-constraints.md. The `vibA` item names come from Photoshop's scripting names and are
unchecked against a real file.

---

## pr/selective-color

Title: Add the Selective Color adjustment (layer and Image > Adjustments)

Selective Color is the classic way to push one color family around (make the reds more orange,
the blues less cyan) and Patchy didn't have it. This adds it as an adjustment layer and an Image >
Adjustments command: nine color families, CMYK sliders, Relative and Absolute, and native `selc`
PSD data (unedited blocks save back byte for byte). The color-family weighting follows the model
FFmpeg's selectivecolor filter documents; formulas and what is uncalibrated are in
docs/adjustments-calibration.md. CMYK documents stay on the RGB math, like Hue/Saturation.

---

## pr/black-white

Title: Add the Black & White adjustment (layer and Image > Adjustments)

Converting to black and white with control over how each color maps to gray is a basic photo and
thumbnail step, and Desaturate gives no control. This adds Photoshop's Black & White adjustment
as a layer and an Image > Adjustments command (Alt+Shift+Ctrl+B): six color sliders with
Photoshop's defaults, Tint, and native `blwh` PSD data that keeps Photoshop's own items when an
imported layer is edited.

Left out for patent reasons: the Auto button (Adobe US 7706606) and dragging on the image to
change a slider (Adobe US 7920739). Notes in docs/adjustments-calibration.md.

---

## pr/channel-mixer

Title: Add the Channel Mixer adjustment (layer and Image > Adjustments)

Channel Mixer is the standard tool for custom black and white mixes and channel swaps, and Patchy
didn't have it. This adds it as an adjustment layer and an Image > Adjustments command: output
channel, source sliders, Constant, the Total warning, Monochrome (40/40/20 gray mix), and native
`mixr` PSD data. The block's record count comes from open-source PSD readers since Adobe's spec
doesn't state it. CMYK Channel Mixer layers are not modeled and are documented as a gap.

---

## pr/photo-filter

Title: Add the Photo Filter adjustment (layer and Image > Adjustments)

Photo Filter is the quick warm or cool tint photographers reach for, and Patchy didn't have it.
This adds the adjustment layer and Image > Adjustments command with Photoshop's 20 filter presets
or a custom color, Density and Preserve Luminosity, and native `phfl` PSD data (version 2 written,
versions 2 and 3 read). Only the Warming Filter (85) preset color has outside confirmation; the
other 19 are unverified. Version 3 color encoding is uncertain and documented.

---

## pr/color-lookup

Title: Add the Color Lookup adjustment with .cube LUT files

Color grading with 3D LUTs is how a lot of people match a look across images, and Patchy couldn't
load one. This adds Color Lookup as an adjustment layer and an Image > Adjustments command that
loads `.cube` files, with tetrahedral interpolation in integer math and an optional dither. The
PSD `clrL` block embeds the LUT file (and a device-link profile built with the bundled lcms2), so
documents reopen without the original file. No presets ship, since Adobe's are copyrighted.

The render paths now pass each pixel's document position to the adjustment so the dither matches
everywhere. Untrusted `.cube` files are parsed defensively (size caps, length checked before
allocating, 15 malformed-file tests). Abstract and Device Link modes and `.3dl` files are left
out. docs/color-lookup.md has the details.

---

## pr/shadows-highlights

Title: Add Image > Adjustments > Shadows/Highlights

Lifting dark areas without flattening the whole image is a common fix for backlit photos, and
Patchy had no tool for it. This adds Photoshop's Shadows/Highlights command with the basic view
and Show More Options (Tone, Radius, Color, Midtone, Black and White Clip) and Save Defaults. It
edits pixels with a live preview and one undo step. The formulas are Patchy's own (a blurred
luminance mask driving a gamma curve) and are not calibrated; docs/shadows-highlights.md has
them. Adobe's 2003 tone-selective patents have expired; the design avoids the newer Apple family.
No Smart Filter support yet.

---

## pr/match-color

Title: Add Image > Adjustments > Match Color

Making one photo's colors match another is a common compositing step, and Patchy had no way to do
it. This adds Match Color: pick any open document and layer (or Merged) as the source, with
Luminance, Color Intensity, Fade, Neutralize, and the selection-based statistics options. The
math is whole-image mean and deviation transfer in Lab (Reinhard 2001), which avoids the active
patents listed in docs/match-color.md. Load and Save Statistics are left out.

---

## pr/replace-color (stacked on pr/color-range)

Title: Add Image > Adjustments > Replace Color

Swapping one color in an image for another (a red shirt to green) is something people expect
from an editor, and Patchy had no command for it. This adds Replace Color: sample colors with the
eyedroppers or click the preview, set Fuzziness, and shift Hue, Saturation and Lightness. It
reuses Color Range's sampled-color scoring and Patchy's calibrated Hue/Saturation math. This PR
moves some sampling code from the Color Range dialog into shared helpers; Color Range's behavior
is unchanged. An existing selection limits the edit at full strength (no soft weighting), which
keeps clear of Adobe US 11223744. Localized Color Clusters is left out for the same reason as in
Color Range.

---

## pr/color-range

Title: Add Select > Color Range

Selecting everything of one color (a sky, a background) is one of the most common selection jobs,
and Patchy's Magic Wand only grabs connected pixels at one tolerance. This adds Select > Color
Range: sampled colors with add and subtract, the color-family and tonal presets, Fuzziness,
Invert, a preview box, and the canvas preview modes. The result is a soft selection that follows
the selection tool's combine mode.

Each pixel is scored from its own color only, after the click, so it stays clear of the Quick
Select live-classification patent rule. Localized Color Clusters, Skin Tones and Detect Faces are
left out on purpose. The canvas preview recomputes on the UI thread, which may feel slow on very
large documents with a preview mode on (the default is None). docs/color-range.md has the notes.

---

## pr/history-brush

Title: Add the History Brush (Y)

Painting part of an image back to an earlier state is a standard retouching move, and Patchy had
no way to do it short of undoing everything. This adds the History Brush. It paints the active
layer back toward the pixels it had in the history source state, through the normal brush
footprint, one undo step per stroke. The source defaults to the document as opened; right-click
any History panel row and choose Set History Brush Source to change it. It refuses when the
source has a different canvas size or no matching layer, as Photoshop does.

Left out: Art History Brush, blend modes, Airbrush, brush dynamics and history snapshots. Each
open document keeps one extra copy of its opened state for this. docs/history-brush.md has the
details.

---

## pr/symmetry-painting

Title: Add Paint Symmetry for the Brush, Mixer Brush and Eraser

Symmetry painting is a big help for character, pattern and mandala work, and Patchy didn't have
it. This adds Photoshop's butterfly button with Vertical, Horizontal, Dual Axis, Diagonal, Radial
and Mandala, a guide you can move and rotate (Transform Symmetry), and one undo step per stroke.
Each dab is computed once and stamped through each copy's mirror or rotation, so dynamics and
scatter mirror exactly; with symmetry off the output is byte-identical (the brush digest test
still passes). Copies that land on the original are skipped, so strokes along an axis don't
double up. Nothing goes into the PSD. Scripted strokes ignore the UI symmetry setting.

Not built: Wavy, Circle, Spiral, Parallel Lines and custom symmetry paths. docs/paint-symmetry.md
has the details and the patent notes.

---

## pr/color-replacement

Title: Add the Color Replacement tool in a new Brush flyout

Recoloring something while keeping its shading (a shirt, hair, a car) is quick with Photoshop's
Color Replacement brush, and Patchy didn't have it. This turns the Brush button into a Brush Tools
flyout (Brush stays on B, Shift+B cycles) and adds Color Replacement: Hue, Saturation, Color and
Luminosity modes, Continuous, Once and Background Swatch sampling, Contiguous, Discontiguous and
Find Edges limits, Tolerance and Anti-alias. It never writes alpha, so Lock Transparent Pixels
holds. The design stays within what Adobe's expired Background Eraser patent (US 7627168) taught;
docs/color-replacement.md lists the active patents checked.

Mixer Brush stays in the Detail flyout so existing shortcuts don't move. Bitmap tips and dynamics
are not supported by this tool yet.

---

Drafted by Claude Opus 5.5 in Claude Code (running in T3 Code), for JT to review and edit before
filing. Whether to keep this note on the PRs themselves is JT's call: Seth's README says he
deletes AI-smelling PRs.
