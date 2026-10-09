# Brushes and Brush Settings panels

Read this before changing `src/ui/brushes_panel.*`, `src/ui/brush_settings_panel.*`,
`src/ui/brush_settings_sections.*`, the section pages in `src/ui/brush_dynamics_popup.*`, the
shared stroke preview in `src/ui/brush_stroke_preview.*`, or the New Brush Preset dialog
(`src/ui/new_brush_preset_dialog.*`). The working brush they edit is described in
[brushes.md](brushes.md), "Current brush"; dock placement and persistence in
[dock-panels.md](dock-panels.md).

## Ownership

Both panels are views of `CurrentBrush`. They never touch a canvas:

- Picks go out as signals and MainWindow runs them through the same paths as the options bar:
  a built-in or saved preset through `pick_brush_preset`, a tip through `set_active_brush_tip`.
- Size and Soft belong to the active size group (the Eraser's own while it is active), so the
  panels emit them to `edit_brush_option`; MainWindow pushes the group back with
  `sync_brush_panels()` from `sync_brush_controls_from_canvas`.
- Smoothing lives in MainWindow's tool state; the Brush Settings Smoothing page drives the
  options-bar Smoothing controls, whose handlers own the canvas and the saved setting.
- Everything else (tip shape, spacing, dynamics, the Build-up flag) the Brush Settings panel
  writes with `CurrentBrush::edit`.

The wiring lives in `main_window_brush.cpp` (`wire_brush_panels`, `sync_brush_panels`,
`new_brush_preset_from_panel`); dock creation in `main_window_docks.cpp` (`create_brush_docks`,
`show_brush_settings_panel`); the Window menu toggles are rows in `add_panel_toggles_to_window_menu`
([color-picker.md](color-picker.md), "Window menu panel toggles").

## Docks

`brushesDock` and `brushSettingsDock` form one tab group (Brushes first) directly above
Layers/Channels/Paths, created right after the Color panel in `create_docks` (Color heads the
column). They start closed: even collapsed, the
group's title strip and tab bar pushed the all-panels-expanded column past a 1080p work area
(`ui_right_dock_panels_expand_within_window_height`). Window > Brushes and Window > Brush
Settings (`window.brushes_panel`, `window.brush_settings_panel`, no default keys; F5 stays
Force Refresh) open a panel expanded and in front of its group. The options-bar Dynamics button
and the Brushes panel's brush button call `show_brush_settings_panel`. Both panels are document
widgets (disabled with no document open). The Brush Settings panel sits in
`brushSettingsScrollArea` with a 12 px floor so a crowded column scrolls it instead of pinning
the window taller.

## Brushes panel

One `BrushPresetList` (a QListWidget) holds folder header rows and brush rows, so the same items
show as a list (tip thumbnail, stroke preview, name) or a thumbnail grid (`View`, remembered as
`brushes/panelView`); headers span the row in the grid. PresetTreeWidget is not reused: it has
no grid flow and its rows are icon plus text. Rows, in order: loose saved presets and loose tips
(no header), the built-in presets under "General Brushes" (a reserved folder key no store can
carry), then every other folder by name. A folder is the union of saved presets' `folder`,
tips' `folder`, and folders made with New Folder while still empty (`brushes/panelFolders`).
Expanded folders persist as `brushes/panelExpandedFolders`; General Brushes starts expanded.
The search field filters by name and ignores collapse.

- A click picks the row. The working brush's base row is the current row and draws the
  selection fill; once `CurrentBrush::modified()` it adds an accent outline and an italic
  "(modified)" (grid: an accent dot). The same state is the row's accessible description
  ("Current brush" / "Current brush (modified)"), which the tests read.
- Stroke previews come from `brush_stroke_preview_pixmap`: the real engine on a scratch document,
  tinted with the theme ink, cached by a key of the tip identity plus its settings (LRU 256), so
  scrolling and repaints never re-render. Tips render on their first cache miss only.
- Context menu and the `...` button: New Brush Preset..., New Folder..., Rename..., Duplicate,
  Delete, Import Brushes... (the existing ABR import), List View, Thumbnail Grid View. Rename
  and Delete route to the right store (`BrushAutomationLibrary::update_entry`/`remove`,
  `BrushTipLibrary::rename_tip`/`remove_tips`); a folder renames or deletes everything filed in
  it. Built-ins and their folder cannot be renamed, moved or deleted; Duplicate of a built-in
  makes a saved preset. Dragging rows onto a folder header or a row in it refiles them.
- The Size slider on top edits the active size group, coalesced over 40 ms.

## New Brush Preset

`request_new_brush_preset` (Name, Folder, Capture brush size in preset, Include tool settings,
Include color) feeds `save_working_brush_as_preset(name, BrushPresetSaveOptions)`. The options
bar's "Save Current Brush..." opens the same dialog. The record always holds complete settings
(scripts resolve it as before); without size or tool settings it adds `captureSize: false` /
`includeToolSettings: false` beside `includeColors`, and `pick_brush_preset` then keeps the
current size, or Opacity, Flow, Smoothing, Mixer values and the global pen mapping. Rename,
Update, Duplicate (manager dialog and scripts) keep both flags. The new preset becomes the base,
unmodified.

## Brush Settings panel

A `QTreeWidget` section list (enable box, name, padlock column) and a stacked page per section in
a scroll area, a lock banner, a live stroke strip, and Create New Brush. At 470 px and wider the
list sits beside the page (Photoshop's layout); the default column stacks it above. The list
shows every row when there is room and keeps five rows (scrolling) in a short dock; `RowsHeight`
re-applies those heights after every polish because the application sheet's
`QTreeWidget { min-height: 20px }` replaces an explicit minimum. Pages built by
`BrushDynamicsPanel` in `Presentation::Pages` stack each row (label and value, then the slider or
combo); the Brush Tips manager's "Edit Dynamics..." uses `Presentation::AllSections` (titled
groups, one line per row, Reset). One form instance lives under MainWindow, so the existing
`dynamics*` objectNames stay unique. Only the shown page sizes the stack (the others get an
Ignored size policy in `show_section_page`), so the page area scrolls by that page's own height
and a section change starts at its top.

Sections, in Photoshop's order (`kBrushSections`; the enum appends, lock keys persist, never
rename): Brush Tip Shape (tip grid, Size, the angle/roundness ellipse and spins with Flip X / Flip
Y, Soft standing in for Hardness, Spacing), Shape Dynamics, Scattering, Texture, Dual Brush, Color
Dynamics, Transfer, Noise, Wet Edges, Build-up (the Airbrush flag), Smoothing. Brush Pose, Brush
Projection and Protect Texture are left out: the engine has no such features (see
[legal-constraints.md](legal-constraints.md) before adding any).

- Enable boxes: Texture, Dual Brush, Color Dynamics, Noise and Wet Edges map onto their
  `BrushDynamics` flags (Noise and Wet Edges are checkbox-only sections with a hint page), Build-up onto `WorkingBrush::airbrush`. Shape Dynamics, Scattering,
  Transfer and Smoothing have no flag; their box shows whether the section changes anything.
  Unticking parks the section's values in the panel and resets them, ticking brings them back
  (Smoothing returns to its last amount, else 10%); ticking an empty section keeps it ticked
  until edited. Parked values belong to the brush showing and reset on every pick. No core state.
- Spacing (`WorkingBrush::spacing`) applies to every tip, the procedural Round and Square
  included: ticked stamps at that fraction of the size, unticked leaves it unset (the tip's own
  spacing; Round and Square keep their smooth stroke, see brushes.md "Current brush").
- The flips are `BrushDynamics::tip_flip_x/y` but belong to Brush Tip Shape: they are never
  locked and "Clear Brush Controls" keeps them. The bitmap-tip cursor outline mirrors with them
  (`brush_tip_stamp_image`), and every stroke preview passes them on (`render_brush_stroke_preview`).
- Texture lists the generated grains, then the Pattern library (`BrushTipLibrary::pattern_library`,
  live), as one picker; a grain clears `texture_pattern_id`, a pattern keeps the last grain as its
  fallback. A brush naming a pattern the library lacks shows "<texture_pattern_name> (missing)"
  with a hint and keeps the reference. Then Invert, Scale, Brightness, Contrast, Mode, Depth
  ([brush-texture.md](brush-texture.md)). The stroke strip renders with the pattern tile.
- Edits coalesce over 40 ms and the stroke strip re-renders at most every 80 ms. A pick drops a
  pending form edit; the panel skips reloading values that only echo its own edit.
- Tools grey out what their strokes ignore (`brush_sections_for_tool`, read from the tool traits
  table in `ui/tool_traits`): `kToolBrushDynamics` (the Brush) honors every section; the other
  `kToolBrushTip` tools the tip (Pattern Stamp, History Brush), plus Smoothing with
  `kToolSmoothing` (Mixer Brush, Eraser); other tools none. The same flags decide where the
  options-bar Dynamics button and Airbrush appear (`tools_honoring`). Keep them in step with what
  `canvas_widget_brush.cpp` strips.
- "Clear Brush Controls" resets every unlocked dynamics section.

## Locks

`CurrentBrush::locks()` is tool state, persisted as `tools/brushSectionLocks` and never part of a
preset. `pick()` and `pick_tip()` produce `picked_over(current, picked, locks)`: the picked brush
with each locked section's values kept from the current brush (`copy_brush_section` is the one
section-to-field map; Smoothing locks are applied in `activate_automation_brush`, the only pick
that sets Smoothing). The merged brush becomes the base, so a pick reads unmodified. Script
`brushes.activate` is a pick and honors locks; script strokes resolve their own settings and
ignore them. Brush Tip Shape cannot be locked. Locks must never go unnoticed: a locked row shows
an accent padlock and a bold name, a banner under the page names every locked section with
Unlock All, and the panel menu and the list's context menu offer "Reset All Locked Settings"
(clears every lock). `ui_brush_section_locks_merge_picked_brush` pins the merge, including that
every persisted `BrushDynamics` field belongs to a section: a new field must be added to
`copy_brush_section`.

## Tests

`brush_pattern_palette_tests_brush_panels.cpp` (part 3 of the brush/pattern/palette group):
placement and the Window toggles including a layout that predates the panels, picks reaching the
canvas and the modified marker, the New Brush Preset round trip with its folder and capture
choices, the lock merge, and section greying per tool. The dynamics form tests in part 2 drive
the panel through `open_brush_settings_panel` (the Dynamics button).
`ui_brush_settings_engine_controls_reach_stroke` drives Spacing on Round, Flip X, Noise and a
Texture pattern (including the missing state) through the panel to the canvas.
