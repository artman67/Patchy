# Color picker views and the Color panel

Read this before changing `src/ui/color_picker_views.*`, `src/ui/color_dock_panel.*`, the picker
tabs in `src/ui/color_panel.cpp`, or the Window menu's panel toggles. The picker's palette swatch
column is documented in [palette-mode.md](palette-mode.md).

## Shared views

`color_picker_views.{hpp,cpp}` holds everything the popup picker and the Color panel share; neither
keeps its own copy.

- `HsvColorModel` is the edited color: the RGB color plus hue, saturation and value kept beside it,
  so a grey keeps its hue and markers do not jump on a round trip. `set_color` (RGB in) and
  `set_hsv` take a `ColorChangeNotification`; the view helpers (`set_saturation_value_from_point`,
  `set_hue_from_point`, `set_hue`, `set_channel`) always notify. The owner reacts in
  `color_model_changed(previous, notification)`.
- Views: `ColorPlaneWidget` (saturation x, value y), `HueSliderWidget` (vertical hue bar),
  `ColorWheelWidget` (hue ring around `ColorWheelInner::Square` or `ColorWheelInner::Triangle`),
  `ColorChannelSlider` (one gradient track per H/S/V/R/G/B channel). Constructors set the popup's
  sizes and object names; the Color panel overrides both.
- `normalized_rgb_color` and `parse_panel_color` (names, `#RGB`, `#RRGGBB`, CSS `#RGBA` and
  `#RRGGBBAA`) are the shared helpers.
- The ring, gradients and two-tone markers are color content, exempt from theme roles like the
  canvas markers. Chrome painted around them (the panel's swatch outlines) uses `theme()` roles.

Wheel geometry: hue 0 (red) at 3 o'clock, hue running counter-clockwise, matching
`QConicalGradient` and the `atan2` hit test. The ring is 11 percent of the wheel's side, clamped
to 12 to 20 px, so a narrow dock keeps room inside it. The triangle's corners sit just inside the
ring: pure hue at the hue angle, white 120 degrees on, black 240 degrees on, so it turns with the
hue. Value is the distance from the black corner and saturation the share of pure hue in the rest;
a drag that leaves the triangle clamps to its nearest edge, and a press inside the ring keeps
editing the inner area until release.

## Popup picker

`PatchyColorPicker` (opened from the toolbar FG/BG swatches, the text color box, and every
`request_patchy_color` prompt) has Square, Wheel and Sliders tabs over one `HsvColorModel`, a
numeric and HTML footer, the palette column, and custom color slots. Settings:
`colorPanel/lastTab`, `colorPanel/customColors`.

## Color panel

`ColorDockPanel` lives in the `colorDock` dock, first in the right column (like Photoshop), and
starts expanded. It sits in `colorDockScrollArea` with a 12 px floor: when the column is crowded
it shrinks and scrolls instead of growing the window, and with room its picker grows with the dock.

- Swatches (`colorDockSwatches`): overlapping foreground and background squares. The one the panel
  edits is drawn on top with an accent ring and a background gap; clicking the other switches. The
  panel starts on the foreground each run.
- Modes (`colorDockModeCombo`): HSV Square (plane plus hue bar, the default), Wheel + Square,
  Wheel + Triangle, HSV Sliders, RGB Sliders, HSV + RGB Sliders. Persisted under `colorDock/mode`
  as `square`, `wheel`, `triangle`, `hsv_sliders`, `rgb_sliders`, `sliders`; the key and tokens are
  permanent. The three slider modes share one page and hide the rows they do not use. Only the
  current mode's page is shown, so hidden pages never add to the minimum height.
- Hex field (`colorDockHexEdit`): accepts what `parse_panel_color` accepts.
- Edits emit `color_edited(background, color)`. MainWindow applies them through
  `apply_foreground_background_edit`, the same path as the Foreground/Background popup (canvas
  color, the active text editor, shape paint while the shape controls are live, status message,
  `refresh_color_buttons`), and mirrors them into an open popup for the same color with its signals
  blocked.
- `refresh_color_buttons` pushes the canvas colors into the panel with `set_colors`, so every path
  that keeps the toolbar swatches current (eyedropper, popups, Palette panel, X, D, document
  switches, Quick Mask) updates the panel too. `set_colors` never emits, and a color equal to the
  current one leaves the HSV state alone. While the panel's own edit is being applied, a
  palette-mode snap only reaches the swatch; the views keep tracking the pointer.
- With no document the panel is disabled, like the toolbar swatches (`register_document_widget`).

Height budget: before the panel the right column already used most of the test window budgets
(29 px of slack for History's 190 px expand in the 780 px test window, about 14 px under the
950 px all-expanded bound). Any new panel costs at least its title strip and a separator, and
the expanded panel takes height the Layers list used to own. Tests that budget the original
stack or the Layers list in that window call `hide_color_dock` (tests/ui/ui_test_support): three
right-dock geometry tests and two Layers-panel tests that need rows on screen and empty space
below them. With the Color panel also expanded, the all-expanded minimum is about 980 px.

## Window menu panel toggles

The Window menu lists Tool Palette and Options, then each right panel's `toggleViewAction()` in
column order, above the open-document list (`add_panel_toggles_to_window_menu`). Choosing a hidden
panel shows it, expands it and raises it in its tab group. Action names are
`window<Name>PanelAction`; hotkey ids (permanent) are `window.color_panel` (F6),
`window.brushes_panel`, `window.brush_settings_panel` (no default keys: F5 stays Force
Refresh), `window.layers_panel` (F7), `window.channels_panel`, `window.paths_panel`,
`window.history_panel`, `window.properties_panel`, `window.info_panel` (F8) and
`window.palette_panel`. F6, F7 and F8 match Photoshop.

Tests: `ui_color_dock_panel_syncs_both_ways_and_toggles_from_window_menu`,
`ui_color_dock_panel_modes_fit_persist_and_align_the_triangle` (also saves
`ui_color_dock_<mode>_<scheme>.png` captures), and the popup's `ui_color_picker_*` tests.
