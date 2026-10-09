#include "brush_pattern_palette_test_support.hpp"

#include "test_harness.hpp"
#include "ui/brush_settings_panel.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QDockWidget>
#include <QToolButton>

#include <cstring>

namespace patchy::test::ui {

QImage make_bar_tip_image() {
  // 16x16 coverage mask with an opaque horizontal bar through the middle (rows 6-9).
  QImage mask(16, 16, QImage::Format_Grayscale8);
  mask.fill(0);
  for (int y = 6; y <= 9; ++y) {
    auto* row = mask.scanLine(y);
    std::memset(row, 255, 16);
  }
  return mask;
}

patchy::ui::BrushSettingsPanel& open_brush_settings_panel(patchy::ui::MainWindow& window) {
  auto* button = window.findChild<QToolButton*>(QStringLiteral("brushDynamicsButton"));
  CHECK(button != nullptr);
  CHECK(button->isVisible());
  CHECK(button->isEnabled());
  button->click();
  for (int pass = 0; pass < 4; ++pass) {
    QApplication::processEvents();
  }
  auto* dock = window.findChild<QDockWidget*>(QStringLiteral("brushSettingsDock"));
  CHECK(dock != nullptr);
  CHECK(dock->isVisible());
  auto* panel = window.findChild<patchy::ui::BrushSettingsPanel*>(QStringLiteral("brushSettingsPanel"));
  CHECK(panel != nullptr);
  CHECK(panel->isVisible());
  return *panel;
}

}  // namespace patchy::test::ui
