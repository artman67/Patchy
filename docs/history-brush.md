# History Brush

The History Brush (Y, hotkey id `tools.history_brush`, object name `toolHistoryBrushAction`, `CanvasTool::HistoryBrush`) paints the active pixel layer back toward the pixels it had in one earlier document state, the **history source**. It is Photoshop's History Brush without the Art History Brush. It sits in the palette's retouch cluster right after the Stamp flyout, as a plain button: Photoshop's flyout partner (Art History Brush) is out of scope, so there is no flyout and no Shift+Y cycle command.

## The source

- Each document session owns one source (`DocumentSession::history_brush_source`, a `shared_ptr<const Document>`, plus `history_brush_source_state_id` for the panel row). `initialize_session_history` sets it to the document as created or opened (File > New, Open, Reopen, New Document From This State), which is Photoshop's default (the opening snapshot). MainWindow pushes it to the session's canvas through `CanvasWidget::set_history_brush_source`; a canvas with no source refuses strokes with a status message.
- The History panel marks the source row with the tool icon (tooltip "Source for the History Brush"; the other rows reserve a blank 20 px slot so labels align, which is why the list has an explicit icon size). Right-click any row, past, current, or redone-away future, and choose **Set History Brush Source** (checked on the current source) to make that state the source (`MainWindow::set_history_brush_source`). This is the equivalent of Photoshop's per-state source column.
- The source is a value copy of the state's document (copy-on-write pixels, so it costs nothing until the live layers diverge). It therefore survives the state being evicted from the undo stack (state cap or memory budget) or discarded by a new edit after an undo; the marker then has no row to sit on, but strokes keep painting from it, like a Photoshop snapshot. `enforce_history_memory_budget` counts the source's marginal pixel bytes but never evicts it; under pressure, undo states go first (down to the usual floor).

## Refusals (Photoshop's rule)

`CanvasWidget::begin_history_brush_stroke` runs on press, before `begin_edit`, so a refused stroke paints nothing and adds no history entry:

- a source with a different canvas width or height: "Could not use the history brush because the history state has a different canvas size" (for example after a crop, Canvas Size, Image Size, or a 90 degree canvas rotation; pick a later state as the source);
- no pixel layer with the active layer's id in the source: "Could not use the history brush because the history state does not contain a corresponding layer" (a layer created after the source state, or one that was a type, shape, or other non-pixel layer there). Layer ids persist across every state of a document, so the id is the correspondence;
- a grayscale target (layer mask, channel, Quick Mask): "History Brush is unavailable while editing a grayscale channel". It is also refused in channel views and while editing a Smart Filter mask, like Pattern Stamp.

## Painting

- The stroke rides the Brush engine (`draw_brush_segment` / `draw_brush_at` with the tool set), so Size, Soft, Opacity, Flow, bitmap tips from the Tip picker, `[`/`]`, Alt+Right-drag sizing, Shift-click stroke connect, the smoother, selections (including feathered coverage), Lock Transparent Pixels, palette snapping, layer expansion, pen pressure through the global pen preferences, and the pen's eraser end (which erases) all behave as on the Brush. Like Pattern Stamp, brush dynamics are stripped and a Round tip stays procedural. Bare digits set Opacity and Shift+digits set Flow. The options bar shows Size, Opacity, Soft, Flow, and Tip.
- At stroke start the source layer's pixels are captured once (`history_brush_stroke_source_`). The writer installed by `install_brush_stroke_compositor` (`write_history_brush_pixel`) uses the Brush's per-stroke accumulation: each dab adds Opacity x Flow x coverage toward the Opacity cap, and the pixel becomes the stroke-start pixel interpolated toward the source pixel by that amount, in premultiplied RGBA. Alpha is interpolated too, so restoring a state where the layer was transparent removes later paint; outside the source layer's bounds the source is transparent (a layer named Background extends white, matching the stroke snapshot). On a layer without alpha, or with Lock Transparent Pixels, alpha is kept and only color moves, weighted by the source alpha. Only pixels under the footprint change, and a stroke is one "History Brush" undo step.

## Photoshop differences and omissions

- No Art History Brush, no blend Mode menu (Photoshop's History Brush has one; Normal is implemented), no Airbrush toggle, no Smoothing control, no brush dynamics.
- No History panel snapshots (Photoshop's camera button) beyond the opening state that the source already acts as; no source checkbox column (the context menu replaces it). The source does not appear in the panel once its state is evicted.
- No Eraser "Erase to History" option and no Fill > History.
- No layer-mask or channel painting from history.

## Patent note (claim check, October 2026)

The History Brush and History palette shipped in Photoshop 5.0 (1998), so painting pixels back from a stored earlier state of the same image is long-standing prior art; any patent on that basic form filed before then has expired. The active claims found in a Google Patents search for history- or undo-sourced painting do not read on this design: Adobe US 9972109 (out-of-order undo, to 2036) claims undo sessions that record the tiles each action affected and undoing a target action together with later "collateral" actions on the same tiles; Patchy keeps whole-document states and never records per-action tiles or undoes an action out of order. Microsoft US 9129416 (to 2033) claims storing undo state as XOR differences over grid cells of a stroke's bounding box (GPU, compressed); Patchy stores copy-on-write document copies. Adobe US 8234591 (thumbnails of edited areas as edit states) lapsed in 2024. Keep the design in this form: no per-action tile logs, no selective out-of-order undo, no XOR or diff-based state storage. A design adding any of those needs a new check.

Coverage: `ui_history_brush_restores_source_under_stroke_in_one_undo_step` (source pixels under the stroke only, selection limit, alpha restore, Opacity, one undo step per stroke) and `ui_history_brush_source_follows_panel_and_refuses_unusable_states` (default source, the panel marker, setting the source, the missing-layer and canvas-size refusals) in tests/ui/history_panel_tests.cpp; the hotkey and icon wiring ride `ui_photoshop_shortcuts_are_registered`, `ui_hotkey_defaults_have_no_conflicts`, and `ui_tool_palette_icons_render_sheet`.
