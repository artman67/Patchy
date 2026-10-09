#pragma once

#include "core/brush_dynamics.hpp"
#include "core/brush_tip.hpp"

#include <QListWidget>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <map>
#include <memory>
#include <optional>

class QLineEdit;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;

namespace patchy::ui {

class BrushAutomationLibrary;
class BrushTipLibrary;
class CurrentBrush;
class BrushesPanel;
struct BrushStrokePreviewSpec;

// The Brushes panel's list: folder header rows and brush rows in one QListWidget, so the same
// items show as a list or as a thumbnail grid. Dropping brushes on a folder (or a brush in it)
// files them there.
class BrushPresetList : public QListWidget {
  Q_OBJECT

public:
  explicit BrushPresetList(BrushesPanel& panel, QWidget* parent = nullptr);
  void sync_header_widths();

protected:
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  BrushesPanel& panel_;
};

// Photoshop's Brushes panel (Window > Brushes): one folder tree of the built-in presets, the
// saved complete presets (BrushAutomationLibrary) and every library tip as a ready-to-use brush.
// Clicking picks through MainWindow (signals); folder and preset management writes the two
// stores directly, never the working brush. See docs/brush-panels.md.
class BrushesPanel : public QWidget {
  Q_OBJECT

public:
  enum class Kind { Header, Builtin, Saved, Tip };
  enum class View { List, Grid };
  // The built-in presets' folder; a key no saved preset or tip can carry.
  static const QString& builtin_folder_key();

  BrushesPanel(CurrentBrush& brush, BrushTipLibrary& tips, BrushAutomationLibrary& presets,
               QWidget* parent = nullptr);

  void set_size(int size);
  void set_view(View view);
  [[nodiscard]] View view() const noexcept { return view_; }
  void reload();
  // The folder of the selected row, for the New Brush Preset dialog's default.
  [[nodiscard]] QString selected_folder() const;
  // Every user folder name (saved presets, tips and empty folders), sorted.
  [[nodiscard]] QStringList folder_names() const;
  [[nodiscard]] BrushPresetList* list() const noexcept { return list_; }

  // Files the dragged brushes into `folder` (empty = no folder). Built-ins stay put.
  void move_entries(const QList<QPair<Kind, QString>>& entries, const QString& folder);
  void create_folder();

signals:
  void preset_picked(const QString& preset_id);
  void tip_picked(const QString& tip_id);
  void size_edited(int size);
  void new_preset_requested(const QString& folder);
  void import_requested();
  void show_settings_requested();

protected:
  void changeEvent(QEvent* event) override;

private:
  friend class BrushEntryDelegate;
  struct SavedPreview {
    QPixmap thumbnail;
    std::shared_ptr<const patchy::BrushTip> tip;
    bool square{false};
    int softness{0};
    std::optional<double> spacing;
    double angle{0.0};
    double roundness{100.0};
    patchy::BrushDynamics dynamics{};
    QString key;
  };

  void rebuild();
  void apply_filter();
  void item_clicked(QListWidgetItem* item);
  void show_context_menu(const QPoint& position);
  void populate_menu(QMenu& menu, QListWidgetItem* target);
  void rename_item(QListWidgetItem* item);
  void duplicate_item(QListWidgetItem* item);
  void delete_item(QListWidgetItem* item);
  void sync_current_row();
  void refresh_marker();
  void save_view_state() const;
  [[nodiscard]] QListWidgetItem* item_for_base() const;
  [[nodiscard]] bool is_base(Kind kind, const QString& id) const;
  [[nodiscard]] bool base_modified() const;
  [[nodiscard]] QPixmap thumbnail_for(Kind kind, const QString& id, int extent);
  [[nodiscard]] QPixmap stroke_for(Kind kind, const QString& id, QSize size, const QColor& ink);
  [[nodiscard]] const SavedPreview* saved_preview(const QString& id);
  [[nodiscard]] QString folder_label(const QString& folder) const;

  CurrentBrush& brush_;
  BrushTipLibrary& tips_;
  BrushAutomationLibrary& presets_;
  BrushPresetList* list_{nullptr};
  QSpinBox* size_spin_{nullptr};
  QSlider* size_slider_{nullptr};
  QLineEdit* search_edit_{nullptr};
  QToolButton* list_view_button_{nullptr};
  QToolButton* grid_view_button_{nullptr};
  QTimer* size_timer_{nullptr};
  View view_{View::List};
  QStringList empty_folders_;
  QSet<QString> expanded_;
  std::map<QString, SavedPreview> saved_previews_;
  std::map<QString, QPixmap> builtin_thumbnails_;
  bool updating_{false};
};

}  // namespace patchy::ui
