#pragma once

#include "core/warp_mesh.hpp"

#include <array>
#include <optional>
#include <vector>

// Perspective Warp geometry (Edit > Perspective Warp; Qt-free, deterministic
// doubles). The user draws quads over planes (Layout) and drags their corners
// (Warp). Each quad's interior maps by its OWN exact homography, layout quad
// onto warped quad; every other lattice node takes the harmonic membrane of
// core/heal_membrane (the expired US 6587592 teaching) over those
// displacements, with a free (Neumann) layer border, so nothing is held fixed.
//
// Patent boundary, binding (docs/perspective-warp.md, docs/legal-constraints.md):
// quads never share an edge or a corner (perspective_quads_are_valid keeps a
// gap), nothing solves quads jointly or moves a corner the user did not move
// (Adobe US 9196096 / US 9805499), there is no "keep this unchanged" constraint
// or locked edge (US 9117253), nothing reads pixels to find lines or vanishing
// points, and the renderer keeps warp_mesh's closed-form bilinear cell inverse
// (never a four-triangle area-ratio inverse, US 10055812).
namespace patchy {

// Corners in document pixels as (x, y) pairs: top-left, top-right,
// bottom-right, bottom-left, clockwise on screen (y down).
using PerspectiveQuad = std::array<double, 8>;

struct PerspectivePlane {
  PerspectiveQuad layout{};  // drawn over the plane in Layout mode
  PerspectiveQuad warped{};  // the same corners after Warp-mode drags
  friend bool operator==(const PerspectivePlane&, const PerspectivePlane&) = default;
};

// Smallest distance allowed between two quads and the shortest usable side,
// both in document pixels.
inline constexpr double kPerspectiveQuadGap = 4.0;
inline constexpr double kPerspectiveQuadMinimumSide = 8.0;

// True when the quad is finite, strictly convex, clockwise on screen, and every
// side is at least kPerspectiveQuadMinimumSide long.
[[nodiscard]] bool perspective_quad_is_valid(const PerspectiveQuad& quad);

// Distance between two convex quads; 0 when they touch, cross, or nest.
[[nodiscard]] double perspective_quad_distance(const PerspectiveQuad& a, const PerspectiveQuad& b);

// Every quad valid and every pair at least kPerspectiveQuadGap apart.
[[nodiscard]] bool perspective_quads_are_valid(const std::vector<PerspectiveQuad>& quads);

// Row-major 3x3 homography taking `from` onto `to` corner for corner; nullopt
// for degenerate quads.
[[nodiscard]] std::optional<std::array<double, 9>> homography_from_quad_to_quad(const PerspectiveQuad& from,
                                                                                const PerspectiveQuad& to);

// One-shot straighten from the quad's own geometry: with `vertical`, each side
// closer to vertical (|dy| > |dx|) moves both endpoints to their mean x; with
// `horizontal`, each side closer to horizontal moves both to their mean y.
// Nothing is recorded, so a later drag may bend the side again.
[[nodiscard]] PerspectiveQuad straighten_perspective_quad(const PerspectiveQuad& quad, bool vertical,
                                                          bool horizontal);

// Forward lattice for resample_warped_rgba8 over a layer whose
// source_width x source_height pixels sit at (layer_left, layer_top) in the
// document. Nodes are at most `max_cell_pixels` apart (cells per axis clamped
// to [1, max_cells]). Nodes inside a layout quad move by that quad's
// homography; the rest take the membrane. A plane whose corners did not move is
// the identity, so with no moved plane every node maps exactly onto itself.
[[nodiscard]] std::optional<WarpSurfaceGrid> build_perspective_warp_grid(const std::vector<PerspectivePlane>& planes,
                                                                         double layer_left, double layer_top,
                                                                         int source_width, int source_height,
                                                                         double max_cell_pixels, int max_cells);

}  // namespace patchy
