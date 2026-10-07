#include "filters/shadows_highlights.hpp"

#include "core/layer_render_utils.hpp"
#include "core/worker_budget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>

namespace patchy {

namespace {

// Full Amount over the darkest (or brightest) neighborhood bends the curve to
// gamma 1 / (1 + kToneStrength) = 1/3; the default 35% gives about 1/1.7.
constexpr double kToneStrength = 2.0;
// Floor for the luminance ratio that scales chroma, so a negative Color
// setting never raises zero to a negative power.
constexpr double kMinimumLuminanceRatio = 1.0 / 1024.0;

bool range_active(const ShadowsHighlightsRange& range) {
  return range.amount > 0 && range.tone > 0;
}

void report_stage(const FilterProgress* progress, int completed, int total) {
  if (progress != nullptr && progress->update && !progress->update(completed, total, FilterProgressStage::Filtering)) {
    throw FilterCancelled();
  }
}

// Rec. 601 luma of an 8-bit RGB pixel on 0..65535; a gray v maps to exactly v * 257.
std::uint16_t luma16(const std::uint8_t* px) {
  const auto weighted = 299U * px[0] + 587U * px[1] + 114U * px[2];
  return static_cast<std::uint16_t>((weighted * 257U + 500U) / 1000U);
}

// Runs row_fn(y) for every row, in parallel strips on large buffers. Nothing
// here reports progress or throws, so no callback ever crosses threads.
template <typename RowFn>
void for_each_row(std::int32_t width, std::int32_t height, const RowFn& row_fn) {
#if !defined(__EMSCRIPTEN__)
  // Preview computes already run on a pooled worker on wasm; a nested fan-out
  // there can stall (see apply_row_spans_in_parallel), so wasm stays sequential.
  const auto area = static_cast<std::int64_t>(width) * height;
  const auto workers =
      area < 1'000'000 ? 1
                       : max_blocking_fanout_workers(std::clamp(std::min(height / 64, hardware_worker_threads()), 1, 16));
  if (workers >= 2) {
    std::vector<std::future<void>> strips;
    strips.reserve(static_cast<std::size_t>(workers));
    const auto rows_per_strip = (height + workers - 1) / workers;
    for (std::int32_t start = 0; start < height; start += rows_per_strip) {
      const auto end = std::min(start + rows_per_strip, height);
      strips.push_back(std::async(std::launch::async, [&row_fn, start, end] {
        for (std::int32_t y = start; y < end; ++y) {
          row_fn(y);
        }
      }));
    }
    for (auto& strip : strips) {
      strip.get();
    }
    return;
  }
#else
  (void)width;
#endif
  for (std::int32_t y = 0; y < height; ++y) {
    row_fn(y);
  }
}

// The luminance neighborhood: luma blurred by a Gaussian of sigma = radius
// (the deterministic three-box approximation the mask feather uses). With
// transparency the blur is alpha-weighted, so transparent pixels never darken
// a neighborhood.
std::vector<std::uint16_t> blurred_luminance(const std::uint8_t* base, std::size_t stride, std::int32_t width,
                                             std::int32_t height, std::size_t pixel_bytes, bool alpha_weighted,
                                             int radius) {
  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint16_t> luma(count);
  std::vector<std::uint16_t> weight(alpha_weighted ? count : 0);
  for (std::int32_t y = 0; y < height; ++y) {
    const auto* px = base + static_cast<std::size_t>(y) * stride;
    auto* out = luma.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
      if (alpha_weighted) {
        out[x] = static_cast<std::uint16_t>((static_cast<std::uint32_t>(luma16(px)) * px[3] + 127U) / 255U);
        weight[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
            static_cast<std::uint16_t>(px[3] * 257U);
      } else {
        out[x] = luma16(px);
      }
    }
  }
  const auto radii = mask_feather_box_radii(static_cast<double>(radius));
  mask_feather_blur(luma, width, height, radii);
  if (alpha_weighted) {
    mask_feather_blur(weight, width, height, radii);
    for (std::size_t index = 0; index < count; ++index) {
      const auto w = static_cast<std::uint32_t>(weight[index]);
      luma[index] = w == 0U ? std::uint16_t{0}
                            : static_cast<std::uint16_t>(std::min<std::uint32_t>(
                                  65535U, (static_cast<std::uint32_t>(luma[index]) * 65535U + w / 2U) / w));
    }
  }
  return luma;
}

// Index of the first level whose running count from `begin` exceeds
// `threshold` samples, walking by `step`; stops at `limit`.
int clip_level(const std::array<std::uint64_t, 256>& histogram, std::uint64_t threshold, int begin, int limit,
               int step) {
  std::uint64_t cumulative = 0;
  int level = begin;
  for (; level != limit; level += step) {
    cumulative += histogram[static_cast<std::size_t>(level)];
    if (cumulative > threshold) {
      break;
    }
  }
  return level;
}

}  // namespace

ShadowsHighlightsSettings clamp_shadows_highlights(ShadowsHighlightsSettings settings) {
  for (auto* range : {&settings.shadows, &settings.highlights}) {
    range->amount = std::clamp(range->amount, 0, 100);
    range->tone = std::clamp(range->tone, 0, 100);
    range->radius = std::clamp(range->radius, 0, kShadowsHighlightsMaxRadius);
  }
  settings.color_correction = std::clamp(settings.color_correction, -100, 100);
  settings.midtone_contrast = std::clamp(settings.midtone_contrast, -100, 100);
  settings.black_clip_hundredths = std::clamp(settings.black_clip_hundredths, 0, kShadowsHighlightsMaxClipHundredths);
  settings.white_clip_hundredths = std::clamp(settings.white_clip_hundredths, 0, kShadowsHighlightsMaxClipHundredths);
  return settings;
}

bool shadows_highlights_has_effect(const ShadowsHighlightsSettings& settings) {
  const auto clamped = clamp_shadows_highlights(settings);
  return range_active(clamped.shadows) || range_active(clamped.highlights) || clamped.midtone_contrast != 0;
}

void apply_shadows_highlights(PixelBuffer& pixels, const ShadowsHighlightsSettings& requested,
                              const FilterProgress* progress) {
  const auto format = pixels.format();
  if (format.bit_depth != BitDepth::UInt8 || format.channels < 3 || pixels.empty() ||
      !shadows_highlights_has_effect(requested)) {
    return;
  }
  const auto settings = clamp_shadows_highlights(requested);
  const auto width = pixels.width();
  const auto height = pixels.height();
  const auto pixel_bytes = bytes_per_pixel(format);
  const auto stride = pixels.stride_bytes();
  const bool has_alpha = format.channels >= 4;
  // Detach copy-on-write storage here, before any worker writes (AGENTS.md).
  auto* base = pixels.data().data();

  bool alpha_weighted = false;
  if (has_alpha) {
    for (std::int32_t y = 0; y < height && !alpha_weighted; ++y) {
      const auto* px = base + static_cast<std::size_t>(y) * stride;
      for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
        if (px[3] != 255) {
          alpha_weighted = true;
          break;
        }
      }
    }
  }

  const bool shadows = range_active(settings.shadows);
  const bool highlights = range_active(settings.highlights);
  const bool black_clip = shadows && settings.black_clip_hundredths > 0;
  const bool white_clip = highlights && settings.white_clip_hundredths > 0;
  const bool shared_plane = shadows && highlights && settings.shadows.radius == settings.highlights.radius;
  const int stages = (shadows ? 1 : 0) + (highlights && !shared_plane ? 1 : 0) + 1 + (black_clip || white_clip ? 1 : 0);
  int stage = 0;
  report_stage(progress, stage, stages);

  std::vector<std::uint16_t> shadow_plane;
  std::vector<std::uint16_t> highlight_plane;
  if (shadows) {
    shadow_plane =
        blurred_luminance(base, stride, width, height, pixel_bytes, alpha_weighted, settings.shadows.radius);
    report_stage(progress, ++stage, stages);
  }
  if (highlights && !shared_plane) {
    highlight_plane =
        blurred_luminance(base, stride, width, height, pixel_bytes, alpha_weighted, settings.highlights.radius);
    report_stage(progress, ++stage, stages);
  }
  const auto* shadow_mask = shadows ? shadow_plane.data() : nullptr;
  const auto* highlight_mask = !highlights ? nullptr : (shared_plane ? shadow_plane.data() : highlight_plane.data());

  const auto shadow_amount = settings.shadows.amount / 100.0;
  const auto shadow_tone = settings.shadows.tone / 100.0;
  const auto highlight_amount = settings.highlights.amount / 100.0;
  const auto highlight_tone = settings.highlights.tone / 100.0;
  const auto midtone = settings.midtone_contrast / 100.0;
  const auto color = settings.color_correction / 100.0;
  for_each_row(width, height, [&](std::int32_t y) {
    auto* px = base + static_cast<std::size_t>(y) * stride;
    const auto row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
      if (has_alpha && px[3] == 0) {
        continue;
      }
      const auto index = row_offset + static_cast<std::size_t>(x);
      const auto original = luma16(px) / 65535.0;
      auto tone = original;
      if (shadow_mask != nullptr) {
        // Weight falls from 1 in a black neighborhood to 0 at the Tone width.
        const auto neighborhood = shadow_mask[index] / 65535.0;
        if (neighborhood < shadow_tone) {
          const auto falloff = 1.0 - neighborhood / shadow_tone;
          tone = std::pow(tone, 1.0 / (1.0 + kToneStrength * shadow_amount * falloff * falloff));
        }
      }
      if (highlight_mask != nullptr) {
        const auto neighborhood = 1.0 - highlight_mask[index] / 65535.0;
        if (neighborhood < highlight_tone) {
          const auto falloff = 1.0 - neighborhood / highlight_tone;
          tone = 1.0 - std::pow(1.0 - tone, 1.0 / (1.0 + kToneStrength * highlight_amount * falloff * falloff));
        }
      }
      if (midtone != 0.0) {
        // Fixed S curve, zero at black and white, monotonic for |midtone| <= 1.
        tone += 2.0 * midtone * (tone - 0.5) * tone * (1.0 - tone);
      }
      if (tone == original) {
        continue;
      }
      // Luminance moves to the new tone; chroma scales by the luminance ratio
      // raised to Color, so 0 keeps chroma and +100 keeps channel ratios.
      const auto chroma_scale =
          original > 0.0 ? std::pow(std::max(tone / original, kMinimumLuminanceRatio), color) : 1.0;
      for (int channel = 0; channel < 3; ++channel) {
        const auto value = tone + (px[channel] / 255.0 - original) * chroma_scale;
        px[channel] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
      }
    }
  });
  report_stage(progress, ++stage, stages);

  if (!black_clip && !white_clip) {
    return;
  }
  // Black Clip re-anchors the lifted shadows and White Clip the lowered
  // highlights: one merged R+G+B histogram of the result (visible pixels), the
  // clip percentage of samples at each end mapped to 0 or 255.
  std::array<std::uint64_t, 256> histogram{};
  std::uint64_t samples = 0;
  for (std::int32_t y = 0; y < height; ++y) {
    const auto* px = base + static_cast<std::size_t>(y) * stride;
    for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
      if (has_alpha && px[3] == 0) {
        continue;
      }
      ++histogram[px[0]];
      ++histogram[px[1]];
      ++histogram[px[2]];
      samples += 3;
    }
  }
  int black = 0;
  int white = 255;
  if (black_clip) {
    black = clip_level(histogram, samples * static_cast<std::uint64_t>(settings.black_clip_hundredths) / 10000U, 0,
                       255, 1);
  }
  if (white_clip) {
    white = clip_level(histogram, samples * static_cast<std::uint64_t>(settings.white_clip_hundredths) / 10000U, 255,
                       black + 1, -1);
  }
  if ((black == 0 && white == 255) || white <= black + 1) {
    report_stage(progress, stages, stages);
    return;
  }
  std::array<std::uint8_t, 256> lut{};
  const auto span = white - black;
  for (int value = 0; value < 256; ++value) {
    const auto shifted = std::clamp(value - black, 0, span);
    lut[static_cast<std::size_t>(value)] = static_cast<std::uint8_t>((shifted * 255 * 2 + span) / (2 * span));
  }
  for_each_row(width, height, [&](std::int32_t y) {
    auto* px = base + static_cast<std::size_t>(y) * stride;
    for (std::int32_t x = 0; x < width; ++x, px += pixel_bytes) {
      if (has_alpha && px[3] == 0) {
        continue;
      }
      px[0] = lut[px[0]];
      px[1] = lut[px[1]];
      px[2] = lut[px[2]];
    }
  });
  report_stage(progress, stages, stages);
}

}  // namespace patchy
