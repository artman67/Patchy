#pragma once

#include "ui/brush_automation.hpp"

#include <QString>
#include <QStringList>

#include <optional>

class QWidget;

namespace patchy::ui {

struct NewBrushPresetRequest {
  QString name;
  BrushPresetSaveOptions options;
};

// Photoshop's New Brush Preset dialog: Name, Folder (any existing folder or a new name), and
// what to capture besides the brush (size, tool settings, color). Modal; nullopt on Cancel.
[[nodiscard]] std::optional<NewBrushPresetRequest> request_new_brush_preset(QWidget* parent,
                                                                           const QString& default_name,
                                                                           const QStringList& folders,
                                                                           const QString& default_folder);

}  // namespace patchy::ui
