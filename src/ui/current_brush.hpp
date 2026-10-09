#pragma once

#include "core/brush_dynamics.hpp"
#include "core/brush_tip.hpp"

#include <QObject>
#include <QString>

#include <memory>
#include <optional>

namespace patchy::ui {

struct BrushPreset;
struct BrushTipEntry;

// The working brush of the tip-stamping tools (Brush, Mixer Brush, Pattern Stamp). Size,
// Opacity, Flow, Soft and Airbrush are the paint group that the other footprint tools share;
// the Eraser keeps its own size group in MainWindow but stamps this brush's tip.
struct WorkingBrush {
  // builtin_round_brush_tip_id(), builtin_square_brush_tip_id(), or a BrushTipLibrary id. With
  // snapshot_tip set it is the id a script captured, or empty for a saved preset's embedded tip.
  QString tip_id;
  std::shared_ptr<const patchy::BrushTip> snapshot_tip;  // pixels for a tip outside the library
  int size{25};
  int opacity{100};
  int flow{100};
  int softness{0};
  bool airbrush{false};
  double angle{0.0};
  double roundness{100.0};
  std::optional<double> spacing;  // fraction of size; nullopt = the tip's own spacing
  patchy::BrushDynamics dynamics{};
};

// Equal settings: the dynamics compare through their persisted JSON form (the per-stroke seed
// is not a setting).
[[nodiscard]] bool same_brush_settings(const WorkingBrush& a, const WorkingBrush& b);

// A built-in preset as a complete working brush: its procedural tip with no dynamics.
[[nodiscard]] WorkingBrush working_brush_from_preset(const BrushPreset& preset);
// Identifies the working brush's tip, so a host can drop a late edit meant for an earlier tip.
[[nodiscard]] QString working_brush_tip_key(const WorkingBrush& brush);

// A partial edit; only the fields that are set change.
struct BrushEdit {
  std::optional<int> size;
  std::optional<int> opacity;
  std::optional<int> flow;
  std::optional<int> softness;
  std::optional<bool> airbrush;
  std::optional<double> angle;
  std::optional<double> roundness;
  std::optional<patchy::BrushDynamics> dynamics;
  std::optional<std::optional<double>> spacing;
};

// What the working brush was last picked from, and the brush as it was picked.
struct BrushBase {
  enum class Kind { Preset, Tip };
  Kind kind{Kind::Preset};
  QString id;  // a built-in or saved preset id, or a tip id
  WorkingBrush settings;
};

// The one owner of the working brush. Every writer (options bar, tip picker, Dynamics popup,
// presets, scripts, canvas gestures) goes through edit()/pick()/pick_tip(); MainWindow pushes
// changed() to the active canvas. See docs/brushes.md, "Current brush".
class CurrentBrush : public QObject {
  Q_OBJECT

public:
  enum Change : unsigned {
    Tip = 1U << 0,
    Size = 1U << 1,
    Opacity = 1U << 2,
    Flow = 1U << 3,
    Softness = 1U << 4,
    Airbrush = 1U << 5,
    TipShape = 1U << 6,  // angle and roundness
    Dynamics = 1U << 7,
    Spacing = 1U << 8,
    Base = 1U << 9,
    All = (1U << 10) - 1U,
  };

  explicit CurrentBrush(QObject* parent = nullptr);

  [[nodiscard]] const WorkingBrush& brush() const noexcept { return brush_; }
  [[nodiscard]] const BrushBase& base() const noexcept { return base_; }
  // True when the live settings differ from the brush as it was picked.
  [[nodiscard]] bool modified() const;
  // True when the working brush stamps a built-in procedural tip (Round or Square).
  [[nodiscard]] bool procedural() const;

  // Edits the working brush only; the base stays. Values clamp to the engine's ranges.
  void edit(const BrushEdit& edit);
  // Replaces the whole working brush and records it as the base.
  void pick(const WorkingBrush& brush, BrushBase::Kind kind, const QString& id);
  // Loads a tip as the starting point: a library entry brings its stored tip shape and
  // dynamics (and, with apply_tool_settings, its imported Flow/Airbrush); the procedural tips
  // bring back the session's last procedural shape and dynamics. Size, Opacity and Soft stay.
  void pick_tip(const QString& tip_id, const BrushTipEntry* entry, bool apply_tool_settings);
  // Records the current brush as the base without changing it (after saving it as a preset).
  void rebase(BrushBase::Kind kind, const QString& id);

signals:
  // Change flags for what may differ; listeners may ignore them and re-read brush().
  void changed(unsigned changes);

private:
  void replace(WorkingBrush next, unsigned changes);

  WorkingBrush brush_;
  BrushBase base_;
  // Round and Square share one session tip shape and dynamics: leaving them for a bitmap tip
  // and coming back restores what was last used on them.
  struct ProceduralShape {
    double angle{0.0};
    double roundness{100.0};
    patchy::BrushDynamics dynamics{};
  } procedural_;
};

}  // namespace patchy::ui
