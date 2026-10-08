#include "core/perspective_warp.hpp"

#include "core/heal_membrane.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace patchy {

namespace {

struct Point {
  double x{0.0};
  double y{0.0};
};

Point corner(const PerspectiveQuad& quad, int index) {
  const auto base = static_cast<std::size_t>((index & 3) * 2);
  return {quad[base], quad[base + 1U]};
}

double cross(Point origin, Point a, Point b) {
  return (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
}

// Inclusive: points on a side count as inside (clockwise on screen keeps
// every interior cross product positive).
bool point_in_quad(const PerspectiveQuad& quad, Point point) {
  for (int side = 0; side < 4; ++side) {
    if (cross(corner(quad, side), corner(quad, side + 1), point) < -1e-9) {
      return false;
    }
  }
  return true;
}

double point_segment_distance(Point point, Point a, Point b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double length_squared = dx * dx + dy * dy;
  double t = 0.0;
  if (length_squared > 0.0) {
    t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length_squared, 0.0, 1.0);
  }
  return std::hypot(point.x - (a.x + t * dx), point.y - (a.y + t * dy));
}

bool segments_intersect(Point a, Point b, Point c, Point d) {
  const double d1 = cross(c, d, a);
  const double d2 = cross(c, d, b);
  const double d3 = cross(a, b, c);
  const double d4 = cross(a, b, d);
  return ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0));
}

std::array<double, 9> multiply(const std::array<double, 9>& a, const std::array<double, 9>& b) {
  std::array<double, 9> result{};
  for (std::size_t row = 0; row < 3U; ++row) {
    for (std::size_t column = 0; column < 3U; ++column) {
      result[row * 3U + column] =
          a[row * 3U] * b[column] + a[row * 3U + 1U] * b[3U + column] + a[row * 3U + 2U] * b[6U + column];
    }
  }
  return result;
}

}  // namespace

bool perspective_quad_is_valid(const PerspectiveQuad& quad) {
  if (!std::all_of(quad.begin(), quad.end(), [](double value) { return std::isfinite(value); })) {
    return false;
  }
  for (int index = 0; index < 4; ++index) {
    const auto a = corner(quad, index);
    const auto b = corner(quad, index + 1);
    const auto c = corner(quad, index + 2);
    const double side = std::hypot(b.x - a.x, b.y - a.y);
    const double next_side = std::hypot(c.x - b.x, c.y - b.y);
    if (side < kPerspectiveQuadMinimumSide) {
      return false;
    }
    // Every turn clockwise on screen and at least ~0.6 degrees off straight,
    // which for four corners is exactly a strictly convex quad.
    if (cross(a, b, c) <= 0.01 * side * next_side) {
      return false;
    }
  }
  return true;
}

double perspective_quad_distance(const PerspectiveQuad& a, const PerspectiveQuad& b) {
  for (int index = 0; index < 4; ++index) {
    if (point_in_quad(b, corner(a, index)) || point_in_quad(a, corner(b, index))) {
      return 0.0;
    }
  }
  double distance = -1.0;
  for (int side_a = 0; side_a < 4; ++side_a) {
    for (int side_b = 0; side_b < 4; ++side_b) {
      if (segments_intersect(corner(a, side_a), corner(a, side_a + 1), corner(b, side_b), corner(b, side_b + 1))) {
        return 0.0;
      }
      const double to_b = point_segment_distance(corner(a, side_a), corner(b, side_b), corner(b, side_b + 1));
      const double to_a = point_segment_distance(corner(b, side_b), corner(a, side_a), corner(a, side_a + 1));
      const double nearest = std::min(to_a, to_b);
      distance = distance < 0.0 ? nearest : std::min(distance, nearest);
    }
  }
  return std::max(distance, 0.0);
}

bool perspective_quads_are_valid(const std::vector<PerspectiveQuad>& quads) {
  for (std::size_t index = 0; index < quads.size(); ++index) {
    if (!perspective_quad_is_valid(quads[index])) {
      return false;
    }
    for (std::size_t other = index + 1U; other < quads.size(); ++other) {
      if (perspective_quad_distance(quads[index], quads[other]) < kPerspectiveQuadGap) {
        return false;
      }
    }
  }
  return true;
}

std::optional<std::array<double, 9>> homography_from_quad_to_quad(const PerspectiveQuad& from,
                                                                const PerspectiveQuad& to) {
  // Unit square -> `to`, composed after `from` -> unit square (Heckbert 1989).
  const auto unit_to_target = homography_from_rect_to_quad(0.0, 0.0, 1.0, 1.0, to);
  const auto unit_to_source = homography_from_rect_to_quad(0.0, 0.0, 1.0, 1.0, from);
  if (!unit_to_target.has_value() || !unit_to_source.has_value()) {
    return std::nullopt;
  }
  const auto source_to_unit = invert_homography(*unit_to_source);
  if (!source_to_unit.has_value()) {
    return std::nullopt;
  }
  return multiply(*unit_to_target, *source_to_unit);
}

PerspectiveQuad straighten_perspective_quad(const PerspectiveQuad& quad, bool vertical, bool horizontal) {
  // Sides are classified and averaged from the ORIGINAL corners, so the result
  // does not depend on the side order.
  auto result = quad;
  for (int side = 0; side < 4; ++side) {
    const auto first = static_cast<std::size_t>(side * 2);
    const auto second = static_cast<std::size_t>(((side + 1) & 3) * 2);
    const double dx = quad[second] - quad[first];
    const double dy = quad[second + 1U] - quad[first + 1U];
    if (vertical && std::abs(dy) > std::abs(dx)) {
      const double x = (quad[first] + quad[second]) / 2.0;
      result[first] = x;
      result[second] = x;
    } else if (horizontal && std::abs(dx) > std::abs(dy)) {
      const double y = (quad[first + 1U] + quad[second + 1U]) / 2.0;
      result[first + 1U] = y;
      result[second + 1U] = y;
    }
  }
  return result;
}

std::optional<WarpSurfaceGrid> build_perspective_warp_grid(const std::vector<PerspectivePlane>& planes,
                                                           double layer_left, double layer_top, int source_width,
                                                           int source_height, double max_cell_pixels, int max_cells) {
  if (source_width <= 0 || source_height <= 0 || max_cell_pixels <= 0.0) {
    return std::nullopt;
  }
  const int clamp_cells = std::max(1, max_cells);
  const int cells_x = std::clamp(static_cast<int>(std::ceil(source_width / max_cell_pixels)), 1, clamp_cells);
  const int cells_y = std::clamp(static_cast<int>(std::ceil(source_height / max_cell_pixels)), 1, clamp_cells);

  // Each moved plane's own homography; an unmoved plane is the exact identity.
  std::vector<std::optional<std::array<double, 9>>> mappings;
  mappings.reserve(planes.size());
  bool any_moved = false;
  for (const auto& plane : planes) {
    if (plane.layout == plane.warped) {
      mappings.emplace_back();
      continue;
    }
    auto mapping = homography_from_quad_to_quad(plane.layout, plane.warped);
    if (!mapping.has_value()) {
      return std::nullopt;
    }
    mappings.push_back(mapping);
    any_moved = true;
  }

  WarpSurfaceGrid grid;
  grid.columns = cells_x + 1;
  grid.rows = cells_y + 1;
  const auto total = static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows);
  grid.source_xs.resize(total);
  grid.source_ys.resize(total);
  std::vector<double> offset_x(total, 0.0);
  std::vector<double> offset_y(total, 0.0);
  // Membrane mask: non-zero = solved, zero = fixed by a quad.
  std::vector<std::uint8_t> interior(total, 1U);
  bool any_interior = false;
  double largest_offset = 0.0;
  for (int row = 0; row < grid.rows; ++row) {
    const double source_y = static_cast<double>(row) * source_height / cells_y;
    for (int column = 0; column < grid.columns; ++column) {
      const double source_x = static_cast<double>(column) * source_width / cells_x;
      const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.columns) +
                         static_cast<std::size_t>(column);
      grid.source_xs[index] = source_x;
      grid.source_ys[index] = source_y;
      const Point document{layer_left + source_x, layer_top + source_y};
      for (std::size_t plane = 0; plane < planes.size(); ++plane) {
        if (!point_in_quad(planes[plane].layout, document)) {
          continue;
        }
        interior[index] = 0U;
        if (mappings[plane].has_value()) {
          const auto mapped = apply_homography(*mappings[plane], document.x, document.y);
          offset_x[index] = mapped[0] - document.x;
          offset_y[index] = mapped[1] - document.y;
          largest_offset = std::max({largest_offset, std::abs(offset_x[index]), std::abs(offset_y[index])});
        }
        break;  // quads never overlap
      }
      any_interior = any_interior || interior[index] != 0U;
    }
  }

  if (any_moved && any_interior && largest_offset > 0.0) {
    // The membrane solver works in int16 per channel; scale the offsets so the
    // largest uses most of that range (finest step 1/1024 px).
    const double scale = std::min(1024.0, 32000.0 / largest_offset);
    std::vector<std::int16_t> offsets(total * 3U, 0);
    for (std::size_t index = 0; index < total; ++index) {
      if (interior[index] == 0U) {
        offsets[index * 3U] = static_cast<std::int16_t>(std::lround(offset_x[index] * scale));
        offsets[index * 3U + 1U] = static_cast<std::int16_t>(std::lround(offset_y[index] * scale));
      }
    }
    solve_heal_membrane(interior.data(), grid.columns, grid.rows, offsets.data());
    for (std::size_t index = 0; index < total; ++index) {
      if (interior[index] != 0U) {
        offset_x[index] = offsets[index * 3U] / scale;
        offset_y[index] = offsets[index * 3U + 1U] / scale;
      }
    }
  }

  grid.doc_xs.resize(total);
  grid.doc_ys.resize(total);
  for (std::size_t index = 0; index < total; ++index) {
    grid.doc_xs[index] = layer_left + grid.source_xs[index] + offset_x[index];
    grid.doc_ys[index] = layer_top + grid.source_ys[index] + offset_y[index];
  }
  return grid;
}

}  // namespace patchy
