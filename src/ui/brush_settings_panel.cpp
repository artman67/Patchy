#include "ui/brush_settings_panel.hpp"

#include "core/pixel_tools.hpp"
#include "ui/action_icons.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_stroke_preview.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/pattern_library.hpp"
#include "ui/theme_palette.hpp"
#include "ui/theme_qss.hpp"
#include "ui/unit_spin_box.hpp"

#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace patchy::ui {

namespace {

constexpr auto kContext = "patchy::ui::BrushSettingsPanel";
constexpr int kSectionRole = Qt::UserRole + 1;
// Coalescing window for slider drags: fast enough to feel live, slow enough that a drag does
// not push (and preview) every intermediate value.
constexpr int kFlushDelayMs = 40;
// Below this width the section list sits above the page instead of beside it.
constexpr int kSideBySideMinimumWidth = 470;

QString panel_tr(const char* source) {
  return QCoreApplication::translate(kContext, source);
}

template <typename Widget>
Widget* bound(Widget* widget, const char* text, const char* tooltip = nullptr) {
  if (text != nullptr) {
    bind_translated_text(widget, text, kContext);
  }
  if (tooltip != nullptr) {
    bind_translated_tooltip(widget, tooltip, kContext);
  }
  apply_bound_translation(widget);
  return widget;
}

// Keeps a list between a minimum and a maximum number of rows. The application sheet's
// "QListWidget, QTreeWidget { min-height: 20px }" overrides an explicit minimum whenever the
// style polishes the widget, after which a short dock squeezed the list to one row; re-apply
// the heights after every polish and font change.
class RowsHeight : public QObject {
public:
  // height(rows) is the widget height that shows that many rows.
  RowsHeight(QAbstractItemView& view, int minimum_rows, int maximum_rows, std::function<int(int)> height)
      : QObject(&view), view_(view), minimum_rows_(minimum_rows), maximum_rows_(maximum_rows),
        height_(std::move(height)) {
    view.installEventFilter(this);
    apply();
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == &view_ && (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange ||
                              event->type() == QEvent::FontChange)) {
      QTimer::singleShot(0, this, [this] { apply(); });
    }
    return false;
  }

  void apply() {
    view_.setMinimumHeight(height_(minimum_rows_));
    view_.setMaximumHeight(height_(maximum_rows_));
  }

private:
  QAbstractItemView& view_;
  int minimum_rows_;
  int maximum_rows_;
  std::function<int(int)> height_;
};

QString brush_settings_key(const WorkingBrush& brush) {
  return working_brush_tip_key(brush) + QLatin1Char('|') +
         QString::fromUtf8(QJsonDocument(working_brush_to_json(brush)).toJson(QJsonDocument::Compact));
}

}  // namespace

// The live stroke strip under the settings: the working brush rendered by the real engine,
// re-rendered at most once per throttle window while settings change.
class BrushStrokePreviewStrip : public QWidget {
public:
  explicit BrushStrokePreviewStrip(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("brushSettingsPreview"));
    setMinimumHeight(44);
    setMaximumHeight(44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    throttle_.setSingleShot(true);
    throttle_.setInterval(80);
    QObject::connect(&throttle_, &QTimer::timeout, this, [this] { update(); });
  }

  void set_brush(const WorkingBrush& brush, std::shared_ptr<const patchy::BrushTip> tip,
                 std::shared_ptr<const patchy::BrushTextureTile> texture_tile) {
    brush_ = brush;
    tip_ = std::move(tip);
    texture_tile_ = std::move(texture_tile);
    if (!throttle_.isActive()) {
      throttle_.start();
    }
  }

protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    const auto& palette = theme();
    QPainter painter(this);
    painter.fillRect(rect(), palette.panel_inset_bg);
    painter.setPen(palette.panel_inset_border);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    BrushStrokePreviewSpec spec;
    spec.tip = tip_;
    spec.square = tip_ == nullptr && brush_.tip_id == builtin_square_brush_tip_id();
    spec.softness = brush_.softness;
    spec.spacing = brush_.spacing;
    spec.angle = brush_.angle;
    spec.roundness = brush_.roundness;
    spec.dynamics = brush_.dynamics;
    spec.texture_tile = texture_tile_;
    const QSize area(width() - 8, height() - 8);
    const auto brush_size = std::clamp(brush_.size, 2, std::max(2, area.height() * 3 / 5));
    const auto pixmap =
        brush_stroke_preview_pixmap(brush_settings_key(brush_), spec, area, palette.text_primary, brush_size);
    if (!pixmap.isNull()) {
      painter.drawPixmap(4, 4, pixmap);
    }
  }

private:
  WorkingBrush brush_;
  std::shared_ptr<const patchy::BrushTip> tip_;
  std::shared_ptr<const patchy::BrushTextureTile> texture_tile_;
  QTimer throttle_;
};

BrushSettingsPanel::BrushSettingsPanel(CurrentBrush& brush, BrushTipLibrary& tips, QWidget* parent)
    : QWidget(parent), brush_(brush), tips_(tips) {
  setObjectName(QStringLiteral("brushSettingsPanel"));
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  body_layout_ = new QBoxLayout(QBoxLayout::TopToBottom);
  body_layout_->setSpacing(6);
  root->addLayout(body_layout_, 1);

  section_list_ = new QTreeWidget(this);
  section_list_->setObjectName(QStringLiteral("brushSettingsSectionList"));
  section_list_->setColumnCount(2);
  section_list_->setHeaderHidden(true);
  section_list_->setRootIsDecorated(false);
  section_list_->setUniformRowHeights(true);
  section_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  section_list_->setFocusPolicy(Qt::NoFocus);
  section_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  section_list_->setIconSize(QSize(16, 16));
  section_list_->header()->setStretchLastSection(false);
  section_list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  section_list_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  section_list_->header()->resizeSection(1, 26);
  section_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  // The rows' enable boxes look like every other checkbox (the application sheet styles
  // QCheckBox, not item-view indicators).
  set_themed_style(*section_list_,
                   QStringLiteral("QTreeWidget#brushSettingsSectionList::indicator { width: 12px; height: 12px; "
                                  "background: @checkbox_indicator_bg; border: 1px solid @checkbox_indicator_border; }"
                                  "QTreeWidget#brushSettingsSectionList::indicator:checked { background: @accent; "
                                  "border-color: @checkbox_accent_border; image: url(@icon(checkmark)); }"
                                  "QTreeWidget#brushSettingsSectionList::indicator:disabled { "
                                  "background: @field_bg_disabled; border-color: @field_border_disabled; }"
                                  "QTreeWidget#brushSettingsSectionList::item { padding: 1px 0; }"));
  for (const auto section : kBrushSections) {
    auto* item = new QTreeWidgetItem(section_list_);
    item->setData(0, kSectionRole, static_cast<int>(section));
    auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (section != BrushSection::TipShape) {
      flags |= Qt::ItemIsUserCheckable;
      item->setCheckState(0, Qt::Unchecked);
    }
    item->setFlags(flags);
  }
  body_layout_->addWidget(section_list_);

  page_scroll_ = new QScrollArea(this);
  page_scroll_->setObjectName(QStringLiteral("brushSettingsPageScroll"));
  page_scroll_->setWidgetResizable(true);
  page_scroll_->setFrameShape(QFrame::NoFrame);
  page_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  page_scroll_->setMinimumHeight(140);
  pages_ = new QStackedWidget(page_scroll_);
  pages_->setObjectName(QStringLiteral("brushSettingsPages"));
  page_scroll_->setWidget(pages_);
  body_layout_->addWidget(page_scroll_, 1);

  form_ = new BrushDynamicsPanel(this, BrushDynamicsPanel::Presentation::Pages);
  form_->set_pattern_library(tips_.pattern_library());
  for (const auto section : kBrushSections) {
    QWidget* page = nullptr;
    if (section == BrushSection::TipShape) {
      page = new QWidget(pages_);
      page->setObjectName(QStringLiteral("brushSettingsTipShapePage"));
      build_tip_shape_page(page);
    } else if (section == BrushSection::BuildUp) {
      page = new QWidget(pages_);
      page->setObjectName(QStringLiteral("brushSettingsPageBuildUp"));
      auto* layout = new QVBoxLayout(page);
      layout->setContentsMargins(0, 0, 0, 0);
      auto* about = bound(new QLabel(page), QT_TR_NOOP("Builds paint while the pointer is held still, like "
                                                       "the Airbrush option. Tick the box to turn it on."));
      about->setWordWrap(true);
      layout->addWidget(about);
      layout->addStretch(1);
    } else if (section == BrushSection::Smoothing) {
      page = new QWidget(pages_);
      page->setObjectName(QStringLiteral("brushSettingsPageSmoothing"));
      build_smoothing_page(page);
    } else {
      page = form_->section_page(section);
    }
    pages_->addWidget(page);
    page_for_section_[section] = page;
  }
  connect(form_, &BrushDynamicsPanel::edited, this, [this] {
    if (updating_) {
      return;
    }
    if (!pending_brush_edit_) {
      pending_brush_edit_.emplace();
    }
    pending_brush_edit_->dynamics = form_->dynamics();
    pending_brush_edit_->angle = form_->base_angle_degrees();
    pending_brush_edit_->roundness = form_->base_roundness();
    schedule_flush();
  });

  lock_banner_ = new QWidget(this);
  lock_banner_->setObjectName(QStringLiteral("brushSettingsLockBanner"));
  lock_banner_->setAttribute(Qt::WA_StyledBackground, true);
  set_themed_style(*lock_banner_,
                   QStringLiteral("QWidget#brushSettingsLockBanner { background: @info_banner_bg; "
                                  "border: 1px solid @info_banner_border; border-radius: 3px; }"
                                  "QLabel { color: @info_banner_text; background: transparent; border: none; }"));
  auto* banner_layout = new QHBoxLayout(lock_banner_);
  banner_layout->setContentsMargins(6, 3, 4, 3);
  banner_layout->setSpacing(6);
  auto* banner_icon = new QLabel(lock_banner_);
  banner_icon->setPixmap(simple_icon(QStringLiteral("lock"), &ThemePalette::info_banner_text).pixmap(16, 16));
  banner_layout->addWidget(banner_icon);
  lock_banner_label_ = new QLabel(lock_banner_);
  lock_banner_label_->setObjectName(QStringLiteral("brushSettingsLockBannerLabel"));
  lock_banner_label_->setWordWrap(true);
  banner_layout->addWidget(lock_banner_label_, 1);
  auto* unlock_all = bound(new QPushButton(lock_banner_), QT_TR_NOOP("Unlock All"),
                           QT_TR_NOOP("Reset All Locked Settings: unlock every section"));
  unlock_all->setObjectName(QStringLiteral("brushSettingsUnlockAllButton"));
  unlock_all->setFocusPolicy(Qt::NoFocus);
  connect(unlock_all, &QPushButton::clicked, this, [this] { brush_.set_locks(0U); });
  banner_layout->addWidget(unlock_all);
  root->addWidget(lock_banner_);

  preview_ = new BrushStrokePreviewStrip(this);
  root->addWidget(preview_);

  auto* footer = new QHBoxLayout();
  footer->setSpacing(4);
  auto* menu_button = new QToolButton(this);
  menu_button->setObjectName(QStringLiteral("brushSettingsMenuButton"));
  menu_button->setText(QStringLiteral("..."));
  bound(menu_button, nullptr, QT_TR_NOOP("Brush Settings options"));
  menu_button->setPopupMode(QToolButton::InstantPopup);
  menu_button->setFocusPolicy(Qt::NoFocus);
  auto* menu = new QMenu(menu_button);
  menu->setObjectName(QStringLiteral("brushSettingsMenu"));
  auto* clear_action = menu->addAction(QString());
  clear_action->setObjectName(QStringLiteral("brushSettingsClearControlsAction"));
  bound(clear_action, QT_TR_NOOP("Clear Brush Controls"));
  connect(clear_action, &QAction::triggered, this, [this] { clear_brush_controls(); });
  auto* reset_locks_action = menu->addAction(QString());
  reset_locks_action->setObjectName(QStringLiteral("brushSettingsResetLocksAction"));
  bound(reset_locks_action, QT_TR_NOOP("Reset All Locked Settings"));
  connect(reset_locks_action, &QAction::triggered, this, [this] { brush_.set_locks(0U); });
  menu_button->setMenu(menu);
  footer->addWidget(menu_button);
  footer->addStretch(1);
  auto* new_button = new QToolButton(this);
  new_button->setObjectName(QStringLiteral("brushSettingsNewPresetButton"));
  new_button->setIcon(simple_icon(QStringLiteral("new")));
  new_button->setIconSize(QSize(20, 20));
  new_button->setFocusPolicy(Qt::NoFocus);
  bound(new_button, nullptr, QT_TR_NOOP("Create new brush preset from these settings"));
  connect(new_button, &QToolButton::clicked, this, [this] {
    flush_pending_edit();
    emit new_preset_requested();
  });
  footer->addWidget(new_button);
  root->addLayout(footer);

  flush_timer_ = new QTimer(this);
  flush_timer_->setSingleShot(true);
  flush_timer_->setInterval(kFlushDelayMs);
  connect(flush_timer_, &QTimer::timeout, this, &BrushSettingsPanel::flush_pending_edit);

  connect(section_list_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
    if (item == nullptr) {
      return;
    }
    show_section_page(static_cast<BrushSection>(item->data(0, kSectionRole).toInt()));
  });
  connect(section_list_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
    if (updating_ || column != 0 || !(item->flags() & Qt::ItemIsUserCheckable)) {
      return;
    }
    const auto section = static_cast<BrushSection>(item->data(0, kSectionRole).toInt());
    const auto checked = item->checkState(0) == Qt::Checked;
    if (checked != section_checked(section)) {
      section_toggled(section, checked);
    }
  });
  connect(section_list_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
    const auto section = static_cast<BrushSection>(item->data(0, kSectionRole).toInt());
    if (column == 1 && brush_section_lockable(section)) {
      toggle_lock(section);
    }
  });
  connect(section_list_, &QTreeWidget::customContextMenuRequested, this,
          &BrushSettingsPanel::show_section_menu);

  connect(&brush_, &CurrentBrush::changed, this, &BrushSettingsPanel::on_brush_changed);
  connect(&brush_, &CurrentBrush::locks_changed, this, [this] { refresh_locks(); });
  connect(&tips_, &BrushTipLibrary::changed, this, [this] {
    rebuild_tip_list();
    refresh_spacing_row();
    refresh_preview();
  });
  if (auto* patterns = tips_.pattern_library(); patterns != nullptr) {
    connect(patterns, &PatternLibrary::changed, this, [this] {
      texture_tile_id_.clear();
      texture_tile_.reset();
      refresh_preview();
    });
  }

  retranslate();
  rebuild_tip_list();
  refresh_from_brush(true);
  refresh_locks();
  section_list_->setCurrentItem(section_item(BrushSection::TipShape));
  show_section_page(BrushSection::TipShape);
  // Every section shows when there is room; a short dock keeps five rows and scrolls the rest,
  // so the selected section's controls keep their space.
  section_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  new RowsHeight(*section_list_, 5, static_cast<int>(kBrushSections.size()), [list = section_list_](int rows) {
    const auto row_height = std::max(list->sizeHintForRow(0), 18);
    return row_height * rows + 2 * list->frameWidth() + 2;
  });
  install_scrub_labels_in(this);
  append_themed_style(*this, dialog_spinbox_button_style());
}

void BrushSettingsPanel::build_tip_shape_page(QWidget* page) {
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  tip_list_ = new QListWidget(page);
  tip_list_->setObjectName(QStringLiteral("brushSettingsTipList"));
  tip_list_->setViewMode(QListView::IconMode);
  tip_list_->setMovement(QListView::Static);
  tip_list_->setResizeMode(QListView::Adjust);
  tip_list_->setIconSize(QSize(32, 32));
  tip_list_->setGridSize(QSize(38, 38));
  tip_list_->setUniformItemSizes(true);
  new RowsHeight(*tip_list_, 2, 2, [list = tip_list_](int rows) { return rows * 38 + 2 * list->frameWidth() + 2; });
  bound(tip_list_, nullptr, QT_TR_NOOP("Brush tips: click one to paint with it"));
  connect(tip_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    flush_pending_edit();
    emit tip_picked(item->data(Qt::UserRole).toString());
  });
  layout->addWidget(tip_list_);

  // Photoshop's order with Soft standing in for Hardness: Size, the angle/roundness block with
  // the flips, Soft, Spacing. One grid keeps the rows' sliders aligned.
  auto* grid = new QGridLayout();
  grid->setContentsMargins(0, 0, 0, 0);
  const auto add_row = [this, page, grid](int row, const char* label, const char* name, int minimum, int maximum,
                                          bool curved, bool percent) {
    grid->addWidget(bound(new QLabel(page), label), row, 0);
    auto* slider = new QSlider(Qt::Horizontal, page);
    slider->setObjectName(QLatin1String(name) + QStringLiteral("Slider"));
    QSpinBox* spin = percent ? new UnitIntSpinBox(SpinUnit::Percent, page) : new QSpinBox(page);
    spin->setObjectName(QLatin1String(name));
    spin->setRange(minimum, maximum);
    if (curved) {
      bind_curved_slider(*slider, *spin);
    } else {
      slider->setRange(minimum, maximum);
      connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
      connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    }
    slider->setMinimumWidth(60);
    grid->addWidget(slider, row, 1);
    grid->addWidget(spin, row, 2);
    return std::pair{slider, spin};
  };
  std::tie(size_slider_, size_spin_) =
      add_row(0, QT_TR_NOOP("Size:"), "brushSettingsSizeSpin", 1, kMaxBrushSize, true, false);
  grid->addWidget(form_->section_page(BrushSection::TipShape), 1, 0, 1, 3);
  std::tie(softness_slider_, softness_spin_) =
      add_row(2, QT_TR_NOOP("Soft:"), "brushSettingsSoftSpin", 0, 100, false, true);
  grid->setColumnStretch(1, 1);
  layout->addLayout(grid);
  connect(size_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
    if (updating_) {
      return;
    }
    if (!pending_size_group_edit_) {
      pending_size_group_edit_.emplace();
    }
    pending_size_group_edit_->size = value;
    schedule_flush();
  });
  connect(softness_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
    if (updating_) {
      return;
    }
    if (!pending_size_group_edit_) {
      pending_size_group_edit_.emplace();
    }
    pending_size_group_edit_->softness = value;
    schedule_flush();
  });

  spacing_check_ = bound(new QCheckBox(page), QT_TR_NOOP("Spacing"),
                         QT_TR_NOOP("Off keeps the tip's own spacing (Round and Square paint a smooth stroke); "
                                    "on sets the distance between stamps as a percentage of the brush size"));
  spacing_check_->setObjectName(QStringLiteral("brushSettingsSpacingCheck"));
  grid->addWidget(spacing_check_, 3, 0);
  spacing_slider_ = new QSlider(Qt::Horizontal, page);
  spacing_slider_->setObjectName(QStringLiteral("brushSettingsSpacingSpinSlider"));
  spacing_slider_->setRange(1, 1000);
  spacing_slider_->setMinimumWidth(60);
  spacing_spin_ = new UnitIntSpinBox(SpinUnit::Percent, page);
  spacing_spin_->setObjectName(QStringLiteral("brushSettingsSpacingSpin"));
  spacing_spin_->setRange(1, 1000);
  connect(spacing_slider_, &QSlider::valueChanged, spacing_spin_, &QSpinBox::setValue);
  connect(spacing_spin_, qOverload<int>(&QSpinBox::valueChanged), spacing_slider_, &QSlider::setValue);
  grid->addWidget(spacing_slider_, 3, 1);
  grid->addWidget(spacing_spin_, 3, 2);
  const auto spacing_edited = [this] {
    if (updating_) {
      return;
    }
    if (!pending_brush_edit_) {
      pending_brush_edit_.emplace();
    }
    pending_brush_edit_->spacing = spacing_check_->isChecked()
                                       ? std::optional<double>(spacing_spin_->value() / 100.0)
                                       : std::optional<double>();
    refresh_spacing_row();
    schedule_flush();
  };
  connect(spacing_check_, &QCheckBox::toggled, this, spacing_edited);
  connect(spacing_spin_, qOverload<int>(&QSpinBox::valueChanged), this, spacing_edited);
  layout->addStretch(1);
}

void BrushSettingsPanel::build_smoothing_page(QWidget* page) {
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  auto* grid = new QGridLayout();
  grid->addWidget(bound(new QLabel(page), QT_TR_NOOP("Smoothing:")), 0, 0);
  smoothing_slider_ = new QSlider(Qt::Horizontal, page);
  smoothing_slider_->setObjectName(QStringLiteral("brushSettingsSmoothingSpinSlider"));
  smoothing_slider_->setRange(0, 100);
  smoothing_slider_->setMinimumWidth(60);
  smoothing_spin_ = new UnitIntSpinBox(SpinUnit::Percent, page);
  smoothing_spin_->setObjectName(QStringLiteral("brushSettingsSmoothingSpin"));
  smoothing_spin_->setRange(0, 100);
  bound(smoothing_spin_, nullptr, QT_TR_NOOP("Stroke smoothing - 0% paints the raw pointer path"));
  connect(smoothing_slider_, &QSlider::valueChanged, smoothing_spin_, &QSpinBox::setValue);
  connect(smoothing_spin_, qOverload<int>(&QSpinBox::valueChanged), smoothing_slider_, &QSlider::setValue);
  grid->addWidget(smoothing_slider_, 0, 1);
  grid->addWidget(smoothing_spin_, 0, 2);
  grid->setColumnStretch(1, 1);
  layout->addLayout(grid);
  const auto add_check = [page, layout](const char* name, const char* text) {
    auto* check = bound(new QCheckBox(page), text);
    check->setObjectName(QLatin1String(name));
    layout->addWidget(check);
    return check;
  };
  pulled_string_check_ = add_check("brushSettingsPulledStringCheck", QT_TR_NOOP("Pulled String Mode"));
  catch_up_check_ = add_check("brushSettingsCatchUpCheck", QT_TR_NOOP("Stroke Catch-up"));
  catch_up_end_check_ = add_check("brushSettingsCatchUpEndCheck", QT_TR_NOOP("Catch-up on Stroke End"));
  zoom_adjust_check_ = add_check("brushSettingsZoomAdjustCheck", QT_TR_NOOP("Adjust for Zoom"));
  layout->addStretch(1);
  const auto edited = [this] {
    if (updating_) {
      return;
    }
    Smoothing next;
    next.amount = smoothing_spin_->value();
    next.pulled_string = pulled_string_check_->isChecked();
    next.catch_up = catch_up_check_->isChecked();
    next.catch_up_end = catch_up_end_check_->isChecked();
    next.zoom_adjust = zoom_adjust_check_->isChecked();
    smoothing_ = next;
    pending_smoothing_ = next;
    refresh_section_states();
    schedule_flush();
  };
  connect(smoothing_spin_, qOverload<int>(&QSpinBox::valueChanged), this, edited);
  for (auto* check : {pulled_string_check_, catch_up_check_, catch_up_end_check_, zoom_adjust_check_}) {
    connect(check, &QCheckBox::toggled, this, edited);
  }
}

void BrushSettingsPanel::rebuild_tip_list() {
  if (tip_list_ == nullptr) {
    return;
  }
  const QSignalBlocker blocker(tip_list_);
  tip_list_->clear();
  const auto add = [this](const QString& id, const QString& name, const QPixmap& thumbnail) {
    auto* item = new QListWidgetItem(QIcon(thumbnail), QString(), tip_list_);
    item->setData(Qt::UserRole, id);
    item->setToolTip(name);
  };
  add(builtin_round_brush_tip_id(), panel_tr(QT_TR_NOOP("Round")), procedural_brush_thumbnail(false, 0, 32));
  add(builtin_square_brush_tip_id(), panel_tr(QT_TR_NOOP("Square")), procedural_brush_thumbnail(true, 0, 32));
  for (const auto& entry : tips_.entries()) {
    add(entry.id, entry.name, brush_tip_thumbnail_with_badge(entry));
  }
  refresh_from_brush(false);
}

void BrushSettingsPanel::on_brush_changed(unsigned changes) {
  if ((changes & CurrentBrush::Base) != 0U) {
    // A pick replaces the brush the parked values and pending edits belonged to.
    parked_.clear();
    parked_smoothing_.reset();
    armed_.clear();
    pending_brush_edit_.reset();
  }
  refresh_from_brush(!pending_brush_edit_.has_value());
}

void BrushSettingsPanel::refresh_from_brush(bool reload_form) {
  const auto& brush = brush_.brush();
  updating_ = true;
  if (reload_form && form_ != nullptr) {
    // Our own coalesced edit echoes back unchanged; reloading it would only fight the control
    // under the pointer.
    const auto same = brush_dynamics_to_json(form_->dynamics()) == brush_dynamics_to_json(brush.dynamics) &&
                      form_->base_angle_degrees() == std::round(brush.angle) &&
                      form_->base_roundness() == std::round(brush.roundness);
    if (!same) {
      form_->set_values(brush.dynamics, brush.angle, brush.roundness);
    }
  }
  if (tip_list_ != nullptr) {
    const auto current = brush.snapshot_tip != nullptr ? QString() : brush.tip_id;
    QListWidgetItem* match = nullptr;
    for (int row = 0; row < tip_list_->count(); ++row) {
      if (tip_list_->item(row)->data(Qt::UserRole).toString() == current) {
        match = tip_list_->item(row);
      }
    }
    const QSignalBlocker blocker(tip_list_);
    if (match != nullptr) {
      tip_list_->setCurrentItem(match);
    } else {
      tip_list_->clearSelection();
      tip_list_->setCurrentItem(nullptr);
    }
  }
  updating_ = false;
  refresh_spacing_row();
  refresh_section_states();
  refresh_preview();
}

void BrushSettingsPanel::refresh_preview() {
  const auto& brush = brush_.brush();
  const auto& pattern_id = brush.dynamics.texture_pattern_id;
  if (pattern_id != texture_tile_id_) {
    texture_tile_id_ = pattern_id;
    texture_tile_.reset();
    auto* patterns = tips_.pattern_library();
    if (!pattern_id.empty() && patterns != nullptr) {
      if (const auto resource = patterns->resource(QString::fromStdString(pattern_id)); resource.has_value()) {
        auto tile = patchy::make_brush_texture_tile(resource->tile);
        if (!tile.empty()) {
          texture_tile_ = std::make_shared<const patchy::BrushTextureTile>(std::move(tile));
        }
      }
    }
  }
  preview_->set_brush(brush, brush.snapshot_tip != nullptr ? brush.snapshot_tip : tips_.tip(brush.tip_id),
                      texture_tile_);
}

void BrushSettingsPanel::show_section_page(BrushSection section) {
  // Only the shown page sizes the stack, so the scroll range is that page's own height.
  auto* shown = page_for_section_.at(section);
  for (auto& [key, page] : page_for_section_) {
    page->setSizePolicy(page == shown ? QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred)
                                      : QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored));
  }
  pages_->setCurrentWidget(shown);
  pages_->updateGeometry();
  page_scroll_->verticalScrollBar()->setValue(0);
}

std::optional<double> BrushSettingsPanel::tip_default_spacing() const {
  // nullopt for the procedural Round and Square: unset spacing paints their continuous stroke.
  const auto& brush = brush_.brush();
  if (brush.snapshot_tip != nullptr) {
    return brush.snapshot_tip->default_spacing;
  }
  if (const auto* entry = tips_.find_entry(brush.tip_id); entry != nullptr) {
    return entry->spacing;
  }
  return std::nullopt;
}

void BrushSettingsPanel::refresh_spacing_row() {
  if (spacing_check_ == nullptr) {
    return;
  }
  const auto& brush = brush_.brush();
  const auto own = tip_default_spacing();
  const auto pending = pending_brush_edit_ && pending_brush_edit_->spacing.has_value();
  const auto spacing = pending ? *pending_brush_edit_->spacing : brush.spacing;
  updating_ = true;
  spacing_check_->setChecked(spacing.has_value());
  // Unticked shows what ticking starts from: the tip's own spacing, else Photoshop's 25%.
  const auto shown = spacing.value_or(own.value_or(0.25));
  if (spacing_spin_->value() != static_cast<int>(std::lround(shown * 100.0))) {
    spacing_spin_->setValue(static_cast<int>(std::lround(shown * 100.0)));
  }
  const auto live = spacing.has_value();
  spacing_spin_->setEnabled(live);
  spacing_slider_->setEnabled(live);
  updating_ = false;
}

void BrushSettingsPanel::set_size_group(int size, int softness) {
  updating_ = true;
  if (!size_slider_->isSliderDown() && size_spin_->value() != size &&
      !(pending_size_group_edit_ && pending_size_group_edit_->size)) {
    size_spin_->setValue(size);
  }
  if (!softness_slider_->isSliderDown() && softness_spin_->value() != softness &&
      !(pending_size_group_edit_ && pending_size_group_edit_->softness)) {
    softness_spin_->setValue(softness);
  }
  updating_ = false;
}

void BrushSettingsPanel::set_smoothing(const Smoothing& smoothing) {
  if (smoothing == smoothing_ || pending_smoothing_) {
    return;
  }
  smoothing_ = smoothing;
  updating_ = true;
  if (!smoothing_slider_->isSliderDown()) {
    smoothing_spin_->setValue(smoothing.amount);
  }
  pulled_string_check_->setChecked(smoothing.pulled_string);
  catch_up_check_->setChecked(smoothing.catch_up);
  catch_up_end_check_->setChecked(smoothing.catch_up_end);
  zoom_adjust_check_->setChecked(smoothing.zoom_adjust);
  updating_ = false;
  refresh_section_states();
}

void BrushSettingsPanel::set_tool_sections(BrushSectionMask sections) {
  tool_sections_ = sections;
  for (const auto section : kBrushSections) {
    const auto honored = (sections & brush_section_bit(section)) != 0U;
    if (auto* item = section_item(section); item != nullptr) {
      item->setDisabled(!honored);
    }
    page_for_section_.at(section)->setEnabled(honored);
  }
}

void BrushSettingsPanel::select_section(BrushSection section) {
  section_list_->setCurrentItem(section_item(section));
}

BrushSection BrushSettingsPanel::current_section() const {
  const auto* item = section_list_->currentItem();
  return item != nullptr ? static_cast<BrushSection>(item->data(0, kSectionRole).toInt()) : BrushSection::TipShape;
}

QTreeWidgetItem* BrushSettingsPanel::section_item(BrushSection section) const {
  for (int row = 0; row < section_list_->topLevelItemCount(); ++row) {
    auto* item = section_list_->topLevelItem(row);
    if (static_cast<BrushSection>(item->data(0, kSectionRole).toInt()) == section) {
      return item;
    }
  }
  return nullptr;
}

bool BrushSettingsPanel::section_checked(BrushSection section) const {
  if (section == BrushSection::TipShape) {
    return true;
  }
  if (armed_.count(section) != 0U) {
    return true;
  }
  if (section == BrushSection::Smoothing) {
    return smoothing_.amount > 0;
  }
  const auto& brush = brush_.brush();
  if (const auto flag = brush_section_enabled_flag(section, brush); flag.has_value()) {
    return *flag;
  }
  return !brush_section_is_default(section, brush);
}

void BrushSettingsPanel::refresh_section_states() {
  updating_ = true;
  for (const auto section : kBrushSections) {
    auto* item = section_item(section);
    if (item != nullptr && (item->flags() & Qt::ItemIsUserCheckable)) {
      item->setCheckState(0, section_checked(section) ? Qt::Checked : Qt::Unchecked);
    }
  }
  updating_ = false;
}

void BrushSettingsPanel::section_toggled(BrushSection section, bool enabled) {
  flush_pending_edit();
  if (section == BrushSection::Smoothing) {
    auto next = smoothing_;
    if (enabled) {
      next.amount = parked_smoothing_.value_or(10);
    } else {
      parked_smoothing_ = smoothing_.amount;
      next.amount = 0;
    }
    smoothing_ = next;
    updating_ = true;
    smoothing_spin_->setValue(next.amount);
    updating_ = false;
    emit smoothing_edited(next);
    refresh_section_states();
    return;
  }
  auto next = brush_.brush();
  if (brush_section_enabled_flag(section, next).has_value()) {
    set_brush_section_enabled_flag(section, next, enabled);
  } else if (enabled) {
    if (const auto parked = parked_.find(section); parked != parked_.end()) {
      copy_brush_section(section, parked->second, next);
      parked_.erase(parked);
    } else {
      armed_.insert(section);
    }
  } else {
    armed_.erase(section);
    if (!brush_section_is_default(section, next)) {
      parked_[section] = next;
    }
    copy_brush_section(section, WorkingBrush{}, next);
  }
  BrushEdit edit;
  edit.dynamics = next.dynamics;
  edit.airbrush = next.airbrush;
  brush_.edit(edit);
  refresh_section_states();
}

void BrushSettingsPanel::toggle_lock(BrushSection section) {
  brush_.set_locks(brush_.locks() ^ brush_section_bit(section));
}

void BrushSettingsPanel::refresh_locks() {
  const auto locks = brush_.locks();
  QStringList locked_names;
  for (const auto section : kBrushSections) {
    auto* item = section_item(section);
    if (item == nullptr || !brush_section_lockable(section)) {
      continue;
    }
    const auto locked = (locks & brush_section_bit(section)) != 0U;
    // Locks must never go unnoticed: an accent padlock and a bold name on the row, and the
    // banner under the page names every locked section.
    item->setIcon(1, locked ? simple_icon(QStringLiteral("lock"), &ThemePalette::accent)
                            : simple_icon(QStringLiteral("unlock"), &ThemePalette::text_disabled));
    item->setToolTip(1, locked ? panel_tr(QT_TR_NOOP("Locked: picking another brush keeps these settings. "
                                                     "Click to unlock."))
                               : panel_tr(QT_TR_NOOP("Click to lock: picking another brush keeps these "
                                                     "settings")));
    auto font = item->font(0);
    font.setBold(locked);
    item->setFont(0, font);
    if (locked) {
      locked_names << panel_tr(brush_section_title_source(section));
    }
  }
  lock_banner_label_->setText(panel_tr(QT_TR_NOOP("Locked: %1")).arg(locked_names.join(QStringLiteral(", "))));
  lock_banner_->setVisible(!locked_names.isEmpty());
}

void BrushSettingsPanel::clear_brush_controls() {
  flush_pending_edit();
  // Photoshop's Clear Brush Controls: every dynamics section off, locked ones kept; the tip
  // shape (its flips ride in the dynamics) stays.
  auto next = picked_over(brush_.brush(), [this] {
    auto cleared = brush_.brush();
    cleared.dynamics = {};
    copy_brush_section(BrushSection::TipShape, brush_.brush(), cleared);
    return cleared;
  }(), brush_.locks());
  BrushEdit edit;
  edit.dynamics = next.dynamics;
  brush_.edit(edit);
  parked_.clear();
  armed_.clear();
  refresh_section_states();
}

void BrushSettingsPanel::show_section_menu(const QPoint& position) {
  auto* item = section_list_->itemAt(position);
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("brushSettingsSectionMenu"));
  if (item != nullptr) {
    const auto section = static_cast<BrushSection>(item->data(0, kSectionRole).toInt());
    if (brush_section_lockable(section)) {
      const auto locked = (brush_.locks() & brush_section_bit(section)) != 0U;
      auto* lock = menu.addAction(locked ? panel_tr(QT_TR_NOOP("Unlock Section")) : panel_tr(QT_TR_NOOP("Lock Section")));
      connect(lock, &QAction::triggered, this, [this, section] { toggle_lock(section); });
    }
  }
  auto* reset = menu.addAction(panel_tr(QT_TR_NOOP("Reset All Locked Settings")));
  reset->setEnabled(brush_.locks() != 0U);
  connect(reset, &QAction::triggered, this, [this] { brush_.set_locks(0U); });
  menu.exec(section_list_->viewport()->mapToGlobal(position));
}

void BrushSettingsPanel::schedule_flush() {
  flush_timer_->start();
}

void BrushSettingsPanel::flush_pending_edit() {
  flush_timer_->stop();
  if (auto edit = std::exchange(pending_size_group_edit_, std::nullopt); edit) {
    emit size_group_edited(*edit);
  }
  if (auto smoothing = std::exchange(pending_smoothing_, std::nullopt); smoothing) {
    emit smoothing_edited(*smoothing);
  }
  if (auto edit = std::exchange(pending_brush_edit_, std::nullopt); edit) {
    brush_.edit(*edit);
  }
}

void BrushSettingsPanel::retranslate() {
  updating_ = true;
  for (const auto section : kBrushSections) {
    if (auto* item = section_item(section); item != nullptr) {
      item->setText(0, panel_tr(brush_section_title_source(section)));
    }
  }
  updating_ = false;
  if (tip_list_ != nullptr && tip_list_->count() >= 2) {
    tip_list_->item(0)->setToolTip(panel_tr(QT_TR_NOOP("Round")));
    tip_list_->item(1)->setToolTip(panel_tr(QT_TR_NOOP("Square")));
  }
  if (lock_banner_label_ != nullptr) {
    refresh_locks();
  }
}

void BrushSettingsPanel::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    for (auto* child : findChildren<QObject*>()) {
      apply_bound_translation(child);
    }
    retranslate();
  }
}

void BrushSettingsPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  const auto wide = event->size().width() >= kSideBySideMinimumWidth;
  if (wide == wide_) {
    return;
  }
  wide_ = wide;
  // Side by side once the panel is wide enough (floating or a widened column), like Photoshop;
  // the default column stacks the list above the page.
  body_layout_->setDirection(wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  section_list_->setMaximumWidth(wide ? 180 : QWIDGETSIZE_MAX);
}

}  // namespace patchy::ui
