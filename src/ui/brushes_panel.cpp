#include "ui/brushes_panel.hpp"

#include "ui/action_icons.hpp"
#include "ui/app_settings.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_stroke_preview.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/current_brush.hpp"
#include "ui/curved_slider.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/localization.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/theme_palette.hpp"

#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace patchy::ui {

namespace {

constexpr auto kContext = "patchy::ui::BrushesPanel";
constexpr int kKindRole = Qt::UserRole + 1;
constexpr int kIdRole = Qt::UserRole + 2;
constexpr int kFolderRole = Qt::UserRole + 3;
constexpr auto kMimeType = "application/x-patchy-brush-entries";
// Persisted Brushes panel state (never rename): folders created empty, expanded folders, view.
constexpr auto kEmptyFoldersKey = "brushes/panelFolders";
constexpr auto kExpandedFoldersKey = "brushes/panelExpandedFolders";
constexpr auto kViewKey = "brushes/panelView";
constexpr int kListRowHeight = 38;
constexpr int kHeaderHeight = 22;
constexpr int kGridTile = 50;

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

BrushesPanel::Kind item_kind(const QListWidgetItem* item) {
  return static_cast<BrushesPanel::Kind>(item->data(kKindRole).toInt());
}

QIcon view_icon(bool grid) {
  return themed_glyph_icon(grid ? QStringLiteral("brushesGridView") : QStringLiteral("brushesListView"), 16.0,
                           &ThemePalette::text_primary, [grid](QPainter& painter, const QColor& ink) {
                             painter.setPen(Qt::NoPen);
                             painter.setBrush(ink);
                             if (grid) {
                               for (int y = 0; y < 2; ++y) {
                                 for (int x = 0; x < 2; ++x) {
                                   painter.drawRect(QRectF(2.5 + x * 6.5, 2.5 + y * 6.5, 4.5, 4.5));
                                 }
                               }
                             } else {
                               for (int y = 0; y < 3; ++y) {
                                 painter.drawRect(QRectF(2.0, 3.0 + y * 4.0, 2.5, 2.5));
                                 painter.drawRect(QRectF(6.0, 3.0 + y * 4.0, 8.0, 2.5));
                               }
                             }
                           });
}

}  // namespace

// Paints header rows (disclosure arrow, folder, count) and brush rows (tip thumbnail, stroke
// preview, name) in the list, or thumbnail tiles in the grid. The working brush's base row is
// highlighted; once edited it also gets an accent outline and "(modified)".
class BrushEntryDelegate : public QStyledItemDelegate {
public:
  explicit BrushEntryDelegate(BrushesPanel& panel) : QStyledItemDelegate(&panel), panel_(panel) {}

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    const auto kind = static_cast<BrushesPanel::Kind>(index.data(kKindRole).toInt());
    if (kind == BrushesPanel::Kind::Header) {
      const auto width = panel_.view() == BrushesPanel::View::Grid
                             ? std::max(40, panel_.list()->viewport()->width() - 6)
                             : option.rect.width();
      return {width, kHeaderHeight};
    }
    if (panel_.view() == BrushesPanel::View::Grid) {
      return {kGridTile, kGridTile};
    }
    return {option.rect.width(), kListRowHeight};
  }

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    const auto& palette = theme();
    const auto kind = static_cast<BrushesPanel::Kind>(index.data(kKindRole).toInt());
    const auto id = index.data(kIdRole).toString();
    const auto rect = option.rect;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (kind == BrushesPanel::Kind::Header) {
      painter->fillRect(rect, palette.panel_title_bg);
      const auto expanded = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
      QPainterPath arrow;
      const QPointF c(rect.left() + 10.0, rect.center().y() + 0.5);
      if (expanded) {
        arrow.moveTo(c + QPointF(-4.0, -2.0));
        arrow.lineTo(c + QPointF(4.0, -2.0));
        arrow.lineTo(c + QPointF(0.0, 3.0));
      } else {
        arrow.moveTo(c + QPointF(-2.0, -4.0));
        arrow.lineTo(c + QPointF(3.0, 0.0));
        arrow.lineTo(c + QPointF(-2.0, 4.0));
      }
      arrow.closeSubpath();
      painter->fillPath(arrow, palette.text_secondary);
      auto font = option.font;
      font.setBold(true);
      painter->setFont(font);
      painter->setPen(palette.text_primary);
      painter->drawText(rect.adjusted(20, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                        QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                      rect.width() - 24));
      painter->restore();
      return;
    }
    const auto base = panel_.is_base(kind, id);
    const auto modified = base && panel_.base_modified();
    const auto hovered = (option.state & QStyle::State_MouseOver) != 0;
    const auto background = rect.adjusted(1, 1, -1, -1);
    if (base) {
      painter->setPen(QPen(modified ? palette.accent : palette.list_selection_border, modified ? 2.0 : 1.0));
      painter->setBrush(palette.list_selection_bg);
      painter->drawRoundedRect(QRectF(background).adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);
    } else if (hovered) {
      painter->fillRect(background, palette.list_row_hover_bg);
    }
    if (panel_.view() == BrushesPanel::View::Grid) {
      const auto thumbnail = panel_.thumbnail_for(kind, id, kGridTile - 10);
      painter->drawPixmap(rect.left() + 5, rect.top() + 5, thumbnail);
      if (modified) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.accent);
        painter->drawEllipse(QPointF(rect.right() - 6.0, rect.top() + 6.0), 3.5, 3.5);
      }
      painter->restore();
      return;
    }
    const auto thumbnail = panel_.thumbnail_for(kind, id, 32);
    painter->drawPixmap(rect.left() + 4, rect.top() + (rect.height() - 32) / 2, thumbnail);
    const auto stroke_width = std::clamp(rect.width() * 2 / 5, 40, 120);
    const QRect stroke_rect(rect.left() + 42, rect.top() + 4, stroke_width, rect.height() - 8);
    const auto stroke = panel_.stroke_for(kind, id, stroke_rect.size(), base ? palette.list_selection_text
                                                                               : palette.text_primary);
    painter->drawPixmap(stroke_rect.topLeft(), stroke);
    const auto text_left = stroke_rect.right() + 8;
    const QRect text_rect(text_left, rect.top(), rect.right() - text_left - 4, rect.height());
    auto name_font = option.font;
    QFontMetrics metrics(name_font);
    auto name = index.data(Qt::DisplayRole).toString();
    QString suffix;
    if (modified) {
      suffix = QLatin1Char(' ') + panel_tr(QT_TRANSLATE_NOOP("patchy::ui::BrushesPanel", "(modified)"));
    }
    auto italic = name_font;
    italic.setItalic(true);
    const QFontMetrics italic_metrics(italic);
    const auto suffix_width = suffix.isEmpty() ? 0 : italic_metrics.horizontalAdvance(suffix);
    name = metrics.elidedText(name, Qt::ElideRight, std::max(10, text_rect.width() - suffix_width));
    painter->setFont(name_font);
    painter->setPen(base ? palette.list_selection_text : palette.text_primary);
    painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, name);
    if (!suffix.isEmpty()) {
      painter->setFont(italic);
      painter->setPen(palette.accent);
      painter->drawText(text_rect.adjusted(metrics.horizontalAdvance(name), 0, 0, 0),
                        Qt::AlignVCenter | Qt::AlignLeft, suffix);
    }
    painter->restore();
  }

private:
  BrushesPanel& panel_;
};

BrushPresetList::BrushPresetList(BrushesPanel& panel, QWidget* parent) : QListWidget(parent), panel_(panel) {
  setObjectName(QStringLiteral("brushesPanelList"));
  setSelectionMode(QAbstractItemView::SingleSelection);
  setMouseTracking(true);
  setUniformItemSizes(false);
  setMovement(QListView::Static);
  setResizeMode(QListView::Adjust);
  setDragEnabled(true);
  setAcceptDrops(true);
  viewport()->setAcceptDrops(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::MoveAction);
  setDropIndicatorShown(false);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setContextMenuPolicy(Qt::CustomContextMenu);
}

void BrushPresetList::sync_header_widths() {
  // Grid mode wraps tiles; a header must span the row to start its folder on a new line.
  for (int row = 0; row < count(); ++row) {
    auto* entry = item(row);
    if (item_kind(entry) == BrushesPanel::Kind::Header) {
      entry->setSizeHint(QSize(panel_.view() == BrushesPanel::View::Grid ? std::max(40, viewport()->width() - 6) : -1,
                               kHeaderHeight));
    }
  }
}

QStringList BrushPresetList::mimeTypes() const {
  return {QString::fromLatin1(kMimeType)};
}

QMimeData* BrushPresetList::mimeData(const QList<QListWidgetItem*>& items) const {
  QJsonArray entries;
  for (const auto* entry : items) {
    const auto kind = item_kind(entry);
    if (kind == BrushesPanel::Kind::Saved || kind == BrushesPanel::Kind::Tip) {
      entries.append(QJsonObject{{"kind", static_cast<int>(kind)}, {"id", entry->data(kIdRole).toString()}});
    }
  }
  if (entries.isEmpty()) {
    return nullptr;
  }
  auto* data = new QMimeData();
  data->setData(QString::fromLatin1(kMimeType), QJsonDocument(entries).toJson(QJsonDocument::Compact));
  return data;
}

void BrushPresetList::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat(QString::fromLatin1(kMimeType))) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void BrushPresetList::dragMoveEvent(QDragMoveEvent* event) {
  if (!event->mimeData()->hasFormat(QString::fromLatin1(kMimeType))) {
    event->ignore();
    return;
  }
  const auto* target = itemAt(event->position().toPoint());
  const auto folder = target != nullptr ? target->data(kFolderRole).toString() : QString();
  if (folder == BrushesPanel::builtin_folder_key()) {
    event->ignore();  // the built-in presets' folder holds only them
    return;
  }
  event->setDropAction(Qt::MoveAction);
  event->accept();
}

void BrushPresetList::dropEvent(QDropEvent* event) {
  if (!event->mimeData()->hasFormat(QString::fromLatin1(kMimeType))) {
    event->ignore();
    return;
  }
  const auto* target = itemAt(event->position().toPoint());
  const auto folder = target != nullptr ? target->data(kFolderRole).toString() : QString();
  if (folder == BrushesPanel::builtin_folder_key()) {
    event->ignore();
    return;
  }
  QList<QPair<BrushesPanel::Kind, QString>> entries;
  const auto array = QJsonDocument::fromJson(event->mimeData()->data(QString::fromLatin1(kMimeType))).array();
  for (const auto& value : array) {
    const auto object = value.toObject();
    entries.append({static_cast<BrushesPanel::Kind>(object["kind"].toInt()), object["id"].toString()});
  }
  event->setDropAction(Qt::CopyAction);  // never let the view remove the dragged rows itself
  event->accept();
  // The move rebuilds this list; finish the drop first.
  QTimer::singleShot(0, &panel_, [panel = &panel_, entries, folder] { panel->move_entries(entries, folder); });
}

void BrushPresetList::resizeEvent(QResizeEvent* event) {
  QListWidget::resizeEvent(event);
  if (panel_.view() == BrushesPanel::View::Grid) {
    sync_header_widths();
  }
}

const QString& BrushesPanel::builtin_folder_key() {
  static const QString key = QStringLiteral("\x01builtin");
  return key;
}

BrushesPanel::BrushesPanel(CurrentBrush& brush, BrushTipLibrary& tips, BrushAutomationLibrary& presets,
                           QWidget* parent)
    : QWidget(parent), brush_(brush), tips_(tips), presets_(presets) {
  setObjectName(QStringLiteral("brushesPanel"));
  {
    const auto settings = app_settings();
    empty_folders_ = settings.value(QLatin1StringView(kEmptyFoldersKey)).toStringList();
    const auto expanded = settings.value(QLatin1StringView(kExpandedFoldersKey), QStringList{builtin_folder_key()})
                              .toStringList();
    expanded_ = QSet<QString>(expanded.begin(), expanded.end());
    view_ = settings.value(QLatin1StringView(kViewKey)).toString() == QStringLiteral("grid") ? View::Grid : View::List;
  }
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(5);

  auto* size_row = new QHBoxLayout();
  size_row->addWidget(bound(new QLabel(this), QT_TR_NOOP("Size:")));
  size_slider_ = new QSlider(Qt::Horizontal, this);
  size_slider_->setObjectName(QStringLiteral("brushesPanelSizeSpinSlider"));
  size_spin_ = new QSpinBox(this);
  size_spin_->setObjectName(QStringLiteral("brushesPanelSizeSpin"));
  size_spin_->setRange(1, kMaxBrushSize);
  bind_curved_slider(*size_slider_, *size_spin_);
  bound(size_spin_, nullptr, QT_TR_NOOP("Brush size in pixels"));
  size_row->addWidget(size_slider_, 1);
  size_row->addWidget(size_spin_);
  root->addLayout(size_row);
  size_timer_ = new QTimer(this);
  size_timer_->setSingleShot(true);
  size_timer_->setInterval(40);  // coalesce a slider drag
  connect(size_timer_, &QTimer::timeout, this, [this] { emit size_edited(size_spin_->value()); });
  connect(size_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
    if (!updating_) {
      size_timer_->start();
    }
  });

  auto* search_row = new QHBoxLayout();
  search_edit_ = new QLineEdit(this);
  search_edit_->setObjectName(QStringLiteral("brushesPanelSearchEdit"));
  search_edit_->setClearButtonEnabled(true);
  bound(search_edit_, QT_TR_NOOP("Search Brushes"));
  connect(search_edit_, &QLineEdit::textChanged, this, [this] { apply_filter(); });
  search_row->addWidget(search_edit_, 1);
  const auto make_view_button = [this, search_row](const char* name, bool grid, const char* tooltip) {
    auto* button = new QToolButton(this);
    button->setObjectName(QLatin1String(name));
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setIcon(view_icon(grid));
    button->setFocusPolicy(Qt::NoFocus);
    bound(button, nullptr, tooltip);
    search_row->addWidget(button);
    connect(button, &QToolButton::clicked, this, [this, grid] { set_view(grid ? View::Grid : View::List); });
    return button;
  };
  list_view_button_ = make_view_button("brushesPanelListViewButton", false, QT_TR_NOOP("List view"));
  grid_view_button_ = make_view_button("brushesPanelGridViewButton", true, QT_TR_NOOP("Thumbnail grid view"));
  root->addLayout(search_row);

  list_ = new BrushPresetList(*this, this);
  list_->setItemDelegate(new BrushEntryDelegate(*this));
  list_->setMinimumHeight(60);
  connect(list_, &QListWidget::itemClicked, this, &BrushesPanel::item_clicked);
  connect(list_, &QListWidget::itemActivated, this, &BrushesPanel::item_clicked);
  connect(list_, &QListWidget::customContextMenuRequested, this, &BrushesPanel::show_context_menu);
  root->addWidget(list_, 1);

  auto* footer = new QHBoxLayout();
  footer->setSpacing(2);
  const auto make_button = [this, footer](const char* name, const QIcon& icon, const char* tooltip) {
    auto* button = new QToolButton(this);
    button->setObjectName(QLatin1String(name));
    button->setIcon(icon);
    button->setIconSize(QSize(20, 20));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    bound(button, nullptr, tooltip);
    footer->addWidget(button);
    return button;
  };
  auto* settings_button = make_button("brushesPanelSettingsButton", simple_icon(QStringLiteral("tool-brush")),
                                      QT_TR_NOOP("Show the Brush Settings panel"));
  connect(settings_button, &QToolButton::clicked, this, &BrushesPanel::show_settings_requested);
  auto* menu_button = make_button("brushesPanelMenuButton", QIcon(), QT_TR_NOOP("Brushes panel options"));
  menu_button->setText(QStringLiteral("..."));
  menu_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  menu_button->setPopupMode(QToolButton::InstantPopup);
  auto* menu = new QMenu(menu_button);
  menu->setObjectName(QStringLiteral("brushesPanelMenu"));
  connect(menu, &QMenu::aboutToShow, this, [this, menu] {
    menu->clear();
    populate_menu(*menu, list_->currentItem());
  });
  menu_button->setMenu(menu);
  footer->addStretch(1);
  auto* folder_button = make_button("brushesPanelNewFolderButton", simple_icon(QStringLiteral("dir")),
                                    QT_TR_NOOP("New Folder..."));
  connect(folder_button, &QToolButton::clicked, this, &BrushesPanel::create_folder);
  auto* new_button = make_button("brushesPanelNewPresetButton", simple_icon(QStringLiteral("new")),
                                 QT_TR_NOOP("New Brush Preset..."));
  connect(new_button, &QToolButton::clicked, this, [this] { emit new_preset_requested(selected_folder()); });
  auto* delete_button = make_button("brushesPanelDeleteButton", simple_icon(QStringLiteral("trash")),
                                    QT_TR_NOOP("Delete the selected brush or folder"));
  connect(delete_button, &QToolButton::clicked, this, [this] { delete_item(list_->currentItem()); });
  root->addLayout(footer);

  connect(&tips_, &BrushTipLibrary::changed, this, [this] { reload(); });
  connect(&presets_, &BrushAutomationLibrary::changed, this, [this] {
    saved_previews_.clear();
    reload();
  });
  connect(&brush_, &CurrentBrush::changed, this, [this](unsigned changes) {
    if ((changes & CurrentBrush::Base) != 0U) {
      sync_current_row();
    } else {
      refresh_marker();
    }
  });

  set_view(view_);
  reload();
}

void BrushesPanel::set_size(int size) {
  if (size_slider_->isSliderDown() || size_timer_->isActive() || size_spin_->value() == size) {
    return;
  }
  updating_ = true;
  size_spin_->setValue(size);
  updating_ = false;
}

void BrushesPanel::set_view(View view) {
  view_ = view;
  list_view_button_->setChecked(view == View::List);
  grid_view_button_->setChecked(view == View::Grid);
  list_->setViewMode(view == View::Grid ? QListView::IconMode : QListView::ListMode);
  list_->setFlow(view == View::Grid ? QListView::LeftToRight : QListView::TopToBottom);
  list_->setWrapping(view == View::Grid);
  list_->setSpacing(view == View::Grid ? 1 : 0);
  // IconMode turns on free movement and drops; the panel files brushes by folder only.
  list_->setMovement(QListView::Static);
  list_->setDragDropMode(QAbstractItemView::DragDrop);
  list_->sync_header_widths();
  list_->doItemsLayout();
  save_view_state();
}

QString BrushesPanel::folder_label(const QString& folder) const {
  if (folder == builtin_folder_key()) {
    return panel_tr(QT_TR_NOOP("General Brushes"));
  }
  return folder;
}

QStringList BrushesPanel::folder_names() const {
  QStringList folders = empty_folders_;
  for (const auto& entry : tips_.entries()) {
    if (!entry.folder.isEmpty()) {
      folders << entry.folder;
    }
  }
  for (const auto& value : presets_.presets()) {
    const auto preset = value.toObject();
    if (preset["source"].toString() == QStringLiteral("user") && !preset["folder"].toString().isEmpty()) {
      folders << preset["folder"].toString();
    }
  }
  folders.removeDuplicates();
  std::sort(folders.begin(), folders.end(),
            [](const QString& a, const QString& b) { return QString::compare(a, b, Qt::CaseInsensitive) < 0; });
  return folders;
}

void BrushesPanel::reload() {
  rebuild();
}

void BrushesPanel::rebuild() {
  struct Row {
    Kind kind;
    QString id;
    QString name;
    QString folder;
  };
  std::vector<Row> rows;
  for (const auto& preset : builtin_brush_presets()) {
    rows.push_back({Kind::Builtin, preset.id, brush_preset_display_name(preset), builtin_folder_key()});
  }
  std::vector<Row> saved;
  for (const auto& value : presets_.presets()) {
    const auto preset = value.toObject();
    if (preset["source"].toString() == QStringLiteral("user")) {
      saved.push_back({Kind::Saved, preset["id"].toString(), preset["name"].toString(), preset["folder"].toString()});
    }
  }
  std::sort(saved.begin(), saved.end(), [](const Row& a, const Row& b) {
    return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
  });
  rows.insert(rows.end(), saved.begin(), saved.end());
  for (const auto& entry : tips_.entries()) {
    rows.push_back({Kind::Tip, entry.id, entry.name, entry.folder});
  }

  // Loose brushes first (no header), then the built-ins, then every other folder by name.
  QStringList order{QString(), builtin_folder_key()};
  order += folder_names();
  const QSignalBlocker blocker(list_);
  list_->clear();
  for (const auto& folder : order) {
    if (!folder.isEmpty()) {
      int count = 0;
      for (const auto& row : rows) {
        count += row.folder == folder ? 1 : 0;
      }
      auto* header = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(folder_label(folder)).arg(count), list_);
      header->setData(kKindRole, static_cast<int>(Kind::Header));
      header->setData(kIdRole, folder);
      header->setData(kFolderRole, folder);
      header->setData(Qt::CheckStateRole, expanded_.contains(folder) ? Qt::Checked : Qt::Unchecked);
      header->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    }
    for (const auto& row : rows) {
      if (row.folder != folder) {
        continue;
      }
      auto* item = new QListWidgetItem(row.name, list_);
      item->setData(kKindRole, static_cast<int>(row.kind));
      item->setData(kIdRole, row.id);
      item->setData(kFolderRole, folder);
      item->setToolTip(row.name);
      auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
      if (row.kind != Kind::Builtin) {
        flags |= Qt::ItemIsDragEnabled;
      }
      item->setFlags(flags);
    }
  }
  list_->sync_header_widths();
  apply_filter();
  sync_current_row();
}

void BrushesPanel::apply_filter() {
  const auto needle = search_edit_->text().trimmed();
  QListWidgetItem* header = nullptr;
  bool header_has_match = false;
  const auto close_header = [&] {
    if (header != nullptr) {
      header->setHidden(!needle.isEmpty() && !header_has_match);
    }
  };
  for (int row = 0; row < list_->count(); ++row) {
    auto* item = list_->item(row);
    if (item_kind(item) == Kind::Header) {
      close_header();
      header = item;
      header_has_match = false;
      continue;
    }
    const auto folder = item->data(kFolderRole).toString();
    if (needle.isEmpty()) {
      item->setHidden(!folder.isEmpty() && !expanded_.contains(folder));
    } else {
      const auto match = item->text().contains(needle, Qt::CaseInsensitive);
      item->setHidden(!match);
      header_has_match = header_has_match || match;
    }
  }
  close_header();
}

void BrushesPanel::item_clicked(QListWidgetItem* item) {
  if (item == nullptr) {
    return;
  }
  const auto kind = item_kind(item);
  const auto id = item->data(kIdRole).toString();
  if (kind == Kind::Header) {
    if (expanded_.contains(id)) {
      expanded_.remove(id);
    } else {
      expanded_.insert(id);
    }
    item->setData(Qt::CheckStateRole, expanded_.contains(id) ? Qt::Checked : Qt::Unchecked);
    save_view_state();
    apply_filter();
    return;
  }
  if (kind == Kind::Tip) {
    emit tip_picked(id);
  } else {
    emit preset_picked(id);
  }
}

bool BrushesPanel::is_base(Kind kind, const QString& id) const {
  const auto& base = brush_.base();
  if (base.id != id) {
    return false;
  }
  return base.kind == BrushBase::Kind::Tip ? kind == Kind::Tip : (kind == Kind::Builtin || kind == Kind::Saved);
}

bool BrushesPanel::base_modified() const {
  return brush_.modified();
}

QListWidgetItem* BrushesPanel::item_for_base() const {
  for (int row = 0; row < list_->count(); ++row) {
    auto* item = list_->item(row);
    if (item_kind(item) != Kind::Header && is_base(item_kind(item), item->data(kIdRole).toString())) {
      return item;
    }
  }
  return nullptr;
}

void BrushesPanel::refresh_marker() {
  // The painted marker, also as the row's accessible description (screen readers, tests).
  const auto marker = base_modified() ? panel_tr(QT_TR_NOOP("Current brush (modified)"))
                                      : panel_tr(QT_TR_NOOP("Current brush"));
  for (int row = 0; row < list_->count(); ++row) {
    auto* entry = list_->item(row);
    const auto is_current = item_kind(entry) != Kind::Header &&
                            is_base(item_kind(entry), entry->data(kIdRole).toString());
    entry->setData(Qt::AccessibleDescriptionRole, is_current ? marker : QString());
  }
  list_->viewport()->update();
}

void BrushesPanel::sync_current_row() {
  const QSignalBlocker blocker(list_);
  refresh_marker();
  if (auto* item = item_for_base(); item != nullptr) {
    list_->setCurrentItem(item);
    // Only once the list has its height: scrolling an unsized list parks the first folder
    // header above the top edge.
    if (!item->isHidden() && list_->isVisible() && list_->viewport()->height() > kListRowHeight) {
      list_->scrollToItem(item);
    }
  } else {
    list_->clearSelection();
  }
  list_->viewport()->update();
}

QString BrushesPanel::selected_folder() const {
  const auto* item = list_->currentItem();
  if (item == nullptr) {
    return {};
  }
  const auto folder = item->data(kFolderRole).toString();
  return folder == builtin_folder_key() ? QString() : folder;
}

const BrushesPanel::SavedPreview* BrushesPanel::saved_preview(const QString& id) {
  if (const auto found = saved_previews_.find(id); found != saved_previews_.end()) {
    return &found->second;
  }
  SavedPreview preview;
  try {
    const auto stroke = presets_.resolve(QJsonObject{{"presetId", id}});
    preview.tip = stroke.tip;
    if (preview.tip == nullptr && !is_builtin_brush_tip_id(stroke.tip_id)) {
      preview.tip = tips_.tip(stroke.tip_id);
    }
    preview.square = preview.tip == nullptr && stroke.tip_id == builtin_square_brush_tip_id();
    preview.softness = stroke.softness;
    preview.spacing = stroke.spacing;
    preview.angle = stroke.angle;
    preview.roundness = stroke.roundness;
    preview.dynamics = stroke.dynamics;
  } catch (const std::exception&) {
    // An unreadable preset previews as a plain Round.
  }
  preview.thumbnail = preview.tip != nullptr ? brush_tip_thumbnail(*preview.tip, 32)
                                             : procedural_brush_thumbnail(preview.square, preview.softness, 32);
  preview.key = QStringLiteral("saved:%1:%2").arg(id, presets_.revision());
  return &saved_previews_.emplace(id, std::move(preview)).first->second;
}

QPixmap BrushesPanel::thumbnail_for(Kind kind, const QString& id, int extent) {
  QPixmap thumbnail;
  if (kind == Kind::Tip) {
    if (const auto* entry = tips_.find_entry(id); entry != nullptr) {
      thumbnail = brush_tip_thumbnail_with_badge(*entry);
    }
  } else if (kind == Kind::Builtin) {
    auto& cached = builtin_thumbnails_[id];
    if (cached.isNull()) {
      if (const auto* preset = find_brush_preset(id); preset != nullptr) {
        cached = procedural_brush_thumbnail(preset->tip_id == builtin_square_brush_tip_id(), preset->softness, 32);
      }
    }
    thumbnail = cached;
  } else if (kind == Kind::Saved) {
    thumbnail = saved_preview(id)->thumbnail;
  }
  if (thumbnail.isNull() || thumbnail.width() == extent) {
    return thumbnail;
  }
  return thumbnail.scaled(extent, extent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap BrushesPanel::stroke_for(Kind kind, const QString& id, QSize size, const QColor& ink) {
  BrushStrokePreviewSpec spec;
  QString key;
  if (kind == Kind::Tip) {
    const auto* entry = tips_.find_entry(id);
    if (entry == nullptr) {
      return {};
    }
    // The tip identity plus its stored settings: an edited sidecar previews fresh.
    key = QStringLiteral("tip:%1:%2:%3:%4:%5")
              .arg(id)
              .arg(entry->spacing)
              .arg(entry->base_angle_degrees)
              .arg(entry->base_roundness)
              .arg(QString::fromUtf8(QJsonDocument(brush_dynamics_to_json(entry->dynamics)).toJson(QJsonDocument::Compact)));
    spec.spacing = entry->spacing;
    spec.angle = entry->base_angle_degrees;
    spec.roundness = entry->base_roundness;
    spec.dynamics = entry->dynamics;
    // Only rendered on a cache miss: loading every visible tip once is the cost of a first scroll.
    spec.tip = tips_.tip(id);
  } else if (kind == Kind::Builtin) {
    const auto* preset = find_brush_preset(id);
    if (preset == nullptr) {
      return {};
    }
    key = QStringLiteral("builtin:") + id;
    spec.square = preset->tip_id == builtin_square_brush_tip_id();
    spec.softness = preset->softness;
  } else {
    const auto* saved = saved_preview(id);
    key = saved->key;
    spec.tip = saved->tip;
    spec.square = saved->square;
    spec.softness = saved->softness;
    spec.spacing = saved->spacing;
    spec.angle = saved->angle;
    spec.roundness = saved->roundness;
    spec.dynamics = saved->dynamics;
  }
  const auto brush_size = std::max(4, size.height() * 3 / 5);
  return brush_stroke_preview_pixmap(key, spec, size, ink, brush_size);
}

void BrushesPanel::show_context_menu(const QPoint& position) {
  auto* target = list_->itemAt(position);
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("brushesPanelContextMenu"));
  populate_menu(menu, target);
  menu.exec(list_->viewport()->mapToGlobal(position));
}

void BrushesPanel::populate_menu(QMenu& menu, QListWidgetItem* target) {
  const auto kind = target != nullptr ? item_kind(target) : Kind::Header;
  const auto folder = target != nullptr ? target->data(kFolderRole).toString() : QString();
  const auto user_folder = target != nullptr && kind == Kind::Header && folder != builtin_folder_key();
  const auto add = [this, &menu](const char* source, const char* name, bool enabled, auto callback) {
    auto* action = menu.addAction(panel_tr(source));
    action->setObjectName(QLatin1String(name));
    action->setEnabled(enabled);
    connect(action, &QAction::triggered, this, callback);
    return action;
  };
  add(QT_TR_NOOP("New Brush Preset..."), "brushesPanelNewPresetAction", true,
      [this, folder] { emit new_preset_requested(folder == builtin_folder_key() ? QString() : folder); });
  add(QT_TR_NOOP("New Folder..."), "brushesPanelNewFolderAction", true, [this] { create_folder(); });
  menu.addSeparator();
  const auto editable = target != nullptr && (kind == Kind::Saved || kind == Kind::Tip || user_folder);
  add(QT_TR_NOOP("Rename..."), "brushesPanelRenameAction", editable, [this, target] { rename_item(target); });
  add(QT_TR_NOOP("Duplicate"), "brushesPanelDuplicateAction", target != nullptr && kind != Kind::Header,
      [this, target] { duplicate_item(target); });
  add(QT_TR_NOOP("Delete"), "brushesPanelDeleteAction", editable, [this, target] { delete_item(target); });
  menu.addSeparator();
  add(QT_TR_NOOP("Import Brushes..."), "brushesPanelImportAction", true, [this] { emit import_requested(); });
  menu.addSeparator();
  auto* list_view = add(QT_TR_NOOP("List View"), "brushesPanelListViewAction", true, [this] { set_view(View::List); });
  list_view->setCheckable(true);
  list_view->setChecked(view_ == View::List);
  auto* grid_view =
      add(QT_TR_NOOP("Thumbnail Grid View"), "brushesPanelGridViewAction", true, [this] { set_view(View::Grid); });
  grid_view->setCheckable(true);
  grid_view->setChecked(view_ == View::Grid);
}

void BrushesPanel::create_folder() {
  bool accepted = false;
  const auto name = QInputDialog::getText(this, panel_tr(QT_TR_NOOP("New Folder")), panel_tr(QT_TR_NOOP("Name:")),
                                          QLineEdit::Normal, panel_tr(QT_TR_NOOP("New Folder")), &accepted)
                        .trimmed();
  if (!accepted || name.isEmpty()) {
    return;
  }
  if (!folder_names().contains(name)) {
    empty_folders_ << name;
  }
  expanded_.insert(name);
  save_view_state();
  rebuild();
}

void BrushesPanel::move_entries(const QList<QPair<Kind, QString>>& entries, const QString& folder) {
  try {
    for (const auto& [kind, id] : entries) {
      if (kind == Kind::Tip) {
        (void)tips_.set_tip_folder(id, folder);
      } else if (kind == Kind::Saved) {
        presets_.update_entry(id, presets_.preset(id)["name"].toString(), folder);
      }
    }
  } catch (const std::exception& error) {
    QMessageBox::warning(this, panel_tr(QT_TR_NOOP("Brushes")), translate_data_text(error.what()));
  }
  if (!folder.isEmpty()) {
    expanded_.insert(folder);
    save_view_state();
  }
  rebuild();
}

void BrushesPanel::rename_item(QListWidgetItem* item) {
  if (item == nullptr) {
    return;
  }
  const auto kind = item_kind(item);
  const auto id = item->data(kIdRole).toString();
  const auto folder = item->data(kFolderRole).toString();
  const auto current = kind == Kind::Header ? folder : item->text();
  bool accepted = false;
  const auto name = QInputDialog::getText(this, panel_tr(QT_TR_NOOP("Rename")), panel_tr(QT_TR_NOOP("Name:")),
                                          QLineEdit::Normal, current, &accepted)
                        .trimmed();
  if (!accepted || name.isEmpty() || name == current) {
    return;
  }
  try {
    if (kind == Kind::Tip) {
      (void)tips_.rename_tip(id, name);
    } else if (kind == Kind::Saved) {
      presets_.update_entry(id, name, folder);
    } else if (kind == Kind::Header && folder != builtin_folder_key()) {
      // A folder lives in each brush that carries it: rename it everywhere.
      for (const auto& entry : tips_.entries()) {
        if (entry.folder == folder) {
          (void)tips_.set_tip_folder(entry.id, name);
        }
      }
      for (const auto& value : presets_.presets()) {
        const auto preset = value.toObject();
        if (preset["source"].toString() == QStringLiteral("user") && preset["folder"].toString() == folder) {
          presets_.update_entry(preset["id"].toString(), preset["name"].toString(), name);
        }
      }
      for (auto& empty : empty_folders_) {
        if (empty == folder) {
          empty = name;
        }
      }
      if (expanded_.remove(folder)) {
        expanded_.insert(name);
      }
      save_view_state();
    }
  } catch (const std::exception& error) {
    QMessageBox::warning(this, panel_tr(QT_TR_NOOP("Brushes")), translate_data_text(error.what()));
  }
  rebuild();
}

void BrushesPanel::duplicate_item(QListWidgetItem* item) {
  if (item == nullptr) {
    return;
  }
  const auto kind = item_kind(item);
  const auto id = item->data(kIdRole).toString();
  const auto name = panel_tr(QT_TR_NOOP("%1 Copy")).arg(item->text());
  try {
    if (kind == Kind::Saved) {
      (void)presets_.duplicate(id, name);
    } else if (kind == Kind::Builtin) {
      // A built-in copy becomes an editable saved preset.
      (void)presets_.save(name, presets_.resolve(QJsonObject{{"presetId", id}}), false);
    } else if (kind == Kind::Tip) {
      const auto* entry = tips_.find_entry(id);
      const auto tip = tips_.tip(id);
      if (entry != nullptr && tip != nullptr) {
        const auto copy = *entry;
        const auto new_id = tips_.add_tip(name, coverage_image_from_brush_tip(*tip), copy.spacing, copy.folder);
        if (!new_id.isEmpty()) {
          (void)tips_.set_tip_dynamics(new_id, copy.dynamics, copy.base_angle_degrees, copy.base_roundness);
        }
      }
    }
  } catch (const std::exception& error) {
    QMessageBox::warning(this, panel_tr(QT_TR_NOOP("Brushes")), translate_data_text(error.what()));
  }
}

void BrushesPanel::delete_item(QListWidgetItem* item) {
  if (item == nullptr) {
    return;
  }
  const auto kind = item_kind(item);
  const auto id = item->data(kIdRole).toString();
  const auto folder = item->data(kFolderRole).toString();
  if (kind == Kind::Builtin || (kind == Kind::Header && folder == builtin_folder_key())) {
    return;  // the built-in presets are always available
  }
  QStringList tip_ids;
  QStringList preset_ids;
  QString question;
  if (kind == Kind::Header) {
    for (const auto& entry : tips_.entries()) {
      if (entry.folder == folder) {
        tip_ids << entry.id;
      }
    }
    for (const auto& value : presets_.presets()) {
      const auto preset = value.toObject();
      if (preset["source"].toString() == QStringLiteral("user") && preset["folder"].toString() == folder) {
        preset_ids << preset["id"].toString();
      }
    }
    question = QCoreApplication::translate("patchy::ui::BrushesPanel",
                                           "Delete the folder \"%1\" and the %n brush(es) in it?", nullptr,
                                           static_cast<int>(tip_ids.size() + preset_ids.size()))
                   .arg(folder);
  } else {
    (kind == Kind::Tip ? tip_ids : preset_ids) << id;
    question = panel_tr(QT_TR_NOOP("Delete the brush \"%1\"?")).arg(item->text());
  }
  if (QMessageBox::question(this, panel_tr(QT_TR_NOOP("Delete")), question) != QMessageBox::Yes) {
    return;
  }
  try {
    for (const auto& preset_id : preset_ids) {
      presets_.remove(preset_id);
    }
    if (!tip_ids.isEmpty()) {
      (void)tips_.remove_tips(tip_ids);
    }
  } catch (const std::exception& error) {
    QMessageBox::warning(this, panel_tr(QT_TR_NOOP("Brushes")), translate_data_text(error.what()));
  }
  if (kind == Kind::Header) {
    empty_folders_.removeAll(folder);
    expanded_.remove(folder);
    save_view_state();
  }
  rebuild();
}

void BrushesPanel::save_view_state() const {
  auto settings = app_settings();
  settings.setValue(QLatin1StringView(kEmptyFoldersKey), empty_folders_);
  settings.setValue(QLatin1StringView(kExpandedFoldersKey), QStringList(expanded_.begin(), expanded_.end()));
  settings.setValue(QLatin1StringView(kViewKey), view_ == View::Grid ? QStringLiteral("grid") : QStringLiteral("list"));
}

void BrushesPanel::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    for (auto* child : findChildren<QObject*>()) {
      apply_bound_translation(child);
    }
    rebuild();  // folder labels, built-in names
  }
}

}  // namespace patchy::ui
