#pragma once

#include "core/pixel_buffer.hpp"
#include "filters/filter_registry.hpp"

namespace patchy {

// Image > Adjustments > Shadows/Highlights (docs/shadows-highlights.md owns the
// formulas). Values use Photoshop's dialog units; the clip percentages are
// stored in hundredths of a percent (1 = 0.01 %).
//
// Legal boundary (docs/legal-constraints.md, Filters): one isotropic Gaussian
// blur of luminance per tonal range and fixed per-pixel formulas driven only by
// the user's settings. No histograms beyond the clip scan's one merged
// whole-layer histogram, no blocks or tiles, no optimization, no edge-aware or
// pyramid filtering, no automatically chosen parameters.

inline constexpr int kShadowsHighlightsMaxRadius = 2500;
inline constexpr int kShadowsHighlightsMaxClipHundredths = 5000;

struct ShadowsHighlightsRange {
  int amount{0};   // 0..100 %
  int tone{50};    // 0..100 %, how far up (or down) the tonal scale the range reaches
  int radius{30};  // 0..2500 px, the Gaussian sigma of the luminance neighborhood
};

struct ShadowsHighlightsSettings {
  ShadowsHighlightsRange shadows{35, 50, 30};
  ShadowsHighlightsRange highlights{0, 50, 30};
  int color_correction{20};     // -100..100
  int midtone_contrast{0};      // -100..100
  int black_clip_hundredths{1};  // 0..5000
  int white_clip_hundredths{1};  // 0..5000
};

[[nodiscard]] ShadowsHighlightsSettings clamp_shadows_highlights(ShadowsHighlightsSettings settings);

// False exactly when apply_shadows_highlights leaves every byte unchanged:
// both ranges have a zero Amount or Tone and Midtone Contrast is 0.
[[nodiscard]] bool shadows_highlights_has_effect(const ShadowsHighlightsSettings& settings);

// Rewrites the color channels of an 8-bit buffer with 3 or 4 channels in place;
// alpha and fully transparent pixels are untouched. Throws FilterCancelled when
// the progress sink asks to stop (checked between stages).
void apply_shadows_highlights(PixelBuffer& pixels, const ShadowsHighlightsSettings& settings,
                              const FilterProgress* progress = nullptr);

}  // namespace patchy
