#pragma once

#include "core/color_range.hpp"
#include "ui/canvas_widget.hpp"

#include <QColor>
#include <QImage>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QBoxLayout;
class QButtonGroup;
class QToolButton;

namespace patchy::ui {

// Pieces shared with Image > Adjustments > Replace Color, which samples and previews
// colors the same way (docs/replace-color.md).

// The preview box: an image fitted into a 240 px square, clickable for sampling like
// the canvas. `clicked` receives the point inside the shown image.
class ColorRangePreview final : public QWidget {
public:
  static constexpr int kBox = 240;

  explicit ColorRangePreview(QWidget* parent);

  std::function<void(QPoint image_point, Qt::KeyboardModifiers modifiers)> clicked;

  void set_image(QImage image);
  [[nodiscard]] const QImage& image() const noexcept { return image_; }
  [[nodiscard]] QRect image_rect() const;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  QImage image_;
};

// How a document maps into the preview box: small documents scale up (fast
// scaling), large ones down (smooth scaling).
struct ColorRangePreviewFit {
  double scale{1.0};
  QSize size;
  Qt::TransformationMode mode{Qt::SmoothTransformation};

  [[nodiscard]] QPoint document_point(QPoint image_point, QSize document_size) const;
};
[[nodiscard]] ColorRangePreviewFit fit_color_range_preview(QSize document_size);

// A row-major gray8 mask as a Grayscale8 image (null when the size does not match).
[[nodiscard]] QImage color_range_gray_image(const std::vector<std::uint8_t>& mask, int width, int height);

// The eyedropper, Add (+) and Subtract (-) buttons, appended to `row`. Object names are
// `<prefix>EyedropperButton`, `<prefix>AddSampleButton`, `<prefix>SubtractSampleButton`.
struct ColorRangeSamplerButtons {
  QToolButton* sample{nullptr};
  QToolButton* add{nullptr};
  QToolButton* subtract{nullptr};
  QButtonGroup* group{nullptr};

  // The checked button's action; Shift adds and Alt subtracts with any of them, as in
  // Photoshop.
  [[nodiscard]] ColorRangeSampleAction action(Qt::KeyboardModifiers modifiers) const;
  void set_enabled(bool enabled) const;
};
[[nodiscard]] ColorRangeSamplerButtons add_color_range_sampler_buttons(QWidget& parent, QBoxLayout* row,
                                                                       const QString& object_prefix);

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
