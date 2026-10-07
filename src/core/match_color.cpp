#include "core/match_color.hpp"

#include "core/worker_budget.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <future>
#include <vector>

namespace patchy {

namespace {

// Standard deviations below this many Lab units count as this value, so a
// nearly flat channel cannot multiply noise without bound. Applied to both
// sides, so a target matched to itself keeps scale exactly 1.
constexpr double kMinDeviation = 2.0;

// D65 reference white for the sRGB primaries.
constexpr double kWhiteX = 0.95047;
constexpr double kWhiteY = 1.0;
constexpr double kWhiteZ = 1.08883;
constexpr double kLabEpsilon = 216.0 / 24389.0;  // (6/29)^3
constexpr double kLabDelta = 6.0 / 29.0;

const std::array<double, 256>& srgb_to_linear_table() {
  static const auto table = [] {
    std::array<double, 256> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
      const auto v = static_cast<double>(index) / 255.0;
      values[index] = v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    }
    return values;
  }();
  return table;
}

double lab_f(double t) {
  return t > kLabEpsilon ? std::cbrt(t) : t / (3.0 * kLabDelta * kLabDelta) + 4.0 / 29.0;
}

double lab_f_inverse(double f) {
  return f > kLabDelta ? f * f * f : 3.0 * kLabDelta * kLabDelta * (f - 4.0 / 29.0);
}

std::array<double, 3> lab_from_bytes(const std::array<double, 256>& linear, std::uint8_t red, std::uint8_t green,
                                     std::uint8_t blue) {
  const auto r = linear[red];
  const auto g = linear[green];
  const auto b = linear[blue];
  const auto fx = lab_f((0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / kWhiteX);
  const auto fy = lab_f((0.2126729 * r + 0.7151522 * g + 0.0721750 * b) / kWhiteY);
  const auto fz = lab_f((0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / kWhiteZ);
  return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

std::uint8_t encode_srgb(double linear) {
  const auto clamped = std::clamp(linear, 0.0, 1.0);
  const auto encoded = clamped <= 0.0031308 ? 12.92 * clamped : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
  return static_cast<std::uint8_t>(std::lround(std::clamp(encoded, 0.0, 1.0) * 255.0));
}

RgbColor rgb_from_lab(const std::array<double, 3>& lab) {
  const auto fy = (lab[0] + 16.0) / 116.0;
  const auto x = kWhiteX * lab_f_inverse(fy + lab[1] / 500.0);
  const auto y = kWhiteY * lab_f_inverse(fy);
  const auto z = kWhiteZ * lab_f_inverse(fy - lab[2] / 200.0);
  return RgbColor{encode_srgb(3.2404542 * x - 1.5371385 * y - 0.4985314 * z),
                  encode_srgb(-0.9692660 * x + 1.8760108 * y + 0.0415560 * z),
                  encode_srgb(0.0556434 * x - 0.2040259 * y + 1.0572252 * z)};
}

// Weighted first and second moments of one row.
struct RowSums {
  std::array<double, 3> sum{};
  std::array<double, 3> square_sum{};
  double weight{0.0};
};

}  // namespace

std::array<double, 3> match_color_lab(RgbColor color) {
  return lab_from_bytes(srgb_to_linear_table(), color.red, color.green, color.blue);
}

MatchColorStatistics match_color_statistics(const PixelBuffer& pixels, const std::uint8_t* coverage) {
  MatchColorStatistics statistics;
  const auto format = pixels.format();
  if (pixels.width() <= 0 || pixels.height() <= 0 || format.bit_depth != BitDepth::UInt8 || format.channels < 3) {
    return statistics;
  }
  const auto& linear = srgb_to_linear_table();
  const auto width = pixels.width();
  const auto height = pixels.height();
  const auto pixel_bytes = bytes_per_pixel(format);
  const auto has_alpha = format.channels >= 4;
  std::vector<RowSums> rows(static_cast<std::size_t>(height));
  const auto sum_rows = [&](std::int32_t begin, std::int32_t end) {
    for (std::int32_t y = begin; y < end; ++y) {
      auto& row_sums = rows[static_cast<std::size_t>(y)];
      const auto* px = pixels.row(y).data();
      const auto* row_coverage =
          coverage != nullptr ? coverage + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) : nullptr;
      for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
        auto weight = 255.0;
        if (has_alpha) {
          weight = px[3];
        }
        if (row_coverage != nullptr) {
          weight = weight * row_coverage[x] / 255.0;
        }
        if (weight <= 0.0) {
          continue;
        }
        weight /= 255.0;
        const auto lab = lab_from_bytes(linear, px[0], px[1], px[2]);
        for (std::size_t channel = 0; channel < 3; ++channel) {
          row_sums.sum[channel] += weight * lab[channel];
          row_sums.square_sum[channel] += weight * lab[channel] * lab[channel];
        }
        row_sums.weight += weight;
      }
    }
  };

  const auto area = static_cast<std::int64_t>(width) * height;
  const auto workers =
      area < 1'000'000
          ? 1
          : max_blocking_fanout_workers(std::clamp(std::min(height / 128, hardware_worker_threads()), 1, 16));
  if (workers < 2) {
    sum_rows(0, height);
  } else {
    std::vector<std::future<void>> strips;
    strips.reserve(static_cast<std::size_t>(workers));
    const auto rows_per_strip = (height + workers - 1) / workers;
    for (std::int32_t start = 0; start < height; start += rows_per_strip) {
      const auto end = std::min(start + rows_per_strip, height);
      strips.push_back(std::async(std::launch::async, [&sum_rows, start, end] { sum_rows(start, end); }));
    }
    for (auto& strip : strips) {
      strip.get();
    }
  }

  RowSums total;
  for (const auto& row_sums : rows) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      total.sum[channel] += row_sums.sum[channel];
      total.square_sum[channel] += row_sums.square_sum[channel];
    }
    total.weight += row_sums.weight;
  }
  if (total.weight <= 0.0) {
    return statistics;
  }
  statistics.weight = total.weight;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const auto mean = total.sum[channel] / total.weight;
    statistics.mean[channel] = mean;
    statistics.deviation[channel] = std::sqrt(std::max(0.0, total.square_sum[channel] / total.weight - mean * mean));
  }
  return statistics;
}

MatchColorTransform make_match_color_transform(const MatchColorStatistics& target, const MatchColorStatistics* source,
                                               MatchColorOptions options) {
  MatchColorTransform transform;
  transform.fade = std::clamp(options.fade, 0, kMatchColorFadeMax);
  const auto luminance =
      std::clamp(options.luminance, kMatchColorLuminanceMin, kMatchColorLuminanceMax) / 100.0;
  const auto intensity =
      std::clamp(options.color_intensity, kMatchColorIntensityMin, kMatchColorIntensityMax) / 100.0;
  // Without usable target statistics there is nothing to match from; the sliders
  // still act on the colors themselves.
  const auto have_target = !target.empty();
  const auto& matched = have_target && source != nullptr && !source->empty() ? *source : target;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    auto scale = 1.0;
    auto offset = 0.0;
    if (have_target) {
      // lab' = mean_s + (dev_s / dev_t) * (lab - mean_t). Neutralize moves the
      // matched a*/b* means to zero, which removes an overall color cast.
      const auto target_mean = target.mean[channel];
      const auto source_mean = options.neutralize && channel > 0 ? 0.0 : matched.mean[channel];
      scale = std::max(matched.deviation[channel], kMinDeviation) /
              std::max(target.deviation[channel], kMinDeviation);
      offset = source_mean - scale * target_mean;
    }
    const auto factor = channel == 0 ? luminance : intensity;
    transform.scale[channel] = scale * factor;
    transform.offset[channel] = offset * factor;
  }
  transform.identity = transform.fade >= kMatchColorFadeMax ||
                       (transform.scale == std::array<double, 3>{1.0, 1.0, 1.0} &&
                        transform.offset == std::array<double, 3>{0.0, 0.0, 0.0});
  return transform;
}

RgbColor apply_match_color(RgbColor color, const MatchColorTransform& transform) {
  if (transform.identity) {
    return color;
  }
  auto lab = match_color_lab(color);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    lab[channel] = transform.scale[channel] * lab[channel] + transform.offset[channel];
  }
  lab[0] = std::clamp(lab[0], 0.0, 100.0);
  const auto adjusted = rgb_from_lab(lab);
  if (transform.fade <= 0) {
    return adjusted;
  }
  const auto keep = transform.fade;
  const auto blend = [keep](std::uint8_t adjusted_value, std::uint8_t original) {
    return static_cast<std::uint8_t>((adjusted_value * (100 - keep) + original * keep + 50) / 100);
  };
  return RgbColor{blend(adjusted.red, color.red), blend(adjusted.green, color.green),
                  blend(adjusted.blue, color.blue)};
}

}  // namespace patchy
