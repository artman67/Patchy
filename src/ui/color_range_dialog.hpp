#pragma once

#include "core/color_range.hpp"
#include "ui/canvas_widget.hpp"

#include <QColor>
#include <QImage>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QWidget;

namespace patchy::ui {

// Photoshop's Selection Preview menu: how the canvas shows the pending selection
// while the dialog is open. None is Photoshop's default.
enum class ColorRangeCanvasPreview {
  None,
  Grayscale,
  BlackMatte,
  WhiteMatte,
  QuickMask
};

struct ColorRangeDialogInput {
  QImage composite;       // the visible document, RGBA8888 straight alpha, document size
  QImage base_selection;  // Grayscale8 document-size selection; null when nothing is selected
  ColorRangeCombine combine{ColorRangeCombine::Replace};
  QColor initial_sample;  // the first sampled color (the foreground color)
};

struct ColorRangeDialogHooks {
  // Installs the canvas eyedropper; an empty callback removes it.
  std::function<void(std::function<void(const CanvasReadGesture&)>)> set_canvas_sampler;
  // Shows a document-size premultiplied overlay over the canvas; a null image clears it.
  std::function<void(const QImage&)> set_canvas_overlay;
};

struct ColorRangeDialogResult {
  // The final document-size selection (the result already combined with the
  // existing selection), row-major gray8.
  std::vector<std::uint8_t> selection;
};

// The canvas overlay one Selection Preview mode draws for `selection` over `composite`.
[[nodiscard]] QImage color_range_canvas_preview_image(const QImage& composite,
                                                      const std::vector<std::uint8_t>& selection,
                                                      ColorRangeCanvasPreview mode);

// Select > Color Range. Runs non-modally so the canvas stays clickable for sampling;
// the caller holds the preview-dialog edit lock. Select, Fuzziness, Invert and the
// Selection Preview mode persist under tools/colorRange*.
[[nodiscard]] std::optional<ColorRangeDialogResult> request_color_range(QWidget* parent,
                                                                        const ColorRangeDialogInput& input,
                                                                        const ColorRangeDialogHooks& hooks);

}  // namespace patchy::ui
