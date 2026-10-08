#include "core/perspective_warp.hpp"

#include "test_harness.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

using patchy::PerspectivePlane;
using patchy::PerspectiveQuad;

PerspectiveQuad rect_quad(double left, double top, double right, double bottom) {
  return {left, top, right, top, right, bottom, left, bottom};
}

bool node_inside(const PerspectiveQuad& quad, double x, double y) {
  for (int side = 0; side < 4; ++side) {
    const auto a = static_cast<std::size_t>(side * 2);
    const auto b = static_cast<std::size_t>(((side + 1) & 3) * 2);
    const double turn = (quad[b] - quad[a]) * (y - quad[a + 1U]) - (quad[b + 1U] - quad[a + 1U]) * (x - quad[a]);
    if (turn < -1e-9) {
      return false;
    }
  }
  return true;
}

void perspective_warp_unmoved_quads_are_exact_identity() {
  const std::vector<PerspectivePlane> planes{{rect_quad(20, 20, 90, 80), rect_quad(20, 20, 90, 80)},
                                             {rect_quad(120, 30, 190, 110), rect_quad(120, 30, 190, 110)}};
  for (const auto& set : {planes, std::vector<PerspectivePlane>{}}) {
    const auto grid = patchy::build_perspective_warp_grid(set, 7.0, -3.0, 213, 131, 4.0, 512);
    CHECK(grid.has_value());
    CHECK(grid->columns == 55 && grid->rows == 34);
    for (std::size_t index = 0; index < grid->doc_xs.size(); ++index) {
      CHECK(grid->doc_xs[index] == 7.0 + grid->source_xs[index]);
      CHECK(grid->doc_ys[index] == -3.0 + grid->source_ys[index]);
    }
  }
}

void perspective_warp_quad_interior_follows_its_own_homography() {
  // A keystoned facade pulled square, with a second, untouched quad beside it.
  const auto layout = PerspectiveQuad{40, 30, 150, 50, 150, 170, 40, 200};
  const auto warped = PerspectiveQuad{35, 20, 160, 20, 160, 200, 35, 200};
  const auto other = rect_quad(200, 40, 280, 160);
  const std::vector<PerspectivePlane> planes{{layout, warped}, {other, other}};
  const auto mapping = patchy::homography_from_quad_to_quad(layout, warped);
  CHECK(mapping.has_value());
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    const auto mapped = patchy::apply_homography(*mapping, layout[corner * 2U], layout[corner * 2U + 1U]);
    CHECK(std::abs(mapped[0] - warped[corner * 2U]) < 1e-9);
    CHECK(std::abs(mapped[1] - warped[corner * 2U + 1U]) < 1e-9);
  }
  const auto grid = patchy::build_perspective_warp_grid(planes, 0.0, 0.0, 320, 240, 4.0, 512);
  CHECK(grid.has_value());
  int inside_layout = 0;
  int inside_other = 0;
  for (std::size_t index = 0; index < grid->doc_xs.size(); ++index) {
    const double x = grid->source_xs[index];
    const double y = grid->source_ys[index];
    if (node_inside(layout, x, y)) {
      const auto expected = patchy::apply_homography(*mapping, x, y);
      CHECK(std::abs(grid->doc_xs[index] - expected[0]) < 1e-9);
      CHECK(std::abs(grid->doc_ys[index] - expected[1]) < 1e-9);
      ++inside_layout;
    } else if (node_inside(other, x, y)) {
      // An unmoved quad is its own identity homography, exactly.
      CHECK(grid->doc_xs[index] == x);
      CHECK(grid->doc_ys[index] == y);
      ++inside_other;
    }
  }
  CHECK(inside_layout > 500);
  CHECK(inside_other > 400);
}

void perspective_warp_membrane_moves_the_rest_smoothly() {
  // Two separate quads pulled different ways; everything else follows.
  const auto left_layout = rect_quad(40, 60, 140, 240);
  const auto left_warped = PerspectiveQuad{20, 60, 140, 60, 140, 240, 50, 240};
  const auto right_layout = rect_quad(240, 60, 360, 240);
  const auto right_warped = PerspectiveQuad{240, 45, 360, 45, 360, 240, 240, 240};
  const std::vector<PerspectivePlane> planes{{left_layout, left_warped}, {right_layout, right_warped}};
  const auto grid = patchy::build_perspective_warp_grid(planes, 0.0, 0.0, 400, 300, 4.0, 512);
  CHECK(grid.has_value());
  const auto columns = static_cast<std::size_t>(grid->columns);
  const auto offset = [&grid](std::size_t index, bool y_axis) {
    return y_axis ? grid->doc_ys[index] - grid->source_ys[index] : grid->doc_xs[index] - grid->source_xs[index];
  };
  for (const bool y_axis : {false, true}) {
    double fixed_min = 0.0;
    double fixed_max = 0.0;
    for (std::size_t index = 0; index < grid->doc_xs.size(); ++index) {
      const double x = grid->source_xs[index];
      const double y = grid->source_ys[index];
      if (node_inside(left_layout, x, y) || node_inside(right_layout, x, y)) {
        fixed_min = std::min(fixed_min, offset(index, y_axis));
        fixed_max = std::max(fixed_max, offset(index, y_axis));
      }
    }
    double largest_step = 0.0;
    for (std::size_t index = 0; index < grid->doc_xs.size(); ++index) {
      // Maximum principle: the membrane never overshoots the quads' own offsets.
      CHECK(offset(index, y_axis) >= fixed_min - 0.01 && offset(index, y_axis) <= fixed_max + 0.01);
      // No tears: neighbouring nodes (4 px apart) move by similar amounts.
      if ((index + 1U) % columns != 0U) {
        largest_step = std::max(largest_step, std::abs(offset(index + 1U, y_axis) - offset(index, y_axis)));
      }
      if (index + columns < grid->doc_xs.size()) {
        largest_step = std::max(largest_step, std::abs(offset(index + columns, y_axis) - offset(index, y_axis)));
      }
    }
    // The stretch peaks beside the corner that moved 20 px (about 2 px per
    // step); a tear would jump by most of that move.
    CHECK(largest_step < 3.0);
  }
  // The layer border is free: even the far corner between the quads moves.
  CHECK(std::abs(offset(0U, false)) > 0.5);
  CHECK(std::abs(offset(grid->doc_xs.size() - 1U, true)) > 0.5);
}

void perspective_warp_straighten_makes_sides_exactly_vertical_or_level() {
  const PerspectiveQuad leaning{31.5, 20.25, 140.0, 33.0, 128.75, 190.0, 44.0, 171.5};
  const auto vertical = patchy::straighten_perspective_quad(leaning, true, false);
  CHECK(vertical[0] == vertical[6]);  // left side
  CHECK(vertical[2] == vertical[4]);  // right side
  CHECK(vertical[0] == (31.5 + 44.0) / 2.0);
  for (const std::size_t y : {1U, 3U, 5U, 7U}) {
    CHECK(vertical[y] == leaning[y]);  // y never moves when only straightening
  }
  const auto level = patchy::straighten_perspective_quad(leaning, false, true);
  CHECK(level[1] == level[3]);  // top side
  CHECK(level[5] == level[7]);  // bottom side
  for (const std::size_t x : {0U, 2U, 4U, 6U}) {
    CHECK(level[x] == leaning[x]);
  }
  const auto both = patchy::straighten_perspective_quad(leaning, true, true);
  CHECK(both[0] == both[6] && both[2] == both[4] && both[1] == both[3] && both[5] == both[7]);
  CHECK(patchy::perspective_quad_is_valid(both));
}

void perspective_warp_quads_cannot_touch_or_share_corners() {
  const auto first = rect_quad(10, 10, 60, 60);
  CHECK(patchy::perspective_quads_are_valid({first, rect_quad(70, 10, 120, 60)}));
  // A shared corner, a shared edge, a gap under the minimum, and an overlap.
  CHECK(!patchy::perspective_quads_are_valid({first, rect_quad(60, 60, 110, 110)}));
  CHECK(!patchy::perspective_quads_are_valid({first, rect_quad(60, 10, 110, 60)}));
  CHECK(!patchy::perspective_quads_are_valid({first, rect_quad(62, 10, 112, 60)}));
  CHECK(!patchy::perspective_quads_are_valid({first, rect_quad(30, 30, 90, 90)}));
  CHECK(!patchy::perspective_quads_are_valid({first, rect_quad(20, -20, 40, 100)}));  // crossing, no corner inside
  CHECK(patchy::perspective_quad_distance(first, rect_quad(70, 10, 120, 60)) == 10.0);
  // Folded, mirrored, and sliver quads are refused on their own.
  CHECK(!patchy::perspective_quad_is_valid({10, 10, 60, 60, 60, 10, 10, 60}));
  CHECK(!patchy::perspective_quad_is_valid({10, 10, 10, 60, 60, 60, 60, 10}));
  CHECK(!patchy::perspective_quad_is_valid(rect_quad(10, 10, 15, 60)));
}

}  // namespace

std::vector<patchy::test::TestCase> perspective_warp_tests() {
  return {
      {"perspective_warp_unmoved_quads_are_exact_identity", perspective_warp_unmoved_quads_are_exact_identity},
      {"perspective_warp_quad_interior_follows_its_own_homography",
       perspective_warp_quad_interior_follows_its_own_homography},
      {"perspective_warp_membrane_moves_the_rest_smoothly", perspective_warp_membrane_moves_the_rest_smoothly},
      {"perspective_warp_straighten_makes_sides_exactly_vertical_or_level",
       perspective_warp_straighten_makes_sides_exactly_vertical_or_level},
      {"perspective_warp_quads_cannot_touch_or_share_corners", perspective_warp_quads_cannot_touch_or_share_corners},
  };
}
