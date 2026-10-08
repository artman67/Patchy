// MainWindow destructive adjustment dialogs, split out of
// main_window_adjustments.cpp: the Levels / Curves / Hue-Saturation /
// Color Balance / Gradient Map / Vibrance / Selective Color / Black & White /
// Channel Mixer / Photo Filter / Match Color / Shadows-Highlights / Color
// Lookup / Replace Color dialogs that rewrite layer pixels in place. Each
// dialog is a DestructiveAdjustmentSpec (its dialog call and pixel render) run
// by run_destructive_adjustment, which owns the layer guards, the async live
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
#include "ui/match_color_dialog.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/print_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/replace_color_dialog.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/tool_cursors.hpp"
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
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
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
  // Selects a document's only layer when none is active, as the Image >
  // Adjustments commands do; Gradient Map never did.
  bool select_only_layer_if_none_active{true};
};

// Guards the active layer, previews the dialog's settings live on it, restores
// it when the dialog closes, then applies the accepted settings in place as
// one undo step behind a cancellable progress dialog.
template <typename Settings>
void MainWindow::run_destructive_adjustment(const DestructiveAdjustmentSpec<Settings>& spec) {
  auto& doc = document();
  if (spec.select_only_layer_if_none_active) {
    select_only_layer_if_none_active();
  }
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

// Per-pixel 0/255 coverage of `region` (document coordinates) over a buffer
// placed at `bounds`, for restricting Match Color statistics to a selection.
std::vector<std::uint8_t> region_coverage(const QRegion& region, Rect bounds) {
  std::vector<std::uint8_t> coverage(static_cast<std::size_t>(std::max(0, bounds.width)) *
                                         static_cast<std::size_t>(std::max(0, bounds.height)),
                                     0);
  for (const auto& rect : region.intersected(QRect(bounds.x, bounds.y, bounds.width, bounds.height))) {
    for (auto y = rect.top(); y <= rect.bottom(); ++y) {
      auto* row = coverage.data() + static_cast<std::size_t>(y - bounds.y) * static_cast<std::size_t>(bounds.width);
      std::fill(row + (rect.left() - bounds.x), row + (rect.right() - bounds.x + 1), std::uint8_t{255});
    }
  }
  return coverage;
}

// Layers Match Color can read statistics from, top first as the Layers panel
// lists them: everything that carries pixels (no groups or adjustment layers).
void collect_match_color_source_layers(const std::vector<Layer>& layers, std::vector<const Layer*>& out) {
  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    if (it->kind() == LayerKind::Group) {
      collect_match_color_source_layers(it->children(), out);
    } else if (it->kind() != LayerKind::Adjustment && !it->pixels().empty()) {
      out.push_back(&*it);
    }
  }
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

void MainWindow::gradient_map_dialog() {
  DestructiveAdjustmentSpec<GradientMapAdjustment> spec;
  spec.display_name = tr("Gradient Map");
  spec.cancelled_message = tr("Cancelled Gradient Map");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_gradient_map(std::move(preview_changed));
  };
  spec.render = apply_gradient_map_to_pixels;
  spec.select_only_layer_if_none_active = false;
  run_destructive_adjustment(spec);
}

void MainWindow::vibrance_dialog() {
  DestructiveAdjustmentSpec<VibranceSettings> spec;
  spec.display_name = tr("Vibrance");
  spec.cancelled_message = tr("Cancelled Vibrance");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_vibrance_settings(this, std::move(preview_changed));
  };
  spec.render = apply_vibrance_to_pixels;
  spec.has_effect = [](const VibranceSettings& settings) {
    const auto clamped = clamp_vibrance(settings);
    return clamped.vibrance != 0 || clamped.saturation != 0;
  };
  run_destructive_adjustment(spec);
}

void MainWindow::selective_color_dialog() {
  DestructiveAdjustmentSpec<SelectiveColorSettings> spec;
  spec.display_name = tr("Selective Color");
  spec.cancelled_message = tr("Cancelled Selective Color");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_selective_color_settings(this, std::move(preview_changed));
  };
  spec.render = apply_selective_color_to_pixels;
  spec.has_effect = [](const SelectiveColorSettings& settings) { return settings.has_effect(); };
  run_destructive_adjustment(spec);
}

void MainWindow::black_white_dialog() {
  DestructiveAdjustmentSpec<BlackWhiteSettings> spec;
  spec.display_name = tr("Black & White");
  spec.cancelled_message = tr("Cancelled Black & White");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_black_white_settings(this, std::move(preview_changed));
  };
  // No has_effect: every setting removes the color, so only an unchecked
  // Preview is the identity.
  spec.render = apply_black_white_to_pixels;
  run_destructive_adjustment(spec);
}

void MainWindow::channel_mixer_dialog() {
  DestructiveAdjustmentSpec<ChannelMixerSettings> spec;
  spec.display_name = tr("Channel Mixer");
  spec.cancelled_message = tr("Cancelled Channel Mixer");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_channel_mixer_settings(this, std::move(preview_changed));
  };
  spec.render = apply_channel_mixer_to_pixels;
  spec.has_effect = channel_mixer_has_effect;
  run_destructive_adjustment(spec);
}

void MainWindow::photo_filter_dialog() {
  DestructiveAdjustmentSpec<PhotoFilterSettings> spec;
  spec.display_name = tr("Photo Filter");
  spec.cancelled_message = tr("Cancelled Photo Filter");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_photo_filter_settings(this, std::move(preview_changed));
  };
  spec.render = apply_photo_filter_to_pixels;
  spec.has_effect = [](const PhotoFilterSettings& settings) {
    AdjustmentSettings adjustment;
    adjustment.kind = AdjustmentKind::PhotoFilter;
    adjustment.photo_filter = settings;
    return adjustment_has_effect(adjustment);
  };
  run_destructive_adjustment(spec);
}

namespace {

// Match Color's dialog settings resolved on the UI thread, because the
// statistics read other open documents: the color map and the region it
// applies to.
struct ResolvedMatchColor {
  MatchColorTransform transform;
  QRegion apply_region;
};

}  // namespace

void MainWindow::match_color_dialog() {
  DestructiveAdjustmentSpec<ResolvedMatchColor> spec;
  spec.display_name = tr("Match Color");
  spec.cancelled_message = tr("Cancelled Match Color");
  spec.request_settings = [this](const DestructiveAdjustmentTarget& target,
                                 auto preview_changed) -> std::optional<ResolvedMatchColor> {
    const auto active_id = target.layer_id;
    const auto bounds = target.bounds;
    const auto& selection = target.selection;
    const auto& original_pixels = target.original_pixels;
    const auto target_session_id = session().session_id;
    const auto* layer = std::as_const(document()).find_layer(active_id);

    // Every open document is a possible source, the target's own included
    // (Photoshop matches two layers of one image that way). Sessions and layers
    // are held by id and looked up again when statistics are computed.
    struct SourceEntry {
      std::int64_t session_id{0};
      std::vector<LayerId> layer_ids;
    };
    std::vector<SourceEntry> source_entries;
    MatchColorDialogSpec dialog_spec;
    dialog_spec.target_description =
        tr("Target: %1 (%2, RGB/8)").arg(session().title, QString::fromStdString(layer->name()));
    dialog_spec.target_has_selection = !selection.isEmpty();
    for (const auto& candidate : sessions_) {
      if (candidate == nullptr) {
        continue;
      }
      const auto& candidate_document = std::as_const(candidate->document);
      std::vector<const Layer*> layers;
      collect_match_color_source_layers(candidate_document.layers(), layers);
      SourceEntry entry{candidate->session_id, {}};
      MatchColorSourceDocument source;
      source.title = candidate->title;
      for (const auto* source_layer : layers) {
        if (candidate_document.active_layer_id() == source_layer->id()) {
          source.default_layer = static_cast<int>(entry.layer_ids.size());
        }
        entry.layer_ids.push_back(source_layer->id());
        source.layer_names.push_back(QString::fromStdString(source_layer->name()));
      }
      source.has_selection = candidate->canvas != nullptr && candidate->canvas->has_selection();
      source_entries.push_back(std::move(entry));
      dialog_spec.sources.push_back(std::move(source));
    }

    // Statistics depend only on the Image Statistics choices, so each choice is
    // computed once on this thread and reused by every preview and the final apply.
    // The target layer always contributes its original pixels, never the preview.
    using StatisticsKey = std::tuple<int, int, bool>;
    auto statistics_cache = std::make_shared<std::map<StatisticsKey, MatchColorStatistics>>();
    const auto target_statistics = [original_pixels, bounds, selection,
                                    statistics_cache](bool use_selection) -> MatchColorStatistics {
      const StatisticsKey key{-2, 0, use_selection};
      if (const auto found = statistics_cache->find(key); found != statistics_cache->end()) {
        return found->second;
      }
      std::vector<std::uint8_t> coverage;
      if (use_selection) {
        coverage = region_coverage(selection, bounds);
      }
      const auto statistics =
          match_color_statistics(*original_pixels, coverage.empty() ? nullptr : coverage.data());
      statistics_cache->emplace(key, statistics);
      return statistics;
    };
    const auto source_statistics = [this, source_entries, active_id, original_pixels, bounds, target_session_id,
                                    statistics_cache](const MatchColorDialogSettings& settings)
        -> std::optional<MatchColorStatistics> {
      if (settings.source < 0 || static_cast<std::size_t>(settings.source) >= source_entries.size()) {
        return std::nullopt;
      }
      const StatisticsKey key{settings.source, settings.source_layer, settings.use_source_selection};
      if (const auto found = statistics_cache->find(key); found != statistics_cache->end()) {
        return found->second;
      }
      const auto& entry = source_entries[static_cast<std::size_t>(settings.source)];
      const DocumentSession* source_session = nullptr;
      for (const auto& candidate : sessions_) {
        if (candidate != nullptr && candidate->session_id == entry.session_id) {
          source_session = candidate.get();
        }
      }
      if (source_session == nullptr) {
        return std::nullopt;
      }
      const auto& source_document = std::as_const(source_session->document);
      const auto same_document = entry.session_id == target_session_id;
      PixelBuffer pixels;
      Rect pixel_bounds;
      std::vector<std::uint8_t> coverage;
      if (settings.source_layer < 0) {
        // Merged: the source composite, weighted by its coverage.
        if (same_document) {
          auto restored = source_document;
          if (auto* restored_layer = restored.find_layer(active_id); restored_layer != nullptr) {
            set_layer_pixels_preserving_origin(*restored_layer, *original_pixels, bounds);
          }
          pixels = Compositor{}.flatten_rgb8(restored, &coverage);
        } else {
          pixels = Compositor{}.flatten_rgb8(source_document, &coverage);
        }
        pixel_bounds = Rect{0, 0, pixels.width(), pixels.height()};
      } else if (static_cast<std::size_t>(settings.source_layer) < entry.layer_ids.size()) {
        const auto layer_id = entry.layer_ids[static_cast<std::size_t>(settings.source_layer)];
        if (same_document && layer_id == active_id) {
          pixels = *original_pixels;
          pixel_bounds = bounds;
        } else if (const auto* source_layer = source_document.find_layer(layer_id); source_layer != nullptr) {
          pixels = source_layer->pixels();
          pixel_bounds = source_layer->bounds();
        }
      }
      if (settings.use_source_selection && source_session->canvas != nullptr) {
        auto mask = region_coverage(source_session->canvas->selected_document_region(), pixel_bounds);
        if (coverage.size() == mask.size()) {
          for (std::size_t index = 0; index < mask.size(); ++index) {
            mask[index] = static_cast<std::uint8_t>(mask[index] == 0 ? 0 : coverage[index]);
          }
        }
        coverage = std::move(mask);
      }
      if (!coverage.empty() &&
          coverage.size() != static_cast<std::size_t>(pixels.width()) * static_cast<std::size_t>(pixels.height())) {
        coverage.clear();
      }
      const auto statistics = match_color_statistics(pixels, coverage.empty() ? nullptr : coverage.data());
      statistics_cache->emplace(key, statistics);
      return statistics;
    };
    const auto resolve = [target_statistics, source_statistics,
                          selection](const MatchColorDialogSettings& settings) {
      const auto target_stats = target_statistics(settings.use_target_selection);
      const auto source = source_statistics(settings);
      return ResolvedMatchColor{
          make_match_color_transform(target_stats, source.has_value() ? &*source : nullptr, settings.options),
          settings.ignore_selection ? QRegion() : selection};
    };

    const auto settings = request_match_color_settings(
        this, dialog_spec,
        [&resolve, preview_changed](bool enabled, const MatchColorDialogSettings& dialog_settings) {
          preview_changed(enabled, enabled ? resolve(dialog_settings) : ResolvedMatchColor{});
        });
    if (!settings.has_value()) {
      return std::nullopt;
    }
    return resolve(*settings);
  };
  spec.render = [](PixelBuffer& pixels, Rect bounds, const QRegion&, const ResolvedMatchColor& resolved,
                   const FilterProgress* progress) {
    apply_match_color_to_pixels(pixels, bounds, resolved.apply_region, resolved.transform, progress);
  };
  spec.has_effect = [](const ResolvedMatchColor& resolved) { return !resolved.transform.identity; };
  run_destructive_adjustment(spec);
}

// Photoshop also applies Shadows/Highlights as a Smart Filter; Patchy has no
// native descriptor for it, so Smart Objects take the destructive-adjustment
// refusal (docs/shadows-highlights.md).
void MainWindow::shadows_highlights_dialog() {
  DestructiveAdjustmentSpec<ShadowsHighlightsSettings> spec;
  spec.display_name = tr("Shadows/Highlights");
  spec.cancelled_message = tr("Cancelled Shadows/Highlights");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_shadows_highlights_settings(this, std::move(preview_changed));
  };
  spec.render = apply_shadows_highlights_to_pixels;
  spec.has_effect = shadows_highlights_has_effect;
  run_destructive_adjustment(spec);
}

void MainWindow::color_lookup_dialog() {
  DestructiveAdjustmentSpec<ColorLookupSettings> spec;
  spec.display_name = tr("Color Lookup");
  spec.cancelled_message = tr("Cancelled Color Lookup");
  spec.request_settings = [this](const DestructiveAdjustmentTarget&, auto preview_changed) {
    return request_color_lookup_settings(this, std::move(preview_changed));
  };
  spec.render = apply_color_lookup_to_pixels;
  spec.has_effect = [](const ColorLookupSettings& settings) {
    return settings.data != nullptr && settings.data->table.valid();
  };
  run_destructive_adjustment(spec);
}

void MainWindow::replace_color_dialog() {
  DestructiveAdjustmentSpec<ReplaceColorSettings> spec;
  spec.display_name = tr("Replace Color");
  spec.cancelled_message = tr("Cancelled Replace Color");
  spec.request_settings = [this](const DestructiveAdjustmentTarget& target, auto preview_changed) {
    ReplaceColorDialogInput input;
    input.layer = qimage_from_pixel_buffer(*target.original_pixels);
    input.layer_origin = QPoint(target.bounds.x, target.bounds.y);
    input.document_size = QSize(document().width(), document().height());
    input.selection = target.selection;
    input.initial_sample = canvas_->primary_color();
    const QPointer<CanvasWidget> target_canvas = canvas_;
    ReplaceColorDialogHooks hooks;
    hooks.set_canvas_sampler = [target_canvas](std::function<void(const CanvasReadGesture&)> callback) {
      if (target_canvas == nullptr) {
        return;
      }
      if (callback) {
        target_canvas->set_transient_read_interaction(std::move(callback), eyedropper_cursor());
      } else {
        target_canvas->clear_transient_read_interaction();
      }
    };
    hooks.preview_changed = std::move(preview_changed);
    return request_replace_color(this, input, hooks);
  };
  spec.render = apply_replace_color_to_pixels;
  spec.has_effect = replace_color_has_effect;
  run_destructive_adjustment(spec);
}

}  // namespace patchy::ui
