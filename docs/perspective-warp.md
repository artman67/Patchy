# Perspective Warp

Implementation and legal contract for `Edit > Perspective Warp`. The patent boundary below is binding; the short form is in [legal-constraints.md](legal-constraints.md). The shared warp renderer is described in [warp.md](warp.md).

## Product and UI contract

- Menu: Edit, directly above Free Transform (Photoshop's place). `editPerspectiveWarpAction`, hotkey id `edit.perspective_warp`, no default binding. Blocked in channel view like the other transform commands.
- Target: one RGB 8-bit pixel layer. `MainWindow::perspective_warp_active_layer` follows Liquify's rules: Quick Mask refuses, Smart Objects are refused with "Rasterize the Smart Object before using Perspective Warp", text and shape layers go through `prompt_rasterize_procedural_layer` first. `CanvasWidget::begin_perspective_warp` re-checks (pixel and position locks, empty layers, edit targets other than the layer or its mask) and commits any open Free Transform or Warp Transform first.
- Layout mode (entry): dragging on empty canvas lays out a rectangle quad (each side at least `kPerspectiveQuadMinimumSide`); corners drag freely; a click inside a quad selects it; Delete or Backspace removes the selected quad. Moving a layout corner carries its warped twin by the same offset. Layout shows the untouched layer.
- Warp mode: corner drags move only that corner of that quad, with a live preview. The canvas never moves a corner the user did not move.
- Quads stay separate in both modes: every quad strictly convex and clockwise, every pair at least `kPerspectiveQuadGap` (4 px) apart (`perspective_quads_are_valid`). A drag that would break this leaves the corner at its last allowed position; a refused new quad reports "Quads can't touch or overlap. Leave a gap between planes."
- Options bar (owns the row like the transform sessions; `perspective_warp_option_actions_`): `perspectiveWarpLayoutButton` / `perspectiveWarpWarpButton` (Warp needs a quad), the straighten trio `perspectiveWarpStraightenVerticalButton`, `perspectiveWarpLevelHorizontalButton`, `perspectiveWarpStraightenBothButton` (Warp mode, enabled with a selected quad), `perspectiveWarpRemoveAllButton`, `perspectiveWarpApplyButton`, `perspectiveWarpCancelButton`.
- Straighten is one-shot on the selected quad (`straighten_perspective_quad`): each side closer to vertical gets both endpoints moved to their mean x, each side closer to horizontal to their mean y. It reads only that quad's corners, records nothing, and shows no marker; a later drag may bend the side again. A result that would fold the quad or break the gap is refused.
- Enter or the apply button commits, Esc or the cancel button cancels, a tool switch or a different layer selection commits (Warp Transform's rule). Untouched corners commit nothing. Edit > Undo inside the session steps back the last quad edit (add, remove, corner drag, straighten, Remove All Quads); Redo is disabled until the session ends.
- Commit: one resample of the original pixels, one undo step "Perspective Warp". Layer bounds grow or shrink to the warped extent; uncovered areas are transparent. The document selection is ignored and layer masks are not warped (Warp Transform's pixel-bake behavior).

## Math

- Each quad maps by its own exact homography, layout quad onto warped quad (`homography_from_quad_to_quad`: the unit-square homographies of `core/warp_mesh`, Heckbert 1989). An unmoved quad is the exact identity.
- `build_perspective_warp_grid` builds the forward lattice over the layer: nodes inside a layout quad take that quad's homography; every other node takes the harmonic membrane of those displacements via `solve_heal_membrane` (int16 offsets scaled so the largest uses most of the range, finest step 1/1024 px). The layer border is a free (Neumann) boundary, so the whole layer follows the quads and nothing is pinned.
- Preview and commit use the same lattice (4 px cells, at most 256 per axis) and the same resampler, `resample_warped_rgba8` with `invert_bilinear_cell`, first writer wins on folds. The membrane's fixed-sweep cascade is not fully converged on large lattices (about 2 percent of the largest move in the far field); the field stays smooth and the preview matches the commit exactly.
- The stretch peaks beside a moved corner (the membrane's corner singularity); the gap between neighbouring quads fills smoothly.

## Photoshop differences

- Left out by the patent boundary: quads sharing edges or snapping together, Shift-click edges that stay straight (shown yellow in Photoshop), the automatic straighten buttons acting on every quad at once and keeping the result, vanishing-point handles, and line detection.
- Smart Objects: refused. Photoshop's Perspective Warp smart-filter descriptor is uncalibrated; do not synthesize it (Liquify rule in [liquify.md](liquify.md)).
- Not implemented: dragging a whole quad, in-session Redo, 16-bit and 32-bit layers. Photoshop's own warp solve differs from the membrane, so results are similar in kind, not identical.
- The name "Perspective Warp" has not been checked against Adobe's trademark list.

## Patent boundary

Claim-level review dated 2026-10-07 (from public claim text; not legal advice). Verdict: clear with the limits below.

Active claims and why the design misses them:

- [Adobe US 9117253](https://patents.google.com/patent/US9117253B2/en) "Perspective warp", to 2033-06-17 (11.5-year fee due 2027-02-25). Every independent claim (1, 15, 19, 22) needs a sub-portion that is to remain unchanged, identified from constraint information the user gives through the UI, kept unchanged in a real-time preview; claim 1 adds a visual indicator. Photoshop's Shift-click yellow straight edge reads on claims 1, 12 and 14. Patchy has no lock, pin, frozen region, straight-line constraint, or locked-part indicator, so the constraint element and with it the indicator and preview elements are absent.
- [Adobe US 9196096](https://patents.google.com/patent/US9196096B2/en) "3D-consistent 2D manipulation of images", to 2033-11-14 (fee due 2027-05-24). Claims 1, 11 and 16 need a second quad sharing an edge with the first and a consistency error computed along that shared edge for a 3D scene constraint. Patchy quads never share an edge or corner and nothing computes a consistency error, w-ratio, or seam measure.
- [Adobe US 9805499](https://patents.google.com/patent/US9805499B2/en) (continuation), to 2033-03-11. Claims 1, 16 and 19 need edge-sharing quads and automatic new positions for the vertices the user did not move. Neither exists: quads stay apart and only the dragged corner moves.
- [Marvell US 10055812](https://patents.google.com/patent/US10055812B1/en) "Image mesh warping", about 2036-10-26 (status unverified). Claims 1 and 7 compute cell coordinates from the areas of four triangles. Patchy keeps the closed-form bilinear inverse; never switch to an area-ratio inverse (same rule as Puppet Warp).
- Not practiced: Adobe US 8525871 / US 9742994 (salient features of wide-angle images), Intel US 10354364 (vanishing points from lines detected in the image), Adobe US 10776973 (vanishing point computation), Adobe US 12307626 (cylindrical surfaces). The straighten command never reads pixels.

Prior art and expired teachings relied on: Heckbert 1989 projective mappings, GIMP's Perspective tool, the expired Adobe Vanishing Point family (US 7525555, US 7808502; Photoshop CS2, 2005), Carroll, Agarwala and Agrawala, SIGGRAPH 2010, and the healing membrane of expired US 6587592 ([healing.md](healing.md)).

Must stay out until the dates shown, each needing a new review first: quads that share edges or corners, corner snapping between quads, seam reconciliation or any joint solve across quads (US 9196096 to 2033-11-14, US 9805499 to 2033-03-11); user-set "remains unchanged" constraints such as locked edges, pinned points or frozen areas, especially with a distinct color (US 9117253 to 2033-06-17); vanishing-point handles and content-detected lines. A future "join quads and smooth the seam" feature aims at the same result as Adobe's claims and needs its own review. Re-check the dates if Adobe skips the 11.5-year fees.

## Regression coverage

- Core (`tests/core/perspective_warp_tests.cpp`): `perspective_warp_unmoved_quads_are_exact_identity`, `perspective_warp_quad_interior_follows_its_own_homography`, `perspective_warp_membrane_moves_the_rest_smoothly` (maximum principle, no tears, free border moves), `perspective_warp_straighten_makes_sides_exactly_vertical_or_level`, `perspective_warp_quads_cannot_touch_or_share_corners`.
- UI (`tests/ui/perspective_warp_tests.cpp`): `ui_perspective_warp_session_commits_one_undo_step_and_escape_restores` (menu, options-bar row, canvas drags, gap refusal, in-session Undo, byte-exact Esc, no-op commit, one undo step), `ui_perspective_warp_straightens_a_leaning_side_in_the_pixels` (straighten button through to committed pixels; saves before, preview and after captures), `ui_perspective_warp_refuses_smart_objects`.
