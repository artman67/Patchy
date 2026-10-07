#include "core/replace_color.hpp"

#include "core/adjustment_layer.hpp"

#include <algorithm>

namespace patchy {

namespace {

// The replacement is Hue/Saturation's calibrated master math (colorize off, no
// bands), so a fully selected pixel matches Hue/Saturation with the same sliders.
AdjustmentSettings replacement_adjustment(const ReplaceColorSettings& settings) {
  AdjustmentSettings adjustment;
  adjustment.kind = AdjustmentKind::HueSaturation;
  adjustment.hue_saturation.hue_shift = std::clamp(settings.hue, -kReplaceColorHueRange, kReplaceColorHueRange);
  adjustment.hue_saturation.saturation_delta =
      std::clamp(settings.saturation, -kReplaceColorSaturationRange, kReplaceColorSaturationRange);
  adjustment.hue_saturation.lightness_delta =
      std::clamp(settings.lightness, -kReplaceColorLightnessRange, kReplaceColorLightnessRange);
  return adjustment;
}

ColorRangeParams sampled_colors_range(const ReplaceColorSettings& settings) {
  auto range = settings.range;
  range.select = ColorRangeSelect::SampledColors;
  range.invert = false;
  return range;
}

}  // namespace

bool replace_color_has_effect(const ReplaceColorSettings& settings) {
  return !settings.range.added.empty() && (settings.hue != 0 || settings.saturation != 0 || settings.lightness != 0);
}

RgbColor replace_color_full_shift(RgbColor color, const ReplaceColorSettings& settings) {
  return apply_adjustment_to_color(color, replacement_adjustment(settings));
}

void apply_replace_color_span(std::uint8_t* pixels, std::int32_t count, std::size_t pixel_bytes,
                              const ReplaceColorSettings& settings) {
  if (pixels == nullptr || count <= 0 || pixel_bytes < 3U || !replace_color_has_effect(settings)) {
    return;
  }
  const auto adjustment = replacement_adjustment(settings);
  const auto range = sampled_colors_range(settings);
  for (std::int32_t index = 0; index < count; ++index, pixels += pixel_bytes) {
    const RgbColor color{pixels[0], pixels[1], pixels[2]};
    const int weight = color_range_alpha(ColorRangeColor{color.red, color.green, color.blue}, range);
    if (weight == 0) {
      continue;
    }
    const auto shifted = apply_adjustment_to_color(color, adjustment);
    // Linear blend toward the shifted color by the score; 255 lands on it exactly.
    const auto blend = [weight](int from, int to) {
      return static_cast<std::uint8_t>((from * (255 - weight) + to * weight + 127) / 255);
    };
    pixels[0] = blend(color.red, shifted.red);
    pixels[1] = blend(color.green, shifted.green);
    pixels[2] = blend(color.blue, shifted.blue);
  }
}

}  // namespace patchy
