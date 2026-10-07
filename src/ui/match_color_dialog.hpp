#pragma once

#include "core/match_color.hpp"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

class QWidget;

namespace patchy::ui {

// One open document the Match Color dialog offers as a statistics source.
struct MatchColorSourceDocument {
  QString title;
  // Its color-bearing layers, top first. The dialog appends a Merged entry.
  QStringList layer_names;
  // Initial Layer entry: an index into layer_names, or -1 for Merged.
  int default_layer{-1};
  bool has_selection{false};
};

struct MatchColorDialogSpec {
  QString target_description;  // Photoshop's "Target: name (layer, RGB/8)" line
  bool target_has_selection{false};
  std::vector<MatchColorSourceDocument> sources;
};

struct MatchColorDialogSettings {
  MatchColorOptions options;
  int source{-1};        // index into MatchColorDialogSpec::sources, -1 = None
  int source_layer{-1};  // index into that source's layer_names, -1 = Merged
  bool use_source_selection{false};
  bool use_target_selection{false};
  bool ignore_selection{false};
};

// Photoshop's Match Color dialog without Load/Save Statistics. Checkboxes that
// need a selection are disabled (and read as unchecked) when there is none.
[[nodiscard]] std::optional<MatchColorDialogSettings> request_match_color_settings(
    QWidget* parent, const MatchColorDialogSpec& spec,
    std::function<void(bool, const MatchColorDialogSettings&)> preview_changed = {});

}  // namespace patchy::ui
