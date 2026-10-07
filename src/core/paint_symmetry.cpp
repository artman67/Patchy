#include "core/paint_symmetry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace patchy {

namespace {

struct Linear {
  double xx{1.0};
  double xy{0.0};
  double yx{0.0};
  double yy{1.0};
};

[[nodiscard]] Linear multiply(const Linear& a, const Linear& b) noexcept {
  return Linear{a.xx * b.xx + a.xy * b.yx, a.xx * b.xy + a.xy * b.yy,
                a.yx * b.xx + a.yy * b.yx, a.yx * b.xy + a.yy * b.yy};
}

// Rotation by `radians`; y points down, so positive turns clockwise on screen.
[[nodiscard]] Linear rotation(double radians) noexcept {
  const auto c = std::cos(radians);
  const auto s = std::sin(radians);
  return Linear{c, -s, s, c};
}

// cos/sin of multiples of 90 degrees leave 1e-16 residue; snapping it keeps
// mirrored and quarter-turned copies on exact pixel positions.
[[nodiscard]] double snap_unit(double value) noexcept {
  constexpr double kEpsilon = 1e-12;
  if (std::abs(value) < kEpsilon) {
    return 0.0;
  }
  if (std::abs(value - 1.0) < kEpsilon) {
    return 1.0;
  }
  if (std::abs(value + 1.0) < kEpsilon) {
    return -1.0;
  }
  return value;
}

[[nodiscard]] SymmetryTransform about_center(Linear linear, double center_x, double center_y) noexcept {
  linear = Linear{snap_unit(linear.xx), snap_unit(linear.xy), snap_unit(linear.yx), snap_unit(linear.yy)};
  SymmetryTransform transform;
  transform.xx = linear.xx;
  transform.xy = linear.xy;
  transform.yx = linear.yx;
  transform.yy = linear.yy;
  transform.tx = center_x - (linear.xx * center_x + linear.xy * center_y);
  transform.ty = center_y - (linear.yx * center_x + linear.yy * center_y);
  return transform;
}

[[nodiscard]] bool same_symmetry_point(double ax, double ay, double bx, double by) noexcept {
  constexpr double kEpsilon = 1e-4;
  return std::abs(ax - bx) <= kEpsilon && std::abs(ay - by) <= kEpsilon;
}

}  // namespace

bool paint_symmetry_uses_segments(PaintSymmetryMode mode) noexcept {
  return mode == PaintSymmetryMode::Radial || mode == PaintSymmetryMode::Mandala;
}

int clamp_paint_symmetry_segments(PaintSymmetryMode mode, int segments) noexcept {
  const auto maximum = mode == PaintSymmetryMode::Mandala ? kPaintSymmetryMaxMandalaSegments
                                                          : kPaintSymmetryMaxRadialSegments;
  return std::clamp(segments, kPaintSymmetryMinSegments, maximum);
}

std::vector<SymmetryTransform> paint_symmetry_transforms(PaintSymmetryMode mode, int segments,
                                                         double center_x, double center_y,
                                                         double angle_degrees) {
  constexpr Linear kMirrorX{-1.0, 0.0, 0.0, 1.0};  // across the vertical axis
  constexpr Linear kMirrorY{1.0, 0.0, 0.0, -1.0};  // across the horizontal axis
  std::vector<Linear> linears;
  switch (mode) {
    case PaintSymmetryMode::Off:
      break;
    case PaintSymmetryMode::Vertical:
      linears.push_back(kMirrorX);
      break;
    case PaintSymmetryMode::Horizontal:
      linears.push_back(kMirrorY);
      break;
    case PaintSymmetryMode::DualAxis:
      linears.push_back(kMirrorX);
      linears.push_back(kMirrorY);
      linears.push_back(Linear{-1.0, 0.0, 0.0, -1.0});
      break;
    case PaintSymmetryMode::Diagonal:
      // The axis runs along (1, -1): up and to the right on screen.
      linears.push_back(Linear{0.0, -1.0, -1.0, 0.0});
      break;
    case PaintSymmetryMode::Radial:
    case PaintSymmetryMode::Mandala: {
      const auto count = clamp_paint_symmetry_segments(mode, segments);
      const auto step = 2.0 * std::numbers::pi / static_cast<double>(count);
      for (int index = 1; index < count; ++index) {
        linears.push_back(rotation(step * static_cast<double>(index)));
      }
      if (mode == PaintSymmetryMode::Mandala) {
        // The dihedral group: every segment is also mirrored, across the
        // vertical axis and its rotations by half a segment.
        for (int index = 0; index < count; ++index) {
          linears.push_back(multiply(rotation(step * static_cast<double>(index)), kMirrorX));
        }
      }
      break;
    }
  }

  std::vector<SymmetryTransform> transforms;
  transforms.reserve(linears.size());
  const auto radians = angle_degrees * std::numbers::pi / 180.0;
  const auto turned = std::abs(std::remainder(angle_degrees, 360.0)) > 1e-9;
  const auto turn = rotation(radians);
  const auto unturn = rotation(-radians);
  for (const auto& linear : linears) {
    transforms.push_back(about_center(turned ? multiply(multiply(turn, linear), unturn) : linear,
                                      center_x, center_y));
  }
  return transforms;
}

bool paint_symmetry_copy_repeats_point(const std::vector<SymmetryTransform>& copies, std::size_t index,
                                       double x, double y) noexcept {
  const auto mapped_x = copies[index].map_x(x, y);
  const auto mapped_y = copies[index].map_y(x, y);
  if (same_symmetry_point(mapped_x, mapped_y, x, y)) {
    return true;
  }
  for (std::size_t earlier = 0; earlier < index; ++earlier) {
    if (same_symmetry_point(mapped_x, mapped_y, copies[earlier].map_x(x, y), copies[earlier].map_y(x, y))) {
      return true;
    }
  }
  return false;
}

bool paint_symmetry_copy_repeats_segment(const std::vector<SymmetryTransform>& copies, std::size_t index,
                                         double x0, double y0, double x1, double y1) noexcept {
  const auto& copy = copies[index];
  const auto ax = copy.map_x(x0, y0);
  const auto ay = copy.map_y(x0, y0);
  const auto bx = copy.map_x(x1, y1);
  const auto by = copy.map_y(x1, y1);
  const auto repeats = [&](double cx, double cy, double dx, double dy) {
    return (same_symmetry_point(ax, ay, cx, cy) && same_symmetry_point(bx, by, dx, dy)) ||
           (same_symmetry_point(ax, ay, dx, dy) && same_symmetry_point(bx, by, cx, cy));
  };
  if (repeats(x0, y0, x1, y1)) {
    return true;
  }
  for (std::size_t earlier = 0; earlier < index; ++earlier) {
    const auto& other = copies[earlier];
    if (repeats(other.map_x(x0, y0), other.map_y(x0, y0), other.map_x(x1, y1), other.map_y(x1, y1))) {
      return true;
    }
  }
  return false;
}

}  // namespace patchy
