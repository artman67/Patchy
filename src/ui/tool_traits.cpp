#include "ui/tool_traits.hpp"

#include "ui/canvas_widget.hpp"

#include <QtGlobal>

#include <array>

namespace patchy::ui {

namespace {

// Tooltip second lines and activation hints, kept out of the rows for
// readability. Translated in the MainWindow context when shown.
constexpr const char* kMoveTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "%CTRL%+click selects a layer, Shift+click toggles it. %CTRL%+drag selects layers in a "
                      "rectangle; hold Shift before dragging to add. %ALT%+drag duplicates. Shift constrains layer movement.");
constexpr const char* kPenTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Click to place points, drag for curves. On a path: click a segment to add a "
                      "point, click a point to delete it, %ALT%+click converts it, %CTRL% moves points.");
constexpr const char* kPathSelectTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Select and move whole shapes. %CTRL%+T transforms the path.");
constexpr const char* kDirectSelectTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Select and drag points and handles. Delete removes the selected points.");
constexpr const char* kAddAnchorTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Click a path segment to insert a point.");
constexpr const char* kDeleteAnchorTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Click a point to remove it.");
constexpr const char* kConvertPointTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Click a point to switch it between corner and smooth.");
constexpr const char* kRotateViewTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Drag to turn the view. Shift snaps to 15 degrees; Esc resets.");
constexpr const char* kHistoryBrushTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Paints pixels back from the history source state. Right-click a "
                      "History panel row to make it the source.");
constexpr const char* kColorReplacementTooltipDetail =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Paints the foreground color over pixels that match the sampled "
                      "color and keeps their shading. %ALT%+click picks the foreground color.");
constexpr const char* kMoveActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Move: %CTRL%+click selects a layer, Shift+click toggles it. %CTRL%+drag selects a "
                      "rectangle; Shift adds. Drag selected artwork to move it, %ALT%+drag to duplicate it.");
constexpr const char* kPenActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Pen: click to add points, drag for curves. On a path, click a segment to add a "
                      "point, click a point to delete it, %ALT%+click converts it, %CTRL%+drag selects or "
                      "moves points.");
constexpr const char* kPathSelectActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Path Select: click a shape to select it, drag to move it. %CTRL%+T transforms the "
                      "path, Delete removes the selected points.");
constexpr const char* kDirectSelectActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Direct Select: click or marquee points, drag points or handles. Shift adds, "
                      "arrows nudge, Delete removes, %CTRL%+T transforms the selected points.");
constexpr const char* kCropActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Crop: drag the handles to crop or extend the canvas (%ALT% resizes about the "
                      "center), drag inside to lay out a new box, drag outside it to rotate. Enter crops, Esc "
                      "resets the box.");
constexpr const char* kAddAnchorActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Add Anchor Point: click a path segment to insert a point.");
constexpr const char* kDeleteAnchorActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Delete Anchor Point: click a point to remove it.");
constexpr const char* kConvertPointActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Convert Point: click a point to switch it between corner and smooth.");
constexpr const char* kHistoryBrushActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "History Brush: paint to restore the active layer from the history source "
                      "state, the document as opened unless you right-click a History panel row to change it.");
constexpr const char* kColorReplacementActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Color Replacement: paint over a color to replace it with the "
                      "foreground color. Tolerance sets how close a pixel must be to the sampled color.");
constexpr const char* kPatchToolActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Patch: draw around the area to fix, then drag the selection to a clean source area, or press Enter to remove the object automatically");
constexpr const char* kMarqueeActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Rectangular Marquee: drag to select. Drag a handle to resize the selection, or drag "
                      "inside it to move it.");
constexpr const char* kEllipticalMarqueeActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Elliptical Marquee: drag to select. Drag a handle to resize the selection, or drag "
                      "inside it to move it.");
constexpr const char* kRotateViewActivationHint =
    QT_TRANSLATE_NOOP("patchy::ui::MainWindow",
                      "Rotate View: drag to turn the canvas view; Shift snaps to 15 degrees. "
                      "Double-click the tool or press Esc to reset.");

// Exactly one row per CanvasTool, in enum order (checked below). Names are
// translated where they are shown: name in the patchy::ui::MainWindow
// context, display_name in the QObject context.
constexpr std::array<ToolTraits, kCanvasToolCount> kToolTraits{{
    {.tool = CanvasTool::Move,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Move"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Move"),
     .hotkey_id = "tools.move",
     .default_key = Qt::Key_V,
     .icon = "tool-move",
     .flags = kToolBlockedOnMasks | kToolBlockedInChannelView,
     .tooltip_detail = kMoveTooltipDetail,
     .activation_hint = kMoveActivationHint},
    {.tool = CanvasTool::Marquee,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Marquee"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Marquee"),
     .hotkey_id = "tools.marquee",
     .default_key = Qt::Key_M,
     .icon = "tool-marquee",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 0,
     .activation_hint = kMarqueeActivationHint},
    {.tool = CanvasTool::EllipticalMarquee,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Elliptical Marquee"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Elliptical Marquee"),
     .hotkey_id = "tools.elliptical_marquee",
     .icon = "tool-marquee-ellipse",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 1,
     .activation_hint = kEllipticalMarqueeActivationHint},
    {.tool = CanvasTool::Lasso,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Lasso"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Lasso"),
     .hotkey_id = "tools.lasso",
     .default_key = Qt::Key_L,
     .icon = "tool-lasso",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 2},
    {.tool = CanvasTool::MagneticLasso,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Magnetic Lasso"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Magnetic Lasso"),
     .hotkey_id = "tools.magnetic_lasso",
     .icon = "tool-magnetic-lasso",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 3},
    {.tool = CanvasTool::MagicWand,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Magic Wand"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Magic Wand"),
     .hotkey_id = "tools.magic_wand",
     .default_key = Qt::Key_W,
     .icon = "tool-wand",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 4},
    {.tool = CanvasTool::QuickSelect,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Quick Select"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Quick Select"),
     .hotkey_id = "tools.quick_select",
     .icon = "tool-quick-select",
     .flags = kToolBlockedOnMasks,
     .selection_slot = 5},
    {.tool = CanvasTool::Brush,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Brush"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Brush"),
     .hotkey_id = "tools.brush",
     .default_key = Qt::Key_B,
     .icon = "tool-brush",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity |
              kToolBrushOpacitySlider | kToolOpacityDigitKeys | kToolFlow | kToolSmoothing |
              kToolBrushPresets | kToolBrushTip | kToolAltClickPicksColor | kToolBrushInfo},
    {.tool = CanvasTool::Clone,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Clone"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Clone Stamp"),
     .hotkey_id = "tools.clone",
     .default_key = Qt::Key_S,
     .icon = "tool-clone",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity |
              kToolBrushOpacitySlider | kToolOpacityDigitKeys | kToolBrushPresets | kToolBlockedOnMasks |
              kToolBlockedInChannelView | kToolBrushInfo},
    // No default key: R belongs to Rotate View (Photoshop's default).
    {.tool = CanvasTool::Smudge,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Smudge"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Smudge"),
     .hotkey_id = "tools.smudge",
     .icon = "tool-smudge",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity |
              kToolBrushOpacitySlider | kToolOpacityDigitKeys | kToolBrushPresets | kToolAltClickPicksColor |
              kToolBlockedOnMasks | kToolBlockedInChannelView | kToolBrushInfo},
    {.tool = CanvasTool::Eraser,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Eraser"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Eraser"),
     .hotkey_id = "tools.eraser",
     .default_key = Qt::Key_E,
     .icon = "tool-eraser",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity |
              kToolBrushOpacitySlider | kToolOpacityDigitKeys | kToolSmoothing | kToolBrushPresets |
              kToolBrushTip | kToolAltClickPicksColor | kToolBrushInfo},
    {.tool = CanvasTool::Gradient,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Gradient"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Gradient"),
     .hotkey_id = "tools.gradient",
     .default_key = Qt::Key_G,
     .icon = "tool-gradient",
     .flags = kToolOpacityDigitKeys | kToolAltClickPicksColor},
    // No default key: Ctrl+Shift+U belongs to Desaturate.
    {.tool = CanvasTool::Line,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Line"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Line"),
     .hotkey_id = "tools.line",
     .icon = "tool-line",
     .flags = kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity | kToolBrushOpacitySlider |
              kToolOpacityDigitKeys | kToolAltClickPicksColor | kToolBrushInfo},
    {.tool = CanvasTool::Rectangle,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Rect"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Rectangle"),
     .hotkey_id = "tools.rect",
     .default_key = Qt::Key_U,
     .icon = "tool-rect",
     .flags = kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity | kToolBrushOpacitySlider |
              kToolOpacityDigitKeys | kToolBrushInfo},
    {.tool = CanvasTool::Ellipse,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Ellipse"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Ellipse"),
     .hotkey_id = "tools.ellipse",
     .icon = "tool-ellipse",
     .flags = kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity | kToolBrushOpacitySlider |
              kToolOpacityDigitKeys | kToolBrushInfo},
    {.tool = CanvasTool::Fill,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Fill"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Fill"),
     .hotkey_id = "tools.fill",
     .icon = "tool-fill",
     .flags = kToolAltClickPicksColor},
    {.tool = CanvasTool::Eyedropper,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pick"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Eyedropper"),
     .hotkey_id = "tools.eyedropper",
     .default_key = Qt::Key_I,
     .icon = "tool-eyedropper"},
    {.tool = CanvasTool::Text,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Type"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Type"),
     .hotkey_id = "tools.type",
     .default_key = Qt::Key_T,
     .icon = "tool-text",
     .flags = kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::Pan,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Hand"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Pan"),
     .hotkey_id = "tools.hand",
     .default_key = Qt::Key_H,
     .icon = "tool-pan",
     .flags = kToolViewOnly},
    {.tool = CanvasTool::Zoom,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Zoom"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Zoom"),
     .hotkey_id = "tools.zoom",
     .default_key = Qt::Key_Z,
     .icon = "tool-zoom",
     .flags = kToolViewOnly},
    {.tool = CanvasTool::Healing,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Healing Brush"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Healing Brush"),
     .hotkey_id = "tools.healing",
     .default_key = Qt::Key_J,
     .icon = "tool-healing",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBrushOpacity |
              kToolBrushOpacitySlider | kToolOpacityDigitKeys | kToolBrushPresets | kToolBlockedOnMasks |
              kToolBlockedInChannelView | kToolBrushInfo},
    {.tool = CanvasTool::Dodge,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Dodge"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Dodge"),
     .hotkey_id = "tools.dodge",
     .default_key = Qt::Key_O,
     .icon = "tool-dodge",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::Burn,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Burn"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Burn"),
     .hotkey_id = "tools.burn",
     .icon = "tool-burn",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::Sponge,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Sponge"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Sponge"),
     .hotkey_id = "tools.sponge",
     .icon = "tool-sponge",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::BlurBrush,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Blur"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Blur"),
     .hotkey_id = "tools.blur",
     .icon = "tool-blur",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::SharpenBrush,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Sharpen"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Sharpen"),
     .hotkey_id = "tools.sharpen",
     .icon = "tool-sharpen",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolBlockedOnMasks | kToolBlockedInChannelView},
    {.tool = CanvasTool::PatternStamp,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pattern Stamp"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Pattern Stamp"),
     .hotkey_id = "tools.pattern_stamp",
     .icon = "tool-pattern-stamp",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushOpacity | kToolOpacityDigitKeys | kToolFlow |
              kToolBrushTip | kToolAltClickPicksColor | kToolBlockedOnMasks | kToolBlockedInChannelView |
              kToolBrushInfo},
    {.tool = CanvasTool::MixerBrush,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Mixer Brush"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Mixer Brush"),
     .hotkey_id = "tools.mixer_brush",
     .icon = "tool-mixer-brush",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolSmoothing | kToolBrushPresets | kToolBrushTip |
              kToolAltClickPicksColor | kToolBlockedOnMasks | kToolBlockedInChannelView | kToolBrushInfo},
    {.tool = CanvasTool::Pen,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Pen"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Pen"),
     .hotkey_id = "tools.pen",
     .default_key = Qt::Key_P,
     .icon = "tool-pen",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kPenTooltipDetail,
     .activation_hint = kPenActivationHint},
    {.tool = CanvasTool::PathSelect,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Path Select"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Path Select"),
     .hotkey_id = "tools.path_select",
     .default_key = Qt::Key_A,
     .icon = "tool-path-select",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kPathSelectTooltipDetail,
     .activation_hint = kPathSelectActivationHint},
    {.tool = CanvasTool::DirectSelect,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Direct Select"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Direct Select"),
     .hotkey_id = "tools.direct_select",
     .icon = "tool-direct-select",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kDirectSelectTooltipDetail,
     .activation_hint = kDirectSelectActivationHint},
    {.tool = CanvasTool::Polygon,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Polygon"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Polygon"),
     .hotkey_id = "tools.polygon",
     .icon = "tool-polygon"},
    {.tool = CanvasTool::CustomShape,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Custom Shape"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Custom Shape"),
     .hotkey_id = "tools.custom_shape",
     .icon = "tool-custom-shape"},
    {.tool = CanvasTool::SpotHealing,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Spot Healing"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Spot Healing"),
     .hotkey_id = "tools.spot_healing",
     .icon = "tool-spot-healing",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolBlockedOnMasks |
              kToolBlockedInChannelView},
    {.tool = CanvasTool::PatchTool,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Patch"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Patch"),
     .hotkey_id = "tools.patch",
     .icon = "tool-patch",
     .flags = kToolBlockedOnMasks | kToolBlockedInChannelView,
     .selection_slot = 6,
     .activation_hint = kPatchToolActivationHint},
    {.tool = CanvasTool::Crop,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Crop"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Crop"),
     .hotkey_id = "tools.crop",
     .default_key = Qt::Key_C,
     .icon = "tool-crop",
     .flags = kToolBlockedOnMasks | kToolBlockedInChannelView,
     .activation_hint = kCropActivationHint},
    {.tool = CanvasTool::AddAnchor,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Add Anchor"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Add Anchor Point"),
     .hotkey_id = "tools.add_anchor",
     .icon = "tool-add-anchor",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kAddAnchorTooltipDetail,
     .activation_hint = kAddAnchorActivationHint},
    {.tool = CanvasTool::DeleteAnchor,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Delete Anchor"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Delete Anchor Point"),
     .hotkey_id = "tools.delete_anchor",
     .icon = "tool-delete-anchor",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kDeleteAnchorTooltipDetail,
     .activation_hint = kDeleteAnchorActivationHint},
    {.tool = CanvasTool::ConvertPoint,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Convert Point"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Convert Point"),
     .hotkey_id = "tools.convert_point",
     .icon = "tool-convert-point",
     .flags = kToolBlockedOnMasks,
     .tooltip_detail = kConvertPointTooltipDetail,
     .activation_hint = kConvertPointActivationHint},
    {.tool = CanvasTool::RotateView,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Rotate View"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Rotate View"),
     .hotkey_id = "tools.rotate_view",
     .default_key = Qt::Key_R,
     .icon = "tool-rotate-view",
     .flags = kToolViewOnly,
     .tooltip_detail = kRotateViewTooltipDetail,
     .activation_hint = kRotateViewActivationHint},
    {.tool = CanvasTool::HistoryBrush,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "History Brush"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "History Brush"),
     .hotkey_id = "tools.history_brush",
     .default_key = Qt::Key_Y,
     .icon = "tool-history-brush",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushOpacity | kToolOpacityDigitKeys | kToolFlow |
              kToolBrushTip | kToolAltClickPicksColor | kToolBlockedOnMasks | kToolBlockedInChannelView |
              kToolBrushInfo,
     .tooltip_detail = kHistoryBrushTooltipDetail,
     .activation_hint = kHistoryBrushActivationHint},
    // A local adjustment so it rides the same stroke flow (snapshot, one undo
    // step, refusals); local_adjustment_brush_segment hands it to its own engine.
    {.tool = CanvasTool::ColorReplacement,
     .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Color Replacement"),
     .display_name = QT_TRANSLATE_NOOP("QObject", "Color Replacement"),
     .hotkey_id = "tools.color_replacement",
     .icon = "tool-color-replacement",
     .flags = kToolPaintsStrokes | kToolBrushSize | kToolBrushSizeSliders | kToolLocalAdjustment |
              kToolAltClickPicksColor | kToolBlockedOnMasks | kToolBlockedInChannelView,
     .tooltip_detail = kColorReplacementTooltipDetail,
     .activation_hint = kColorReplacementActivationHint},
}};

constexpr bool rows_follow_enum_order() {
  for (std::size_t index = 0; index < kToolTraits.size(); ++index) {
    if (static_cast<std::size_t>(kToolTraits[index].tool) != index || kToolTraits[index].name == nullptr) {
      return false;
    }
  }
  return true;
}
static_assert(rows_follow_enum_order(), "kToolTraits needs exactly one row per CanvasTool, in enum order");

constexpr bool selection_slots_fit() {
  for (const auto& traits : kToolTraits) {
    if (traits.selection_slot < -1 ||
        traits.selection_slot >= static_cast<int>(CanvasWidget::kSelectionToolCount)) {
      return false;
    }
  }
  return true;
}
static_assert(selection_slots_fit(), "selection_slot must index CanvasWidget's per-selection-tool arrays");

// Out-of-range values (never produced by the palette) keep the old switch
// fallbacks: a generic name and no capabilities.
constexpr ToolTraits kUnknownToolTraits{.tool = static_cast<CanvasTool>(-1),
                                        .name = QT_TRANSLATE_NOOP("patchy::ui::MainWindow", "Tool"),
                                        .display_name = QT_TRANSLATE_NOOP("QObject", "Tool"),
                                        .hotkey_id = "tools.unknown",
                                        .icon = "tool-move"};

}  // namespace

const ToolTraits& tool_traits(CanvasTool tool) noexcept {
  const auto index = static_cast<std::size_t>(tool);
  return index < kToolTraits.size() ? kToolTraits[index] : kUnknownToolTraits;
}

std::vector<CanvasTool> tools_with(ToolTraitFlags flag) {
  std::vector<CanvasTool> tools;
  for (const auto& traits : kToolTraits) {
    if ((traits.flags & flag) != 0) {
      tools.push_back(traits.tool);
    }
  }
  return tools;
}

}  // namespace patchy::ui
