// Gradient Map: the New Adjustment Layer flow (create, preview, edit,
// thumbnail), the destructive Image > Adjustments command, and photo
// artifacts for visual review.

#include "ui_test_support.hpp"
#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"

#include "core/adjustment_layer.hpp"
#include "local_psd_fixtures.hpp"
#include "render/compositor.hpp"
#include "test_harness.hpp"
#include "ui/filter_preview_proxy.hpp"
#include "ui/image_document_io.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QDialog>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

namespace {

using namespace patchy::test::ui;

constexpr QColor kForeground(20, 10, 90);
constexpr QColor kBackground(255, 190, 40);

patchy::RgbColor rgb(QColor color) {
  return patchy::RgbColor{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                          static_cast<std::uint8_t>(color.blue())};
}

QColor mapped(bool reverse, std::uint8_t luminance) {
  patchy::GradientMapAdjustment settings;
  settings.gradient = patchy::default_gradient_map_definition(rgb(kForeground), rgb(kBackground));
  settings.reverse = reverse;
  const auto color = patchy::build_gradient_map_lut(settings)[luminance];
  return QColor(color.red, color.green, color.blue);
}

// Runs `drive` on the Gradient Map dialog once the action below opens it. A
// failed CHECK leaves the dialog loop by exception instead of throwing
// across Qt's event dispatch (docs/testing.md).
void drive_gradient_map_dialog(const std::function<void(QDialog&)>& drive) {
  QTimer::singleShot(0, [drive] {
    try {
      QDialog* dialog = nullptr;
      for (auto* widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == QStringLiteral("patchyGradientMapDialog")) {
          dialog = qobject_cast<QDialog*>(widget);
        }
      }
      CHECK(dialog != nullptr);
      drive(*dialog);
    } catch (...) {
      patchy::ui::unwind_non_modal_dialog_loop(std::current_exception());
    }
  });
}

void fill_gray_100_with_duotone_colors(patchy::ui::MainWindow& window, patchy::ui::CanvasWidget* canvas) {
  canvas->set_primary_color(QColor(100, 100, 100));
  use_solid_fill_settings(canvas);
  require_action(window, "layerFillForegroundAction")->trigger();
  QApplication::processEvents();
  canvas->set_primary_color(kForeground);
  canvas->set_secondary_color(kBackground);
}

void save_photo_artifact(const QImage& photo, const std::string& name, const patchy::GradientMapAdjustment& settings) {
  patchy::Document document(photo.width(), photo.height(), patchy::PixelFormat::rgba8());
  document.add_pixel_layer("Photo", patchy::ui::pixels_from_image_rgba(photo));
  patchy::AdjustmentSettings adjustment;
  adjustment.kind = patchy::AdjustmentKind::GradientMap;
  adjustment.gradient_map = settings;
  patchy::Layer layer(document.allocate_layer_id(), "Gradient Map", patchy::LayerKind::Adjustment);
  layer.set_bounds(patchy::Rect::from_size(document.width(), document.height()));
  patchy::configure_adjustment_layer(layer, adjustment);
  document.add_layer(std::move(layer));
  ensure_artifact_dir();
  const auto image = patchy::ui::image_from_pixels(patchy::Compositor{}.flatten_rgb8(document));
  CHECK(image.save(QString::fromStdString("test-artifacts/" + name + ".png")));
}

void ui_gradient_map_adjustment_layer_creates_edits_and_shows_gradient_thumbnail() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  auto* layer_list = window.findChild<QListWidget*>(QStringLiteral("layerList"));
  CHECK(layer_list != nullptr);
  fill_gray_100_with_duotone_colors(window, canvas);

  // A new map runs foreground to background with Photoshop's Perceptual
  // default, and previews live.
  bool saw_preview = false;
  drive_gradient_map_dialog([&](QDialog& dialog) {
    auto* method = dialog.findChild<QComboBox*>(QStringLiteral("gradientMapMethodCombo"));
    auto* reverse = dialog.findChild<QCheckBox*>(QStringLiteral("gradientMapReverseCheck"));
    CHECK(method != nullptr && reverse != nullptr);
    CHECK(method->currentData().toInt() == static_cast<int>(patchy::GradientInterpolationMethod::Perceptual));
    CHECK(!reverse->isChecked());
    CHECK(dialog.findChild<QWidget*>(QStringLiteral("gradientMapStopsEditor"))->isVisible());
    process_events_for(150);
    saw_preview = color_close(canvas_pixel(*canvas, QPoint(70, 70)), mapped(false, 100), 6);
    save_widget_artifact("gradient-map-dialog", dialog);
    dialog.accept();
  });
  require_action(window, "layerNewGradientMapAdjustmentAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_preview);
  CHECK(layer_list->item(0) != nullptr);
  CHECK(layer_list->item(0)->text() == QStringLiteral("Gradient Map"));
  CHECK(color_close(canvas_pixel(*canvas, QPoint(70, 70)), mapped(false, 100), 6));
  const auto& document = patchy::ui::MainWindowTestAccess::document(window);
  const auto* layer = document.find_layer(*document.active_layer_id());
  CHECK(layer != nullptr && layer->kind() == patchy::LayerKind::Adjustment);
  const auto settings = patchy::adjustment_settings_from_layer(*layer);
  CHECK(settings.has_value() && settings->kind == patchy::AdjustmentKind::GradientMap);
  CHECK(settings->gradient_map.gradient.color_stops.front().color.blue == kForeground.blue());

  // The row thumbnail shows the mapped gradient: dark blue left, gold right.
  auto* row = layer_list->itemWidget(layer_list->item(0));
  CHECK(row != nullptr);
  auto* thumbnail = row->findChild<QLabel*>(QStringLiteral("layerContentThumbnail"));
  CHECK(thumbnail != nullptr);
  const auto image = thumbnail->pixmap(Qt::ReturnByValue).toImage();
  const auto left = image.pixelColor(image.width() * 9 / 28, image.height() / 2);
  const auto right = image.pixelColor(image.width() * 19 / 28, image.height() / 2);
  CHECK(left.blue() > left.red());
  CHECK(right.red() > right.blue());

  // Editing reopens the stored settings; Reverse flips the mapping.
  drive_gradient_map_dialog([&](QDialog& dialog) {
    auto* reverse = dialog.findChild<QCheckBox*>(QStringLiteral("gradientMapReverseCheck"));
    CHECK(reverse != nullptr && !reverse->isChecked());
    reverse->setChecked(true);
    process_events_for(120);
    dialog.accept();
  });
  require_action(window, "layerEditAdjustmentAction")->trigger();
  QApplication::processEvents();
  CHECK(color_close(canvas_pixel(*canvas, QPoint(70, 70)), mapped(true, 100), 6));

  // The quick picker replaces the gradient with a library preset and keeps
  // the options.
  drive_gradient_map_dialog([&](QDialog& dialog) {
    auto* preset_button = dialog.findChild<QPushButton*>(QStringLiteral("gradientMapPresetButton"));
    CHECK(preset_button != nullptr && preset_button->isEnabled());
    preset_button->click();
    QApplication::processEvents();
    auto* popup = preset_button->findChild<QFrame*>(QStringLiteral("gradientMapPresetButtonPopup"));
    CHECK(popup != nullptr && popup->isVisible());
    auto* tree = popup->findChild<QTreeWidget*>(QStringLiteral("gradientMapPresetButtonTree"));
    CHECK(tree != nullptr);
    const auto found = tree->findItems(QStringLiteral("Cyanotype"), Qt::MatchExactly | Qt::MatchRecursive);
    CHECK(found.size() == 1);
    tree->scrollToItem(found.front());
    QApplication::processEvents();
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(found.front()).center());
    process_events_for(120);
    dialog.accept();
  });
  require_action(window, "layerEditAdjustmentAction")->trigger();
  QApplication::processEvents();
  const auto* edited = document.find_layer(*document.active_layer_id());
  const auto cyanotype = patchy::adjustment_settings_from_layer(*edited);
  CHECK(cyanotype.has_value());
  CHECK(cyanotype->gradient_map.gradient.color_stops.size() == 3U);
  CHECK(cyanotype->gradient_map.gradient.color_stops.front().color.blue == 0x33);
  CHECK(cyanotype->gradient_map.reverse);
  const auto cyanotype_color = cyanotype->gradient_map.lut->at(100);
  CHECK(color_close(canvas_pixel(*canvas, QPoint(70, 70)),
                    QColor(cyanotype_color.red, cyanotype_color.green, cyanotype_color.blue), 6));

  // Photo-like artifacts for visual review: a two-color duotone and a
  // multi-stop map over the committed CC0 city photo.
  QImage photo(QString::fromStdString(
      patchy::test::committed_format_fixture_path("readme", "san_francisco_cityscape_cc0.jpg").string()));
  CHECK(!photo.isNull());
  photo = photo.scaledToWidth(640, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
  patchy::GradientMapAdjustment duotone;
  duotone.gradient = patchy::default_gradient_map_definition({38, 12, 70}, {255, 196, 140});
  save_photo_artifact(photo, "gradient-map-photo-duotone", duotone);
  patchy::GradientMapAdjustment multistop;
  multistop.gradient = patchy::default_gradient_map_definition({0, 0, 0}, {252, 255, 164});
  multistop.gradient.color_stops.insert(multistop.gradient.color_stops.begin() + 1,
                                        {patchy::GradientColorStop{0.35F, {120, 28, 109}},
                                         patchy::GradientColorStop{0.7F, {237, 105, 37}}});
  save_photo_artifact(photo, "gradient-map-photo-multistop", multistop);
}

void ui_image_gradient_map_command_rewrites_layer_pixels_and_undoes() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  fill_gray_100_with_duotone_colors(window, canvas);
  auto& document = patchy::ui::MainWindowTestAccess::document(window);
  const auto layer_count = document.layers().size();
  const auto layer_id = *document.active_layer_id();
  const auto pixel_at = [&document, layer_id] {
    const auto* px = std::as_const(*document.find_layer(layer_id)).pixels().pixel(10, 10);
    return QColor(px[0], px[1], px[2]);
  };

  drive_gradient_map_dialog([&](QDialog& dialog) {
    auto* reverse = dialog.findChild<QCheckBox*>(QStringLiteral("gradientMapReverseCheck"));
    CHECK(reverse != nullptr);
    reverse->setChecked(true);
    process_events_for(120);
    dialog.accept();
  });
  require_action(window, "imageAdjustGradientMapAction")->trigger();
  QApplication::processEvents();

  // The pixels themselves change (no adjustment layer is added), exactly as
  // the adjustment layer would render them.
  CHECK(document.layers().size() == layer_count);
  CHECK(pixel_at() == mapped(true, 100));
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(pixel_at() == QColor(100, 100, 100));
}

}  // namespace

std::vector<patchy::test::TestCase> gradient_map_tests() {
  return {
      {"ui_gradient_map_adjustment_layer_creates_edits_and_shows_gradient_thumbnail",
       ui_gradient_map_adjustment_layer_creates_edits_and_shows_gradient_thumbnail},
      {"ui_image_gradient_map_command_rewrites_layer_pixels_and_undoes",
       ui_image_gradient_map_command_rewrites_layer_pixels_and_undoes},
  };
}
