// MainWindow's per-layer controls, split out of main_window_tool_options.cpp:
// the selected-layer id queries and lock-flag helpers, and the active layer's
// opacity/fill/blend/visibility/lock/clipping handlers with their pending-edit
// coalescing.
// Pure function moves; behavior must stay identical.

#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "core/warp_mesh.hpp"
#include "core/layer_render_utils.hpp"
#include "core/layer_tree.hpp"
#include "core/palette_presets.hpp"
#include "core/pattern_presets.hpp"
#include "core/pixel_tools.hpp"
#include "formats/palette_io.hpp"
#include "filters/builtin_filters.hpp"
#include "formats/aseprite_document_io.hpp"
#include "formats/bmp_document_io.hpp"
#include "formats/heif_document_io.hpp"
#include "formats/raw_document_io.hpp"
#include "plugins/legacy_photoshop_adapter.hpp"
#include "psd/psd_document_io.hpp"
#include "psd/psd_filter_effects.hpp"
#include "psd/psd_smart_objects.hpp"
#include "ui/action_icons.hpp"
#include "ui/app_settings.hpp"
#include "render/compositor.hpp"
#include "ui/blend_mode_ui.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_settings_panel.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/current_brush.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/raw_develop_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "ui/gradient_preset_popup.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/font_picker.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_dock_panel.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/localization.hpp"
#include "ui/measurement_units.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/pattern_library.hpp"
#include "ui/photo_pattern_presets.hpp"
#include "ui/shape_appearance_dialog.hpp"
#include "ui/style_library.hpp"
#include "ui/print_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/scanner_import.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/start_panel.hpp"
#include "ui/tile_preview_window.hpp"
#include "ui/warp_text_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/update_checker.hpp"
#include "ui/zoom_status_bar.hpp"
#include "ui/theme_qss.hpp"
#include "ui/theme_palette.hpp"
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
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QDateTime>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QColorSpace>
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
#include <QFile>
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
#include <QMutex>
#include <QRawFont>
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
#include <QStyleOption>
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
#include <numeric>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
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

std::vector<LayerId> MainWindow::selected_layer_ids() const {
  std::vector<LayerId> ids;
  if (layer_list_ == nullptr) {
    return ids;
  }
  ids.reserve(static_cast<std::size_t>(layer_list_->selectedItems().size()));
  for (int row = 0; row < layer_list_->count(); ++row) {
    const auto* item = layer_list_->item(row);
    if (item != nullptr && item->isSelected()) {
      ids.push_back(static_cast<LayerId>(item->data(kLayerIdRole).toULongLong()));
    }
  }
  return ids;
}

void MainWindow::report_layer_selection_count(const std::vector<LayerId>& selected_ids) {
  const std::unordered_set<LayerId> selected(selected_ids.begin(), selected_ids.end());
  // Count the document tree, not the panel rows. Descendants of a selected
  // group count once even when they also have selected rows of their own.
  const auto count_selected = [&](const auto& self, const std::vector<Layer>& layers,
                                   bool ancestor_selected) -> std::size_t {
    std::size_t count = 0;
    for (const auto& layer : layers) {
      const bool included = ancestor_selected || selected.contains(layer.id());
      if (included) ++count;
      count += self(self, layer.children(), included);
    }
    return count;
  };
  const auto count = has_active_document() && !selected.empty()
      ? count_selected(count_selected, std::as_const(document()).layers(), false) : 0U;
  statusBar()->showMessage(count == 1U ? tr("1 layer selected")
                                      : tr("%1 layers selected").arg(count));
}

std::vector<LayerId> MainWindow::selected_or_active_layer_ids() const {
  auto ids = selected_layer_ids();
  const auto active = document().active_layer_id();
  if (ids.empty() && active.has_value()) {
    ids.push_back(*active);
  }
  return ids;
}

LayerLockFlags MainWindow::layer_id_effective_lock_flags(LayerId id) const {
  return has_active_document() ? patchy::layer_effective_lock_flags(document().layers(), id) : kLayerLockNone;
}

LayerLockFlags MainWindow::layer_id_ancestor_lock_flags(LayerId id) const {
  return has_active_document() ? patchy::layer_ancestor_lock_flags(document().layers(), id) : kLayerLockNone;
}

bool MainWindow::layer_id_locks_image_pixels(LayerId id) const {
  return (layer_id_effective_lock_flags(id) & kLayerLockImagePixels) != kLayerLockNone;
}

bool MainWindow::layer_id_locks_position(LayerId id) const {
  return (layer_id_effective_lock_flags(id) & kLayerLockPosition) != kLayerLockNone;
}

bool MainWindow::layer_id_locks_transparent_pixels(LayerId id) const {
  return (layer_id_effective_lock_flags(id) & kLayerLockTransparentPixels) != kLayerLockNone;
}

std::vector<LayerId> MainWindow::layer_ids_without_image_pixel_lock(std::vector<LayerId> ids) const {
  ids.erase(std::remove_if(ids.begin(), ids.end(), [this](LayerId id) { return layer_id_locks_image_pixels(id); }),
            ids.end());
  return ids;
}

bool MainWindow::show_pixel_lock_message_if_all_locked(const std::vector<LayerId>& requested_ids,
                                                       const std::vector<LayerId>& editable_ids) {
  if (!requested_ids.empty() && editable_ids.empty()) {
    show_status_error(tr("Layer pixels are locked."));
    return true;
  }
  return false;
}

void MainWindow::set_active_layer_from_selection() {
  if (!updating_layer_controls_) finish_pending_shape_appearance_edit();
  if (updating_layer_controls_) {
    return;
  }
  if (canvas_ != nullptr && layer_list_ != nullptr &&
      canvas_->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor")) != nullptr) {
    // Commit any in-progress text edit before honoring the new selection.
    // Finishing the editor rebuilds the layer list and can reset the current
    // item, so remember which layer the user clicked and restore it afterwards.
    std::optional<LayerId> requested_id;
    if (auto* item = layer_list_->currentItem(); item != nullptr) {
      requested_id = static_cast<LayerId>(item->data(kLayerIdRole).toULongLong());
    }
    finish_active_text_editor();
    if (requested_id.has_value() && document().find_layer(*requested_id) != nullptr) {
      const QSignalBlocker blocker(layer_list_);
      for (int row = 0; row < layer_list_->count(); ++row) {
        auto* item = layer_list_->item(row);
        if (item != nullptr && static_cast<LayerId>(item->data(kLayerIdRole).toULongLong()) == *requested_id) {
          layer_list_->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
          break;
        }
      }
    }
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    // Restore the selection to the active layer, but defer the rebuild: clearing
    // and repopulating the list from inside this itemSelectionChanged handler
    // deletes the items Qt is still using and crashes.
    QTimer::singleShot(0, this, [this] { refresh_layer_list(); });
    return;
  }
  const UiProfileScope profile_scope("set_active_layer_from_selection");
  const QPointer<CanvasWidget> selecting_canvas(canvas_);
  if (selecting_canvas) { selecting_canvas->begin_processing_operation(tr("Selecting layers...")); }
  const auto finish_selection = qScopeGuard([selecting_canvas] {
    if (selecting_canvas) { selecting_canvas->end_processing_operation(); }
  });
  const auto selection_progress = [&] {
    if (selecting_canvas) { selecting_canvas->tick_processing_operation(); }
  };
  selection_progress();
  const auto selected_ids = selected_layer_ids();
  // A click on the panel's blank area empties the selection while Qt keeps the
  // current row; that is the user deselecting every layer, so the active layer
  // goes too (the Move tool's transform box otherwise stayed on the old active
  // layer, Seth, October 2026). Rebuilds run under a signal blocker and never
  // get here; an empty list has nothing to deselect.
  if (selected_ids.empty() && layer_list_->count() > 0 && has_active_document() &&
      document().active_layer_id().has_value()) {
    deselect_all_layers();
    return;
  }
  if (canvas_ != nullptr) {
    canvas_->set_selected_layer_ids(selected_ids);
  }
  report_layer_selection_count(selected_ids);
  // A pure multi-selection change (same active layer) still decides whether
  // Combine Shapes applies.
  refresh_combine_shapes_action_states();
  refresh_layer_alignment_action_states();
  selection_progress();
  if (layer_list_->currentItem() == nullptr) {
    // Deselecting every layer drops the current row as well. Rows selected
    // afterwards without one (setSelected from a script or a test) would leave
    // a selection with no active layer, so the topmost selected row becomes it.
    QListWidgetItem* topmost = nullptr;
    const auto selected_items = layer_list_->selectedItems();
    for (auto* item : selected_items) {
      if (topmost == nullptr || layer_list_->row(item) < layer_list_->row(topmost)) {
        topmost = item;
      }
    }
    if (topmost == nullptr) {
      return;
    }
    const QSignalBlocker blocker(layer_list_);
    layer_list_->setCurrentItem(topmost, QItemSelectionModel::NoUpdate);
  }

  const auto id = static_cast<LayerId>(layer_list_->currentItem()->data(kLayerIdRole).toULongLong());
  auto& doc = document();
  if (doc.find_layer(id) != nullptr) {
    const auto previous_active = doc.active_layer_id();
    if (!previous_active.has_value() || *previous_active != id) {
      doc.set_active_layer(id);
      if (canvas_ != nullptr) {
        canvas_->set_layer_edit_target(CanvasWidget::LayerEditTarget::Content);
      }
      // The Paths panel's transient layer-path row (and its Photoshop-style
      // auto-targeting) follows the active layer.
      refresh_paths_panel();
    }
    if (canvas_ != nullptr) {
      update_layer_target_styles(layer_list_, doc.active_layer_id(), canvas_->layer_edit_target());
    }
    refresh_layer_controls();
    selection_progress();
    restyle_layer_rows(layer_list_);
    selection_progress();
  }
}

void MainWindow::set_active_layer_opacity(int value) {
  if (updating_layer_controls_) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }

  if (!pending_layer_opacity_edit_active_) {
    if (!has_active_document()) {
      return;
    }
    auto ids = selected_or_active_layer_ids();
    auto& doc = document();
    ids.erase(std::remove_if(ids.begin(), ids.end(), [&doc](LayerId id) { return doc.find_layer(id) == nullptr; }),
              ids.end());
    if (ids.empty()) {
      return;
    }
    push_undo_snapshot(tr("Opacity"));
    pending_layer_opacity_ids_ = std::move(ids);
    pending_layer_opacity_edit_active_ = true;
  }

  pending_layer_opacity_value_ = std::clamp(value, 0, 100);
  if (layer_opacity_apply_timer_ != nullptr) {
    layer_opacity_apply_timer_->start();
  } else {
    apply_pending_layer_opacity();
  }
  if (layer_opacity_idle_timer_ != nullptr) {
    layer_opacity_idle_timer_->start();
  }
}

void MainWindow::apply_pending_layer_opacity() {
  if (!pending_layer_opacity_value_.has_value()) {
    return;
  }
  const auto value = *pending_layer_opacity_value_;
  pending_layer_opacity_value_.reset();
  if (!has_active_document()) {
    return;
  }

  auto& doc = document();
  bool changed = false;
  for (const auto id : pending_layer_opacity_ids_) {
    auto* layer = doc.find_layer(id);
    if (layer == nullptr) {
      continue;
    }
    layer->set_opacity(static_cast<float>(value) / 100.0F);
    changed = true;
  }
  if (changed && canvas_ != nullptr) {
    canvas_->document_changed_async_preview();
  }
}

void MainWindow::finish_pending_layer_opacity_edit() {
  if (layer_opacity_apply_timer_ != nullptr) {
    layer_opacity_apply_timer_->stop();
  }
  if (layer_opacity_idle_timer_ != nullptr) {
    layer_opacity_idle_timer_->stop();
  }
  apply_pending_layer_opacity();
  reset_pending_layer_opacity_edit();
}

void MainWindow::reset_pending_layer_opacity_edit() {
  pending_layer_opacity_ids_.clear();
  pending_layer_opacity_value_.reset();
  pending_layer_opacity_edit_active_ = false;
}

void MainWindow::set_active_layer_fill_opacity(int value) {
  if (updating_layer_controls_) return;
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  if (!pending_layer_fill_opacity_edit_active_) {
    if (!has_active_document()) return;
    auto ids = selected_or_active_layer_ids();
    auto& doc = document();
    ids.erase(std::remove_if(ids.begin(), ids.end(), [&doc](LayerId id) {
                const auto* layer = doc.find_layer(id);
                return layer == nullptr || layer->kind() == LayerKind::Group;
              }), ids.end());
    if (ids.empty()) return;
    push_undo_snapshot(tr("Fill Opacity"));
    pending_layer_fill_opacity_ids_ = std::move(ids);
    pending_layer_fill_opacity_edit_active_ = true;
  }
  pending_layer_fill_opacity_value_ = std::clamp(value, 0, 100);
  if (layer_fill_opacity_apply_timer_ != nullptr) layer_fill_opacity_apply_timer_->start();
  else apply_pending_layer_fill_opacity();
  if (layer_fill_opacity_idle_timer_ != nullptr) layer_fill_opacity_idle_timer_->start();
}

void MainWindow::apply_pending_layer_fill_opacity() {
  if (!pending_layer_fill_opacity_value_.has_value()) return;
  const auto value = *pending_layer_fill_opacity_value_;
  pending_layer_fill_opacity_value_.reset();
  if (!has_active_document()) return;
  bool changed = false;
  for (const auto id : pending_layer_fill_opacity_ids_) {
    if (auto* layer = document().find_layer(id); layer != nullptr && layer->kind() != LayerKind::Group) {
      layer->set_fill_opacity(static_cast<float>(value) / 100.0F);
      changed = true;
    }
  }
  if (changed && canvas_ != nullptr) canvas_->document_changed_async_preview();
}

void MainWindow::finish_pending_layer_fill_opacity_edit() {
  if (layer_fill_opacity_apply_timer_ != nullptr) layer_fill_opacity_apply_timer_->stop();
  if (layer_fill_opacity_idle_timer_ != nullptr) layer_fill_opacity_idle_timer_->stop();
  apply_pending_layer_fill_opacity();
  reset_pending_layer_fill_opacity_edit();
}

void MainWindow::reset_pending_layer_fill_opacity_edit() {
  pending_layer_fill_opacity_ids_.clear();
  pending_layer_fill_opacity_value_.reset();
  pending_layer_fill_opacity_edit_active_ = false;
}

void MainWindow::set_active_layer_blend(int index) {
  if (updating_layer_controls_ || index < 0) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  // Stepping through modes with the arrow keys or the wheel is one run and one
  // undo entry, like an Opacity drag; the run ends after a pause or at the next
  // history change or layer-control refresh.
  if (!pending_layer_blend_edit_active_) {
    if (!has_active_document()) {
      return;
    }
    auto ids = selected_or_active_layer_ids();
    if (ids.empty()) {
      return;
    }
    push_undo_snapshot(tr("Blend mode"));
    pending_layer_blend_ids_ = std::move(ids);
    pending_layer_blend_edit_active_ = true;
  }
  if (layer_blend_idle_timer_ != nullptr) {
    layer_blend_idle_timer_->start();
  }
  auto& doc = document();
  Rect affected;
  for (const auto id : pending_layer_blend_ids_) {
    auto* layer = doc.find_layer(id);
    if (layer == nullptr) {
      continue;
    }
    layer->set_blend_mode(static_cast<BlendMode>(blend_combo_->itemData(index).toInt()));
    affected = unite_rect(affected, layer_render_bounds(*layer));
  }
  canvas_->document_changed(to_qrect(affected));
}

void MainWindow::finish_pending_layer_blend_edit() {
  if (layer_blend_idle_timer_ != nullptr) {
    layer_blend_idle_timer_->stop();
  }
  pending_layer_blend_ids_.clear();
  pending_layer_blend_edit_active_ = false;
}

void MainWindow::set_active_layer_visible(bool visible) {
  if (updating_layer_controls_) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  const auto ids = selected_or_active_layer_ids();
  if (ids.empty()) {
    return;
  }
  auto& doc = document();
  push_undo_snapshot(tr("Visibility"));
  Rect affected;
  for (const auto id : ids) {
    auto* layer = doc.find_layer(id);
    if (layer == nullptr) {
      continue;
    }
    layer->set_visible(visible);
    affected = unite_rect(affected, layer_render_bounds(*layer));
  }
  canvas_->document_changed(to_qrect(affected));
  refresh_layer_list();
  refresh_layer_controls();
}

void MainWindow::set_layer_lock_flag_state(LayerId id, LayerLockFlags flag, bool locked) {
  if (updating_layer_controls_ || !has_active_document()) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  auto* layer = document().find_layer(id);
  if (layer == nullptr || ((layer_lock_flags(*layer) & flag) != kLayerLockNone) == locked) {
    refresh_layer_list();
    refresh_layer_controls();
    return;
  }
  push_undo_snapshot(tr("Lock layer"));
  set_layer_lock_flag(*layer, flag, locked);
  refresh_layer_list();
  refresh_layer_controls();
  statusBar()->showMessage(locked ? tr("Layer lock enabled") : tr("Layer lock disabled"));
}

void MainWindow::set_active_layer_lock_flag(LayerLockFlags flag, bool locked) {
  if (updating_layer_controls_) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  // The lock buttons don't take focus, so an open inline text edit would not auto-commit and
  // the snapshot below would embed its provisional layer; settle the edit first like every
  // other document-mutating action.
  finish_active_text_editor();
  const auto ids = selected_or_active_layer_ids();
  if (ids.empty()) {
    return;
  }
  auto& doc = document();
  push_undo_snapshot(tr("Lock layer"));
  for (const auto id : ids) {
    auto* layer = doc.find_layer(id);
    if (layer == nullptr) {
      continue;
    }
    set_layer_lock_flag(*layer, flag, locked);
  }
  refresh_layer_list();
  refresh_layer_controls();
  statusBar()->showMessage(locked ? tr("Layer lock enabled") : tr("Layer lock disabled"));
}

void MainWindow::refresh_layer_clipping_action_state() {
  if (layer_clipping_mask_action_ == nullptr) {
    return;
  }
  bool enabled = false;
  bool clipped = false;
  if (canvas_ != nullptr && has_active_document()) {
    const auto& doc = std::as_const(document());
    if (const auto active = doc.active_layer_id(); active.has_value()) {
      if (const auto location = find_layer_location(doc.layers(), *active);
          location.has_value() && location->siblings != nullptr) {
        const auto& layer = (*location->siblings)[location->index];
        clipped = layer.clipped();
        if (layer.kind() != LayerKind::Group) {
          enabled = clipped || effective_clip_base(*location->siblings, location->index) != nullptr;
        }
      }
    }
  }
  layer_clipping_mask_action_->setText(clipped ? tr("Release Clipping Mask") : tr("Create Clipping Mask"));
  layer_clipping_mask_action_->setEnabled(enabled);
}

void MainWindow::toggle_active_layer_clipping() {
  if (updating_layer_controls_) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    return;
  }
  finish_active_text_editor();
  auto& doc = document();
  const auto active = doc.active_layer_id();
  if (!active.has_value()) {
    return;
  }
  const auto location = find_layer_location(std::as_const(doc).layers(), *active);
  if (!location.has_value() || location->siblings == nullptr) {
    return;
  }
  const auto& layer = (*location->siblings)[location->index];
  const bool clipped = layer.clipped();
  if (layer.kind() == LayerKind::Group) {
    return;
  }
  if (!clipped && effective_clip_base(*location->siblings, location->index) == nullptr) {
    show_status_error(tr("Create Clipping Mask needs a pixel layer or group below"));
    return;
  }

  push_undo_snapshot(clipped ? tr("Release clipping mask") : tr("Create clipping mask"));
  auto* mutable_layer = doc.find_layer(*active);
  if (mutable_layer == nullptr) {
    return;
  }
  mutable_layer->set_clipped(!clipped);
  QRect affected = to_qrect(layer_render_bounds(*mutable_layer));
  // The base's rendering changes too (the isolated group re-forms around it);
  // bump its render revision so the undo diff repaints its footprint.
  if (const auto base_location = find_layer_location(std::as_const(doc).layers(), *active);
      base_location.has_value()) {
    if (const auto* base = effective_clip_base(*base_location->siblings, base_location->index); base != nullptr) {
      if (auto* mutable_base = doc.find_layer(base->id()); mutable_base != nullptr) {
        mutable_base->mark_render_changed();
        affected = affected.united(to_qrect(layer_render_bounds(*mutable_base)));
      }
    }
  }
  if (canvas_ != nullptr) {
    canvas_->document_changed(affected);
  }
  refresh_layer_list();
  refresh_layer_controls();
  statusBar()->showMessage(clipped ? tr("Clipping mask released") : tr("Clipping mask created"));
}

void MainWindow::set_active_layer_lock_all(bool locked) {
  if (updating_layer_controls_) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_layer_controls();
    return;
  }
  // See set_active_layer_lock_flag: settle an open inline text edit before snapshotting.
  finish_active_text_editor();
  const auto ids = selected_or_active_layer_ids();
  if (ids.empty()) {
    return;
  }
  auto& doc = document();
  push_undo_snapshot(tr("Lock layer"));
  for (const auto id : ids) {
    auto* layer = doc.find_layer(id);
    if (layer == nullptr) {
      continue;
    }
    set_layer_locks_all(*layer, locked);
  }
  refresh_layer_list();
  refresh_layer_controls();
  statusBar()->showMessage(locked ? tr("Layer locked") : tr("Layer unlocked"));
}

}  // namespace patchy::ui
