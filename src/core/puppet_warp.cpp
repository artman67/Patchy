#include "core/puppet_warp.hpp"

#include <algorithm>
#include <cmath>

namespace patchy {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Point constraints fed to the solver: every pin, plus four segment ends per
// Fixed-rotation pin. p = rest, q = current.
struct MlsConstraints {
  std::vector<double> px;
  std::vector<double> py;
  std::vector<double> qx;
  std::vector<double> qy;
  std::vector<double> depth;
};

MlsConstraints build_constraints(const std::vector<PuppetPin>& pins, double handle_radius) {
  MlsConstraints constraints;
  const auto add = [&constraints](double px, double py, double qx, double qy, double depth) {
    constraints.px.push_back(px);
    constraints.py.push_back(py);
    constraints.qx.push_back(qx);
    constraints.qy.push_back(qy);
    constraints.depth.push_back(depth);
  };
  for (const auto& pin : pins) {
    const auto depth = static_cast<double>(pin.depth);
    add(pin.rest_x, pin.rest_y, pin.x, pin.y, depth);
    if (!pin.fixed_rotation) {
      continue;
    }
    // Schaefer section 5 uses line-segment handles; two short crossing segments
    // sampled at their ends keep the closed-form point solve while pinning the angle.
    const double radians = pin.rotation_degrees * kPi / 180.0;
    const double c = std::cos(radians) * handle_radius;
    const double s = std::sin(radians) * handle_radius;
    add(pin.rest_x + handle_radius, pin.rest_y, pin.x + c, pin.y + s, depth);
    add(pin.rest_x - handle_radius, pin.rest_y, pin.x - c, pin.y - s, depth);
    add(pin.rest_x, pin.rest_y + handle_radius, pin.x - s, pin.y + c, depth);
    add(pin.rest_x, pin.rest_y - handle_radius, pin.x + s, pin.y - c, depth);
  }
  return constraints;
}

struct MlsResult {
  double x{0.0};
  double y{0.0};
  double depth{0.0};
};

// One closed-form moving-least-squares solve at v (weights 1/|p - v|^2, alpha = 1).
MlsResult solve_mls(const MlsConstraints& c, PuppetWarpMode mode, double vx, double vy) {
  const auto count = c.px.size();
  double weight_sum = 0.0;
  double p_star_x = 0.0;
  double p_star_y = 0.0;
  double q_star_x = 0.0;
  double q_star_y = 0.0;
  double depth = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double dx = c.px[i] - vx;
    const double dy = c.py[i] - vy;
    const double distance_squared = dx * dx + dy * dy;
    if (distance_squared < 1e-12) {
      // On a constraint the interpolating solution is exactly its target.
      return MlsResult{c.qx[i], c.qy[i], c.depth[i]};
    }
    const double weight = 1.0 / distance_squared;
    weight_sum += weight;
    p_star_x += weight * c.px[i];
    p_star_y += weight * c.py[i];
    q_star_x += weight * c.qx[i];
    q_star_y += weight * c.qy[i];
    depth += weight * c.depth[i];
  }
  p_star_x /= weight_sum;
  p_star_y /= weight_sum;
  q_star_x /= weight_sum;
  q_star_y /= weight_sum;
  depth /= weight_sum;

  // Moments of the centered constraints.
  double a = 0.0;   // sum w (p^ . q^)
  double b = 0.0;   // sum w (p^ x q^)
  double mu = 0.0;  // sum w |p^|^2
  double axx = 0.0, axy = 0.0, ayy = 0.0;               // sum w p^ p^T
  double bxx = 0.0, bxy = 0.0, byx = 0.0, byy = 0.0;    // sum w p^ q^T
  for (std::size_t i = 0; i < count; ++i) {
    const double dx = c.px[i] - vx;
    const double dy = c.py[i] - vy;
    const double weight = 1.0 / (dx * dx + dy * dy);
    const double phx = c.px[i] - p_star_x;
    const double phy = c.py[i] - p_star_y;
    const double qhx = c.qx[i] - q_star_x;
    const double qhy = c.qy[i] - q_star_y;
    a += weight * (phx * qhx + phy * qhy);
    b += weight * (phx * qhy - phy * qhx);
    mu += weight * (phx * phx + phy * phy);
    axx += weight * phx * phx;
    axy += weight * phx * phy;
    ayy += weight * phy * phy;
    bxx += weight * phx * qhx;
    bxy += weight * phx * qhy;
    byx += weight * phy * qhx;
    byy += weight * phy * qhy;
  }
  const double ux = vx - p_star_x;
  const double uy = vy - p_star_y;
  if (mu <= 1e-12 * weight_sum) {
    // One effective constraint: a pure translation.
    return MlsResult{ux + q_star_x, uy + q_star_y, depth};
  }

  if (mode == PuppetWarpMode::Distort) {
    // Affine: f(v) = (v - p*) A^-1 B + q* (row vectors). Fewer than three
    // non-collinear constraints leave A singular; fall back to the similarity solve.
    const double det = axx * ayy - axy * axy;
    if (det > 1e-10 * (axx + ayy) * (axx + ayy)) {
      const double ixx = ayy / det;
      const double ixy = -axy / det;
      const double iyy = axx / det;
      const double mxx = ixx * bxx + ixy * byx;
      const double mxy = ixx * bxy + ixy * byy;
      const double myx = ixy * bxx + iyy * byx;
      const double myy = ixy * bxy + iyy * byy;
      return MlsResult{ux * mxx + uy * myx + q_star_x, ux * mxy + uy * myy + q_star_y, depth};
    }
  }

  // Similarity: [[a, -b], [b, a]] / mu. Rigid: the same rotation, unit scale.
  double cos_term = a / mu;
  double sin_term = b / mu;
  if (mode != PuppetWarpMode::Distort) {
    const double norm = std::sqrt(a * a + b * b);
    const double rigid_cos = norm > 1e-12 * mu ? a / norm : 1.0;
    const double rigid_sin = norm > 1e-12 * mu ? b / norm : 0.0;
    if (mode == PuppetWarpMode::Rigid) {
      cos_term = rigid_cos;
      sin_term = rigid_sin;
    } else {
      cos_term = 0.5 * (cos_term + rigid_cos);
      sin_term = 0.5 * (sin_term + rigid_sin);
    }
  }
  return MlsResult{cos_term * ux - sin_term * uy + q_star_x, sin_term * ux + cos_term * uy + q_star_y, depth};
}

bool constraints_are_identity(const MlsConstraints& c) {
  for (std::size_t i = 0; i < c.px.size(); ++i) {
    if (c.px[i] != c.qx[i] || c.py[i] != c.qy[i]) {
      return false;
    }
  }
  return true;
}

// Summed-area table over a width x height byte mask (1 = set), (width+1)*(height+1).
std::vector<std::uint32_t> summed_area(const std::vector<std::uint8_t>& mask, int width, int height) {
  const auto stride = static_cast<std::size_t>(width) + 1U;
  std::vector<std::uint32_t> table(stride * (static_cast<std::size_t>(height) + 1U), 0U);
  for (int y = 0; y < height; ++y) {
    std::uint32_t row_sum = 0U;
    for (int x = 0; x < width; ++x) {
      row_sum += mask[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
      table[(static_cast<std::size_t>(y) + 1U) * stride + static_cast<std::size_t>(x) + 1U] =
          table[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) + 1U] + row_sum;
    }
  }
  return table;
}

// Set count in [x0, x1) x [y0, y1), clamped to the mask.
std::uint32_t area_count(const std::vector<std::uint32_t>& table, int width, int height, int x0, int y0, int x1,
                         int y1) {
  x0 = std::clamp(x0, 0, width);
  x1 = std::clamp(x1, 0, width);
  y0 = std::clamp(y0, 0, height);
  y1 = std::clamp(y1, 0, height);
  if (x1 <= x0 || y1 <= y0) {
    return 0U;
  }
  const auto stride = static_cast<std::size_t>(width) + 1U;
  const auto at = [&](int x, int y) { return table[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x)]; };
  return at(x1, y1) - at(x0, y1) - at(x1, y0) + at(x0, y0);
}

}  // namespace

int puppet_warp_grid_spacing(PuppetWarpDensity density, int width, int height) {
  const int longest = std::max({width, height, 1});
  const int cells = density == PuppetWarpDensity::FewerPoints ? 24 : (density == PuppetWarpDensity::MorePoints ? 96 : 48);
  return std::max(2, (longest + cells - 1) / cells);
}

PuppetWarpMesh build_puppet_warp_mesh(const std::vector<std::uint8_t>& alpha, int left, int top, int width,
                                      int height, PuppetWarpDensity density, int expansion) {
  PuppetWarpMesh mesh;
  if (width <= 0 || height <= 0 ||
      alpha.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    return mesh;
  }
  mesh.spacing = puppet_warp_grid_spacing(density, width, height);
  mesh.handle_radius = 2.0 * mesh.spacing;
  const int grow = std::max(expansion, 0);
  const int region_left = left - grow;
  const int region_top = top - grow;
  const int cells_x = (width + 2 * grow + mesh.spacing - 1) / mesh.spacing;
  const int cells_y = (height + 2 * grow + mesh.spacing - 1) / mesh.spacing;
  mesh.origin_x = region_left;
  mesh.origin_y = region_top;
  mesh.columns = cells_x + 1;
  mesh.rows = cells_y + 1;

  std::vector<std::uint8_t> mask(alpha.size());
  std::transform(alpha.begin(), alpha.end(), mask.begin(), [](std::uint8_t value) {
    return static_cast<std::uint8_t>(value > 0 ? 1 : 0);
  });
  auto table = summed_area(mask, width, height);
  if (expansion < 0) {
    // Erosion: keep pixels whose whole (2e+1)^2 square is opaque (outside the layer
    // counts as transparent), then test cells against the eroded mask.
    const int erode = -expansion;
    const auto full = static_cast<std::uint32_t>((2 * erode + 1) * (2 * erode + 1));
    std::vector<std::uint8_t> eroded(mask.size(), 0);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const bool inside = x - erode >= 0 && y - erode >= 0 && x + erode < width && y + erode < height;
        eroded[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
            static_cast<std::uint8_t>(
                inside && area_count(table, width, height, x - erode, y - erode, x + erode + 1, y + erode + 1) == full
                    ? 1
                    : 0);
      }
    }
    table = summed_area(eroded, width, height);
  }
  mesh.active.assign(static_cast<std::size_t>(cells_x) * static_cast<std::size_t>(cells_y), 0);
  for (int cy = 0; cy < cells_y; ++cy) {
    for (int cx = 0; cx < cells_x; ++cx) {
      // Cell rect in layer-local pixels, grown by the dilation.
      const int x0 = region_left - left + cx * mesh.spacing - grow;
      const int y0 = region_top - top + cy * mesh.spacing - grow;
      const int x1 = x0 + mesh.spacing + 2 * grow;
      const int y1 = y0 + mesh.spacing + 2 * grow;
      if (area_count(table, width, height, x0, y0, x1, y1) > 0U) {
        mesh.active[static_cast<std::size_t>(cy) * static_cast<std::size_t>(cells_x) + static_cast<std::size_t>(cx)] = 1;
      }
    }
  }
  return mesh;
}

bool puppet_warp_mesh_is_empty(const PuppetWarpMesh& mesh) {
  return std::none_of(mesh.active.begin(), mesh.active.end(), [](std::uint8_t value) { return value != 0; });
}

bool puppet_warp_is_identity(const std::vector<PuppetPin>& pins) {
  return std::all_of(pins.begin(), pins.end(), [](const PuppetPin& pin) {
    return pin.x == pin.rest_x && pin.y == pin.rest_y && (!pin.fixed_rotation || pin.rotation_degrees == 0.0);
  });
}

std::array<double, 2> puppet_warp_map_point(const std::vector<PuppetPin>& pins, PuppetWarpMode mode,
                                            double handle_radius, double x, double y) {
  if (pins.empty() || puppet_warp_is_identity(pins)) {
    return {x, y};
  }
  const auto result = solve_mls(build_constraints(pins, handle_radius), mode, x, y);
  return {result.x, result.y};
}

double puppet_warp_pin_angle(const std::vector<PuppetPin>& pins, PuppetWarpMode mode, double handle_radius,
                             std::size_t index) {
  if (index >= pins.size()) {
    return 0.0;
  }
  const auto& pin = pins[index];
  if (pin.fixed_rotation) {
    return pin.rotation_degrees;
  }
  // The image of a short horizontal segment centred on the pin.
  const double half = std::max(1.0, 0.5 * handle_radius);
  const auto right = puppet_warp_map_point(pins, mode, handle_radius, pin.rest_x + half, pin.rest_y);
  const auto left = puppet_warp_map_point(pins, mode, handle_radius, pin.rest_x - half, pin.rest_y);
  const double degrees = std::atan2(right[1] - left[1], right[0] - left[0]) * 180.0 / kPi;
  return std::abs(degrees) < 1e-9 ? 0.0 : degrees;
}

WarpSurfaceGrid deform_puppet_warp_mesh(const PuppetWarpMesh& mesh, const std::vector<PuppetPin>& pins,
                                        PuppetWarpMode mode, int subdivisions, double source_left,
                                        double source_top) {
  WarpSurfaceGrid grid;
  if (mesh.columns < 2 || mesh.rows < 2) {
    return grid;
  }
  const int split = std::max(1, subdivisions);
  const double step = static_cast<double>(mesh.spacing) / split;
  const int mesh_cells_x = mesh.columns - 1;
  grid.columns = mesh_cells_x * split + 1;
  grid.rows = (mesh.rows - 1) * split + 1;
  const int cells_x = grid.columns - 1;
  const int cells_y = grid.rows - 1;
  const auto node_count = static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows);

  // Lattice cells inherit their mesh cell's membership; only nodes of member cells
  // are solved.
  std::vector<std::uint8_t> used(node_count, 0);
  for (int row = 0; row < cells_y; ++row) {
    for (int column = 0; column < cells_x; ++column) {
      const auto parent = static_cast<std::size_t>(row / split) * static_cast<std::size_t>(mesh_cells_x) +
                          static_cast<std::size_t>(column / split);
      if (mesh.active[parent] == 0) {
        continue;
      }
      grid.cell_order.push_back(row * cells_x + column);
      const auto node = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                        static_cast<std::size_t>(column);
      used[node] = used[node + 1] = 1;
      used[node + static_cast<std::size_t>(grid.columns)] = used[node + static_cast<std::size_t>(grid.columns) + 1] = 1;
    }
  }

  const auto constraints = build_constraints(pins, mesh.handle_radius);
  const bool identity = constraints.px.empty() || constraints_are_identity(constraints);
  const bool layered = std::any_of(pins.begin(), pins.end(),
                                   [&pins](const PuppetPin& pin) { return pin.depth != pins.front().depth; });
  grid.doc_xs.resize(node_count);
  grid.doc_ys.resize(node_count);
  grid.source_xs.resize(node_count);
  grid.source_ys.resize(node_count);
  std::vector<double> depths(layered ? node_count : 0U, 0.0);
  for (int row = 0; row < grid.rows; ++row) {
    for (int column = 0; column < grid.columns; ++column) {
      const auto node = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                        static_cast<std::size_t>(column);
      const double rest_x = mesh.origin_x + column * step;
      const double rest_y = mesh.origin_y + row * step;
      grid.source_xs[node] = rest_x - source_left;
      grid.source_ys[node] = rest_y - source_top;
      grid.doc_xs[node] = rest_x;
      grid.doc_ys[node] = rest_y;
      if (identity || used[node] == 0) {
        continue;
      }
      const auto result = solve_mls(constraints, mode, rest_x, rest_y);
      grid.doc_xs[node] = result.x;
      grid.doc_ys[node] = result.y;
      if (layered) {
        depths[node] = result.depth;
      }
    }
  }

  if (layered) {
    // Front-most first; equal depths keep row-major order (deterministic).
    std::vector<double> cell_depth(static_cast<std::size_t>(cells_x) * static_cast<std::size_t>(cells_y), 0.0);
    for (const auto cell : grid.cell_order) {
      const auto row = cell / cells_x;
      const auto column = cell % cells_x;
      const auto node = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                        static_cast<std::size_t>(column);
      const auto below = node + static_cast<std::size_t>(grid.columns);
      cell_depth[static_cast<std::size_t>(cell)] =
          0.25 * (depths[node] + depths[node + 1] + depths[below] + depths[below + 1]);
    }
    std::stable_sort(grid.cell_order.begin(), grid.cell_order.end(), [&cell_depth](std::int32_t lhs, std::int32_t rhs) {
      return cell_depth[static_cast<std::size_t>(lhs)] > cell_depth[static_cast<std::size_t>(rhs)];
    });
  }
  return grid;
}

std::optional<std::array<double, 2>> puppet_warp_rest_point(const WarpSurfaceGrid& grid, double x, double y,
                                                            double source_left, double source_top) {
  if (grid.columns < 2 || grid.rows < 2) {
    return std::nullopt;
  }
  const int cells_x = grid.columns - 1;
  for (const auto cell : grid.cell_order) {
    const auto row = cell / cells_x;
    const auto column = cell % cells_x;
    const auto i00 = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                     static_cast<std::size_t>(column);
    const auto i10 = i00 + 1;
    const auto i01 = i00 + static_cast<std::size_t>(grid.columns);
    const auto i11 = i01 + 1;
    const double min_x = std::min({grid.doc_xs[i00], grid.doc_xs[i10], grid.doc_xs[i11], grid.doc_xs[i01]});
    const double max_x = std::max({grid.doc_xs[i00], grid.doc_xs[i10], grid.doc_xs[i11], grid.doc_xs[i01]});
    const double min_y = std::min({grid.doc_ys[i00], grid.doc_ys[i10], grid.doc_ys[i11], grid.doc_ys[i01]});
    const double max_y = std::max({grid.doc_ys[i00], grid.doc_ys[i10], grid.doc_ys[i11], grid.doc_ys[i01]});
    if (x < min_x || x > max_x || y < min_y || y > max_y) {
      continue;
    }
    const auto st = invert_bilinear_cell(x, y, grid.doc_xs[i00], grid.doc_ys[i00], grid.doc_xs[i10],
                                         grid.doc_ys[i10], grid.doc_xs[i11], grid.doc_ys[i11], grid.doc_xs[i01],
                                         grid.doc_ys[i01]);
    if (!st.has_value()) {
      continue;
    }
    const double s = (*st)[0];
    const double t = (*st)[1];
    const double source_x = (1.0 - t) * ((1.0 - s) * grid.source_xs[i00] + s * grid.source_xs[i10]) +
                            t * ((1.0 - s) * grid.source_xs[i01] + s * grid.source_xs[i11]);
    const double source_y = (1.0 - t) * ((1.0 - s) * grid.source_ys[i00] + s * grid.source_ys[i10]) +
                            t * ((1.0 - s) * grid.source_ys[i01] + s * grid.source_ys[i11]);
    return std::array<double, 2>{source_x + source_left, source_y + source_top};
  }
  return std::nullopt;
}

}  // namespace patchy
