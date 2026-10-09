// Part 3 of the brush/pattern/palette UI group: the Brushes and Brush Settings panels
// (docs/brush-panels.md).

#include "brush_pattern_palette_test_support.hpp"

#include "test_harness.hpp"
#include "ui/app_settings.hpp"
#include "ui/brush_automation.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_settings_panel.hpp"
#include "ui/brush_settings_sections.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brushes_panel.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/current_brush.hpp"
#include "ui/main_window.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QSpinBox>
#include <QTabBar>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QHeaderView>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using namespace patchy::test::ui;
using patchy::ui::BrushSection;

void settle() {
  for (int pass = 0; pass < 4; ++pass) {
    QApplication::processEvents();
  }
}

QDockWidget& require_brush_dock(patchy::ui::MainWindow& window, const char* name) {
  auto* dock = window.findChild<QDockWidget*>(QLatin1String(name));
  CHECK(dock != nullptr);
  return *dock;
}

QListWidgetItem* brush_row(QListWidget& list, const QString& name) {
  for (int row = 0; row < list.count(); ++row) {
    if (list.item(row)->text() == name || list.item(row)->text().startsWith(name + QStringLiteral(" ("))) {
      return list.item(row);
    }
  }
  return nullptr;
}

void click_row(QListWidget& list, QListWidgetItem* item) {
  CHECK(item != nullptr);
  CHECK(!item->isHidden());
  list.scrollToItem(item);
  QApplication::processEvents();
  click_widget_like_a_user(*list.viewport(), list.visualItemRect(item).center());
  settle();
}

// Every persisted BrushDynamics field set away from its default (the JSON form lists them all).
patchy::BrushDynamics fully_customized_dynamics() {
  auto json = patchy::ui::brush_dynamics_to_json(patchy::BrushDynamics{});
  for (auto it = json.begin(); it != json.end(); ++it) {
    if (it.value().isBool()) {
      it.value() = !it.value().toBool();
    } else if (it.value().isString()) {
      it.value() = it.key() == QStringLiteral("textureStyle") ? QStringLiteral("canvas") : QStringLiteral("penPressure");
    } else if (it.key() == QStringLiteral("count")) {
      it.value() = 3;
    } else if (it.key().endsWith(QStringLiteral("FadeSteps"))) {
      it.value() = 7;
    } else if (it.key() == QStringLiteral("textureSeed")) {
      it.value() = 12345.0;
    } else {
      it.value() = it.value().toDouble() == 0.3 ? 0.4 : 0.3;
    }
  }
  return patchy::ui::brush_dynamics_from_json(json);
}

patchy::ui::BrushesPanel& open_brushes_panel(patchy::ui::MainWindow& window) {
  require_action(window, "windowBrushesPanelAction")->trigger();
  settle();
  CHECK(require_brush_dock(window, "brushesDock").isVisible());
  auto* panel = window.findChild<patchy::ui::BrushesPanel*>(QStringLiteral("brushesPanel"));
  CHECK(panel != nullptr);
  return *panel;
}

void ui_brush_panels_open_as_a_tab_group_above_layers() {
  // Window > Brushes and Window > Brush Settings open one tab group (Brushes first) directly
  // above Layers, both for a fresh install and for a saved layout that predates them.
  const auto check_placement = [](patchy::ui::MainWindow& window) {
    auto& brushes = require_brush_dock(window, "brushesDock");
    auto& settings = require_brush_dock(window, "brushSettingsDock");
    CHECK(!brushes.isVisible() && !settings.isVisible());  // closed until asked for
    for (const auto* name : {"windowBrushesPanelAction", "windowBrushSettingsPanelAction"}) {
      auto* action = require_action(window, name);
      CHECK(action->menuRole() == QAction::NoRole);
      CHECK(action->shortcuts().isEmpty());  // F5 stays Force Refresh
      action->trigger();
      settle();
    }
    CHECK(brushes.isVisible() && settings.isVisible());
    CHECK(window.tabifiedDockWidgets(&brushes).contains(&settings));
    auto* layers = window.findChild<QDockWidget*>(QStringLiteral("layersDock"));
    const auto group_top = std::min(brushes.mapTo(&window, QPoint()).y(), settings.mapTo(&window, QPoint()).y());
    CHECK(group_top < layers->mapTo(&window, QPoint()).y());
    // The group's tab bar lists Brushes first.
    bool found_tabs = false;
    for (auto* bar : window.findChildren<QTabBar*>()) {
      if (bar->count() == 2 && bar->tabText(1) == settings.windowTitle()) {
        found_tabs = true;
        CHECK(bar->tabText(0) == brushes.windowTitle());
      }
    }
    CHECK(found_tabs);
  };
  {
    patchy::ui::MainWindow window;
    show_window_empty(window);
    CHECK(window.hotkey_registry().find_command(QStringLiteral("window.brushes_panel")) != nullptr);
    CHECK(window.hotkey_registry().find_command(QStringLiteral("window.brush_settings_panel")) != nullptr);
    check_placement(window);
  }
  // A layout saved before the panels existed.
  {
    patchy::ui::MainWindow window;
    show_window_empty(window);
    window.removeDockWidget(&require_brush_dock(window, "brushesDock"));
    window.removeDockWidget(&require_brush_dock(window, "brushSettingsDock"));
    window.close();
    QApplication::processEvents();
  }
  {
    auto settings = patchy::ui::app_settings();
    auto docks = settings.value(QStringLiteral("window/panelLayoutDocks")).toStringList();
    CHECK(docks.removeAll(QStringLiteral("brushesDock")) == 1);
    CHECK(docks.removeAll(QStringLiteral("brushSettingsDock")) == 1);
    settings.setValue(QStringLiteral("window/panelLayoutDocks"), docks);
    settings.sync();
  }
  patchy::ui::MainWindow restored;
  show_window_empty(restored);
  settle();
  check_placement(restored);
}

void ui_brushes_panel_pick_reaches_working_brush_and_canvas() {
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  auto& library = window.brush_tip_library();
  const auto tip_id = library.add_tip(QStringLiteral("Bar"), make_bar_tip_image(), 0.25, QStringLiteral("Test Tips"));
  CHECK(!tip_id.isEmpty());
  auto& panel = open_brushes_panel(window);
  auto& list = *panel.list();

  // A built-in preset: the whole brush, on the canvas.
  click_row(list, brush_row(list, QStringLiteral("Soft Round")));
  CHECK(window.current_brush().base().id == QStringLiteral("soft_round"));
  CHECK(canvas->brush_softness() == 75);
  CHECK(!canvas->has_brush_tip());

  // A library tip as a ready-to-use brush (its folder opens with a click on the header).
  click_row(list, brush_row(list, QStringLiteral("Test Tips")));
  auto* tip_row = brush_row(list, QStringLiteral("Bar"));
  click_row(list, tip_row);
  CHECK(window.current_brush().base().kind == patchy::ui::BrushBase::Kind::Tip);
  CHECK(canvas->has_brush_tip());
  CHECK(list.currentItem() == tip_row);
  CHECK(tip_row->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Current brush"));

  // The Size slider on top drives the active size group.
  auto* size = panel.findChild<QSpinBox*>(QStringLiteral("brushesPanelSizeSpin"));
  CHECK(size != nullptr);
  size->setValue(40);
  process_events_for(120);
  CHECK(canvas->brush_size() == 40);

  // An edit in Brush Settings marks the picked row modified.
  auto* settings_button = panel.findChild<QToolButton*>(QStringLiteral("brushesPanelSettingsButton"));
  CHECK(settings_button != nullptr);
  settings_button->click();
  settle();
  auto* settings_panel = window.findChild<patchy::ui::BrushSettingsPanel*>(QStringLiteral("brushSettingsPanel"));
  CHECK(settings_panel != nullptr && settings_panel->isVisible());
  settings_panel->select_section(BrushSection::Scattering);
  settings_panel->findChild<QSpinBox*>(QStringLiteral("dynamicsScatterSpin"))->setValue(150);
  process_events_for(120);
  CHECK(window.current_brush().modified());
  CHECK(std::abs(canvas->brush_dynamics().scatter - 1.5) < 1e-9);
  CHECK(tip_row->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Current brush (modified)"));
  // Review artifacts at a 1080p-class window with the shipped tips in the library.
  window.resize(1500, 1000);
  settle();
  for (const auto section : {BrushSection::TipShape, BrushSection::Scattering}) {
    settings_panel->select_section(section);
    settle();
    save_widget_artifact(section == BrushSection::TipShape ? "ui_brush_settings_panel_tip_shape"
                                                           : "ui_brush_settings_panel_scattering",
                         *settings_panel);
  }
  (void)library.restore_default_tips();
  require_action(window, "windowBrushesPanelAction")->trigger();  // hide
  require_action(window, "windowBrushesPanelAction")->trigger();  // and bring Brushes back in front
  settle();
  click_row(list, brush_row(list, QStringLiteral("Patchy Defaults")));
  save_widget_artifact("ui_brushes_panel_list", panel);
  panel.set_view(patchy::ui::BrushesPanel::View::Grid);
  settle();
  save_widget_artifact("ui_brushes_panel_grid", panel);
  panel.set_view(patchy::ui::BrushesPanel::View::List);
  clear_brush_tip_test_state();
}

void ui_new_brush_preset_round_trip_keeps_folder_and_capture_choices() {
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  auto& panel = open_brushes_panel(window);
  window.pick_brush_preset(QStringLiteral("ink"));
  patchy::BrushDynamics dynamics;
  dynamics.size_jitter = 0.3;
  patchy::ui::BrushEdit edit;
  edit.dynamics = dynamics;
  window.current_brush().edit(edit);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* dialog = qobject_cast<QDialog*>(widget);
      if (dialog == nullptr || dialog->objectName() != QStringLiteral("newBrushPresetDialog")) {
        continue;
      }
      saw_dialog = true;
      dialog->findChild<QLineEdit*>(QStringLiteral("newBrushPresetNameEdit"))->setText(QStringLiteral("Panel Probe"));
      dialog->findChild<QComboBox*>(QStringLiteral("newBrushPresetFolderCombo"))->setCurrentText(QStringLiteral("Inks"));
      dialog->findChild<QCheckBox*>(QStringLiteral("newBrushPresetCaptureSizeCheck"))->setChecked(false);
      save_widget_artifact("ui_new_brush_preset_dialog", *dialog);
      dialog->accept();
    }
  });
  panel.findChild<QToolButton*>(QStringLiteral("brushesPanelNewPresetButton"))->click();
  settle();
  CHECK(saw_dialog);

  auto& presets = window.brush_automation_library();
  QJsonObject saved;
  for (const auto& value : presets.presets()) {
    if (value.toObject()["name"].toString() == QStringLiteral("Panel Probe")) {
      saved = value.toObject();
    }
  }
  CHECK(!saved.isEmpty());
  const auto saved_id = saved["id"].toString();
  CHECK(saved["folder"].toString() == QStringLiteral("Inks"));
  CHECK(saved["captureSize"].toBool(true) == false);
  CHECK(window.current_brush().base().id == saved_id);
  CHECK(!window.current_brush().modified());
  CHECK(brush_row(*panel.list(), QStringLiteral("Inks")) != nullptr);  // its folder header

  // Without a captured size, picking the preset keeps the size in use; the rest comes back.
  window.pick_brush_preset(QStringLiteral("hard_round"));
  CHECK(canvas->brush_size() == 18);
  CHECK(canvas->brush_dynamics().size_jitter == 0.0);
  window.pick_brush_preset(saved_id);
  CHECK(canvas->brush_size() == 18);
  CHECK(std::abs(canvas->brush_dynamics().size_jitter - 0.3) < 1e-9);
  CHECK(canvas->brush_softness() == 20);  // Ink's Soft
  presets.remove(saved_id);
  clear_brush_tip_test_state();
}

void ui_brush_section_locks_merge_picked_brush() {
  // picked_over: locked sections keep the current values, everything else comes from the pick.
  patchy::ui::WorkingBrush current;
  // Every persisted dynamics field away from its default, so the section mapping must cover
  // them all: copying every lockable section has to reproduce the whole set.
  current.dynamics = fully_customized_dynamics();
  current.airbrush = true;
  const patchy::ui::WorkingBrush picked;
  patchy::ui::BrushSectionMask all = 0U;
  for (const auto section : patchy::ui::kBrushSections) {
    all |= patchy::ui::brush_section_bit(section);
  }
  const auto locked_all = patchy::ui::picked_over(current, picked, all);
  CHECK(patchy::ui::brush_dynamics_to_json(locked_all.dynamics) ==
        patchy::ui::brush_dynamics_to_json(current.dynamics));
  CHECK(locked_all.airbrush);
  CHECK(locked_all.angle == picked.angle);  // Brush Tip Shape is never locked
  const auto texture_only =
      patchy::ui::picked_over(current, picked, patchy::ui::brush_section_bit(BrushSection::Texture));
  CHECK(texture_only.dynamics.texture_enabled == current.dynamics.texture_enabled);
  CHECK(texture_only.dynamics.texture_scale == current.dynamics.texture_scale);
  CHECK(texture_only.dynamics.scatter == 0.0);
  CHECK(!texture_only.airbrush);
  CHECK(patchy::ui::brush_dynamics_to_json(patchy::ui::picked_over(current, picked, 0U).dynamics) ==
        patchy::ui::brush_dynamics_to_json(picked.dynamics));

  // In the app: a locked Transfer survives a preset pick, the lock is announced, and Reset All
  // Locked Settings clears it.
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  auto& settings_panel = open_brush_settings_panel(window);
  patchy::BrushDynamics dynamics;
  dynamics.opacity_jitter = 0.4;
  dynamics.scatter = 1.0;
  patchy::ui::BrushEdit edit;
  edit.dynamics = dynamics;
  window.current_brush().edit(edit);
  auto* sections = settings_panel.findChild<QTreeWidget*>(QStringLiteral("brushSettingsSectionList"));
  CHECK(sections != nullptr);
  auto* transfer_row = sections->topLevelItem(static_cast<int>(BrushSection::Transfer));
  sections->scrollToItem(transfer_row);
  settle();
  // A click on the row's padlock column.
  QTest::mouseClick(sections->viewport(), Qt::LeftButton, {},
                    QPoint(sections->header()->sectionViewportPosition(1) + sections->columnWidth(1) / 2,
                           sections->visualItemRect(transfer_row).center().y()));
  settle();
  CHECK(window.current_brush().locks() == patchy::ui::brush_section_bit(BrushSection::Transfer));
  auto* banner = settings_panel.findChild<QLabel*>(QStringLiteral("brushSettingsLockBannerLabel"));
  CHECK(banner != nullptr && banner->isVisible());
  CHECK(banner->text().contains(QStringLiteral("Transfer")));
  CHECK(transfer_row->font(0).bold());
  save_widget_artifact("ui_brush_settings_panel_locked", settings_panel);
  window.pick_brush_preset(QStringLiteral("hard_round"));
  CHECK(std::abs(canvas->brush_dynamics().opacity_jitter - 0.4) < 1e-9);
  CHECK(canvas->brush_dynamics().scatter == 0.0);
  CHECK(!window.current_brush().modified());
  require_action(settings_panel, "brushSettingsResetLocksAction")->trigger();
  settle();
  CHECK(window.current_brush().locks() == 0U);
  CHECK(!banner->isVisible());
  window.pick_brush_preset(QStringLiteral("round"));
  CHECK(canvas->brush_dynamics().opacity_jitter == 0.0);
  clear_brush_tip_test_state();
}

void ui_brush_dynamics_form_keeps_fields_it_does_not_edit() {
  // The form edits only the fields it shows: everything else it was loaded with (the imported
  // textureSeed today, engine fields added later) survives an edit of one control.
  patchy::ui::BrushDynamicsPanel form(nullptr, patchy::ui::BrushDynamicsPanel::Presentation::Pages);
  const auto loaded = fully_customized_dynamics();
  form.set_values(loaded, 15.0, 80.0);
  auto* size_jitter = form.section_page(BrushSection::ShapeDynamics)->findChild<QSpinBox*>(
      QStringLiteral("dynamicsSizeJitterSpin"));
  CHECK(size_jitter != nullptr);
  size_jitter->setValue(55);
  auto expected = loaded;
  expected.size_jitter = 0.55;
  CHECK(patchy::ui::brush_dynamics_to_json(form.dynamics()) == patchy::ui::brush_dynamics_to_json(expected));
  CHECK(form.dynamics().texture_seed == 12345U);
}

void ui_brush_settings_sections_grey_out_per_tool() {
  // Sections the active tool's strokes ignore are greyed: the Eraser and Mixer Brush keep the
  // tip and Smoothing, Pattern Stamp only the tip, other tools nothing.
  clear_brush_tip_test_state();
  patchy::ui::MainWindow window;
  show_window(window);
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  auto& panel = open_brush_settings_panel(window);
  auto* sections = panel.findChild<QTreeWidget*>(QStringLiteral("brushSettingsSectionList"));
  CHECK(sections != nullptr);
  const auto enabled = [sections](BrushSection section) {
    return !sections->topLevelItem(static_cast<int>(section))->isDisabled();
  };
  for (const auto section : patchy::ui::kBrushSections) {
    CHECK(enabled(section));
  }
  require_action_by_text(window, QStringLiteral("Eraser"))->trigger();
  settle();
  CHECK(enabled(BrushSection::TipShape));
  CHECK(enabled(BrushSection::Smoothing));
  CHECK(!enabled(BrushSection::ShapeDynamics));
  CHECK(!enabled(BrushSection::BuildUp));
  CHECK(!window.findChild<QToolButton*>(QStringLiteral("brushDynamicsButton"))->isVisible());
  save_widget_artifact("ui_brush_settings_panel_eraser", panel);
  require_action_by_text(window, QStringLiteral("Pattern Stamp"))->trigger();
  settle();
  CHECK(enabled(BrushSection::TipShape));
  CHECK(!enabled(BrushSection::Smoothing));
  require_action_by_text(window, QStringLiteral("Move"))->trigger();
  settle();
  CHECK(!enabled(BrushSection::TipShape));
  require_action_by_text(window, QStringLiteral("Brush"))->trigger();
  settle();
  CHECK(enabled(BrushSection::Texture));
  clear_brush_tip_test_state();
}

}  // namespace

std::vector<patchy::test::TestCase> brush_pattern_palette_tests_part3() {
  return {
      {"ui_brush_panels_open_as_a_tab_group_above_layers", ui_brush_panels_open_as_a_tab_group_above_layers},
      {"ui_brushes_panel_pick_reaches_working_brush_and_canvas", ui_brushes_panel_pick_reaches_working_brush_and_canvas},
      {"ui_new_brush_preset_round_trip_keeps_folder_and_capture_choices",
       ui_new_brush_preset_round_trip_keeps_folder_and_capture_choices},
      {"ui_brush_section_locks_merge_picked_brush", ui_brush_section_locks_merge_picked_brush},
      {"ui_brush_dynamics_form_keeps_fields_it_does_not_edit", ui_brush_dynamics_form_keeps_fields_it_does_not_edit},
      {"ui_brush_settings_sections_grey_out_per_tool", ui_brush_settings_sections_grey_out_per_tool},
  };
}
