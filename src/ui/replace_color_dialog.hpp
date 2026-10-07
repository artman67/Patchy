#pragma once

#include "core/replace_color.hpp"
#include "ui/canvas_widget.hpp"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRegion>
#include <QSize>

#include <functional>
#include <optional>

class QWidget;

namespace patchy::ui {

struct ReplaceColorDialogInput {
  QImage layer;           // the active layer's pixels, RGBA8888 straight alpha
  QPoint layer_origin;    // the layer's top-left in document coordinates
  QSize document_size;
  QRegion selection;      // the document selection; empty means the whole layer
  QColor initial_sample;  // the first sampled color (the foreground color)
};

struct ReplaceColorDialogHooks {
  // Installs the canvas eyedropper; an empty callback removes it.
  std::function<void(std::function<void(const CanvasReadGesture&)>)> set_canvas_sampler;
  // Live canvas preview: `enabled` follows the Preview checkbox.
  std::function<void(bool enabled, const ReplaceColorSettings& settings)> preview_changed;
};

// Image > Adjustments > Replace Color (docs/replace-color.md). Runs non-modally so the
// canvas stays clickable for sampling; the caller holds the preview-dialog edit lock.
// The eyedroppers read the layer's own pixels, from the copy taken before any preview.
// Fuzziness persists under tools/replaceColorFuzziness.
[[nodiscard]] std::optional<ReplaceColorSettings> request_replace_color(QWidget* parent,
                                                                        const ReplaceColorDialogInput& input,
                                                                        const ReplaceColorDialogHooks& hooks);

}  // namespace patchy::ui
