#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace patchy {

// Select > Color Range: one global, per-pixel color test with a soft falloff, the
// classic Photoshop 3-era command (see docs/color-range.md). Every pixel is scored
// from its own color alone: no spatial distance to a sample point, no neighbour
// comparison, no local color model and no segmentation. That keeps the command
// outside Adobe's active selection patents recorded in docs/color-range.md
// (US 8050498 coherent live classification, US 8004536 distance-from-click energy,
// US 11223744 spatial-plus-range adjustment masks). Photoshop's Localized Color
// Clusters, Skin Tones and Detect Faces are deliberately absent; adding them needs a
// new claim check.

// Photoshop's Select menu entries, in its order (Skin Tones and Out of Gamut omitted).
enum class ColorRangeSelect {
  SampledColors,
  Reds,
  Yellows,
  Greens,
  Cyans,
  Blues,
  Magentas,
  Highlights,
  Midtones,
  Shadows
};

inline constexpr int kColorRangeMaxFuzziness = 200;
inline constexpr int kColorRangeDefaultFuzziness = 40;

struct ColorRangeColor {
  std::uint8_t r{0};
  std::uint8_t g{0};
  std::uint8_t b{0};
  friend bool operator==(const ColorRangeColor&, const ColorRangeColor&) = default;
};

struct ColorRangeParams {
  ColorRangeSelect select{ColorRangeSelect::SampledColors};
  // 0..200, Sampled Colors only (the presets use fixed ranges, as Photoshop's
  // classic dialog did).
  int fuzziness{kColorRangeDefaultFuzziness};
  std::vector<ColorRangeColor> added;       // the eyedropper and Add samples
  std::vector<ColorRangeColor> subtracted;  // the Subtract samples
  bool invert{false};
};

// How one eyedropper click edits the sample lists: the plain eyedropper replaces them
// with the clicked color, Add appends it, Subtract adds a removal sample. Shared with
// Image > Adjustments > Replace Color (docs/replace-color.md).
enum class ColorRangeSampleAction {
  Replace,
  Add,
  Subtract
};

void apply_color_range_sample(ColorRangeParams& params, ColorRangeColor sample, ColorRangeSampleAction action);

// How the result combines with the selection that existed before the command.
enum class ColorRangeCombine {
  Replace,  // within the existing selection (Photoshop: Color Range never reaches outside it)
  Add,
  Subtract,
  Intersect
};

// Selection strength (0..255) of one opaque color. Deterministic across toolchains:
// integer color math plus one correctly rounded square root.
[[nodiscard]] std::uint8_t color_range_alpha(ColorRangeColor color, const ColorRangeParams& params);

// Scores `width` x `height` RGBA8888 rows (straight alpha, `stride_bytes` apart) into a
// row-major gray8 mask. A pixel's score is scaled by its own alpha, so transparent
// pixels are never selected (Invert applies to the color score first).
[[nodiscard]] std::vector<std::uint8_t> color_range_mask(const std::uint8_t* rgba, std::int32_t width,
                                                         std::int32_t height, std::ptrdiff_t stride_bytes,
                                                         const ColorRangeParams& params);

// Combines `candidate` into `base` in place (same size gray8 masks) with the canvas
// combine arithmetic (Add max, Intersect min, Subtract multiplies by the complement).
// With no prior selection (`base_is_empty`) Subtract yields nothing and every other
// mode takes the candidate as is, as the canvas does for a selection tool.
void combine_color_range_mask(std::vector<std::uint8_t>& base, const std::vector<std::uint8_t>& candidate,
                              ColorRangeCombine combine, bool base_is_empty);

}  // namespace patchy
