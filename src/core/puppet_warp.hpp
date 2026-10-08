#pragma once

#include "core/warp_mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Puppet Warp deformation (Qt-free, deterministic doubles): moving least squares
// (Schaefer, McPhail and Warren, SIGGRAPH 2006) solved in closed form at every node
// of a regular grid over the layer, rendered through core/warp_mesh's surface grid
// and its exact bilinear cell inverse.
//
// PATENT BOUNDARY (docs/puppet-warp.md, docs/legal-constraints.md): each node is
// solved on its own. Never replace this with a triangle-mesh as-rigid-as-possible
// solve (similarity-step mesh, per-triangle fit, edge-vector solve: University of
// Tokyo US 8063917, treated as active to about 2027-08-07), bounded biharmonic
// weights, iterative optimization, or automatic pin placement without a new review.
namespace patchy {

// Photoshop's Mode menu mapped onto Schaefer's variants: Rigid = rigid MLS, Normal =
// the average of the rigid and similarity solutions, Distort = affine MLS.
enum class PuppetWarpMode { Rigid, Normal, Distort };

// Photoshop's Density menu: the mesh spacing relative to the layer's longest side.
enum class PuppetWarpDensity { FewerPoints, Normal, MorePoints };

// One user-placed pin. Rest is where it was placed on the undeformed layer, x/y where
// it sits now (both document pixels).
struct PuppetPin {
  double rest_x{0.0};
  double rest_y{0.0};
  double x{0.0};
  double y{0.0};
  // Fixed rotation: the pin also carries two short crossing segments, sampled at
  // their four ends, turned by rotation_degrees (clockwise on screen). Auto leaves
  // the rotation to the MLS solution.
  bool fixed_rotation{false};
  double rotation_degrees{0.0};
  int depth{0};  // higher depth draws in front where the warp folds over itself
};

// The mesh: a regular lattice over the layer bounds grown by the expansion; a cell
// belongs to the mesh when the layer's alpha mask (alpha > 0), dilated or eroded by
// `expansion` pixels with a square structuring element, touches it.
struct PuppetWarpMesh {
  double origin_x{0.0};  // document position of lattice node (0, 0)
  double origin_y{0.0};
  int spacing{1};        // cell edge in pixels
  int columns{0};        // lattice nodes per row (cells + 1)
  int rows{0};
  std::vector<std::uint8_t> active;  // (rows - 1) * (columns - 1) cells, 1 = in the mesh
  double handle_radius{4.0};         // half-length of a Fixed-rotation pin's segments
};

[[nodiscard]] int puppet_warp_grid_spacing(PuppetWarpDensity density, int width, int height);

// `alpha` holds width*height bytes for a layer whose top-left sits at (left, top).
[[nodiscard]] PuppetWarpMesh build_puppet_warp_mesh(const std::vector<std::uint8_t>& alpha, int left, int top,
                                                    int width, int height, PuppetWarpDensity density,
                                                    int expansion);

[[nodiscard]] bool puppet_warp_mesh_is_empty(const PuppetWarpMesh& mesh);

// True when no pin moved and none carries a nonzero Fixed rotation.
[[nodiscard]] bool puppet_warp_is_identity(const std::vector<PuppetPin>& pins);

// Maps one rest-space document point through the deformation.
[[nodiscard]] std::array<double, 2> puppet_warp_map_point(const std::vector<PuppetPin>& pins, PuppetWarpMode mode,
                                                          double handle_radius, double x, double y);

// The angle (degrees, clockwise) the deformation turns the layer at pin `index`: a
// Fixed pin reports its own angle, an Auto pin the local rotation of the solution.
[[nodiscard]] double puppet_warp_pin_angle(const std::vector<PuppetPin>& pins, PuppetWarpMode mode,
                                           double handle_radius, std::size_t index);

// Deforms the mesh into a render lattice: every mesh cell splits into
// `subdivisions` x `subdivisions` lattice cells, each node gets its own closed-form
// MLS solve, source coordinates are rest positions minus (source_left, source_top),
// and cell_order lists the mesh's cells front-most pin depth first.
[[nodiscard]] WarpSurfaceGrid deform_puppet_warp_mesh(const PuppetWarpMesh& mesh, const std::vector<PuppetPin>& pins,
                                                      PuppetWarpMode mode, int subdivisions, double source_left,
                                                      double source_top);

// The rest position under deformed document point (x, y): the front-most rendered
// cell containing it, inverted exactly. nullopt when the point is off the mesh.
[[nodiscard]] std::optional<std::array<double, 2>> puppet_warp_rest_point(const WarpSurfaceGrid& grid, double x,
                                                                          double y, double source_left,
                                                                          double source_top);

}  // namespace patchy
