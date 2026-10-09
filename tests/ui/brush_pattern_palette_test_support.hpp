#pragma once

// Shared helper moved verbatim from tests/ui/brush_pattern_palette_tests.cpp when
// that TU was split into part files (used by both parts). Moved, never copied.

#include <QImage>

namespace patchy::ui {
class BrushSettingsPanel;
class MainWindow;
}  // namespace patchy::ui

namespace patchy::test::ui {

QImage make_bar_tip_image();

// Shows the Brush Settings panel the way the artist does, through the options-bar Dynamics
// button (the Brush tool must be active), and returns it.
patchy::ui::BrushSettingsPanel& open_brush_settings_panel(patchy::ui::MainWindow& window);

}  // namespace patchy::test::ui
