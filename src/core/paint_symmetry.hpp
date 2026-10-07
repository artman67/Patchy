#pragma once

// Paint Symmetry (docs/paint-symmetry.md): Photoshop-style mirrored and radial
// painting. A symmetry is a fixed set of rigid copy placements around one
// center. The brush engine paints every dab and segment once in the stroke's
// own frame and repeats it through each placement (EditOptions::symmetry), so
// spacing, dynamics jitter and scatter are computed once and every copy is an
// exact mirror or rotation of the original. Pure geometry, no Qt types.
//
// Patent boundary (claim check 2026-10-06, docs/paint-symmetry.md): straight
// axes and rotations about one point only, applied to raster dabs at paint
// time. Never re-render or re-clip already painted pixels when the axis moves
// (no live reflected object, Adobe US 11062493), never reflect about a curved
// path, and never derive an axis from existing content or snap to one.

#include <cstddef>
#include <vector>

namespace patchy {

// Persisted nowhere (session tool state), but the menu order follows this.
enum class PaintSymmetryMode {
  Off,
  Vertical,    // mirror across a vertical axis
  Horizontal,  // mirror across a horizontal axis
  DualAxis,    // both axes: four copies
  Diagonal,    // mirror across a 45 degree axis rising to the right
  Radial,      // N rotated copies
  Mandala      // N rotated copies, each also mirrored inside its segment
};

inline constexpr int kPaintSymmetryMinSegments = 2;
inline constexpr int kPaintSymmetryMaxRadialSegments = 12;   // Photoshop's Radial range
inline constexpr int kPaintSymmetryMaxMandalaSegments = 10;  // Photoshop's Mandala range
inline constexpr int kPaintSymmetryDefaultSegments = 6;

// One rigid copy placement: p' = (xx * x + xy * y + tx, yx * x + yy * y + ty).
// The 2x2 part is orthogonal (a rotation or a reflection), so its inverse is
// its transpose: unmap_offset turns an offset around a copy's dab center back
// into the original dab's frame, which is how copies reuse the original
// footprint math unchanged.
struct SymmetryTransform {
  double xx{1.0};
  double xy{0.0};
  double yx{0.0};
  double yy{1.0};
  double tx{0.0};
  double ty{0.0};

  [[nodiscard]] double map_x(double x, double y) const noexcept { return xx * x + xy * y + tx; }
  [[nodiscard]] double map_y(double x, double y) const noexcept { return yx * x + yy * y + ty; }
  [[nodiscard]] double unmap_offset_x(double dx, double dy) const noexcept { return xx * dx + yx * dy; }
  [[nodiscard]] double unmap_offset_y(double dx, double dy) const noexcept { return xy * dx + yy * dy; }
};

[[nodiscard]] bool paint_symmetry_uses_segments(PaintSymmetryMode mode) noexcept;
[[nodiscard]] int clamp_paint_symmetry_segments(PaintSymmetryMode mode, int segments) noexcept;

// The copies for `mode` about (center_x, center_y), in the brush engine's
// sample coordinates (a dab at x is centered on pixel x), with the whole
// symmetry rotated by `angle_degrees` (clockwise on screen, y down). The
// identity (the stroke itself) is not included; Off returns no copies.
[[nodiscard]] std::vector<SymmetryTransform> paint_symmetry_transforms(PaintSymmetryMode mode,
                                                                       int segments,
                                                                       double center_x,
                                                                       double center_y,
                                                                       double angle_degrees = 0.0);

// True when copies[index] lands (x, y) where the stroke itself or an earlier
// copy already put it: on a mirror axis or at the radial center. The brush
// engine skips such a copy so the axis never double-stamps (two identical
// soft dabs would build their edges past one dab's coverage).
[[nodiscard]] bool paint_symmetry_copy_repeats_point(const std::vector<SymmetryTransform>& copies,
                                                     std::size_t index, double x, double y) noexcept;
// The segment form: copies[index] maps the segment onto itself or onto an
// earlier copy's, in either direction (a stroke crossing a mirror axis at a
// right angle maps onto itself reversed).
[[nodiscard]] bool paint_symmetry_copy_repeats_segment(const std::vector<SymmetryTransform>& copies,
                                                       std::size_t index, double x0, double y0,
                                                       double x1, double y1) noexcept;

}  // namespace patchy
