#include "core/color_range.hpp"

#include "core/adjustment_layer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace patchy {

namespace {

// Weighted RGB distance (the "redmean" approximation of perceptual difference),
// scaled so black-to-white is about 255, the unit Fuzziness is measured in. The
// weights are the usual 2 + rmean/256, 4, 2 + (255 - rmean)/256, kept as integers
// times 256; one correctly rounded sqrt keeps the result identical on every
// toolchain.
double sampled_color_distance(ColorRangeColor a, ColorRangeColor b) {
  const int rmean = (static_cast<int>(a.r) + static_cast<int>(b.r)) / 2;
  const int dr = static_cast<int>(a.r) - static_cast<int>(b.r);
  const int dg = static_cast<int>(a.g) - static_cast<int>(b.g);
  const int db = static_cast<int>(a.b) - static_cast<int>(b.b);
  const auto weighted = static_cast<std::int64_t>(512 + rmean) * dr * dr + std::int64_t{1024} * dg * dg +
                        static_cast<std::int64_t>(767 - rmean) * db * db;
  return std::sqrt(static_cast<double>(weighted)) / 48.0;
}

int round_to_byte(double value) {
  return std::clamp(static_cast<int>(value + 0.5), 0, 255);
}

// Fuzziness falloff around one sample: fully selected within half the fuzziness,
// then a linear ramp to nothing at the full fuzziness. Fuzziness 0 selects only
// the exact color.
int sample_strength(ColorRangeColor color, ColorRangeColor sample, int fuzziness) {
  if (fuzziness <= 0) {
    return color == sample ? 255 : 0;
  }
  const auto distance = sampled_color_distance(color, sample);
  const auto full = static_cast<double>(fuzziness) / 2.0;
  if (distance <= full) {
    return 255;
  }
  if (distance >= static_cast<double>(fuzziness)) {
    return 0;
  }
  return round_to_byte(255.0 * (static_cast<double>(fuzziness) - distance) / full);
}

int strongest_sample(ColorRangeColor color, const std::vector<ColorRangeColor>& samples, int fuzziness) {
  int strongest = 0;
  for (const auto& sample : samples) {
    strongest = std::max(strongest, sample_strength(color, sample, fuzziness));
    if (strongest == 255) {
      break;
    }
  }
  return strongest;
}

// The six color families reuse Photoshop's Hue/Saturation hextants
// (kHueSaturationDefaultBandRanges): full strength between the inner stops,
// linear ramps to the outer stops. Gray has no hue, so the hue weight is scaled by
// chroma, reaching full strength at half the channel range.
int color_family_strength(ColorRangeColor color, int family_index) {
  const int r = color.r;
  const int g = color.g;
  const int b = color.b;
  const int maximum = std::max({r, g, b});
  const int chroma = maximum - std::min({r, g, b});
  if (chroma == 0) {
    return 0;
  }
  double hue = 0.0;
  if (maximum == r) {
    hue = 60.0 * static_cast<double>(g - b) / static_cast<double>(chroma);
  } else if (maximum == g) {
    hue = 120.0 + 60.0 * static_cast<double>(b - r) / static_cast<double>(chroma);
  } else {
    hue = 240.0 + 60.0 * static_cast<double>(r - g) / static_cast<double>(chroma);
  }
  if (hue < 0.0) {
    hue += 360.0;
  }
  const auto& stops = kHueSaturationDefaultBandRanges[static_cast<std::size_t>(family_index)];
  // Distance forward around the wheel from the band's outer start.
  const auto forward = [](double from, double to) { return to >= from ? to - from : to - from + 360.0; };
  const double outer_start = stops[0];
  const auto ramp_in = forward(outer_start, stops[1]);
  const auto inner_end = forward(outer_start, stops[2]);
  const auto outer_end = forward(outer_start, stops[3]);
  const auto position = forward(outer_start, hue);
  double weight = 0.0;
  if (position < ramp_in) {
    weight = position / ramp_in;
  } else if (position <= inner_end) {
    weight = 1.0;
  } else if (position < outer_end) {
    weight = (outer_end - position) / (outer_end - inner_end);
  }
  const auto chroma_weight = std::min(1.0, static_cast<double>(chroma) / 128.0);
  return round_to_byte(255.0 * weight * chroma_weight);
}

// Highlights, Midtones and Shadows: the Lab lightness windows Photoshop's classic
// dialog documented (Shadows full to L 40, fading out by 55; Midtones full 55-75,
// fading over 40-55 and 75-85; Highlights full from 80, fading in from 75),
// converted to sRGB gray levels and applied to integer Rec. 601 luma, with
// linear shoulders.
int tonal_strength(ColorRangeColor color, ColorRangeSelect select) {
  const int luma = (299 * color.r + 587 * color.g + 114 * color.b + 500) / 1000;
  // 255 on [full_low, full_high], linear to 0 at zero_low and zero_high.
  const auto window = [luma](int zero_low, int full_low, int full_high, int zero_high) {
    if (luma >= full_low && luma <= full_high) {
      return 255;
    }
    if (luma <= zero_low || luma >= zero_high) {
      return 0;
    }
    const int span = luma < full_low ? full_low - zero_low : zero_high - full_high;
    const int distance = luma < full_low ? luma - zero_low : zero_high - luma;
    return (255 * distance + span / 2) / span;
  };
  switch (select) {
    case ColorRangeSelect::Shadows:
      return window(-1, 0, 94, 132);
    case ColorRangeSelect::Midtones:
      return window(94, 132, 185, 212);
    case ColorRangeSelect::Highlights:
      return window(185, 198, 255, 256);
    default:
      return 0;
  }
}

int raw_strength(ColorRangeColor color, const ColorRangeParams& params) {
  switch (params.select) {
    case ColorRangeSelect::SampledColors: {
      const auto fuzziness = std::clamp(params.fuzziness, 0, kColorRangeMaxFuzziness);
      const auto added = strongest_sample(color, params.added, fuzziness);
      if (added == 0 || params.subtracted.empty()) {
        return added;
      }
      const auto removed = strongest_sample(color, params.subtracted, fuzziness);
      return (added * (255 - removed) + 127) / 255;
    }
    case ColorRangeSelect::Reds:
    case ColorRangeSelect::Yellows:
    case ColorRangeSelect::Greens:
    case ColorRangeSelect::Cyans:
    case ColorRangeSelect::Blues:
    case ColorRangeSelect::Magentas:
      return color_family_strength(
          color, static_cast<int>(params.select) - static_cast<int>(ColorRangeSelect::Reds));
    case ColorRangeSelect::Highlights:
    case ColorRangeSelect::Midtones:
    case ColorRangeSelect::Shadows:
      return tonal_strength(color, params.select);
  }
  return 0;
}

}  // namespace

std::uint8_t color_range_alpha(ColorRangeColor color, const ColorRangeParams& params) {
  const auto strength = raw_strength(color, params);
  return static_cast<std::uint8_t>(params.invert ? 255 - strength : strength);
}

void apply_color_range_sample(ColorRangeParams& params, ColorRangeColor sample, ColorRangeSampleAction action) {
  switch (action) {
    case ColorRangeSampleAction::Replace:
      params.added = {sample};
      params.subtracted.clear();
      break;
    case ColorRangeSampleAction::Add:
      params.added.push_back(sample);
      std::erase(params.subtracted, sample);
      break;
    case ColorRangeSampleAction::Subtract:
      params.subtracted.push_back(sample);
      std::erase(params.added, sample);
      break;
  }
}

std::vector<std::uint8_t> color_range_mask(const std::uint8_t* rgba, std::int32_t width, std::int32_t height,
                                           std::ptrdiff_t stride_bytes, const ColorRangeParams& params) {
  if (rgba == nullptr || width <= 0 || height <= 0) {
    return {};
  }
  std::vector<std::uint8_t> mask(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  // Photos repeat colors heavily, so a direct-mapped memo of recent colors skips
  // most of the per-sample square roots. Keys carry a valid bit above the 24 color bits.
  constexpr std::size_t kMemoSize = 1U << 16U;
  std::vector<std::uint32_t> memo_keys(kMemoSize, 0U);
  std::vector<std::uint8_t> memo_values(kMemoSize, 0U);
  for (std::int32_t y = 0; y < height; ++y) {
    const auto* row = rgba + static_cast<std::ptrdiff_t>(y) * stride_bytes;
    auto* out = mask.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    for (std::int32_t x = 0; x < width; ++x) {
      const auto* pixel = row + static_cast<std::ptrdiff_t>(x) * 4;
      const auto alpha = pixel[3];
      if (alpha == 0U) {
        out[x] = 0U;
        continue;
      }
      const auto key = (1U << 24U) | (static_cast<std::uint32_t>(pixel[0]) << 16U) |
                       (static_cast<std::uint32_t>(pixel[1]) << 8U) | pixel[2];
      const auto slot = static_cast<std::size_t>((key ^ (key >> 13U) ^ (key >> 7U)) & (kMemoSize - 1U));
      std::uint8_t value = 0U;
      if (memo_keys[slot] == key) {
        value = memo_values[slot];
      } else {
        value = color_range_alpha(ColorRangeColor{pixel[0], pixel[1], pixel[2]}, params);
        memo_keys[slot] = key;
        memo_values[slot] = value;
      }
      out[x] = alpha == 255U ? value : static_cast<std::uint8_t>((value * alpha + 127U) / 255U);
    }
  }
  return mask;
}

void combine_color_range_mask(std::vector<std::uint8_t>& base, const std::vector<std::uint8_t>& candidate,
                              ColorRangeCombine combine, bool base_is_empty) {
  if (base.size() != candidate.size()) {
    return;
  }
  if (base_is_empty) {
    if (combine == ColorRangeCombine::Subtract) {
      std::fill(base.begin(), base.end(), std::uint8_t{0});
    } else {
      base = candidate;
    }
    return;
  }
  for (std::size_t index = 0; index < base.size(); ++index) {
    const int existing = base[index];
    const int value = candidate[index];
    switch (combine) {
      case ColorRangeCombine::Add:
        base[index] = static_cast<std::uint8_t>(std::max(existing, value));
        break;
      case ColorRangeCombine::Replace:
      case ColorRangeCombine::Intersect:
        base[index] = static_cast<std::uint8_t>(std::min(existing, value));
        break;
      case ColorRangeCombine::Subtract:
        base[index] = static_cast<std::uint8_t>((existing * (255 - value)) / 255);
        break;
    }
  }
}

}  // namespace patchy
