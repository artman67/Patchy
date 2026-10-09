#include "ui/brush_stroke_preview.hpp"

#include "core/document.hpp"
#include "core/pixel_tools.hpp"
#include "ui/brush_tip_library.hpp"

#include <QHash>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <list>
#include <utility>

namespace patchy::ui {

namespace {

// The procedural Round and Square as small bitmap stamps: a preview strip is tiny, and stamping
// every stroke through a tip keeps one path for soft edges, dynamics and the opacity cap.
std::shared_ptr<const patchy::BrushTip> procedural_preview_tip(bool square) {
  static const auto make = [](bool make_square) {
    constexpr std::int32_t kSize = 64;
    auto tip = std::make_shared<patchy::BrushTip>();
    tip->width = kSize;
    tip->height = kSize;
    tip->default_spacing = 0.1;
    tip->mask.resize(static_cast<std::size_t>(kSize) * kSize);
    for (std::int32_t y = 0; y < kSize; ++y) {
      for (std::int32_t x = 0; x < kSize; ++x) {
        const auto dx = static_cast<float>(x) + 0.5F - 32.0F;
        const auto dy = static_cast<float>(y) + 0.5F - 32.0F;
        const auto coverage =
            make_square ? 1.0F : std::clamp(31.5F - std::sqrt(dx * dx + dy * dy), 0.0F, 1.0F);
        tip->mask[static_cast<std::size_t>(y) * kSize + x] =
            static_cast<std::uint8_t>(std::lround(coverage * 255.0F));
      }
    }
    return std::shared_ptr<const patchy::BrushTip>(std::move(tip));
  };
  static const auto round = make(false);
  static const auto square_tip = make(true);
  return square ? square_tip : round;
}

}  // namespace

QImage render_brush_stroke_preview(const BrushStrokePreviewSpec& spec, QSize size, int brush_size) {
  const auto tip = spec.tip != nullptr && !spec.tip->empty() ? spec.tip : procedural_preview_tip(spec.square);
  if (size.width() <= 4 || size.height() <= 4) {
    return {};
  }
  const auto document_width = std::max(64, size.width());
  const auto document_height = std::max(24, size.height());
  patchy::Document document(document_width, document_height, patchy::PixelFormat::rgba8());
  patchy::PixelBuffer pixels(document_width, document_height, patchy::PixelFormat::rgba8());
  pixels.clear(0);
  const auto layer_id = document.add_pixel_layer("Preview", std::move(pixels)).id();

  const auto mips = patchy::build_brush_tip_mips(*tip);
  if (brush_size <= 0) {
    brush_size = std::clamp(std::max(tip->width, tip->height), 8, std::max(8, document_height / 2));
  }
  auto scaled = patchy::make_scaled_brush_tip(mips, brush_size);
  if (scaled.empty()) {
    return {};
  }
  const auto softness = std::clamp(spec.softness, 0, 100);
  if (softness > 0) {
    patchy::soften_scaled_brush_tip(
        scaled, static_cast<int>(std::lround(static_cast<double>(brush_size) * softness / 400.0)));
  }

  patchy::EditOptions options;
  options.primary = patchy::EditColor{0, 0, 0, 255};
  options.brush_size = brush_size;
  options.brush_tip = &scaled;
  // The canvas stamps a dynamics-active Round or Square at 25% spacing (canvas_widget_brush).
  const auto procedural_spacing = spec.dynamics.active() ? 0.25 : tip->default_spacing;
  const auto own_spacing = spec.tip != nullptr && !spec.tip->empty() ? tip->default_spacing : procedural_spacing;
  options.brush_tip_spacing = std::clamp(spec.spacing.value_or(own_spacing), 0.01, 10.0);
  options.brush_angle_degrees = spec.angle;
  options.brush_roundness = static_cast<int>(std::lround(std::clamp(spec.roundness, 1.0, 100.0)));
  options.brush_flip_x = spec.dynamics.tip_flip_x;
  options.brush_flip_y = spec.dynamics.tip_flip_y;
  options.brush_texture_tile = spec.texture_tile.get();
  options.brush_dynamics = spec.dynamics;
  options.brush_dynamics.seed = 1234;  // fixed seed: a stable preview instead of reshuffling per repaint
  // Like the canvas stroke compositor at 100% Flow, overlapping dabs cap at full coverage
  // instead of piling up, so a soft brush previews soft.
  options.stroke_pixel_writer = [](std::int32_t, std::int32_t, std::uint8_t* pixel, std::uint16_t channels,
                                   float coverage, const patchy::EditColor&) {
    if (channels < 4) {
      return false;
    }
    const auto alpha = static_cast<std::uint8_t>(std::lround(std::clamp(coverage, 0.0F, 1.0F) * 255.0F));
    if (alpha <= pixel[3]) {
      return false;
    }
    pixel[0] = 0;
    pixel[1] = 0;
    pixel[2] = 0;
    pixel[3] = alpha;
    return true;
  };

  // A gentle S-curve across the preview, chopped into short segments like real input.
  patchy::BrushTipStrokeState state;
  const auto margin = static_cast<double>(brush_size) / 2.0 + 4.0;
  const auto usable_width = static_cast<double>(document_width) - 2.0 * margin;
  const auto center_y = static_cast<double>(document_height) / 2.0;
  const auto wave_height = std::max(2.0, static_cast<double>(document_height) / 2.0 - margin);
  constexpr int kSegments = 48;
  double previous_x = margin;
  double previous_y = center_y;
  for (int step = 1; step <= kSegments; ++step) {
    const auto t = static_cast<double>(step) / kSegments;
    const auto x = margin + usable_width * t;
    const auto y = center_y - std::sin(t * 2.0 * 3.14159265358979323846) * wave_height;
    (void)patchy::paint_brush_segment(document, layer_id, previous_x, previous_y, x, y, options, false, state);
    previous_x = x;
    previous_y = y;
  }

  const auto* layer = document.find_layer(layer_id);
  if (layer == nullptr) {
    return {};
  }
  const auto& painted = layer->pixels();
  QImage image(painted.width(), painted.height(), QImage::Format_RGBA8888);
  for (std::int32_t y = 0; y < painted.height(); ++y) {
    const auto row = painted.row(y);
    std::copy_n(row.data(), static_cast<std::size_t>(painted.width()) * 4U, image.scanLine(y));
  }
  return image;
}

QPixmap brush_stroke_preview_pixmap(const QString& key, const BrushStrokePreviewSpec& spec, QSize size,
                                    const QColor& ink, int brush_size) {
  // A small LRU: the Brushes panel shows a few dozen rows at once, and each row's stroke only
  // re-renders when its settings key, size or the theme ink changes.
  static constexpr std::size_t kCapacity = 256;
  static std::list<std::pair<QString, QPixmap>> cache;
  const auto full_key = QStringLiteral("%1|%2x%3|%4|%5")
                            .arg(key)
                            .arg(size.width())
                            .arg(size.height())
                            .arg(ink.rgba())
                            .arg(brush_size);
  for (auto it = cache.begin(); it != cache.end(); ++it) {
    if (it->first == full_key) {
      cache.splice(cache.begin(), cache, it);
      return cache.front().second;
    }
  }
  auto image = render_brush_stroke_preview(spec, size, brush_size);
  QPixmap pixmap;
  if (!image.isNull()) {
    // Ink keeps the rendered coverage as alpha.
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), ink);
    painter.end();
    pixmap = QPixmap::fromImage(image);
  }
  cache.emplace_front(full_key, pixmap);
  if (cache.size() > kCapacity) {
    cache.pop_back();
  }
  return pixmap;
}

QPixmap procedural_brush_thumbnail(bool square, int softness, int extent) {
  constexpr std::int32_t kSize = 48;
  patchy::BrushTip tip;
  tip.width = kSize;
  tip.height = kSize;
  tip.mask.resize(static_cast<std::size_t>(kSize) * kSize);
  // Soft fades the outer part of the radius, like the canvas feather reads at thumbnail size.
  const auto radius = kSize / 2.0 - 1.0;
  const auto feather = std::max(1.0, radius * std::clamp(softness, 0, 100) / 100.0);
  for (std::int32_t y = 0; y < kSize; ++y) {
    for (std::int32_t x = 0; x < kSize; ++x) {
      const auto dx = x + 0.5 - kSize / 2.0;
      const auto dy = y + 0.5 - kSize / 2.0;
      const auto distance = square ? std::max(std::abs(dx), std::abs(dy)) : std::hypot(dx, dy);
      const auto coverage = std::clamp((radius - distance) / feather, 0.0, 1.0);
      tip.mask[static_cast<std::size_t>(y) * kSize + x] = static_cast<std::uint8_t>(std::lround(coverage * 255.0));
    }
  }
  return brush_tip_thumbnail(tip, extent);
}

}  // namespace patchy::ui
