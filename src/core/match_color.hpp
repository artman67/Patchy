#pragma once

#include "core/layer.hpp"
#include "core/pixel_buffer.hpp"

#include <array>
#include <cstdint>

namespace patchy {

// Image > Adjustments > Match Color: a global color-statistics transfer
// (Reinhard et al., "Color Transfer between Images", 2001) in CIE L*a*b*.
// Formulas, slider semantics, and what is approximated: docs/match-color.md.
//
// Legal boundary (docs/legal-constraints.md, Filters): one whole-image affine
// map per Lab channel built from per-channel means and standard deviations,
// plus the fixed Luminance / Color Intensity / Fade / Neutralize controls. No
// cross-channel covariance or rotation, no histograms, percentiles, luma or
// hue ranges, no tone curves, no regions or segmentation, no learned models.

inline constexpr int kMatchColorLuminanceMin = 1;
inline constexpr int kMatchColorLuminanceMax = 200;
inline constexpr int kMatchColorIntensityMin = 1;
inline constexpr int kMatchColorIntensityMax = 200;
inline constexpr int kMatchColorFadeMax = 100;

// Per-channel L*, a*, b* statistics of a pixel set. `weight` is the summed
// pixel weight (alpha times coverage, in 0..1 per pixel); zero means no pixel
// counted, and callers then treat the statistics as missing.
struct MatchColorStatistics {
  std::array<double, 3> mean{};
  std::array<double, 3> deviation{};
  double weight{0.0};

  [[nodiscard]] bool empty() const noexcept { return weight <= 0.0; }
};

struct MatchColorOptions {
  int luminance{100};        // 1..200, scales L*
  int color_intensity{100};  // 1..200, scales a* and b*
  int fade{0};               // 0..100, blends back toward the original
  bool neutralize{false};    // matches the a*/b* means to neutral gray
};

// The resolved per-pixel map: lab' = scale * lab + offset per channel, then the
// Fade blend in 8-bit RGB. `identity` is true when the map cannot change a pixel.
struct MatchColorTransform {
  std::array<double, 3> scale{1.0, 1.0, 1.0};
  std::array<double, 3> offset{};
  int fade{0};
  bool identity{true};
};

// Statistics over every pixel of an 8-bit RGB or RGBA buffer. Each pixel weighs
// its alpha (when present) times `coverage[y * width + x] / 255` (when non-null;
// a selection mask, or the compositor's merged alpha). Rows fan out across
// threads, but sums are reduced in row order, so results do not depend on the
// thread count.
[[nodiscard]] MatchColorStatistics match_color_statistics(const PixelBuffer& pixels,
                                                          const std::uint8_t* coverage = nullptr);

// Builds the map that moves `target`'s statistics onto `source`'s. A missing
// source (Photoshop's Source: None) or an empty one matches the target to
// itself, so only Luminance, Color Intensity, Neutralize and Fade act.
[[nodiscard]] MatchColorTransform make_match_color_transform(const MatchColorStatistics& target,
                                                             const MatchColorStatistics* source,
                                                             MatchColorOptions options);

[[nodiscard]] RgbColor apply_match_color(RgbColor color, const MatchColorTransform& transform);

// sRGB (D65) to CIE L*a*b* (D65 white), exposed for tests.
[[nodiscard]] std::array<double, 3> match_color_lab(RgbColor color);

}  // namespace patchy
