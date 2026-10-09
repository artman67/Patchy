#pragma once

#include "core/brush_dynamics.hpp"
#include "core/brush_tip.hpp"

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

#include <memory>
#include <optional>

namespace patchy::ui {

// What a stroke preview paints: a tip (null = the procedural Round, or Square) with the working
// brush's shape, Soft and dynamics.
struct BrushStrokePreviewSpec {
  std::shared_ptr<const patchy::BrushTip> tip;
  bool square{false};
  int softness{0};
  std::optional<double> spacing;  // fraction of size; nullopt = the tip's own (procedural 25%)
  double angle{0.0};
  double roundness{100.0};
  patchy::BrushDynamics dynamics{};
};

// Paints a gentle S-curve stroke with the real stamping engine into a scratch document and
// returns it as black ink with coverage in alpha. brush_size 0 fits the stroke to the height.
// Dynamics use a fixed seed, so a preview never reshuffles between repaints. Shared by the
// Brush Tips manager, the Brushes panel rows and the Brush Settings preview strip.
[[nodiscard]] QImage render_brush_stroke_preview(const BrushStrokePreviewSpec& spec, QSize size,
                                                 int brush_size = 0);

// render_brush_stroke_preview tinted with `ink`, cached by (key, size, ink, brush_size) so
// scrolling and repaints never re-render. The key must change whenever the settings do (the
// panels use the tip identity plus the brush's persisted JSON).
[[nodiscard]] QPixmap brush_stroke_preview_pixmap(const QString& key, const BrushStrokePreviewSpec& spec,
                                                  QSize size, const QColor& ink, int brush_size = 0);

// A thumbnail of the procedural Round or Square tip with its Soft edge, drawn like the library
// tips' thumbnails (brush_tip_thumbnail).
[[nodiscard]] QPixmap procedural_brush_thumbnail(bool square, int softness, int extent);

}  // namespace patchy::ui
