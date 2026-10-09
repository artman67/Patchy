#pragma once

#include "core/brush_tip.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/current_brush.hpp"

#include <QWidget>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

class QBoxLayout;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace patchy::ui {

class BrushDynamicsPanel;
class BrushStrokePreviewStrip;
class BrushTipLibrary;

// Photoshop's Brush Settings panel (Window > Brush Settings): a section list (enable box, name,
// lock) over or beside the selected section's controls, a live stroke preview, and Create New
// Brush. It edits the working brush through CurrentBrush only; Size/Soft and Smoothing belong
// to MainWindow's size group and tool settings and go out as signals. Edits coalesce so a
// slider drag does not push every intermediate value. See docs/brush-panels.md.
class BrushSettingsPanel : public QWidget {
  Q_OBJECT

public:
  struct Smoothing {
    int amount{0};
    bool pulled_string{false};
    bool catch_up{true};
    bool catch_up_end{true};
    bool zoom_adjust{true};
    friend bool operator==(const Smoothing&, const Smoothing&) = default;
  };

  BrushSettingsPanel(CurrentBrush& brush, BrushTipLibrary& tips, QWidget* parent = nullptr);

  // The active size group's Size and Soft (the Eraser's own while it is active).
  void set_size_group(int size, int softness);
  void set_smoothing(const Smoothing& smoothing);
  // Sections the active tool honors; the rest grey out.
  void set_tool_sections(BrushSectionMask sections);
  void select_section(BrushSection section);
  [[nodiscard]] BrushSection current_section() const;
  // Applies a coalesced edit now.
  void flush_pending_edit();

signals:
  void size_group_edited(const patchy::ui::BrushEdit& edit);
  void tip_picked(const QString& tip_id);
  void smoothing_edited(const patchy::ui::BrushSettingsPanel::Smoothing& smoothing);
  void new_preset_requested();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void changeEvent(QEvent* event) override;

private:
  void build_tip_shape_page(QWidget* page);
  void build_smoothing_page(QWidget* page);
  void rebuild_tip_list();
  void on_brush_changed(unsigned changes);
  void refresh_from_brush(bool reload_form);
  void refresh_section_states();
  void refresh_locks();
  void refresh_spacing_row();
  void refresh_preview();
  void show_section_page(BrushSection section);
  void schedule_flush();
  void section_toggled(BrushSection section, bool enabled);
  void toggle_lock(BrushSection section);
  void clear_brush_controls();
  void show_section_menu(const QPoint& position);
  void retranslate();
  [[nodiscard]] QTreeWidgetItem* section_item(BrushSection section) const;
  [[nodiscard]] std::optional<double> tip_default_spacing() const;
  [[nodiscard]] bool section_checked(BrushSection section) const;

  CurrentBrush& brush_;
  BrushTipLibrary& tips_;
  QBoxLayout* body_layout_{nullptr};
  QTreeWidget* section_list_{nullptr};
  QScrollArea* page_scroll_{nullptr};
  QStackedWidget* pages_{nullptr};
  std::map<BrushSection, QWidget*> page_for_section_;
  BrushDynamicsPanel* form_{nullptr};
  QListWidget* tip_list_{nullptr};
  QSpinBox* size_spin_{nullptr};
  QSlider* size_slider_{nullptr};
  QSpinBox* softness_spin_{nullptr};
  QSlider* softness_slider_{nullptr};
  QCheckBox* spacing_check_{nullptr};
  QSlider* spacing_slider_{nullptr};
  QSpinBox* spacing_spin_{nullptr};
  QSpinBox* smoothing_spin_{nullptr};
  QSlider* smoothing_slider_{nullptr};
  QCheckBox* pulled_string_check_{nullptr};
  QCheckBox* catch_up_check_{nullptr};
  QCheckBox* catch_up_end_check_{nullptr};
  QCheckBox* zoom_adjust_check_{nullptr};
  QWidget* lock_banner_{nullptr};
  QLabel* lock_banner_label_{nullptr};
  BrushStrokePreviewStrip* preview_{nullptr};
  QTimer* flush_timer_{nullptr};
  // The preview's Brush Texture tile, built once per pattern id.
  std::string texture_tile_id_;
  std::shared_ptr<const patchy::BrushTextureTile> texture_tile_;

  // Edits waiting for the coalescing timer.
  std::optional<BrushEdit> pending_brush_edit_;
  std::optional<BrushEdit> pending_size_group_edit_;
  std::optional<Smoothing> pending_smoothing_;

  Smoothing smoothing_{};
  BrushSectionMask tool_sections_{kAllBrushSections};
  // Sections without an enable flag of their own (Shape Dynamics, Scattering, Transfer,
  // Smoothing) tick when they change anything. Unticking parks the values here so ticking
  // brings them back; ticking an empty section keeps it ticked until edited. Both reset on
  // every pick, since they belong to the brush that was showing.
  std::map<BrushSection, WorkingBrush> parked_;
  std::optional<int> parked_smoothing_;
  std::set<BrushSection> armed_;
  bool updating_{false};
  bool wide_{false};
};

}  // namespace patchy::ui
