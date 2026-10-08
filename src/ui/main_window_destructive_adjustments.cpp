// MainWindow destructive adjustment dialogs, split out of
// main_window_adjustments.cpp: the Levels / Curves / Hue-Saturation /
// Color Balance dialogs that rewrite layer pixels in place. Each dialog is a
// DestructiveAdjustmentSpec (its dialog call and pixel render) run by
// run_destructive_adjustment, which owns the layer guards, the async live
// preview (make_destructive_adjustment_preview_state in main_window_shared),
// the restore on close, and the cancellable apply.


#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "core/layer_metadata.hpp"
#include "core/layer_render_utils.hpp"
#include "core/layer_tree.hpp"
#include "core/palette.hpp"
#include "core/palette_presets.hpp"
#include "core/pixel_tools.hpp"
#include "formats/palette_io.hpp"
#include "filters/builtin_filters.hpp"
#include "filters/smart_filter_recipe_mapping.hpp"
#include "filters/smart_filter_renderer.hpp"
#include "formats/bmp_document_io.hpp"
#include "plugins/legacy_photoshop_adapter.hpp"
#include "psd/psd_document_io.hpp"
#include "psd/psd_filter_effects.hpp"
#include "psd/psd_smart_objects.hpp"
#include "ui/action_icons.hpp"
#include "ui/background_workers.hpp"
#include "ui/app_settings.hpp"
#include "render/compositor.hpp"
#include "ui/blend_mode_ui.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/liquify_dialog.hpp"
#include "ui/localization.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/print_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/update_checker.hpp"
#include "ui/visual_filter_gallery_dialog.hpp"
#include "ui/zoom_status_bar.hpp"
#include "support/string_utils.hpp"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLayout>
#include <QResizeEvent>
#include <QIcon>
#include <QImageReader>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QKeySequence>
#include <QListWidget>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygon>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QRegion>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QScopeGuard>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTextOption>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTransform>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <tchar.h>
#include <tpcshrd.h>
#endif

// Icon resources live in the static patchy_ui library; force registration before first use.
int qInitResources_icons();


namespace patchy::ui {

// The layer a destructive adjustment edits, captured after the rasterize
// prompt. Every preview and the final apply start from original_pixels.
struct DestructiveAdjustmentTarget {
  LayerId layer_id{};
  Rect bounds;
  QRegion selection;
  std::shared_ptr<const PixelBuffer> original_pixels;
};

// One destructive Image > Adjustments dialog for run_destructive_adjustment.
template <typename Settings>
struct DestructiveAdjustmentSpec {
  // Rasterize prompt, progress text, undo label and status messages.
  QString display_name;
  // Status message when the dialog or the apply is cancelled.
  QString cancelled_message;
  // Opens the dialog, reporting every settings change to preview_changed;
  // returns the accepted settings, or nothing when cancelled.
  std::function<std::optional<Settings>(const DestructiveAdjustmentTarget& target,
                                        std::function<void(bool, const Settings&)> preview_changed)>
      request_settings;
  // Renders into `pixels`, a copy of the original. Previews call it on a
  // worker thread without progress, so it must only use its arguments.
  std::function<void(PixelBuffer& pixels, Rect bounds, const QRegion& selection, const Settings& settings,
                     const FilterProgress* progress)>
      render;
  // False for settings that leave the pixels alone: the preview shows the
  // original and the apply skips rendering. Empty means every setting has an
  // effect.
  std::function<bool(const Settings&)> has_effect;
  // Replaces the in-place apply with another commit (Hue/Saturation and Color
  // Balance add an adjustment layer). Such dialogs skip the pixel-lock check
  // because the accepted result does not rewrite the layer's pixels.
  std::function<void(const Settings&)> apply_accepted;
  // Previews and the close restore the whole original Layer instead of only
  // its pixels (Curves, as its pre-launcher worker did).
  bool restore_whole_layer{false};
};

// Guards the active layer, previews the dialog's settings live on it, restores
// it when the dialog closes, then applies the accepted settings in place as
// one undo step behind a cancellable progress dialog.
template <typename Settings>
void MainWindow::run_destructive_adjustment(const DestructiveAdjustmentSpec<Settings>& spec) {
  auto& doc = document();
  select_only_layer_if_none_active();
  const auto active = doc.active_layer_id();
  if (!active.has_value()) {
    return;
  }
  auto* layer = doc.find_layer(*active);
  if (!editable_rgb8_layer(layer)) {
    show_status_error(tr("Select an editable RGB pixel layer"));
    return;
  }
  if (layer_is_smart_object(*layer)) {
    show_status_error(tr(
        "Rasterize the Smart Object before applying destructive filters or adjustments"));
    return;
  }
  if (!spec.apply_accepted && layer_id_locks_image_pixels(*active)) {
    show_status_error(tr("Layer pixels are locked."));
    return;
  }
  if (!prompt_rasterize_procedural_layer(*active, spec.display_name, false)) {
    return;
  }
  layer = doc.find_layer(*active);
  if (!editable_rgb8_layer(layer)) {
    show_status_error(tr("Select an editable RGB pixel layer"));
    return;
  }
  const auto active_id = *active;
  const auto bounds = layer->bounds();
  auto original_layer = std::make_shared<const Layer>(*layer);
  auto original_pixels = std::make_shared<const PixelBuffer>(original_layer->pixels());
  const auto selection = canvas_->selected_document_region();
  const auto restore_original = [original_layer, original_pixels, bounds,
                                 whole_layer = spec.restore_whole_layer](Layer& target) {
    if (whole_layer) {
      target = *original_layer;
    } else {
      set_layer_pixels_preserving_origin(target, *original_pixels, bounds);
    }
  };
  DestructiveAdjustmentPreviewHooks preview_hooks;
  preview_hooks.original_pixels = original_pixels;
  preview_hooks.restore_identity = [this, active_id, bounds, restore_original] {
    if (auto* preview_layer = document().find_layer(active_id); preview_layer != nullptr) {
      restore_original(*preview_layer);
      if (canvas_ != nullptr) {
        canvas_->document_changed(to_qrect(bounds));
      }
    }
  };
  preview_hooks.apply_result = [window = QPointer<MainWindow>(this), active_id,
                                bounds](PixelBuffer result) {
    if (window == nullptr) {
      return;
    }
    if (auto* preview_layer = window->document().find_layer(active_id); preview_layer != nullptr) {
      set_layer_pixels_preserving_origin(*preview_layer, std::move(result), bounds);
      if (window->canvas_ != nullptr) {
        window->canvas_->document_changed(to_qrect(bounds));
      }
    }
  };
  preview_hooks.preview_render_active = [window = QPointer<MainWindow>(this)](bool active) {
    if (window != nullptr && window->canvas_ != nullptr) {
      if (active) {
        window->canvas_->begin_preview_render();
      } else {
        window->canvas_->end_preview_render();
      }
    }
  };
  auto preview_state = make_destructive_adjustment_preview_state(std::move(preview_hooks));
  const auto preview_changed = [preview_state, bounds, selection, render = spec.render,
                                has_effect = spec.has_effect](bool enabled, const Settings& settings) {
    const auto identity = !enabled || (has_effect && !has_effect(settings));
    DestructiveAdjustmentPreviewRequest request;
    request.identity = identity;
    if (!identity) {
      request.render = [render, bounds, selection, settings](PixelBuffer& pixels) {
        render(pixels, bounds, selection, settings, nullptr);
      };
    }
    enqueue_async_pixel_preview(preview_state, std::move(request), identity);
  };

  auto preview_edit_lock = lock_preview_dialog_edits();
  auto preview_cleanup = qScopeGuard([this, &doc, preview_state, original_layer] {
    close_async_pixel_preview(preview_state);
    if (auto* target = doc.find_layer(original_layer->id()); target != nullptr) {
      *target = *original_layer;
      canvas_->document_changed();
    }
  });
  const auto settings =
      spec.request_settings(DestructiveAdjustmentTarget{active_id, bounds, selection, original_pixels},
                            preview_changed);
  close_async_pixel_preview(preview_state);
  layer = doc.find_layer(active_id);
  if (layer == nullptr) {
    return;
  }
  restore_original(*layer);
  canvas_->document_changed(to_qrect(bounds));
  preview_cleanup.dismiss();
  preview_edit_lock.release();
  if (!settings.has_value()) {
    statusBar()->showMessage(spec.cancelled_message);
    return;
  }
  if (spec.apply_accepted) {
    spec.apply_accepted(*settings);
    return;
  }

  const auto& display_name = spec.display_name;
  auto final_pixels = *original_pixels;
  if (!spec.has_effect || spec.has_effect(*settings)) {
    if (canvas_ != nullptr) {
      canvas_->begin_processing_operation();
    }
    const auto finish_processing = qScopeGuard([this] {
      if (canvas_ != nullptr) {
        canvas_->end_processing_operation();
      }
    });
    QProgressDialog progress(tr("Applying %1...").arg(display_name), tr("Cancel"), 0, 100, this);
    progress.setObjectName(QStringLiteral("adjustmentProgressDialog"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(kFilterProgressMinimumDurationMs);
    remember_dialog_position(progress);
    progress.setValue(0);
    try {
      run_filter_compute_with_progress(
          progress,
          [display_name](const QString& detail) { return tr("Applying %1...\n%2").arg(display_name, detail); },
          [this] {
            if (canvas_ != nullptr) {
              canvas_->tick_processing_operation();
            }
          },
          [&](FilterProgress& filter_progress) {
            spec.render(final_pixels, bounds, selection, *settings, &filter_progress);
          });
      progress.setValue(100);
    } catch (const FilterCancelled&) {
      // The layer already holds the original pixels again (restored above).
      statusBar()->showMessage(spec.cancelled_message);
      return;
    }
  }
  if (pixel_buffers_equal(final_pixels, *original_pixels)) {
    statusBar()->showMessage(tr("%1 made no changes").arg(display_name));
    return;
  }
  push_undo_snapshot(display_name);
  layer = doc.find_layer(active_id);
  if (layer == nullptr) {
    return;
  }
  set_layer_pixels_preserving_origin(*layer, std::move(final_pixels), bounds);
  canvas_->document_changed(to_qrect(bounds));
  statusBar()->showMessage(tr("Applied %1").arg(display_name));
}

namespace {

bool levels_settings_have_effect(LevelsSettings settings) {
  AdjustmentSettings adjustment;
  adjustment.kind = AdjustmentKind::Levels;
  adjustment.levels = sanitized_levels_adjustment(settings);
  return adjustment_has_effect(adjustment);
}

bool curves_settings_have_effect(const CurvesSettings& curves) {
  AdjustmentSettings adjustment;
  adjustment.kind = AdjustmentKind::Curves;
  adjustment.curves = curves;
  return adjustment_has_effect(adjustment);
}

}  // namespace

void MainWindow::levels_dialog() {
  DestructiveAdjustmentSpec<LevelsSettings> spec;
  spec.display_name = tr("Levels");
  spec.cancelled_message = tr("Cancelled Levels");
  spec.request_settings = [this](const DestructiveAdjustmentTarget& target, auto preview_changed) {
    return request_levels_settings(this, std::move(preview_changed), {}, target.original_pixels.get());
  };
  spec.render = apply_levels_to_pixels;
  spec.has_effect = levels_settings_have_effect;
  run_destructive_adjustment(spec);
}

void MainWindow::curves_dialog() {
  DestructiveAdjustmentSpec<CurvesSettings> spec;
  spec.display_name = tr("Curves");
  spec.cancelled_message = tr("Cancelled Curves");
  spec.request_settings = [this](const DestructiveAdjustmentTarget& target, auto preview_changed) {
    const auto histograms = curves_histograms_from_pixels(target.original_pixels.get());
    // The canvas samplers read the original pixels, never the preview.
    const auto hooks = curves_canvas_hooks(canvas_, [this, target](QPoint point) {
      const auto image = qimage_from_document_rect_with_layer_pixels(
          std::as_const(document()), QRect(point, QSize(1, 1)), true, target.layer_id, *target.original_pixels,
          target.bounds);
      return image.isNull() ? QColor{} : image.pixelColor(0, 0);
    });
    return request_curves_settings(this, std::move(preview_changed), {}, histograms, hooks);
  };
  spec.render = apply_curves_to_pixels;
  spec.has_effect = curves_settings_have_effect;
  spec.restore_whole_layer = true;
  run_destructive_adjustment(spec);
}

void MainWindow::hue_saturation_dialog() {
  DestructiveAdjustmentSpec<HueSaturationSettings> spec;
  spec.display_name = tr("Hue/Saturation");
  spec.cancelled_message = tr("Cancelled Hue/Saturation");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_hue_saturation_settings(this, std::move(preview_changed));
  };
  spec.render = apply_hue_saturation_to_pixels;
  spec.has_effect = [](const HueSaturationSettings& settings) {
    return settings.colorize || settings.hue_shift != 0 || settings.saturation_delta != 0 ||
           settings.lightness_delta != 0;
  };
  spec.apply_accepted = [this](const HueSaturationSettings& settings) {
    apply_hue_saturation_adjustment(settings);
  };
  run_destructive_adjustment(spec);
}

void MainWindow::color_balance_dialog() {
  DestructiveAdjustmentSpec<ColorBalanceSettings> spec;
  spec.display_name = tr("Color Balance");
  spec.cancelled_message = tr("Cancelled Color Balance");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_color_balance_settings(this, std::move(preview_changed));
  };
  spec.render = apply_color_balance_to_pixels;
  spec.has_effect = [](const ColorBalanceSettings& settings) {
    return !(settings.cyan_red == 0 && settings.magenta_green == 0 && settings.yellow_blue == 0);
  };
  spec.apply_accepted = [this](const ColorBalanceSettings& settings) {
    apply_color_balance_adjustment(settings.cyan_red, settings.magenta_green, settings.yellow_blue);
  };
  run_destructive_adjustment(spec);
}

}  // namespace patchy::ui
