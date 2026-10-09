// The Options-bar row for the Type tool: font, style, size, smoothing, color,
// alignment, orientation and direction, the Warp/Character/Paragraph buttons,
// and the inline editor's apply/cancel pair.
// Row builder called by build_options_bar (main_window_actions_options_bar.cpp);
// a pure move out of its single body, so behavior and construction order are unchanged.

#include "ui/main_window.hpp"
#include "ui/appearance_edits.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/main_window_options_bar_internal.hpp"

#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "core/vector_shape.hpp"
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
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/current_brush.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/modifier_names.hpp"
#include "ui/raw_develop_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/font_picker.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/localization.hpp"
#include "ui/measurement_units.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/pattern_library.hpp"
#include "ui/pattern_manager_dialog.hpp"
#include "ui/photo_pattern_presets.hpp"
#include "ui/style_library.hpp"
#include "ui/print_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/scanner_import.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/tile_preview_window.hpp"
#include "ui/warp_text_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/update_checker.hpp"
#include "ui/zoom_status_bar.hpp"
#include "ui/tool_traits.hpp"
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
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
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
#include <numeric>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <initializer_list>
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

void MainWindow::build_type_tool_options(OptionsBarBuildContext& bar) {
  auto* toolbar = bar.toolbar;
  auto* options_flow = bar.options_flow;
  const auto& add_option_widget = bar.add_option_widget;
  const auto& add_option_label = bar.add_option_label;

  add_option_label(QT_TR_NOOP("Font:"), {CanvasTool::Text});
  text_font_combo_ = new FontPickerCombo(toolbar);
  text_font_combo_->setObjectName(QStringLiteral("textFontCombo"));
  text_font_combo_->setCurrentFont(font());
  text_font_combo_->setFixedWidth(210);
  add_option_widget(text_font_combo_, {CanvasTool::Text});
  // Photoshop's style picker: the family's OWN face list, and the only face control in the bar
  // (no B/I buttons, like Photoshop). A family with no italic simply does not offer one, and
  // Ctrl+B / Ctrl+I toggle the real face during a session, falling back to faux when the family
  // lacks that axis.
  text_style_combo_ = new QComboBox(toolbar);
  text_style_combo_->setObjectName(QStringLiteral("textStyleCombo"));
  bind_tooltip(text_style_combo_, QT_TR_NOOP("Font style"));
  text_style_combo_->setFixedWidth(132);
  refresh_text_style_combo(text_font_combo_->currentFont().family(), QString());
  add_option_widget(text_style_combo_, {CanvasTool::Text});
  add_option_label(QT_TR_NOOP("Size:"), {CanvasTool::Text});
  text_size_spin_ = new UnitSpinBox(SpinUnit::Points, toolbar);
  text_size_spin_->setObjectName(QStringLiteral("textSizeSpin"));
  text_size_spin_->set_context_provider([this] {
    return UnitConversionContext{has_active_document() ? text_size_ppi(document()) : 300.0, 0.0};
  });
  text_size_spin_->setDecimals(3);
  text_size_spin_->setRange(0.01, 10000.0);
  // Applies on Enter, focus loss or a step, like the Character panel fields: with no session
  // every value change commits a re-render of each selected text layer as an undo step, so
  // typing "120" must not land three of them.
  text_size_spin_->setKeyboardTracking(false);
  // Typing accepts up to 10000 pt, but the popup slider stays usable at 0..200.
  text_size_spin_->setProperty(kToolbarSpinboxSliderMaxProperty, 200.0);
  text_size_spin_->setSingleStep(0.25);
  // 48 px at the default document's 72 ppi = 48 pt (startup builds the bar with
  // no document open).
  text_size_spin_->setValue(has_active_document() ? text_pixels_to_points(48, document()) : 48.0);
  text_size_auto_points_ = text_size_spin_->value();
  configure_toolbar_spinbox(text_size_spin_, 74);
  add_option_widget(text_size_spin_, {CanvasTool::Text});
  add_option_label(QT_TR_NOOP("Smoothing:"), {CanvasTool::Text});
  text_smoothing_combo_ = new QComboBox(toolbar);
  text_smoothing_combo_->setObjectName(QStringLiteral("textSmoothingCombo"));
  bind_tooltip(text_smoothing_combo_, QT_TR_NOOP("Text smoothing"));
  text_smoothing_combo_->addItem(tr("None"), 0);
  text_smoothing_combo_->addItem(tr("Sharp"), 4);
  text_smoothing_combo_->addItem(tr("Crisp"), 2);
  text_smoothing_combo_->addItem(tr("Strong"), 1);
  text_smoothing_combo_->addItem(tr("Smooth"), 3);
  text_smoothing_combo_->addItem(tr("Windows LCD"), 5);
  text_smoothing_combo_->addItem(tr("Windows"), 6);
  register_retranslation([this] {
    const QSignalBlocker blocker(text_smoothing_combo_);
    const char* sources[] = {QT_TR_NOOP("None"), QT_TR_NOOP("Sharp"), QT_TR_NOOP("Crisp"),
                             QT_TR_NOOP("Strong"), QT_TR_NOOP("Smooth"), QT_TR_NOOP("Windows LCD"), QT_TR_NOOP("Windows")};
    for (int index = 0; index < 7; ++index) text_smoothing_combo_->setItemText(index, tr(sources[index]));
  });
  text_smoothing_combo_->setFixedWidth(116);
  set_text_smoothing_combo_value(
      text_smoothing_combo_,
      app_settings().value(QStringLiteral("tools/textSmoothing"), kDefaultTextAntiAlias).toInt());
  add_option_widget(text_smoothing_combo_, {CanvasTool::Text});
  add_option_label(QT_TR_NOOP("Color:"), {CanvasTool::Text});
  text_color_button_ = new QPushButton(tr("T"), toolbar);
  text_color_button_->setObjectName(QStringLiteral("textColorButton"));
  bind_tooltip(text_color_button_, QT_TR_NOOP("Text color"));
  text_color_button_->setFixedSize(30, 26);
  add_option_widget(text_color_button_, {CanvasTool::Text});
  add_option_label(QT_TR_NOOP("Align:"), {CanvasTool::Text});
  auto* text_alignment_group = new QButtonGroup(toolbar);
  text_alignment_group->setExclusive(true);
  text_align_left_button_ = new QPushButton(tr("L"), toolbar);
  text_align_left_button_->setObjectName(QStringLiteral("textAlignLeftButton"));
  text_align_left_button_->setCheckable(true);
  text_align_left_button_->setChecked(true);
  bind_tooltip(text_align_left_button_, QT_TR_NOOP("Align Left"));
  text_align_left_button_->setFixedSize(30, 26);
  text_alignment_group->addButton(text_align_left_button_);
  add_option_widget(text_align_left_button_, {CanvasTool::Text});
  text_align_center_button_ = new QPushButton(tr("C"), toolbar);
  text_align_center_button_->setObjectName(QStringLiteral("textAlignCenterButton"));
  text_align_center_button_->setCheckable(true);
  bind_tooltip(text_align_center_button_, QT_TR_NOOP("Align Center"));
  text_align_center_button_->setFixedSize(30, 26);
  text_alignment_group->addButton(text_align_center_button_);
  add_option_widget(text_align_center_button_, {CanvasTool::Text});
  text_align_right_button_ = new QPushButton(tr("R"), toolbar);
  text_align_right_button_->setObjectName(QStringLiteral("textAlignRightButton"));
  text_align_right_button_->setCheckable(true);
  bind_tooltip(text_align_right_button_, QT_TR_NOOP("Align Right"));
  text_align_right_button_->setFixedSize(30, 26);
  text_alignment_group->addButton(text_align_right_button_);
  add_option_widget(text_align_right_button_, {CanvasTool::Text});
  // Photoshop's "Toggle text orientation": one Type tool with a vertical switch instead of a
  // second tool. Qt::NoFocus like the session buttons, so toggling never auto-commits.
  text_orientation_button_ = new QPushButton(tr("Vertical"), toolbar);
  bind_widget_text(text_orientation_button_, QT_TR_NOOP("Vertical"));
  text_orientation_button_->setObjectName(QStringLiteral("textOrientationButton"));
  text_orientation_button_->setCheckable(true);
  text_orientation_button_->setChecked(false);
  bind_tooltip(text_orientation_button_, QT_TR_NOOP("Vertical text: columns read top to bottom, right to left"));
  text_orientation_button_->setFocusPolicy(Qt::NoFocus);
  add_option_widget(text_orientation_button_, {CanvasTool::Text});
  add_option_label(QT_TR_NOOP("Direction:"), {CanvasTool::Text});
  text_direction_combo_ = new QComboBox(toolbar);
  text_direction_combo_->setObjectName(QStringLiteral("textDirectionCombo"));
  bind_tooltip(text_direction_combo_, QT_TR_NOOP("Paragraph direction (auto follows the first strong character)"));
  text_direction_combo_->addItem(tr("Auto"), static_cast<int>(Qt::LayoutDirectionAuto));
  text_direction_combo_->addItem(tr("Left to right"), static_cast<int>(Qt::LeftToRight));
  text_direction_combo_->addItem(tr("Right to left"), static_cast<int>(Qt::RightToLeft));
  register_retranslation([this] {
    const QSignalBlocker blocker(text_direction_combo_);
    const char* sources[] = {QT_TR_NOOP("Auto"), QT_TR_NOOP("Left to right"), QT_TR_NOOP("Right to left")};
    for (int index = 0; index < 3; ++index) text_direction_combo_->setItemText(index, tr(sources[index]));
  });
  text_direction_combo_->setFixedWidth(116);
  add_option_widget(text_direction_combo_, {CanvasTool::Text});
  text_warp_button_ = new QPushButton(tr("Warp..."), toolbar);
  bind_widget_text(text_warp_button_, QT_TR_NOOP("Warp..."));
  text_warp_button_->setObjectName(QStringLiteral("textWarpButton"));
  bind_tooltip(text_warp_button_, QT_TR_NOOP("Warp Text (Photoshop-style styles: arc, flag, fish, ...)"));
  add_option_widget(text_warp_button_, {CanvasTool::Text});
  // Character panel: works on the LIVE editor session, so Qt::NoFocus is load-bearing here
  // exactly like the session apply/cancel buttons (a focus-taking button would fire the
  // editor's focus-loss auto-commit on mouse press).
  text_character_button_ = new QPushButton(tr("Character..."), toolbar);
  bind_widget_text(text_character_button_, QT_TR_NOOP("Character..."));
  text_character_button_->setObjectName(QStringLiteral("textCharacterButton"));
  bind_tooltip(text_character_button_, QT_TR_NOOP("Character panel (leading, tracking, glyph scales)"));
  text_character_button_->setFocusPolicy(Qt::NoFocus);
  add_option_widget(text_character_button_, {CanvasTool::Text});
  connect(text_character_button_, &QPushButton::clicked, this, [this] { open_text_character_dialog(); });
  // Paragraph panel: same live-session rules as the Character panel (Qt::NoFocus).
  text_paragraph_button_ = new QPushButton(tr("Paragraph..."), toolbar);
  bind_widget_text(text_paragraph_button_, QT_TR_NOOP("Paragraph..."));
  text_paragraph_button_->setObjectName(QStringLiteral("textParagraphButton"));
  bind_tooltip(text_paragraph_button_, QT_TR_NOOP("Paragraph panel (alignment, indents, spacing)"));
  text_paragraph_button_->setFocusPolicy(Qt::NoFocus);
  add_option_widget(text_paragraph_button_, {CanvasTool::Text});
  connect(text_paragraph_button_, &QPushButton::clicked, this, [this] { open_text_paragraph_dialog(); });
  connect(text_font_combo_, &QFontComboBox::currentFontChanged, this, [this](const QFont& chosen) {
    // Repopulate before applying: apply_text_family_to_active_editor renders with the style the
    // combo is showing, and the outgoing family's style list may not contain it.
    refresh_text_style_combo(chosen.family(), current_text_style_name());
    apply_text_family_to_active_editor();
  });
  connect(text_style_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { apply_text_style_to_active_editor(); });
  connect(text_size_spin_, &QDoubleSpinBox::valueChanged, this,
          [this](double) {
    apply_text_size_to_active_editor();
    refresh_document_info();
  });
  connect(text_smoothing_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) {
            apply_text_smoothing_to_active_editor();
            save_tool_settings();
            refresh_document_info();
          });
  connect(text_color_button_, &QPushButton::clicked, this, [this] { choose_text_color(); });
  connect(text_align_left_button_, &QPushButton::clicked, this,
          [this] { apply_text_alignment_to_active_editor(Qt::AlignLeft); });
  connect(text_align_center_button_, &QPushButton::clicked, this,
          [this] { apply_text_alignment_to_active_editor(Qt::AlignHCenter); });
  connect(text_align_right_button_, &QPushButton::clicked, this,
          [this] { apply_text_alignment_to_active_editor(Qt::AlignRight); });
  connect(text_warp_button_, &QPushButton::clicked, this, [this] { request_warp_text_dialog(); });
  connect(text_orientation_button_, &QPushButton::toggled, this,
          [this](bool checked) { apply_text_orientation(checked); });
  connect(text_direction_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    if (index < 0) {
      return;
    }
    apply_text_direction_to_active_editor(
        static_cast<Qt::LayoutDirection>(text_direction_combo_->itemData(index).toInt()));
  });
  // Session apply/cancel, shown only while an inline text editor is open (the
  // text controls above stay visible too -- they apply live to the editor, so
  // unlike a transform session the bar keeps them).  Qt::NoFocus is load-bearing:
  // a focus-taking button would fire the editor's focus-loss auto-commit on
  // mouse press, committing the text before a Cancel click could cancel it.
  text_apply_button_ = new QPushButton(toolbar);
  text_apply_button_->setObjectName(QStringLiteral("textApplyButton"));
  text_apply_button_->setIcon(simple_icon(QStringLiteral("ok"), QColor(160, 220, 165)));
  bind_tooltip(text_apply_button_, QT_TR_NOOP("Apply text edit"));
  text_apply_button_->setFixedWidth(30);
  text_apply_button_->setIconSize(QSize(20, 20));
  text_apply_button_->setProperty("optionsSessionButton", true);
  text_apply_button_->setFocusPolicy(Qt::NoFocus);
  options_flow->addWidget(text_apply_button_);
  text_cancel_button_ = new QPushButton(toolbar);
  text_cancel_button_->setObjectName(QStringLiteral("textCancelButton"));
  text_cancel_button_->setIcon(simple_icon(QStringLiteral("clear"), QColor(255, 150, 150)));
  bind_tooltip(text_cancel_button_, QT_TR_NOOP("Cancel text edit"));
  text_cancel_button_->setFixedWidth(30);
  text_cancel_button_->setIconSize(QSize(20, 20));
  text_cancel_button_->setProperty("optionsSessionButton", true);
  text_cancel_button_->setFocusPolicy(Qt::NoFocus);
  options_flow->addWidget(text_cancel_button_);
  connect(text_apply_button_, &QPushButton::clicked, this, [this] { commit_active_text_editor(); });
  connect(text_cancel_button_, &QPushButton::clicked, this, [this] { cancel_active_text_editor(); });
}

}  // namespace patchy::ui
