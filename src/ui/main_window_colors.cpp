// MainWindow's color and gradient controls, split out of
// main_window_tool_options.cpp: the foreground/background/text color pickers
// and buttons, swap/default colors, and the Gradient tool's stop editor,
// preset picker and preview button, plus the anonymous-namespace gradient-stop
// dialog cluster only they use.
// current_text_color and sync_text_options_from_active_editor stay in
// main_window.cpp: they read the inline text editor's formats through the
// internal text helpers there.
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

namespace {

QColor qcolor_from_edit_color(EditColor color) {
  return QColor(color.r, color.g, color.b, color.a);
}

QString gradient_css_stops(const std::vector<GradientStop>& stops, int opacity, bool reverse) {
  QStringList css_stops;
  const auto normalized = normalized_gradient_stops(stops);
  const auto global_opacity = static_cast<float>(std::clamp(opacity, 0, 100)) / 100.0F;
  for (const auto& stop : normalized) {
    auto color = stop.color;
    color.a = static_cast<std::uint8_t>(
        std::clamp(std::lround(static_cast<float>(color.a) * global_opacity), 0L, 255L));
    css_stops << QStringLiteral("stop:%1 rgba(%2, %3, %4, %5)")
                     .arg(reverse ? 1.0 - static_cast<double>(stop.location) : static_cast<double>(stop.location),
                          0, 'f', 3)
                     .arg(color.r)
                     .arg(color.g)
                     .arg(color.b)
                     .arg(color.a);
  }
  return css_stops.join(QStringLiteral(", "));
}

QString gradient_preview_button_style(const std::vector<GradientStop>& stops, int opacity, bool reverse) {
  return QStringLiteral(
             "QPushButton { background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, %1); "
             "border: 1px solid @swatch_border; border-radius: 2px; min-width: 78px; min-height: 22px; padding: 0; }"
             "QPushButton:hover { border: 2px solid @accent_bright; }")
      .arg(gradient_css_stops(stops, opacity, reverse));
}

// The Gradient tool's stop model is a flat std::vector<GradientStop>: applying
// a library preset resolves the dynamic Foreground/Background stop kinds from
// the current colors, then flattens the definition (smoothness, midpoints,
// Noise) into sampled stops. Shared by the Edit Gradient Stops dialog's
// Preset... button and the options-bar Presets popup.
[[nodiscard]] std::vector<GradientStop> sampled_gradient_stops_from_definition(
    const GradientDefinition& definition, const QColor& foreground, const QColor& background) {
  LayerStyleGradient gradient;
  static_cast<GradientDefinition&>(gradient) = definition;
  for (auto& stop : gradient.color_stops) {
    if (stop.kind == GradientColorStop::Kind::Foreground) {
      stop.color = RgbColor{static_cast<std::uint8_t>(foreground.red()),
                            static_cast<std::uint8_t>(foreground.green()),
                            static_cast<std::uint8_t>(foreground.blue())};
    } else if (stop.kind == GradientColorStop::Kind::Background) {
      stop.color = RgbColor{static_cast<std::uint8_t>(background.red()),
                            static_cast<std::uint8_t>(background.green()),
                            static_cast<std::uint8_t>(background.blue())};
    }
    stop.kind = GradientColorStop::Kind::User;
  }
  std::vector<GradientStop> sampled;
  const int sample_count = gradient.form == GradientDefinitionForm::Noise ? 65 : 33;
  sampled.reserve(sample_count);
  for (int index = 0; index < sample_count; ++index) {
    const auto position = static_cast<float>(index) / static_cast<float>(sample_count - 1);
    const auto color = gradient_color(gradient, position);
    sampled.push_back(GradientStop{
        position, EditColor{color.red, color.green, color.blue,
                            static_cast<std::uint8_t>(std::clamp(
                                std::lround(gradient_stop_opacity(gradient, position) * 255.0F), 0L, 255L))}});
  }
  return sampled;
}

class GradientStopTableDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    auto item_option = option;
    const auto selected = item_option.state.testFlag(QStyle::State_Selected);
    item_option.state.setFlag(QStyle::State_Selected, false);
    item_option.showDecorationSelected = false;
    QStyledItemDelegate::paint(painter, item_option, index);
    if (!selected) {
      return;
    }

    painter->save();
    QPen pen(QColor(99, 166, 255), 2);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(option.rect.adjusted(1, 1, -2, -2));
    painter->restore();
  }
};

std::optional<std::optional<std::vector<GradientStop>>> request_gradient_stops_dialog(
    QWidget* parent, const std::vector<GradientStop>& initial_stops, bool has_custom_stops, QColor foreground,
    QColor background, GradientLibrary* gradient_library) {
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("gradientStopsDialog"));
  dialog.resize(560, 430);

  auto* layout = install_dark_dialog_chrome(dialog, new QVBoxLayout(&dialog), QObject::tr("Edit Gradient Stops"));
  auto* preview = new GradientStopsEditorWidget(&dialog);
  preview->setObjectName(QStringLiteral("gradientStopsPreview"));
  layout->addWidget(preview);

  auto* table = new QTableWidget(0, 3, &dialog);
  table->setObjectName(QStringLiteral("gradientStopsTable"));
  table->setHorizontalHeaderLabels({QObject::tr("Location %"), QObject::tr("Color"), QObject::tr("Alpha %")});
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setItemDelegate(new GradientStopTableDelegate(table));
  table->setMinimumHeight(180);
  layout->addWidget(table, 1);

  auto* selected_row = new QWidget(&dialog);
  auto* selected_layout = new QHBoxLayout(selected_row);
  selected_layout->setContentsMargins(0, 0, 0, 0);
  selected_layout->setSpacing(6);
  auto* choose_color = new QPushButton(QObject::tr("Choose Color..."), selected_row);
  choose_color->setObjectName(QStringLiteral("gradientChooseStopColorButton"));
  auto* add_stop = new QPushButton(QObject::tr("Add Stop"), selected_row);
  add_stop->setObjectName(QStringLiteral("gradientAddStopButton"));
  auto* remove_stop = new QPushButton(QObject::tr("Remove Stop"), selected_row);
  remove_stop->setObjectName(QStringLiteral("gradientRemoveStopButton"));
  auto* reset_stops = new QPushButton(QObject::tr("Reset to FG/BG"), selected_row);
  reset_stops->setObjectName(QStringLiteral("gradientResetStopsButton"));
  auto* presets = new QPushButton(QObject::tr("Preset..."), selected_row);
  presets->setObjectName(QStringLiteral("gradientPresetButton"));
  presets->setEnabled(gradient_library != nullptr);
  selected_layout->addWidget(presets);
  selected_layout->addWidget(choose_color);
  selected_layout->addWidget(add_stop);
  selected_layout->addWidget(remove_stop);
  selected_layout->addStretch(1);
  selected_layout->addWidget(reset_stops);
  layout->addWidget(selected_row);

  bool using_default_stops = !has_custom_stops;
  bool loading = false;

  const auto default_stops = [&foreground, &background] {
    auto primary = edit_color(foreground);
    primary.a = 255;
    auto secondary = edit_color(background);
    secondary.a = 255;
    return normalized_gradient_stops({GradientStop{0.0F, primary}, GradientStop{1.0F, secondary}});
  };
  const auto cell_value = [table](int row, int column, int fallback) {
    const auto* item = table->item(row, column);
    bool ok = false;
    const auto value = item == nullptr ? fallback : item->text().toInt(&ok);
    return ok ? value : fallback;
  };
  const auto row_color = [table](int row) {
    const auto* item = table->item(row, 1);
    if (item == nullptr) {
      return QColor(Qt::black);
    }
    const auto stored = item->data(Qt::UserRole).value<QColor>();
    const auto typed = QColor(item->text().trimmed());
    return typed.isValid() ? typed : stored.isValid() ? stored : QColor(Qt::black);
  };
  const auto set_item = [table](int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(column == 1 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, column, item);
    return item;
  };
  const auto update_row_color = [table, &row_color](int row) {
    if (row < 0 || row >= table->rowCount()) {
      return;
    }
    const auto color = row_color(row);
    auto* item = table->item(row, 1);
    if (item == nullptr) {
      return;
    }
    item->setText(color.name(QColor::HexRgb).toUpper());
    item->setData(Qt::UserRole, color);
    item->setBackground(color);
    const auto text = color.red() * 3 + color.green() * 6 + color.blue() > 1280 ? QColor(20, 24, 30)
                                                                                : QColor(245, 248, 252);
    item->setForeground(text);
  };
  const auto read_row_stops = [&] {
    std::vector<GradientStop> stops;
    stops.reserve(static_cast<std::size_t>(table->rowCount()));
    for (int row = 0; row < table->rowCount(); ++row) {
      const auto color = row_color(row);
      stops.push_back(GradientStop{
          std::clamp(static_cast<float>(cell_value(row, 0, row == 0 ? 0 : 100)) / 100.0F, 0.0F, 1.0F),
          EditColor{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                    static_cast<std::uint8_t>(color.blue()),
                    static_cast<std::uint8_t>(std::clamp(cell_value(row, 2, 100), 0, 100) * 255 / 100)}});
    }
    return stops;
  };
  const auto read_stops = [&] {
    auto stops = read_row_stops();
    auto normalized = normalized_gradient_stops(stops);
    if (normalized.size() < 2U) {
      normalized = default_stops();
    }
    return normalized;
  };
  const auto refresh_preview = [&] {
    const QSignalBlocker blocker(table);
    for (int row = 0; row < table->rowCount(); ++row) {
      update_row_color(row);
    }
    preview->set_stops(read_row_stops());
    preview->set_current_row(table->currentRow());
    remove_stop->setEnabled(table->rowCount() > 2);
  };
  const auto add_row = [&](const GradientStop& stop) {
    const auto row = table->rowCount();
    table->insertRow(row);
    set_item(row, 0, QString::number(static_cast<int>(std::round(std::clamp(stop.location, 0.0F, 1.0F) * 100.0F))));
    auto* color_item = set_item(row, 1, qcolor_from_edit_color(stop.color).name(QColor::HexRgb).toUpper());
    color_item->setData(Qt::UserRole, QColor(stop.color.r, stop.color.g, stop.color.b));
    set_item(row, 2, QString::number(static_cast<int>(std::round(static_cast<double>(stop.color.a) * 100.0 / 255.0))));
    update_row_color(row);
  };
  const auto load_stops = [&](const std::vector<GradientStop>& stops) {
    loading = true;
    const QSignalBlocker blocker(table);
    table->setRowCount(0);
    for (const auto& stop : normalized_gradient_stops(stops)) {
      add_row(stop);
    }
    if (table->rowCount() > 0) {
      table->setCurrentCell(0, 0);
    }
    loading = false;
    refresh_preview();
  };
  const auto set_row_color = [&](int row, QColor color) {
    if (row < 0 || row >= table->rowCount() || !color.isValid()) {
      return;
    }
    auto* item = table->item(row, 1);
    if (item == nullptr) {
      item = set_item(row, 1, QString());
    }
    item->setText(color.name(QColor::HexRgb).toUpper());
    item->setData(Qt::UserRole, color);
    using_default_stops = false;
    refresh_preview();
  };
  const auto set_row_location = [&](int row, int location) {
    if (row < 0 || row >= table->rowCount()) {
      return;
    }
    auto* item = table->item(row, 0);
    if (item == nullptr) {
      item = set_item(row, 0, QString());
    }
    item->setText(QString::number(std::clamp(location, 0, 100)));
    using_default_stops = false;
    refresh_preview();
  };
  const auto remove_row = [&](int row) {
    if (table->rowCount() <= 2 || row < 0 || row >= table->rowCount()) {
      return;
    }
    table->removeRow(row);
    table->setCurrentCell(std::min(row, table->rowCount() - 1), 0);
    using_default_stops = false;
    refresh_preview();
  };
  const auto choose_current_color = [&] {
    if (table->rowCount() <= 0) {
      return;
    }
    const auto row = std::clamp(table->currentRow(), 0, table->rowCount() - 1);
    const auto original_color = row_color(row);
    const bool was_using_default_stops = using_default_stops;
    const auto chosen = request_patchy_color(&dialog, original_color, QObject::tr("Choose Gradient Stop Color"),
                                             [&](QColor color) { set_row_color(row, color); });
    if (!chosen.has_value()) {
      set_row_color(row, original_color);
      using_default_stops = was_using_default_stops;
      refresh_preview();
      return;
    }
    set_row_color(row, *chosen);
  };

  load_stops(has_custom_stops ? initial_stops : default_stops());
  QObject::connect(table, &QTableWidget::itemChanged, &dialog, [&](QTableWidgetItem*) {
    if (!loading) {
      using_default_stops = false;
    }
    refresh_preview();
  });
  QObject::connect(table, &QTableWidget::currentCellChanged, &dialog, [&](int, int, int, int) { refresh_preview(); });
  QObject::connect(add_stop, &QPushButton::clicked, &dialog, [&] {
    const auto source_row = std::clamp(table->currentRow(), 0, std::max(0, table->rowCount() - 1));
    const auto color = row_color(source_row);
    const auto location = std::clamp(cell_value(source_row, 0, 50) + (table->rowCount() > 0 ? 10 : 0), 0, 100);
    add_row(GradientStop{static_cast<float>(location) / 100.0F,
                         EditColor{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                                   static_cast<std::uint8_t>(color.blue()),
                                   static_cast<std::uint8_t>(std::clamp(cell_value(source_row, 2, 100), 0, 100) *
                                                             255 / 100)}});
    table->setCurrentCell(table->rowCount() - 1, 0);
    using_default_stops = false;
    refresh_preview();
  });
  QObject::connect(remove_stop, &QPushButton::clicked, &dialog, [&] {
    remove_row(std::clamp(table->currentRow(), 0, table->rowCount() - 1));
  });
  QObject::connect(choose_color, &QPushButton::clicked, &dialog, choose_current_color);
  QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog, [table, &choose_current_color](int row, int column) {
    if (column != 1) {
      return;
    }
    table->setCurrentCell(std::clamp(row, 0, table->rowCount() - 1), 1);
    choose_current_color();
  });
  QObject::connect(reset_stops, &QPushButton::clicked, &dialog, [&] {
    load_stops(default_stops());
    using_default_stops = true;
  });
  QObject::connect(presets, &QPushButton::clicked, &dialog, [&, presets] {
    if (gradient_library == nullptr) return;
    // Frame-reference captures are safe here: the popup is a child of the
    // dialog's button and cannot outlive the blocked dialog frame.
    const auto use_entry = [&](const GradientLibraryEntry& entry) {
      load_stops(sampled_gradient_stops_from_definition(entry.definition, foreground, background));
      using_default_stops = false;
    };
    show_gradient_preset_popup(presets, *gradient_library, use_entry, [&, use_entry] {
      const auto selected = request_gradient_manager(&dialog, *gradient_library, {});
      if (const auto* entry = gradient_library->find_entry(selected); entry != nullptr) {
        use_entry(*entry);
      }
    });
  });
  preview->stop_selected = [&](int row) {
    if (row >= 0 && row < table->rowCount()) {
      table->setCurrentCell(row, 0);
    }
  };
  preview->choose_stop_color_requested = [&](int row) {
    if (row >= 0 && row < table->rowCount()) {
      table->setCurrentCell(row, 1);
      choose_current_color();
    }
  };
  preview->stop_location_changed = [&](int row, int location) { set_row_location(row, location); };
  preview->stop_color_picked = [&](int row, QColor color) { set_row_color(row, color); };
  preview->stop_add_requested = [&](GradientStop stop) {
    add_row(stop);
    const int row = table->rowCount() - 1;
    table->setCurrentCell(row, 0);
    using_default_stops = false;
    refresh_preview();
    return row;
  };
  preview->stop_delete_requested = [&](int row) { remove_row(row); };

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  remember_dialog_position(dialog);
  if (run_non_modal_dialog(dialog) != QDialog::Accepted) {
    return std::nullopt;
  }
  if (using_default_stops) {
    return std::optional<std::vector<GradientStop>>{};
  }
  return read_stops();
}

}  // namespace

void MainWindow::choose_primary_color() {
  show_color_panel(true);
}

void MainWindow::choose_secondary_color() {
  show_color_panel(false);
}

void MainWindow::choose_text_color() {
  if (canvas_ == nullptr) {
    return;
  }
  sync_text_options_from_active_editor();
  if (color_dialog_ != nullptr) {
    if (color_dialog_->property("patchy.colorTarget").toString() == QStringLiteral("text")) {
      color_dialog_->show();
      color_dialog_->raise();
      color_dialog_->activateWindow();
      return;
    }
    color_dialog_->close();
  }

  auto* editor = canvas_->findChild<QTextEdit*>(QStringLiteral("inlineTextEditor"));
  auto* dialog = create_patchy_color_panel(this, current_text_color(), tr("Text Color"),
                                           [this, editor = QPointer<QTextEdit>(editor)](QColor color) {
    if (canvas_ == nullptr) {
      return;
    }
    color.setAlpha(255);
    canvas_->set_primary_color(color);
    if (editor != nullptr) {
      editor->setProperty("patchy.documentTextColor", color);
      apply_text_color_to_active_editor();
    } else {
      // No session when the panel opened: the selected text layers take the color (issue 31).
      apply_text_color_to_selected_layers_debounced(color);
    }
    refresh_color_buttons();
    statusBar()->showMessage(tr("Text color changed"));
  });
  dialog->setProperty("patchy.colorTarget", QStringLiteral("text"));
  color_dialog_ = dialog;
  connect(dialog, &QObject::destroyed, this, [this, dialog] {
    if (color_dialog_ == dialog) {
      color_dialog_ = nullptr;
    }
  });
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::show_color_panel(bool foreground) {
  if (canvas_ == nullptr) {
    return;
  }
  const auto color_target = foreground ? QStringLiteral("foreground") : QStringLiteral("background");
  if (color_dialog_ != nullptr) {
    if (color_dialog_->property("patchy.colorTarget").toString() == color_target) {
      color_dialog_->show();
      color_dialog_->raise();
      color_dialog_->activateWindow();
      return;
    }
    color_dialog_->close();
  }

  auto* dialog = create_patchy_color_panel(
      this, foreground ? canvas_->primary_color() : canvas_->secondary_color(),
      foreground ? tr("Foreground Color") : tr("Background Color"),
      [this, foreground](QColor color) { apply_foreground_background_edit(foreground, color); });
  dialog->setProperty("patchy.colorTarget", color_target);
  color_dialog_ = dialog;
  connect(dialog, &QObject::destroyed, this, [this, dialog] {
    if (color_dialog_ == dialog) {
      color_dialog_ = nullptr;
    }
  });
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::apply_foreground_background_edit(bool foreground, QColor color) {
  // The popup is non-modal and outlives sessions: after Close All there is no canvas.
  if (canvas_ == nullptr) {
    return;
  }
  color.setAlpha(255);
  if (foreground) {
    canvas_->set_primary_color(color);
    apply_primary_color_to_active_text_editor(color);
    apply_foreground_color_to_shape_paint(color);
    statusBar()->showMessage(tr("Foreground color changed"));
  } else {
    canvas_->set_secondary_color(color);
    statusBar()->showMessage(tr("Background color changed"));
  }
  refresh_color_buttons();
}

void MainWindow::swap_colors() {
  if (canvas_ == nullptr) {
    return;
  }
  const auto primary = canvas_->primary_color();
  canvas_->set_primary_color(canvas_->secondary_color());
  canvas_->set_secondary_color(primary);
  refresh_color_buttons();
  statusBar()->showMessage(tr("Swapped foreground/background"));
}

void MainWindow::default_colors() {
  if (canvas_ == nullptr) {
    return;
  }
  canvas_->set_primary_color(Qt::black);
  canvas_->set_secondary_color(Qt::white);
  refresh_color_buttons();
  statusBar()->showMessage(tr("Default colors"));
}

void MainWindow::refresh_color_buttons() {
  const auto primary_color = canvas_ != nullptr ? canvas_->primary_color() : QColor(Qt::black);
  const auto secondary_color = canvas_ != nullptr ? canvas_->secondary_color() : QColor(Qt::white);
  const auto named_tooltip = [this](const QString& text, QColor color) {
    if (!has_active_document()) { return text; }
    const auto name = palette_color_name(std::as_const(document()),
        {static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()), static_cast<std::uint8_t>(color.blue())});
    if (name.empty()) { return text; }
    return QStringLiteral("<qt>%1<br>%2</qt>")
        .arg(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())).toHtmlEscaped(), text.toHtmlEscaped());
  };
  if (primary_color_button_ != nullptr) {
    primary_color_button_->setText(tr("FG"));
    primary_color_button_->setToolTip(named_tooltip(tr("Foreground color %1").arg(primary_color.name(QColor::HexRgb).toUpper()), primary_color));
    set_themed_style(*primary_color_button_, color_button_style(primary_color));
  }
  if (secondary_color_button_ != nullptr) {
    secondary_color_button_->setText(tr("BG"));
    secondary_color_button_->setToolTip(named_tooltip(tr("Background color %1").arg(secondary_color.name(QColor::HexRgb).toUpper()), secondary_color));
    set_themed_style(*secondary_color_button_, color_button_style(secondary_color));
  }
  // Every path that keeps the toolbar swatches current ends here, so the Color
  // panel follows them.
  if (color_dock_panel_ != nullptr) {
    color_dock_panel_->set_colors(primary_color, secondary_color);
  }
  refresh_text_color_button();
  refresh_gradient_controls_from_canvas();
}

void MainWindow::refresh_text_color_button() {
  if (text_color_button_ == nullptr || canvas_ == nullptr) {
    return;
  }
  const auto color = current_text_color();
  text_color_button_->setText(tr("T"));
  text_color_button_->setToolTip(tr("Text color %1").arg(color.name(QColor::HexRgb).toUpper()));
  set_themed_style(*text_color_button_, color_button_style(color));
}

void MainWindow::edit_gradient_stops() {
  if (canvas_ == nullptr) {
    return;
  }
  const auto result = request_gradient_stops_dialog(this, canvas_->effective_gradient_stops(),
                                                    canvas_->gradient_stops().has_value(), canvas_->primary_color(),
                                                    canvas_->secondary_color(), &gradient_library());
  if (!result.has_value() || canvas_ == nullptr) {
    return;  // cancelled, or the document was closed while the dialog was open
  }
  canvas_->set_gradient_stops(*result);
  refresh_gradient_controls_from_canvas();
  save_tool_settings();
  refresh_document_info();
}

void MainWindow::choose_gradient_preset() {
  if (canvas_ == nullptr || gradient_presets_button_ == nullptr) {
    return;
  }
  const auto apply_entry = [this](const GradientLibraryEntry& entry) {
    if (canvas_ == nullptr) {
      return;
    }
    canvas_->set_gradient_stops(sampled_gradient_stops_from_definition(
        entry.definition, canvas_->primary_color(), canvas_->secondary_color()));
    refresh_gradient_controls_from_canvas();
    save_tool_settings();
    refresh_document_info();
    statusBar()->showMessage(tr("Gradient preset: %1").arg(gradient_library_entry_display_name(entry)));
  };
  show_gradient_preset_popup(gradient_presets_button_, gradient_library(), apply_entry,
                             [this, apply_entry] {
                               const auto selected = request_gradient_manager(this, gradient_library(), {});
                               if (const auto* entry = gradient_library().find_entry(selected); entry != nullptr) {
                                 apply_entry(*entry);
                               }
                             });
}

void MainWindow::refresh_gradient_controls_from_canvas() {
  if (canvas_ == nullptr) {
    return;
  }
  if (gradient_method_combo_ != nullptr) {
    QSignalBlocker blocker(gradient_method_combo_);
    const auto index = gradient_method_combo_->findData(static_cast<int>(canvas_->gradient_method()));
    gradient_method_combo_->setCurrentIndex(std::max(0, index));
  }
  if (gradient_opacity_spin_ != nullptr) {
    QSignalBlocker blocker(gradient_opacity_spin_);
    gradient_opacity_spin_->setValue(canvas_->gradient_opacity());
  }
  if (gradient_opacity_slider_ != nullptr) {
    QSignalBlocker blocker(gradient_opacity_slider_);
    gradient_opacity_slider_->setValue(canvas_->gradient_opacity());
  }
  if (gradient_reverse_check_ != nullptr) {
    QSignalBlocker blocker(gradient_reverse_check_);
    gradient_reverse_check_->setChecked(canvas_->gradient_reverse());
  }
  if (gradient_preview_button_ != nullptr) {
    set_themed_style(*gradient_preview_button_, gradient_preview_button_style(
        canvas_->effective_gradient_stops(), canvas_->gradient_opacity(), canvas_->gradient_reverse()));
    gradient_preview_button_->setToolTip(tr("Gradient preview"));
  }
}

}  // namespace patchy::ui
