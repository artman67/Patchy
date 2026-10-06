# Rotate View

Read this before changing the Rotate View tool, the canvas view/widget mapping, or any
canvas code that paints, hit-tests, or repaints by widget position.

Rotate View turns the on-screen view of a document, like turning paper on a desk. It is
presentation state only: it never touches pixels, history, the modified flag, settings or
files. Each `CanvasWidget` keeps its own angle, so every document tab keeps its own
rotation, including across tab switches and Float in Window (the canvas moves, its state
with it). New documents start at 0.

## Tool and controls

- The View Tools flyout (`viewToolButton`, menu `viewToolMenu`) holds the Hand (default)
  and Rotate View (`CanvasTool::RotateView`, appended to the enum; command id
  `tools.rotate_view`).
- **Default shortcut change.** R belongs to Rotate View, Photoshop's default. Smudge
  (`tools.smudge`) ships unbound, as in current Photoshop; Shift+R still cycles the Detail
  flyout and Shift+H cycles the View Tools flyout (`toolCycleViewAction`). Command ids
  are unchanged, and a user override of either command still wins over the defaults
  (`resolve_hotkey_assignments`). `ui_hotkey_defaults_have_no_conflicts` and the pinned
  shortcut list in `pickers_notices_hotkeys_tests.cpp` cover the change.
- Dragging turns the view about the viewport center by the pointer's angle change around
  that center (screen y points down, so positive angles are clockwise). Shift snaps the
  resulting angle to multiples of 15 degrees. The angle is normalized to (-180, 180].
- Options bar: Rotation Angle (`rotateViewAngleSpin`, a degree `UnitSpinBox`, whole
  degrees, wraps) and Reset View (`rotateViewResetButton`).
  `MainWindow::sync_view_rotation_controls` mirrors the active canvas's angle into the
  field on every view change and options-bar refresh.
- Reset: the Reset View button, Esc with the tool active (handled in
  `CanvasWidget::keyPressEvent` ahead of the layer-deselect Escape), or a double-click on
  the View Tools button while Rotate View is its current tool (`ToolFlyoutEventFilter`'s
  double-click hook; with the Hand showing, a double-click opens the flyout like every
  other flyout).
- Rotate View is a view tool: it works while editing is locked and during a preview
  dialog's edit lock, like the Hand and Zoom.
- Fit on Screen and Zoom to rect size the document by its rotated bounding box; zoom
  presets, wheel zoom and zoom-at-point keep the angle.

Not implemented: Photoshop's Rotate All Windows option, and a scripting API for the angle.

## Design: an unrotated view space

`CanvasWidget` keeps working in the coordinate system it always had, now called view
space: `pan_`, `zoom_`, `widget_position*`, `document_position*`, `last_mouse_position_`,
hit tests, overlay geometry and dirty rects are all unrotated. The rotation is one extra
transform between view space and the real widget, `view_to_widget_transform()`: a turn by
the angle about the widget center. Every helper is an exact identity while the view is
unrotated, so the unrotated paths are unchanged. The mapping has two crossings:

- **Input.** `CanvasWidget::event()` rebuilds positioned pointer events (mouse, tablet,
  wheel, enter) in view space before any handler runs (`dispatch_view_mapped_event`) and
  copies the accepted state back, so Qt's tablet-to-mouse synthesis and parent
  propagation still work. Tools, handles, pen and path editing, selections, crop, guides,
  text placement and the eyedropper therefore see the positions they always did. Code that
  reads the pointer outside an event maps it itself: spacebar panning
  (`begin_pan_at_global_position`), `refresh_info_display`, and anything turning a view
  point back into a global position (`global_point_for_view_point`: read gestures, the
  brush-adjust pointer warp, the tablet bridge's synthetic mouse events).
- **Paint.** `paintEvent` fills the backdrop in widget space, sets the painter transform,
  and draws the document and every document-anchored overlay in view space. The exposed
  rect becomes the view-space bounding rect of the widget's exposed rect.

Rules for canvas code:

- A position or rect in view space that must reach Qt's widget machinery is converted:
  `update_view_rect` / `update_view_region` instead of `update(rect)` (bounding rect of the
  turned rect, plus one pixel), `widget_point_from_view`, `view_point_from_widget`,
  `widget_rect_for_view_rect`.
- "The visible area" in view space is `visible_view_rect()`, not `rect()` (vector preview
  frame, selection outline cache key, crop shield, channel paint preview, snap-guide
  clipping).
- Screen chrome resets the painter transform and works in real widget coordinates: the
  rulers, the processing HUD, the scroll-bar corner, the W x H and transform readouts, and
  the brush-adjust readout. Rulers keep their bars (guides still drag out of them) but draw
  no ticks while rotated, because their numbers measure along document axes.
- Screen-direction gestures convert deltas with `view_delta_from_widget_delta`: wheel
  panning, the scroll bars, the pan clamp, the pen zoom drag (up zooms in on screen), and
  the brush size/softness drag.
- Pixel snapping is screen-pixel alignment, so it is off while rotated:
  `pixel_snapped_view()` and `deep_zoom_pixel_view()` replace
  `uses_pixel_aligned_view`/`uses_deep_zoom_pixel_renderer` wherever a coordinate is
  rounded to a screen pixel or the per-pixel deep-zoom renderer is chosen, and
  `document_position` drops its deep-zoom edge correction. Which document positions carry
  grid lines still depends on zoom alone.

The public API crossing the canvas boundary uses real widget coordinates:
`widget_position_for_document_point`, `widget_point_for_document_point`,
`document_point_for_widget_position`, `zoom_at_widget_point` (internal callers use
`zoom_at_view_point`). `widget_position_f` stays view space for the canvas's own use.
Callers outside the canvas that did their own `origin + point * zoom` arithmetic go
through these instead when rotated (Stroke Path's synthetic pen, the layer-style
gradient drag).

## Scroll bars and the pan clamp

The bars and `constrain_pan` work on `document_widget_bounds()`, the document's bounding
box on the real widget (pan and the zoomed size exactly while unrotated). The bars scroll
the screen horizontally and vertically at any angle, the 10%-visible rule applies to the
rotated bounds, and a bar move or clamp becomes a pan change through the inverse
rotation. Preset zooms apply Photoshop's center-or-clamp rule to the same bounds. Resizing
a rotated canvas shifts the pan so the document stays put on screen, as it does
unrotated (the rotation center moves with the widget center).

## Rendering

Nothing new is composited: the rotated frame draws the same `render_cache_`, display
mips, move/transform/warp bases, patches and mask overlays through the painter transform
(Qt's transformed blit samples only the destination pixels it covers). Sampling while
rotated: smooth below 200% (below that a document pixel covers under two screen pixels
and nearest sampling shimmers), nearest from 200% up, so deep-zoom pixels stay crisp
squares that the grid lines meet exactly. The transparency checkerboard is one pattern
fill (`checkerboard_tile`) phase-locked to the document corner, so it turns with the
document. Grid and guide lines antialias while rotated.

Dynamic Vector Preview renders the view-space bounding box of the viewport
(`vector_preview_view` folds its top-left into the offset) and the frame is drawn with
smoothing while rotated; this costs up to about twice the pixels at 45 degrees.

Brush feedback turns with the view: OS brush cursors bake the angle into the pixmap
(square footprints and one-pixel cursors are rotated and sized by their diagonal; tip
outlines trace the turned stamp, `stamp_turned_for_view`), the large-brush hover outline
is drawn upright in widget space from the same turned stamp, and the brush cursor cache
keys on the angle. Tablet tilt and barrel rotation arrive in screen terms and are turned
into document terms when the sample is built, so tilt-steered tips keep their direction
relative to the artwork.

## Text sessions

The inline text editor is a real `QTextEdit` child of the canvas and cannot turn. While a
visible text session is open, MainWindow suspends the rotation
(`set_view_rotation_suspended`): the view shows upright, the stored angle and the
options-bar field keep their value, and the angle comes back when the session commits or
cancels (`resume_view_rotation_after_text_session`, a scope guard in both funnels). The
click that opens the session is mapped with the rotation still in force, so the text
lands where it was clicked. Editing text on a turned view, as Photoshop does, would need
the transformed-text overlay path to carry the view rotation.

## Tests

`tests/ui/rotate_view_tests.cpp` (filter `ui_rotate_view`): mapping round trips and
the fixed center across angles and zooms, normalization, the rotated scroll-bar range;
marquee, brush and tablet input landing on the right document pixels (with tilt
compensation); the tool's drag, Shift snap and Esc reset without touching the document;
the MainWindow wiring (R, options bar, per-document angle, no history or modified flag,
the text-session suspension, Fit on Screen, Reset View, the flyout double-click); and
rendered checks with artifacts `rotate_view_low_zoom.png`, `rotate_view_deep_zoom.png`,
`rotate_view_deep_zoom_grid.png` and `rotate_view_square_brush_cursor.png`.
