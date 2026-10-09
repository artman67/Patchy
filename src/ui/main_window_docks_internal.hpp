#pragma once

// Shared by main_window_docks.cpp (the right column's layout, resize and
// collapse machinery) and main_window_dock_builders.cpp (the panel builders
// create_docks() calls). Never include this header outside those two TUs.

#include <QString>
#include <QWidget>

#include <functional>

class QDockWidget;

namespace patchy::ui {

inline constexpr int kRightDockResizeHandleWidth = 7;
// A panel that starts closed lists its built-in tab partners here (restore_panel_layout).
inline constexpr auto kBuiltInTabPartnersProperty = "patchy.builtInTabPartners";

void install_collapsible_dock_title(QDockWidget* dock,
                                    QWidget* content,
                                    const QString& object_prefix,
                                    int expanded_minimum_height = 0,
                                    int expanded_maximum_height = QWIDGETSIZE_MAX,
                                    bool initially_expanded = true,
                                    int expanded_preferred_height = 0,
                                    std::function<void(bool)> panel_toggled = {});

}  // namespace patchy::ui
