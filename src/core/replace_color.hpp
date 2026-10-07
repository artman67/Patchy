#pragma once

#include "core/color_range.hpp"
#include "core/layer.hpp"

#include <cstddef>
#include <cstdint>

namespace patchy {

// Image > Adjustments > Replace Color (docs/replace-color.md): Color Range's Sampled
// Colors score weights a Hue/Saturation master shift of each pixel. Like Color Range,
// a pixel is scored from its own color and the user's samples only (no position,
// neighbours, clusters, or histogram analysis), which keeps the command outside the
// claims recorded in docs/replace-color.md. Localized Color Clusters stays out.

inline constexpr int kReplaceColorHueRange = 180;
inline constexpr int kReplaceColorSaturationRange = 100;
inline constexpr int kReplaceColorLightnessRange = 100;

struct ReplaceColorSettings {
  // Fuzziness and the eyedropper samples; `select` stays Sampled Colors and `invert`
  // stays off (Photoshop's Replace Color has neither).
  ColorRangeParams range;
  int hue{0};         // -180..180
  int saturation{0};  // -100..100
  int lightness{0};   // -100..100
};

// False when the command cannot change a pixel: no sample, or all three sliders at zero.
[[nodiscard]] bool replace_color_has_effect(const ReplaceColorSettings& settings);

// `color` with the full replacement shift (the dialog's Result swatch).
[[nodiscard]] RgbColor replace_color_full_shift(RgbColor color, const ReplaceColorSettings& settings);

// Rewrites `count` 8-bit pixels in place (`pixel_bytes` apart, RGB first): each moves
// toward its fully shifted color by its Sampled Colors score (0..255). Alpha and any
// further channels are left untouched; alpha does not weaken the score, as in any
// destructive adjustment.
void apply_replace_color_span(std::uint8_t* pixels, std::int32_t count, std::size_t pixel_bytes,
                              const ReplaceColorSettings& settings);

}  // namespace patchy
