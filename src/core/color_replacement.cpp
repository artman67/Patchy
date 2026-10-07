#include "core/color_replacement.hpp"

#include "core/blend_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace patchy {

namespace {

int largest_channel_difference(ColorReplacementRgb a, ColorReplacementRgb b) noexcept {
  int largest = 0;
  for (std::size_t channel = 0; channel < a.size(); ++channel) {
    largest = std::max(largest, std::abs(static_cast<int>(a[channel]) - static_cast<int>(b[channel])));
  }
  return largest;
}

}  // namespace

int color_replacement_tolerance_levels(int tolerance_percent) noexcept {
  return (std::clamp(tolerance_percent, 1, 100) * 255 + 50) / 100;
}

float color_replacement_match(ColorReplacementRgb pixel, ColorReplacementRgb sample, int tolerance_percent,
                              bool anti_alias) noexcept {
  const auto tolerance = color_replacement_tolerance_levels(tolerance_percent);
  const auto difference = largest_channel_difference(pixel, sample);
  if (difference > tolerance) {
    return 0.0F;
  }
  if (!anti_alias) {
    return 1.0F;
  }
  // The expired Smart Erasure Brush's tent with a flat cap: full strength up
  // to three quarters of the tolerance, then a linear ramp that reaches 0 one
  // level past the tolerance.
  const auto band = std::max(1, tolerance / 4);
  return std::clamp(static_cast<float>(tolerance + 1 - difference) / static_cast<float>(band + 1), 0.0F, 1.0F);
}

void color_replacement_dab_weights(std::span<const ColorReplacementRgb> pixels, std::vector<float>& weights,
                                   int width, int height, int center_x, int center_y,
                                   ColorReplacementRgb sample, const ColorReplacementSettings& settings) {
  const auto count = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height));
  if (count == 0 || pixels.size() < count || weights.size() < count) {
    return;
  }
  for (std::size_t index = 0; index < count; ++index) {
    if (weights[index] > 0.0F) {
      weights[index] *= color_replacement_match(pixels[index], sample, settings.tolerance, settings.anti_alias);
    }
  }
  if (settings.limits == ColorReplacementLimits::Discontiguous) {
    return;
  }

  // A plain 4-connected stack flood from the dab center over matching cells;
  // every cell it never reaches is cleared.
  std::vector<std::uint8_t> reached(count, 0);
  const auto center_inside = center_x >= 0 && center_y >= 0 && center_x < width && center_y < height;
  if (center_inside) {
    const auto edge_step = std::max(1, color_replacement_tolerance_levels(settings.tolerance) / 2);
    const auto find_edges = settings.limits == ColorReplacementLimits::FindEdges;
    const auto start = static_cast<std::size_t>(center_y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(center_x);
    std::vector<std::pair<int, int>> stack;
    if (weights[start] > 0.0F) {
      reached[start] = 1;
      stack.emplace_back(center_x, center_y);
    }
    while (!stack.empty()) {
      const auto [x, y] = stack.back();
      stack.pop_back();
      const auto from = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
      constexpr std::array<std::pair<int, int>, 4> kSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
      for (const auto& [step_x, step_y] : kSteps) {
        const auto next_x = x + step_x;
        const auto next_y = y + step_y;
        if (next_x < 0 || next_y < 0 || next_x >= width || next_y >= height) {
          continue;
        }
        const auto next = static_cast<std::size_t>(next_y) * static_cast<std::size_t>(width) +
                          static_cast<std::size_t>(next_x);
        if (reached[next] != 0 || weights[next] <= 0.0F) {
          continue;
        }
        // Find Edges: the expired teaching's "same side of an edge" rule. A
        // hard transition between neighbours stops the flood.
        if (find_edges && largest_channel_difference(pixels[from], pixels[next]) > edge_step) {
          continue;
        }
        reached[next] = 1;
        stack.emplace_back(next_x, next_y);
      }
    }
  }
  for (std::size_t index = 0; index < count; ++index) {
    if (reached[index] == 0) {
      weights[index] = 0.0F;
    }
  }
}

ColorReplacementRgb color_replacement_color(ColorReplacementRgb pixel, ColorReplacementRgb foreground,
                                            ColorReplacementMode mode) {
  auto blend = BlendMode::Color;
  switch (mode) {
    case ColorReplacementMode::Hue:
      blend = BlendMode::Hue;
      break;
    case ColorReplacementMode::Saturation:
      blend = BlendMode::Saturation;
      break;
    case ColorReplacementMode::Color:
      blend = BlendMode::Color;
      break;
    case ColorReplacementMode::Luminosity:
      blend = BlendMode::Luminosity;
      break;
  }
  return blend_rgb(foreground, pixel, blend);
}

}  // namespace patchy
