# JT's Patchy fork: roadmap

Fork-only file. Lives on the `jt` branch, never on a `feature/*` branch.

## Goal

Close the gaps between Patchy and Photopea in four areas (selections, adjustments, warping,
painting) so Patchy can replace Photoshop for thumbnails and painting on Linux. AI features
(Select Subject, Remove BG, generative fill) are out of scope for now. Each feature is built on
its own branch as a clean, tested, upstream-quality PR candidate. JT uses them on the `jt`
branch first; only the ones Seth wants go upstream, and only when JT says so.

## Ground rules

- Follow upstream `AGENTS.md` and `docs/` rules. Local substitutes are in `agents_local.md`.
- Every PR: the feature, a few focused tests, the matching `docs/` update, all 8 translations,
  zero new warnings, and a plain description with screenshots.
- No Photoshop on this machine yet. PSD-writing features follow the published spec, round-trip
  through Patchy, and say that Photoshop verification is still owed.
- Features near Adobe patents (marked "patent") get a short research note in the style of
  `docs/patent-research.md` before code, and stay inside the boundaries Seth already set.

## Branches and status

| # | Branch | Feature | Status |
|---|---|---|---|
| F0 | `fix/gcc16-build` | Build fixes for GCC 16 (missing include, switch case, false positive) | done, PR-ready |
| P1 | `feature/rotate-view` | Rotate View tool (R), Shift snaps 15 deg, Reset View | done, in jt, PR-ready |
| A1 | `feature/gradient-map` | Gradient Map adjustment (`grdm`) | done, in jt, PR-ready |
| A2 | `feature/selective-color` | Selective Color adjustment (`selc`) | done, in jt, PR-ready |
| A3 | `feature/vibrance` | Vibrance adjustment (`vibA`); Exposure shipped upstream in v1.06 | done, in jt, PR-ready |
| A4a | `feature/black-white` | Black & White adjustment (`blwh`) | done, in jt, PR-ready |
| A4b | `feature/photo-filter` | Photo Filter adjustment (`phfl`) | done, in jt, PR-ready |
| A4c | `feature/channel-mixer` | Channel Mixer adjustment (`mixr`) | done, in jt, PR-ready |
| A5 | `feature/color-lookup` | Color Lookup adjustment (.cube, `clrL`) | done, in jt, PR-ready |
| A6a | `feature/shadows-highlights` | Image > Adjustments > Shadows/Highlights | done, in jt, PR-ready |
| A6b | `feature/match-color` | Image > Adjustments > Match Color | done, in jt, PR-ready |
| A6c | `feature/replace-color` | Image > Adjustments > Replace Color (stacked on S1) | done, in jt, PR-ready |
| A6d | - | Desaturate | dropped: exists upstream |
| P2 | `feature/symmetry-painting` | Paint Symmetry (mirror, radial, mandala) | done, in jt, PR-ready |
| P3 | `feature/color-replacement` | Color Replacement tool (Brush flyout, Shift+B) | done, in jt, PR-ready |
| P4 | `feature/history-brush` | History Brush (Y) | done, in jt, PR-ready |
| S1 | `feature/color-range` | Select > Color Range | done, in jt, PR-ready |
| S2 | - | Refine Edge / Select and Mask | dropped by JT 2026-10-07 |
| W3 | - | Content-Aware Move | dropped by JT 2026-10-07 (Remove Object covers fill) |
| W1 | `feature/perspective-warp` | Perspective Warp, separate quads only (patent limits to 2033) | done, in jt, PR-ready |
| W2 | `feature/puppet-warp` | Puppet Warp on moving least squares (ARAP patented to ~2027-08) | done, in jt, PR-ready |
| W4 | - | Content-Aware Scale | dropped by JT 2026-10-07 |

PR branches are `pr/<topic>` (feature minus the GCC fix, on upstream v1.06). Drafts and filing
notes: [pr-drafts.md](pr-drafts.md). Warp patent study: [patent-research-warps.md](patent-research-warps.md), summarized in each
warp's docs file.

## After wave 1

Open one GitHub issue on SethRobinson/Patchy showing the wave 1 work and asking which of the
rest he would accept upstream. JT posts it or approves the text first.
