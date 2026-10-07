#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace patchy {

// The Color Replacement brush (docs/color-replacement.md): repaints pixels
// under the brush that match a sampled color with the foreground's hue,
// saturation, color, or luminosity, keeping the rest of each pixel.
//
// Patent boundary (October 2026, docs/color-replacement.md): sampling, the
// tolerance match, and the Contiguous / Find Edges limits are the teaching of
// Adobe's expired Smart Erasure Brush (US 7627168, expired 2019). Each dab
// tests its own footprint against one sample with fixed integer math and a
// plain flood from the dab center. Do not add histogram-derived hue ranges
// (US 8218860), a whole-image adjustment preview that the brush masks in
// (US 8687015), smoothing or Sobel/Canny edge maps feeding a segmentation
// (US 10416790 family), or selection masks with on-canvas markers built during
// the stroke (US 8175409) without a new review.

enum class ColorReplacementMode { Hue, Saturation, Color, Luminosity };
enum class ColorReplacementSampling { Continuous, Once, BackgroundSwatch };
enum class ColorReplacementLimits { Discontiguous, Contiguous, FindEdges };

struct ColorReplacementSettings {
  ColorReplacementMode mode{ColorReplacementMode::Color};
  ColorReplacementSampling sampling{ColorReplacementSampling::Continuous};
  ColorReplacementLimits limits{ColorReplacementLimits::Contiguous};
  int tolerance{30};  // percent, 1..100
  bool anti_alias{true};

  bool operator==(const ColorReplacementSettings&) const = default;
};

using ColorReplacementRgb = std::array<std::uint8_t, 3>;

// Tolerance percent as a largest per-channel difference on the 0..255 scale.
[[nodiscard]] int color_replacement_tolerance_levels(int tolerance_percent) noexcept;

// How strongly a pixel matches the sample: 1 when its largest per-channel
// difference is within the tolerance, 0 beyond it. Anti-alias ramps the last
// quarter of the tolerance down to 0, so pixels beyond the tolerance never
// change either way.
[[nodiscard]] float color_replacement_match(ColorReplacementRgb pixel, ColorReplacementRgb sample,
                                            int tolerance_percent, bool anti_alias) noexcept;

// One dab. `pixels` holds the footprint box (row-major width x height) from
// the stroke-start snapshot; `weights` enters with the brush coverage of each
// cell (0 outside the brush or on transparent pixels) and leaves with coverage
// x match after the Limits option: Discontiguous keeps every match,
// Contiguous keeps matches 4-connected to the center cell through matching
// cells, and Find Edges also refuses to step between neighbours whose colors
// differ by more than half the tolerance (a hard edge), so a stroke stays on
// its side of a sharp boundary even when both sides are within tolerance.
void color_replacement_dab_weights(std::span<const ColorReplacementRgb> pixels, std::vector<float>& weights,
                                   int width, int height, int center_x, int center_y,
                                   ColorReplacementRgb sample, const ColorReplacementSettings& settings);

// The replacement color for one pixel: Photoshop's non-separable Hue,
// Saturation, Color, or Luminosity blend of the foreground over the pixel.
[[nodiscard]] ColorReplacementRgb color_replacement_color(ColorReplacementRgb pixel, ColorReplacementRgb foreground,
                                                          ColorReplacementMode mode);

}  // namespace patchy
