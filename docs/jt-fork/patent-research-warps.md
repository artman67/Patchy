# Patent research: Perspective Warp and Puppet Warp (claim check 2026-10-07)

Engineering research record for two proposed Patchy features, written in the style of
`docs/patent-research.md`. It is drawn from public claim text, not legal advice. Read
`docs/legal-constraints.md` first. When in doubt this record picks the more conservative answer.

Sources: Google Patents pages and its query endpoint (rate-limited partway through the session),
USPTO full-image PDFs (`image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/<number>`)
OCR'd locally for claim text and front-page term data, and web search. "Status" means Google
Patents' legal-events table where it could be read; anything marked UNVERIFIED was not read from a
legal-status source.

## Verdicts

| Feature | Verdict | Gating active claims | Date that changes the answer |
|---|---|---|---|
| Perspective Warp | **CLEAR WITH LIMITS** | Adobe US 9117253 (user-chosen parts that stay unchanged), Adobe US 9196096 / US 9805499 (edge-sharing quads made 3D-consistent) | 2033-03-11 / 2033-06-17 / 2033-11-14, or earlier if Adobe skips the 11.5-year fees (due 2027-02-25, 2027-05-24, 2029-04-30) |
| Puppet Warp | **CLEAR WITH LIMITS** (the Photoshop-style triangle-mesh ARAP solver is BLOCKED until about 2027-08-07; a moving-least-squares design is clear now) | Univ. of Tokyo US 8063917 (Igarashi's two-step ARAP solver); Adobe US 9053553 / US 9454797 (pin pop-up menu); Adobe US 10643365 / US 10964081 (on-canvas bend/rotate handle ring) | about 2027-08-07 for US 8063917 (status UNVERIFIED, could already be fee-lapsed); 2030-02-26 / 2033-01-25 for the pop-up claims; 2038-11-20 for the ring control |

---

## 1. Perspective Warp

### What Photoshop does

Layout mode: the user draws quads over planes; corners snap so quads can share an edge. Warp mode:
the user drags corner pins, and the whole layer is re-rendered as one smooth warp. Helpers: three
auto buttons (straighten near-vertical edges, level horizontally, straighten vertically) and
Shift-click on an edge to straighten it and keep it straight (shown in yellow). References:
[Adobe help](https://helpx.adobe.com/photoshop/desktop/repair-retouch/clean-restore-images/edit-different-perspectives-in-the-same-image.html).

### Active claims

**Adobe US 9117253 B2, "Perspective warp"** ([GP](https://patents.google.com/patent/US9117253B2/en)).
Intwala and Agarwala. Priority and filing 2012-07-20. Granted 2015-08-25. Active. Expires
**2033-06-17** (with term adjustment). Fees: 4-year paid 2019-02-25, 8-year paid 2023-02-27. The
11.5-year fee is due by 2027-02-25 (grace period to 2027-08-25).
- Claim 1 (method) requires ALL of:
  - (a) receiving an indication of a portion of media to be perspective-manipulated;
  - (b) a UI arising from that portion;
  - (c) identifying a sub-portion inside it that is **to remain unchanged**, based on
    **constraint information received through the UI**;
  - (d) a **visual indicator** marking that sub-portion;
  - (e) performing the manipulation according to the user input and the constraint;
  - (f) a **real-time preview in which the identified sub-portion is not changed**.
- Claim 15 (program product): mirrors claim 1.
- Claims 19 (program product) and 22 (system) require: information about a user-defined
  quadrilateral; identifying an unchanged sub-portion inside the quad from **constraint
  information received via the UI**; translating direct interaction with the quad into the
  manipulation without altering that sub-portion; and a real-time preview that keeps it unchanged.
  These two claims drop the visual indicator, but the user-supplied "remains unchanged" constraint
  is in every independent claim.
- Dependent claims add: quads defined by 4 points (2); constraint types point, line, or quad (3);
  automatic perspective from a default setting (6); ARAP for the rest of the image (7); extension
  regions (8, 9); automatic detection to help the markup (10); a linear region, quad edge, or
  stationary point that stays put (11 to 13); a differently colored indicator for the locked part
  (14).
- Photoshop's Shift-click "straighten and keep straight" edge in yellow reads on claims 1, 12 and
  14 almost word for word.

**Adobe US 9196096 B2, "3D-consistent 2D manipulation of images"**
([GP](https://patents.google.com/patent/US9196096B2/en)). Agarwala. Priority and filing
2013-03-11. Granted 2015-11-24. Active. Expires **2033-11-14**. Fees: 4-year paid 2019-05-24,
8-year paid 2023-05-24. The 11.5-year fee is due by 2027-05-24.
- Claim 1 requires ALL of:
  - selection of a first quad;
  - selection of a **second quad sharing an edge** with the first;
  - a manipulation of either quad;
  - **calculating a consistency error** tied to the manipulation, **based on the shared edge**;
  - displaying an updated view based on a **3D scene constraint** that depends on that error and
    the manipulation.
- Independent claims 11 and 16 share those elements and add no tearing along the shared edge (11)
  and no 3D model reconstruction (16).
- The specification defines the error as the mismatch, along the shared edge, between the
  homogeneous w-ratios of the two quads' homographies. It is minimized with Gauss-Newton together
  with cursor and keep-other-vertices-still terms.
- Dependent claims: real-time display (2); dragging a corner, vanishing point, or pin on a line
  (3 to 5); keep a line horizontal or vertical (6, 7); a fixed vertex (8).

**Adobe US 9805499 B2** (continuation of US 9196096)
([GP](https://patents.google.com/patent/US9805499B2/en)). Filed 2015-10-15. Active. Expires
**2033-03-11**; no terminal disclaimer is shown. Fees: 4-year paid 2021-04-30, 8-year paid
2025-04-30.
- Claim 1 requires ALL of:
  - first quad, plus a **second quad sharing an edge**;
  - a manipulation of a vertex;
  - in response, **determining new positions for the remaining vertices** of both quads so they
    **satisfy a 3D scene constraint** (a function keeping a consistent w-ratio of the shared edge's
    two vertices under the two homographies);
  - generating an updated view.
- Independent claims 16 (medium) and 19 (system) as summarized: the same structure, apparently
  without the w-ratio wording. That makes them **broader**: any automatic repositioning of the
  vertices the user did not drag, in edge-sharing quads, to meet a "3D scene constraint".
  Dependent claims cover keeping lines vertical or horizontal and moving vanishing points.

**Adobe US 8525871 B2 / US 9742994 B2, "Content-aware wide-angle images"** (Agarwala, Carroll).
Priority 2008-08-08. Active. US 8525871 expires 2031-08-09; US 9742994 has a terminal disclaimer.
- Every independent claim needs a source **wide-angle** image, **identified locally salient
  features**, and a spatially varying mapping solved from salience-weighted constraint equations.
- Not practiced: Patchy detects no features and only applies user-drawn quads.

**Intel US 10354364 B2, "Automatic perspective control using vanishing points."** Priority
2015-09-14. Active to 2037-11-12.
- Every independent claim needs a vanishing point determined from **lines detected in the image**,
  then rotating and warping.
- Not practiced, as long as the helpers never read pixels.

**Adobe US 12307626 B2** (warping artwork onto perspective cylinders with bicubic patches,
priority 2022-11-21, to 2043-07-12). Claim 1 requires wrapping onto a **cylindrical** surface with
zero-width edge patches. Not practiced.

**Marvell US 10055812 B1, "Image mesh warping."** Filed 2016-08-29, term adjustment 58 days, so
about 2036-10-26. Status UNVERIFIED.
- Claims 1 and 7: for each destination pixel, a bounding box around its cell quad, an inside test,
  then coordinates computed as **percentages of two side lengths using the areas of the four
  triangles** formed by the pixel and the cell corners.
- Patchy's existing `invert_bilinear_cell` (closed-form inverse bilinear) and plain triangle
  barycentrics are a different calculation. Keep the renderer on those and do not switch to a
  four-triangle area-ratio inverse. This applies to Puppet Warp too.

### Expired (now prior art)

- **Adobe Vanishing Point family:**
  - US 7525555 ("image-editing operations across multiple perspective planes", priority
    2004-10-26): Expired, Lifetime, 2025-06-02 per Google Patents.
  - US 7808502 ("Perspective editing tools for 2-D images"): continuation of 10/974,547 filed
    2004-10-26, with a terminal disclaimer and no term adjustment, so it ended about 2024-10-26
    (taken from the front page; the status row was not read).
  - Photoshop CS2 Vanishing Point (2005) shipped multiple edge-joined perspective planes.
- US 7952595 (TU Munich, physical-model image deformation): lapsed for fees 2019. US 8400472
  (Technion, complex barycentric cage deformation): expired for unpaid fees.

### Prior art (fallback only; the design below does not depend on any of it)

- Heckbert 1989 projective mappings (rectangle to quad homography); GIMP's Perspective tool (1990s).
- Photoshop CS2 Vanishing Point (2005).
- Hugin and other perspective-correction tutorials (2008, cited on US 9117253's face).
- Carroll, Agarwala and Agrawala, "Image Warps for Artistic Perspective Manipulation", SIGGRAPH
  2010: planar regions mapped by homographies, line constraints, and an otherwise shape-preserving
  warp. Published more than a year before US 9117253's 2012 filing.
- Patchy's own rect-to-quad homography (`core/warp_mesh`, `homography_from_rect_to_quad`), already
  cleared for Divide Scanned Photos.

### Design that clears every active claim above

1. **Layout.** The user draws quads; each quad has 4 corners. **Quads may not share an edge or a
   corner** in v1:
   - No snapping between quads. A drop that would make them touch keeps a minimum gap of a few
     pixels.
   - Reason: every independent claim of US 9196096 and US 9805499 needs a second quad *sharing an
     edge*. Without that element both patents are structurally out of reach, whatever the solver
     does.
   - The gap between neighbouring quads is not a tear. It is filled smoothly by step 3.
2. **Warp.** Dragging a corner moves **only that corner**. Each quad maps by its own exact
   homography from its layout quad to its warped quad (the existing
   `homography_from_rect_to_quad` math, generalized quad to quad).
   - Patchy never repositions a vertex the user did not drag.
   - No consistency error, w-ratio, edge-mismatch measure, or "3D scene constraint" is ever
     computed, and no seam warning is shown.
   - This misses US 9805499's "determining new positions for remaining vertices" and US 9196096's
     "calculating a consistency error".
3. **Rest of the layer.** Build one forward displacement field on a grid over the layer:
   - Inside each layout quad, displacement = H_i(x) - x.
   - Everywhere else, displacement is interpolated harmonically (relaxation of Laplace's equation
     with the quad interiors as fixed boundary values, the same machinery class as
     `core/heal_membrane`, an expired-patent teaching).
   - The layer border uses a **free (Neumann) boundary, not a fixed one**, so no part of the image
     is held still. The user never marks anything as fixed.
   - Render through the existing forward lattice plus inverse-bilinear resampler
     (`WarpSurfaceGrid` / `resample_warped_rgba8`), with first-writer-wins on folds as today.
4. **No "keep unchanged" constraints of any kind.**
   - No pins, locks, frozen edges, "keep this line straight", or fixed points.
   - No colored indicator for locked parts.
   - So element (c) of claim 1 and the matching element of claims 15, 19 and 22 of US 9117253 is
     absent, and with it elements (d) and (f).
   - A live preview while dragging is then allowed, because US 9117253's preview element only
     bites when it preserves a user-identified sub-portion.
5. **Straighten helpers (allowed in a narrow form).** A one-shot command acting on the
   **selected quad only**: "make this edge vertical" or "make this edge horizontal". It moves only
   that quad's own corner(s) and computes from quad geometry alone.
   - It records no lasting constraint (a later drag can bend the edge again) and shows no highlight.
   - It never reads pixels: no line or vanishing-point detection, which keeps it clear of Intel
     US 10354364, Adobe US 10776973 (vanishing point computation) and Upright-style features.
   - Since quads cannot share edges, it can never move a "remaining vertex of an edge-sharing quad".
6. **Commit.** One resample of the original pixels and one undo step, following the Warp Transform
   precedent.
   - Smart Objects: refuse, or require rasterizing, as Liquify does. Photoshop's Perspective Warp
     smart-filter descriptor is uncalibrated, so do not synthesize it.

**Must stay out until the dates shown (each needs a new check before then):**
- Quads that share edges or corners, corner snapping between quads, or any seam reconciliation or
  joint solve across quads: until **2033-11-14** (US 9196096) and **2033-03-11** (US 9805499).
- User-set "remains unchanged" constraints (Shift-click locked edges, pinned points, frozen areas),
  especially with a distinct color: until **2033-06-17** (US 9117253).
- Vanishing-point handles and content-detected lines.

Adobe could let these lapse early: watch for the 11.5-year fee deadlines in the table above.

**Element-by-element summary:**
- US 9117253 claims 1/15/19/22: no UI-supplied unchanged-sub-portion constraint, so they are
  missed. That also removes the indicator element of claim 1 and the "preview preserving it"
  element.
- US 9196096 claims 1/11/16: no second quad sharing an edge, and no consistency error; missed
  twice.
- US 9805499 claims 1/16/19: no shared edge, and no automatic repositioning of other vertices;
  missed twice.
- US 8525871 / US 9742994: no salient-feature detection and not a wide-angle projection.
- Intel US 10354364: no lines detected from the image.
- Marvell US 10055812: renderer uses the closed-form bilinear inverse, not four-triangle area
  ratios.

Doctrine-of-equivalents note: the shared-edge prohibition matters because the gap-plus-membrane
approach is a *different way* (no shared edge exists, and no 3D model or consistency term is
computed). A future "join quads and smooth the seam" feature would be aiming at the same result as
Adobe's claims and needs its own review.

---

## 2. Puppet Warp

### What Photoshop does

A triangle mesh is generated over the layer's opaque area (Density sets the triangle count,
Expansion grows or shrinks the outline). The user clicks to add pins and drags them; the mesh
deforms as rigidly as possible. Mode is Rigid, Normal or Distort. Pin depth buttons in the options
bar decide which part is in front where parts overlap. Pin rotation is Auto or Fixed (options bar),
or set by Alt-hovering near a pin and dragging the circle that appears.
[Adobe help](https://helpx.adobe.com/photoshop/desktop/effects-filters/artistic-stylize-filters/distort-specific-image-areas-with-puppet-warp.html).

### Active claims

**The University of Tokyo, US 8063917 B2, "Image processing system and program"** (inventor Takeo
Igarashi; this is the patent on the SIGGRAPH 2005 "As-Rigid-As-Possible Shape Manipulation"
algorithm). Front page, OCR'd from the
[USPTO PDF](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8063917):
- JP priority 2005-04-01; PCT filed 2006-03-30 (PCT/JP2006/306727); 371 date 2009-02-17;
  granted 2011-11-22.
- Term adjustment **495 days**, so it expires about **2027-08-07** (2026-03-30 plus 495 days).
- STATUS UNVERIFIED: Google Patents returned 503 for the rest of the session, and Patent Center,
  Justia, FPO and Patexia all blocked automated access. The 11.5-year fee was due 2023-05-22
  (grace to 2023-11-22); if the university skipped any fee, it lapsed early. **Treat it as active
  until someone confirms otherwise.**
- Foreign family: JP 4613313 (filed 2005-04-01, so its 20-year term ended about 2025-04-01) and
  EP 1865467 B1 (filed 2006-03-30, so it ended about 2026-03-30). Neither status was read, but both
  are past their maximum term.

Independent claims 1, 7, 13, 14 and 15 all require ALL of:
- (A) an original triangle mesh of the object, plus handle positions including **a fixed point and
  a movable point**;
- (B) a **first step**: an intermediate mesh that minimizes a quadratic error allowing each
  triangle **rotation, translation and uniform scale** (the "similarity" step);
- (C) a **fitting step**: each original triangle is rotated and translated to best fit its
  intermediate triangle;
- (D) a **final step**: the final mesh minimizes the quadratic difference of **triangle edge
  vectors** against the fitted triangles;
- (E) storing each mesh.

Claims 14 and 15 also require the precomputed and updated G matrices and inverses as handles are
added or removed.

This is exactly Igarashi's two-step closed-form solver: the standard "Puppet Warp" algorithm and
the one most open-source ports implement. A per-triangle local/global ARAP iteration (Liu et al.
2008 style) practices (C) and (D) on every iteration. It differs only if it is not started from a
similarity-step solve. That is a single-element miss with real doctrine-of-equivalents exposure, so
this record does **not** clear it.

**Adobe US 9053553 B2, "Methods and apparatus for manipulating images and objects within images"**
(Popović, Chien, Intwala, Kong; the Photoshop CS5 Puppet Warp team)
([GP](https://patents.google.com/patent/US9053553B2/en)). Filed 2010-02-26. Active. Expires
**2033-01-25**. Fees: 4-year paid 2018-12-10, 8-year paid 2022-12-09. The 11.5-year fee is due by
2026-12-09 (grace to 2027-06-09).
- Claims 1, 10 and 16 each require ALL of:
  - user-specified control points on vertices of a polygon mesh;
  - selecting a control point's on-canvas marker;
  - in response, a **pop-up menu displayed adjacent to the control point**, taking values for
    **translation, rotation and depth** properties;
  - receiving a value in that pop-up;
  - **propagating** it to vertices to form an initial deformed mesh with a rotation at each vertex;
  - **iterative optimization** into an improved mesh;
  - deforming and displaying.

**Adobe US 9454797 B2, "Deforming a surface via a control point"** (continuation of US 9053553)
([GP](https://patents.google.com/patent/US9454797B2/en)). Granted 2016-09-27. Active. Expires
**2030-02-26**. Fees: 4-year paid 2020-03-27, 8-year paid 2024-03-27.
- Claim 1: a GUI with a surface and a control point; a **pop-up menu adjacent to the control
  point**, opened on selecting it, taking translation, rotation and depth values; deforming at the
  control point from the value.
- Claims 8 and 14: the same, plus a control-point selection mode.
- Dependent claims: add-pin and move-pin mode elements (5 to 7, 12, 13, 18 to 20).
- **The pop-up menu adjacent to the pin is in every independent claim of both patents.** Photoshop
  itself puts depth and rotation in the options bar.

**Adobe US 10643365 B1 and US 10964081 B2, "Deformation mesh control for a computer animated
artwork"** (Stone; Character Animator)
([GP](https://patents.google.com/patent/US10643365B1/en),
[GP](https://patents.google.com/patent/US10964081B2/en)). Priority 2018-11-20. Active. Expires
**2038-11-20**.
- Every independent claim needs a **bend-handle control** with a **stationary portion** (which the
  user uses to set a control value, and which surrounds the pin) and a **movable portion** that
  follows the deformed position during the drag. Claims 18 and 3 add that the stationary portion
  snaps to the movable one when the drag ends.
- Claim 1 also needs a mesh bounded by the artwork's alpha edge.
- An on-canvas rotation ring around a pin is uncomfortably close to this.

**Adobe US 9865073 B2 / US 10078910 B2** (fast 2D deformation, Wampler, priority 2015-10-05;
expire 2036-04-06 and 2035-10-05) and **US 10217262 B2** (adaptive meshing, same priority, to
2035-10-05).
- Every independent claim requires **bounded biharmonic weights** for the handles.
- US 9865073 and US 10078910 also require handle affine transforms from a closed-form
  convex-concave expression over rotation clusters.
- US 10217262 also requires iterative mesh refinement driven by weight differences.
- Not practiced if Patchy uses no biharmonic weights.

**Adobe US 10672174 B2** (handle locations, priority 2018-06-28, to 2038-06-28). Requires a
**neural network** that outputs clusters whose centroids become pins. Not practiced: pins are only
user-placed.

**Other active Adobe claims examined and judged not to read on the design below:**

| Patent | Topic | Expiry (from front page) | Why it does not read |
|---|---|---|---|
| US 12482056 B2 | local deformation | about 2044-08-01 (filed 2023-07-21, PTA 377 days) | Every claim needs an energy with an elastic term plus a **thresholded regularization term** on vertex displacement (claim 7 adds three-block ADMM) |
| US 10410317 B1 | spline handles | 2038-03-26 | Needs a **spline handle segment** with tangent handles |
| US 10832446 B2 | bone handles | 2039-01-07 | **Vector** objects, mesh contraction and skeleton extraction |
| US 10140764 B2 | multiple input meshes | about 2037-02-22 | Needs a **plurality of input meshes** blended in shape space |
| US 10510186 B2 | vector Bezier continuity | 2037-12-22 | Vector Bezier continuity |
| US 11164355 B1 | vector sculpting | 2040-04-23 | Vector sculpting constraints with priority ordering |
| US 11682149 B2 | edge-targeting handle | about 2041-08-16 | Needs an edge **determined from the image**, a vector-spline handle, and two meshes |
| US 8229247 B1 | structure-preserving editing | about 2031-05-21, status UNVERIFIED | Needs user **symmetry information** plus detected structural similarities |
| US 12586148 B2 | row shifting | 2044 | Row-shift warping |

**Expired or lapsed:**
- US 7952595 (TU Munich): fee-lapsed 2019. Its claim 18 included per-region "depth".
- US 8400472 (Technion): fee-expired.
- Adobe's classic brush-warp family (US 6765589, US 7098932, US 7567263): expired, already recorded
  in `docs/liquify.md`.

No active US patent on 2D moving-least-squares (MLS) image deformation was found; the only hit was
the Chinese application CN104143204A, which has no US family seen.

### Prior art

- Igarashi, Moscovich and Hughes, SIGGRAPH 2005 (July 2005; the same disclosure as US 8063917).
- Schaefer, McPhail and Warren, "Image Deformation Using Moving Least Squares", SIGGRAPH 2006
  (rigid, similarity and affine MLS, point and line handles).
- After Effects CS3 Puppet tool (July 2007): deform, starch and **overlap/depth pins**.
- Zhu and Gortler 2007 (MLS with approximate geodesic distances).
- Sorkine and Alexa 2007 (local/global ARAP).
- GIMP Cage Transform (GSoC 2010, shipped in 2.8, 2012).
- GIMP N-Point Deformation (ARAP, GSoC 2013; Playground in 2.10, 2018).
- Krita Cage transform (2.x era).
- Photoshop CS5 Puppet Warp (April 2010).

### Design that clears every active claim above (v1)

**Solver: rigid moving least squares (Schaefer 2006), not a triangle-mesh ARAP solve.**
1. **Pins.** User-placed only, by click; no automatic placement. Each pin has a rest position and a
   current position.
2. **Deformation.** On a regular grid (Density = grid spacing), each grid node gets the
   closed-form best **rigid** transform of the pins' rest positions onto their current positions,
   weighted by w_i = 1 / |p_i - v|^(2α).
   - **Rigid, Normal and Distort** map onto Schaefer's own variants: rigid MLS, a blend of rigid
     and similarity MLS, and affine MLS.
   - Each node is solved on its own: no triangle mesh solve, no intermediate similarity mesh, no
     per-triangle fit, and no edge-vector least squares. So elements (B), (C) and (D) of every
     US 8063917 claim are absent. There is no iteration, which also misses US 9053553's "iterative
     optimization".
3. **Mesh and region.** Show the grid as the "mesh".
   - Restrict it to the layer's opaque pixels grown or shrunk by **Expansion** (plain morphological
     dilation or erosion of the alpha mask).
   - Render through the existing forward lattice plus `invert_bilinear_cell` resampler; keep that
     inverse, not Marvell's area-ratio inverse.
   - A plain grid avoids any alpha-bounded tessellation; that is only one element of US 10643365,
     but it adds margin.
4. **Pin rotation.** "Auto" means the MLS solution's own local rotation. "Fixed" means the pin acts
   as a short MLS line-segment handle (Schaefer section 5) at a user-entered angle.
   - Angle and depth are set **only in the options bar**, never through a pop-up next to the pin
     and never through an on-canvas ring or dial around the pin.
   - This misses the pop-up element of every claim of US 9053553 and US 9454797, and the
     stationary-plus-movable bend-handle control in every claim of US 10643365 and US 10964081.
5. **Pin depth.** Options-bar "bring forward / send backward" for the selected pin (Photoshop's own
   placement; After Effects CS3 overlap pins are 2007 prior art).
   - Each grid node carries a depth interpolated from pin depths with the same MLS weights.
   - Where the warped surface folds over itself, the higher depth wins, replacing first-writer-wins
     for this tool only.
6. **Weights.** v1 uses plain Euclidean distance.
   - Known limitation: nearby limbs can drag each other.
   - Geodesic or inside-the-shape distance weights are a likely v2 (Zhu and Gortler 2007 prior art)
     but need their own short check before shipping.
7. **Commit.** One resample from the original pixels and one undo step.
   - Pins are edit-session state only. Do not author Photoshop's Puppet Warp smart-filter
     descriptor; Smart Objects need rasterizing, like Liquify.
   - Live preview during drag is fine: no claim found turns on preview timing.

**Must stay out until the dates shown (each needs a new check before then):**
- Igarashi's two-step triangle solver, or a per-triangle local/global ARAP initialized from a
  similarity solve: until about **2027-08-07** (US 8063917), or until someone confirms it lapsed
  for non-payment.
  - After that, a Photoshop-like triangle-mesh ARAP becomes possible, still subject to the UI
    limits below.
  - Do not port GIMP's N-Point Deformation code: it is GPL and implements the same ARAP family.
- A pop-up or popover next to a pin carrying translation, rotation and depth fields: until
  2030-02-26 (US 9454797) and 2033-01-25 (US 9053553).
  - Adobe might skip the 11.5-year fee on US 9053553, which is due 2026-12-09.
- An on-canvas rotate ring, dial or "bend handle" around a pin: until **2038-11-20**.
- Bounded biharmonic weights: until 2035/2036. Neural or automatic pin placement: until 2038.
  Thresholded-regularization "local" energies: until 2044. Spline or bone handles derived from a
  skeleton.

**Element-by-element summary:**
- US 8063917 claims 1/7/13/14/15: no similarity-step intermediate mesh, no per-triangle rigid fit,
  no edge-vector solve, no triangle mesh solve at all. (B), (C) and (D) are absent.
- US 9053553 claims 1/10/16: no pop-up adjacent to a pin, no iterative optimization.
- US 9454797 claims 1/8/14: no pop-up.
- US 10643365 / US 10964081: no bend-handle control with stationary and movable portions.
- US 9865073 / US 10078910 / US 10217262: no bounded biharmonic weights.
- US 10672174: no neural network.
- US 12482056: no thresholded regularization term.

---

## Unverified and limits of this check

- **US 8063917 legal status** (did the University of Tokyo pay the 3.5, 7.5 and 11.5-year fees?)
  could not be read. If it lapsed, the triangle-mesh ARAP block disappears now. Worth one manual
  look at Google Patents' legal events or USPTO Patent Center before deciding between MLS and ARAP.
- Statuses read from front pages only (no legal-events check): US 7808502, US 8229247,
  US 10055812, and the OCR'd Adobe items in the US 10410317 to US 12586148 group.
- The keyword and classification sweep was cut short when Google rate-limited after about 15
  queries. Coverage is strong for Adobe (CPC G06T3/18 plus keyword sweeps for mesh deformation,
  control points and rigidity, and perspective quadrilateral warp). It is thin for non-Adobe
  holders (Apple, Google, Microsoft, Celsys, whose Clip Studio Paint 4.0 Puppet Warp shipped in
  2025, and Chinese and Korean families with US members). Japanese and European patents were only
  checked for the Igarashi family.
- Prosecution history was not read. US 9053553's pop-up limitation may have been added to get
  around prior art, which would narrow doctrine-of-equivalents reach (Festo). That is unconfirmed,
  and the design does not rely on it.
- Trademarks: "Puppet Warp" and "Perspective Warp" were not checked against Adobe's trademark list.
  Following the Filter Gallery precedent, use descriptive labels or check before naming the
  commands.
