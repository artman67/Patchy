# Paint Symmetry

Photoshop-style mirrored and radial painting for the Brush, Mixer Brush, and Eraser (pen eraser end included). The brush engine lives in [brushes.md](brushes.md) and [mixer.md](mixer.md); the binding patent boundary is in [legal-constraints.md](legal-constraints.md), with the claim check recorded below.

## Where the user finds it

The options bar's butterfly button (`paintSymmetryButton`, `paint_symmetry_icon()` in action_icons) closes the Brush, Mixer Brush, and Eraser rows, after the Smoothing controls, as in Photoshop. It wears the accent outline while a symmetry is on (`symmetryActive` property, the Dynamics button's QSS rule). Its menu: Symmetry Off; Vertical, Horizontal, Dual Axis, Diagonal, Radial..., Mandala...; Transform Symmetry, Reset Symmetry, Hide Symmetry. Radial and Mandala ask for a segment count each time (Photoshop's ranges: Radial 2 to 12, Mandala 2 to 10; default 6). Choosing a mode does not open a transform session; Transform Symmetry does.

Mode, segments and Hide are application-wide session state mirrored through `MainWindow::current_paint_symmetry_*` (new sessions and `activate_document_canvas` push them). Nothing is persisted: every launch starts with Symmetry Off, like the brush tip. The center and rotation belong to each document's canvas.

## Geometry

`core/paint_symmetry.*` builds the copies: `paint_symmetry_transforms(mode, segments, center, angle)` returns rigid placements (`SymmetryTransform`, an orthogonal 2x2 plus translation), the stroke itself excluded.

- Vertical / Horizontal: one mirror across the vertical / horizontal axis. Dual Axis: both mirrors plus the half turn. Diagonal: one mirror across the 45 degree axis rising to the right.
- Radial N: N - 1 rotations by 360/N. Mandala N: the dihedral group, N - 1 rotations plus N mirrors (the vertical axis and its rotations by half a segment), so each segment is mirrored inside itself and Mandala 2 equals Dual Axis.
- The whole symmetry turns by the canvas angle (clockwise on screen, y down). Quarter-turn matrix entries snap to exact 0 and 1.

The guide sits on pixel edges: the default center is (width / 2, height / 2) in document coordinates, stored per canvas as a fraction of the canvas size and snapped to half pixels. The engine centers a dab at x on pixel x, so `CanvasWidget::paint_symmetry_copies()` passes the center minus half a pixel. With a half-pixel center, mirrors always map whole pixels onto whole pixels, so mirrored copies are byte-identical images of the stroke; quarter turns do too when both center coordinates sit on pixel edges or both on pixel centers (the default center of an even-by-even or odd-by-odd canvas).

## Engine

`EditOptions::symmetry` carries the copies; `CanvasWidget::current_brush_edit_options` fills it only for Brush, Mixer Brush, and Eraser with a symmetry on. Empty is the historical path bit for bit (`tool_write_paths_digest_baseline` unchanged).

- Replication sits in the three leaf painters of `core/pixel_tools.cpp`: `paint_tip_dab` (bitmap tips, Round with dynamics), `paint_brush_dab` (procedural dabs, the canvas dab-stroke path, Flow, Airbrush ticks) and the capsule and one-pixel line inside `paint_brush_segment`. Each dab is computed once in the stroke's frame (spacing, Shape/Scattering/Transfer/Color dynamics, Count, Direction) and painted again at each copy's mapped center; each copy pixel evaluates the original footprint at its offset mapped back through the copy (`unmap_offset_*`). Copies are therefore exact mirror images or rotations, jitter and asymmetric tips included.
- All copies share the per-stroke snapshot compositor, pixel gate, accumulated alpha, Wet Edges union mask and dirty rect, so overlapping copies behave like a stroke crossing itself, the stroke stays one undo step, and the Opacity cap holds across copies.
- A copy whose dab center (or segment, either direction) coincides with the stroke's or an earlier copy's is skipped (`paint_symmetry_copy_repeats_point/segment`, 1e-4 px). A stroke on the axis or a dab at the radial center paints exactly what it paints without symmetry. Cost: an asymmetric tip exactly on an axis shows only the original orientation there.
- One-pixel footprints map the pixel index (`symmetry_copy_pixel`), not floor() of the mapped position, which would land one pixel off past a mirror axis; the line walk maps each visited pixel.
- Mixer Brush: each copy keeps its own pickup average (`mixer_brush_state_for_copy`, started lazily per stroke) sampled where that copy paints. `dab_primary_provider` receives the copy index. One shared average would alternate between the two sides and carry nothing.
- Layer masks and grayscale channels: `draw_mask_brush_segment` repeats each segment through the same copies (procedural footprint, same skip rule).

## Transform Symmetry

`CanvasWidget::begin_paint_symmetry_transform` (canvas_widget_symmetry.cpp) turns left drags into placement edits until Enter (keeps), Escape (restores the placement from the session start), Symmetry Off, a switch to another tool, or a document switch (keeps). A press within 12 screen px of the center moves it; elsewhere it rotates the axes around the center, Shift snapping to 15 degrees. Space still pans; middle-button pans pass through. Enter and Escape are claimed in the `ShortcutOverride` block while the session is live. Reset Symmetry restores the canvas center and angle 0. Drags record no history.

The guide (`draw_paint_symmetry_guide`, theme role `paint_symmetry_guide`, drawn after the move snap guides) shows the axes, or one spoke per segment starting straight up, clipped to the canvas, while a symmetry tool is active and Hide Symmetry is off, and always during a transform session (with a center ring).

## Persistence and automation

Documents store only painted pixels. Photoshop keeps symmetry as a special path in the document; Patchy writes no path, resource or metadata, so PSDs stay ordinary raster files that open cleanly everywhere, and an imported symmetry path is not recognized as one.

Native script strokes (`layer.drawStrokes`, `strokePath`, MCP through scripting) never replicate: `paint_script_stroke` turns the artist's symmetry off for its duration, because scripts state geometry explicitly and the artist's hidden UI state must not change their output. A script mirrors by sending its own copies.

## Photoshop differences

Not implemented: Wavy, Circle, Spiral, Parallel Lines, New Path (custom symmetry paths), symmetry paths in the Paths panel and PSD, Last Used Symmetry, scaling the path (it is always canvas-wide), and the automatic transform box after choosing a mode. Patchy has no separate Pencil; a hard Brush or Square tip stands in. Photoshop's handling of mirrored tip orientation and dynamics has not been compared (no Photoshop on the development machine for this work); Patchy mirrors tips and jitter exactly.

## Patent check (2026-10-06, Google Patents claim text)

Searched: Adobe and others on symmetry painting, mirrored strokes, radial and kaleidoscope drawing. Examined: Adobe US 2019/0244403 (paint reflection around a curvilinear path; abandoned 2021, never granted), Adobe US 11062493 B1 (to 2040: a source object clipped by a path relative to a user-moved symmetry axis, with the reflected object regenerated in real time as the axis or source moves), Adobe US 10705700 B2 (vector mirror snapping with automatically derived axes), Adobe US 11631204 B2 (axes derived from convex hulls of vector objects, mirror snap points), Adobe US 12147656 B1 (spirograph roulette curves), Genue US 2015/0302624 A1 (repeating pattern tiles in a design app; no granted family member surfaced, status unconfirmed). None reads on raster dabs replicated at paint time through fixed straight axes or rotations about a user-placed point. Prior art: MacPaint Mirrors (1984, horizontal, vertical and diagonal), Krita multibrush (mirror and radial, 2011), GIMP Symmetry Painting (mirror, tiling, mandala, 2018). The boundary lives in [legal-constraints.md](legal-constraints.md).

## Tests

Core: `paint_symmetry_transforms_place_mirrors_and_rotations`, `tool_paint_symmetry_mirrors_every_footprint_exactly` (soft capsule, one-pixel line, snapped square, asymmetric tip with angle jitter and scatter), `tool_paint_symmetry_never_double_stamps_on_the_axis`. UI: `ui_paint_symmetry_button_mirrors_stroke_in_one_undo_step` (menu wiring, one undo step, Transform Symmetry, tool visibility), `ui_paint_symmetry_mandala_and_mixer_copies_paint_their_own_side` (writes `ui_paint_symmetry_mandala.png`).
