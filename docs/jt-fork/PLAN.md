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
| F0 | `fix/gcc16-build` | Build fixes for GCC 16 (missing include, switch case, false positive) | built |
| P1 | `feature/rotate-view` | Rotate View tool: rotate the canvas view (R), Shift snaps 15 deg, Reset View | wave 1 |
| A1 | `feature/gradient-map` | Gradient Map adjustment layer with native PSD `grdm` block | wave 1 |
| A2 | `feature/selective-color` | Selective Color adjustment layer | wave 2 |
| A3 | `feature/exposure-vibrance` | Exposure and Vibrance adjustment layers | wave 2 |
| A4 | `feature/channel-adjustments` | Black & White, Photo Filter, Channel Mixer adjustment layers | wave 2 |
| A5 | `feature/color-lookup` | Color Lookup adjustment layer (.cube) | wave 2 |
| A6 | `feature/destructive-adjustments` | Desaturate, Shadows/Highlights, Replace Color, Match Color | wave 2 |
| P2 | `feature/symmetry-painting` | Symmetry painting (mirror, radial) | wave 2 |
| P3 | `feature/color-replacement` | Color Replacement tool | wave 2 |
| P4 | `feature/history-brush` | History Brush | wave 2 (optional) |
| S1 | `feature/color-range` | Select > Color Range | wave 3 |
| S2 | `feature/refine-edge` | Refine Edge / lighter Select and Mask (patent) | wave 3 |
| W3 | `feature/content-aware-move` | Content-Aware Move (inside the existing inpainting boundary) | wave 4 |
| W1 | `feature/perspective-warp` | Perspective Warp (patent) | wave 4 |
| W2 | `feature/puppet-warp` | Puppet Warp (patent) | wave 4 |
| W4 | `feature/content-aware-scale` | Content-Aware Scale (patent) | wave 4 |

## After wave 1

Open one GitHub issue on SethRobinson/Patchy showing the wave 1 work and asking which of the
rest he would accept upstream. JT posts it or approves the text first.
