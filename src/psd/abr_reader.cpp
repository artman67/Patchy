#include "psd/abr_reader.hpp"

#include "psd/psd_binary.hpp"
#include "psd/psd_descriptor.hpp"
#include "psd/psd_patterns.hpp"
#include "support/translate_noop.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace patchy::psd {

namespace {

constexpr std::int32_t kMaxBrushDimension = 4096;
// A computed brush renders at least this many pixels across so it stays smooth when enlarged,
// and at most Patchy's largest brush size.
constexpr std::int32_t kMinComputedMaskSize = 256;
constexpr std::int32_t kMaxComputedMaskSize = 1024;

struct DescBrushInfo {
  bool computed{false};    // 'computedBrush' (no bitmap) rather than 'sampledBrush'
  std::size_t desc_index{0};  // position in the desc 'Brsh' list, which 'phry' follows
  std::string name;
  std::string sample_id;   // the sampledBrush 'sampledData' uuid naming its 'samp' entry
  std::optional<double> spacing;  // fraction of diameter
  std::optional<double> diameter;  // 'Dmtr'
  bool capture_size{true};  // Photoshop's "Capture Brush Size in Preset" ('useBrushSize')
  double hardness{1.0};     // computed 'Hrdn' as a fraction
  double base_angle_degrees{0.0};
  double base_roundness{100.0};
  BrushDynamics dynamics{};
  std::optional<int> tool_flow_percent;
  std::optional<bool> tool_airbrush;
};

// One 'brVr' variation object: {'bVTy' control, 'fStp' fade steps, 'jitter' %, 'Mnm ' minimum %}.
struct VariationRead {
  double jitter{0.0};     // 0..1
  double minimum{0.0};    // 0..1
  int control{0};         // raw bVTy value
  int fade_steps{25};
};

VariationRead read_variation(const DescriptorObject& preset, std::string_view key) {
  VariationRead out;
  const auto* object = descriptor_object(preset, key);
  if (object == nullptr) {
    return out;
  }
  out.jitter = std::clamp(descriptor_number(*object, "jitter") / 100.0, 0.0, 10.0);
  out.minimum = std::clamp(descriptor_number(*object, "Mnm ") / 100.0, 0.0, 1.0);
  out.control = static_cast<int>(descriptor_number(*object, "bVTy"));
  out.fade_steps = std::max(1, static_cast<int>(descriptor_number(*object, "fStp", 25.0)));
  return out;
}

// Photoshop's 'bVTy' control values: 0 Off, 1 Fade, 2 Pen Pressure, 3 Pen Tilt, 4 Stylus Wheel,
// 5 Rotation, 6 Initial Direction, 7 Direction. Unknown values degrade to Off (the jitter still
// imports).
[[nodiscard]] BrushDynamicControl control_from_bvty(int value) {
  switch (value) {
    case 1: return BrushDynamicControl::Fade;
    case 2: return BrushDynamicControl::PenPressure;
    case 3: return BrushDynamicControl::PenTilt;
    case 4: return BrushDynamicControl::StylusWheel;
    case 5: return BrushDynamicControl::PenRotation;
    case 6: return BrushDynamicControl::InitialDirection;
    case 7: return BrushDynamicControl::Direction;
    default: return BrushDynamicControl::Off;
  }
}

// bVTy for a non-angle dynamic. A Photoshop "Off" (0) means the preset author chose no control,
// which in Patchy maps to the slot's default: GlobalDefault for size/roundness/opacity so the
// user's global pen preferences keep working on imported jitter-only packs (Photoshop's
// options-bar pressure-override buttons are the analog of those preferences), plain Off for
// scatter/count. Direction/InitialDirection are angle-only and degrade to Off.
[[nodiscard]] BrushDynamicControl non_angle_control_from_bvty(int value,
                                                              BrushDynamicControl zero_default) {
  switch (value) {
    case 1: return BrushDynamicControl::Fade;
    case 2: return BrushDynamicControl::PenPressure;
    case 3: return BrushDynamicControl::PenTilt;
    case 4: return BrushDynamicControl::StylusWheel;
    case 5: return BrushDynamicControl::PenRotation;
    case 6:
    case 7: return BrushDynamicControl::Off;
    default: return zero_default;
  }
}

// Extracts the supported dynamics from a brushPreset descriptor. Keys verified against a
// Photoshop 2026 export (test-fixtures/abr/photoshop-dynamics.abr): the preset-level
// flipX/flipY are the flip jitters (the static tip flips live inside the 'Brsh' object), the
// minimum diameter/roundness are preset-level siblings of the 'brVr' objects, and 'Cnt ' is a
// double. Every dynamic's control imports (size/roundness/opacity map bVTy 0 through
// non_angle_control_from_bvty to GlobalDefault). The later blocks add the compatible static
// Texture, single Dual Brush, Color Dynamics, and Wet Edges subsets.
[[nodiscard]] std::uint32_t stable_string_seed(std::string_view text) noexcept {
  std::uint32_t hash = 2166136261U;
  for (const auto character : text) {
    hash ^= static_cast<std::uint8_t>(character);
    hash *= 16777619U;
  }
  return hash;
}

[[nodiscard]] std::string descriptor_string(const DescriptorObject& object, std::string_view key) {
  const auto* value = descriptor_value(object, key);
  return value != nullptr && value->type == DescriptorValue::Type::String ? value->string_value
                                                                          : std::string{};
}

[[nodiscard]] std::string descriptor_enum(const DescriptorObject& object, std::string_view key) {
  const auto* value = descriptor_value(object, key);
  return value != nullptr && value->type == DescriptorValue::Type::Enum ? value->enum_value
                                                                        : std::string{};
}

[[nodiscard]] BrushTextureStyle texture_style_from_name(std::string_view name) noexcept {
  const auto contains = [name](std::string_view needle) {
    return name.find(needle) != std::string_view::npos;
  };
  if (contains("Canvas") || contains("canvas") || contains("Burlap") || contains("Towel") ||
      contains("Paper") || contains("paper")) {
    return BrushTextureStyle::Canvas;
  }
  if (contains("Dot") || contains("dot") || contains("Pebble") || contains("Stone")) {
    return BrushTextureStyle::Speckle;
  }
  return BrushTextureStyle::FineGrain;
}

// Photoshop writes the texture 'BlnM' value as a 4-char code for the classic modes and as a
// long id for the newer ones; both spellings are accepted. nullopt = an unknown mode.
[[nodiscard]] std::optional<BrushTextureMode> texture_mode_from_code(std::string_view code) noexcept {
  struct Entry {
    std::string_view code;
    BrushTextureMode mode;
  };
  static constexpr std::array<Entry, 18> kModes{{
      {"Mltp", BrushTextureMode::Multiply},
      {"multiply", BrushTextureMode::Multiply},
      {"Sbtr", BrushTextureMode::Subtract},
      {"subtract", BrushTextureMode::Subtract},
      {"Drkn", BrushTextureMode::Darken},
      {"darken", BrushTextureMode::Darken},
      {"Ovrl", BrushTextureMode::Overlay},
      {"overlay", BrushTextureMode::Overlay},
      {"CDdg", BrushTextureMode::ColorDodge},
      {"colorDodge", BrushTextureMode::ColorDodge},
      {"CBrn", BrushTextureMode::ColorBurn},
      {"colorBurn", BrushTextureMode::ColorBurn},
      {"linearBurn", BrushTextureMode::LinearBurn},
      {"hardMix", BrushTextureMode::HardMix},
      {"linearHeight", BrushTextureMode::LinearHeight},
      {"Hght", BrushTextureMode::Height},
      {"height", BrushTextureMode::Height},
      {"Nrml", BrushTextureMode::Multiply},  // never written for textures; harmless default
  }};
  for (const auto& entry : kModes) {
    if (entry.code == code) {
      return entry.mode;
    }
  }
  return std::nullopt;
}

[[nodiscard]] BrushDynamics parse_brush_dynamics(const DescriptorObject& preset,
                                                  const DescriptorObject& primary_brush) {
  BrushDynamics dynamics;
  if (descriptor_bool(preset, "useTipDynamics")) {
    const auto size = read_variation(preset, "szVr");
    dynamics.size_jitter = std::clamp(size.jitter, 0.0, 1.0);
    dynamics.minimum_diameter =
        std::clamp(descriptor_number(preset, "minimumDiameter", size.minimum * 100.0) / 100.0, 0.0, 1.0);
    dynamics.size_control =
        non_angle_control_from_bvty(size.control, BrushDynamicControl::GlobalDefault);
    dynamics.size_fade_steps = size.fade_steps;
    const auto angle = read_variation(preset, "angleDynamics");
    dynamics.angle_jitter = std::clamp(angle.jitter, 0.0, 1.0);
    dynamics.angle_control = control_from_bvty(angle.control);
    dynamics.angle_fade_steps = angle.fade_steps;
    const auto roundness = read_variation(preset, "roundnessDynamics");
    dynamics.roundness_jitter = std::clamp(roundness.jitter, 0.0, 1.0);
    dynamics.minimum_roundness =
        std::clamp(descriptor_number(preset, "minimumRoundness", 25.0) / 100.0, 0.0, 1.0);
    dynamics.roundness_control =
        non_angle_control_from_bvty(roundness.control, BrushDynamicControl::GlobalDefault);
    dynamics.roundness_fade_steps = roundness.fade_steps;
    dynamics.flip_x_jitter = descriptor_bool(preset, "flipX");
    dynamics.flip_y_jitter = descriptor_bool(preset, "flipY");
  }
  if (descriptor_bool(preset, "useScatter")) {
    const auto scatter = read_variation(preset, "scatterDynamics");
    dynamics.scatter = std::clamp(scatter.jitter, 0.0, 10.0);
    dynamics.scatter_both_axes = descriptor_bool(preset, "bothAxes");
    dynamics.scatter_control =
        non_angle_control_from_bvty(scatter.control, BrushDynamicControl::Off);
    dynamics.scatter_fade_steps = scatter.fade_steps;
    dynamics.count =
        std::clamp(static_cast<int>(std::lround(descriptor_number(preset, "Cnt ", 1.0))), 1, 16);
    const auto count = read_variation(preset, "countDynamics");
    dynamics.count_jitter = std::clamp(count.jitter, 0.0, 1.0);
    dynamics.count_control = non_angle_control_from_bvty(count.control, BrushDynamicControl::Off);
    dynamics.count_fade_steps = count.fade_steps;
  }
  if (descriptor_bool(preset, "usePaintDynamics")) {
    const auto opacity = read_variation(preset, "opVr");
    dynamics.opacity_jitter = std::clamp(opacity.jitter, 0.0, 1.0);
    dynamics.minimum_opacity = std::clamp(opacity.minimum, 0.0, 1.0);
    dynamics.opacity_control =
        non_angle_control_from_bvty(opacity.control, BrushDynamicControl::GlobalDefault);
    dynamics.opacity_fade_steps = opacity.fade_steps;
    const auto flow = read_variation(preset, "prVr");
    dynamics.flow_jitter = std::clamp(flow.jitter, 0.0, 1.0);
    dynamics.minimum_flow = std::clamp(flow.minimum, 0.0, 1.0);
    dynamics.flow_control = non_angle_control_from_bvty(flow.control, BrushDynamicControl::Off);
    dynamics.flow_fade_steps = flow.fade_steps;
  }
  if (descriptor_bool(preset, "useTexture")) {
    dynamics.texture_enabled = true;
    dynamics.texture_scale =
        std::clamp(descriptor_number(preset, "textureScale", 100.0) / 100.0, 0.01, 10.0);
    dynamics.texture_depth =
        std::clamp(descriptor_number(preset, "textureDepth", 50.0) / 100.0, 0.0, 1.0);
    dynamics.texture_invert = descriptor_bool(preset, "InvT") ||
                              descriptor_bool(preset, "invertTexture");
    dynamics.texture_mode =
        texture_mode_from_code(descriptor_enum(preset, "textureBlendMode")).value_or(BrushTextureMode::Multiply);
    dynamics.texture_brightness =
        std::clamp(descriptor_number(preset, "textureBrightness", 0.0), -150.0, 150.0);
    dynamics.texture_contrast = std::clamp(descriptor_number(preset, "textureContrast", 0.0), -50.0, 100.0);
    if (const auto* texture = descriptor_object(preset, "Txtr"); texture != nullptr) {
      auto identity = descriptor_string(*texture, "Idnt");
      const auto name = descriptor_string(*texture, "Nm  ");
      // The pattern is referenced by id: a matching Pattern library tile (embedded in this file
      // or imported from a .pat) textures the brush; otherwise the seeded grain stands in.
      dynamics.texture_pattern_id = identity;
      dynamics.texture_pattern_name = name;
      if (identity.empty()) {
        identity = name;
      }
      if (!identity.empty()) {
        dynamics.texture_seed = stable_string_seed(identity);
      }
      dynamics.texture_style = texture_style_from_name(name);
    }
    // Deliberate patent design-around: textureDepthDynamics/minimumDepth are observed for
    // compatibility but never connected to pressure, velocity, direction, or stylus pose.
  }
  if (const auto* dual = descriptor_object(preset, "dualBrush");
      dual != nullptr && descriptor_bool(*dual, "useDualBrush")) {
    dynamics.dual_brush_enabled = true;
    if (const auto* secondary = descriptor_object(*dual, "Brsh"); secondary != nullptr) {
      const auto primary_diameter = std::max(1.0, descriptor_number(primary_brush, "Dmtr", 1.0));
      const auto secondary_diameter = descriptor_number(*secondary, "Dmtr", primary_diameter * 0.5);
      dynamics.dual_brush_size =
          std::clamp(secondary_diameter / primary_diameter, 0.05, 4.0);
      dynamics.dual_brush_hardness =
          std::clamp(descriptor_number(*secondary, "Hrdn", 100.0) / 100.0, 0.0, 1.0);
      dynamics.dual_brush_spacing =
          std::clamp(descriptor_number(*secondary, "Spcn", 100.0) / 100.0, 0.1, 10.0);
    }
  }
  if (descriptor_bool(preset, "useColorDynamics")) {
    dynamics.color_dynamics_enabled = true;
    const auto foreground_background = read_variation(preset, "clVr");
    dynamics.foreground_background_jitter =
        std::clamp(foreground_background.jitter, 0.0, 1.0);
    dynamics.color_control = non_angle_control_from_bvty(foreground_background.control,
                                                         BrushDynamicControl::Off);
    dynamics.color_fade_steps = foreground_background.fade_steps;
    dynamics.hue_jitter =
        std::clamp(descriptor_number(preset, "H   ") / 100.0, 0.0, 1.0);
    dynamics.saturation_jitter =
        std::clamp(descriptor_number(preset, "Strt") / 100.0, 0.0, 1.0);
    dynamics.brightness_jitter =
        std::clamp(descriptor_number(preset, "Brgh") / 100.0, 0.0, 1.0);
    dynamics.purity =
        std::clamp(descriptor_number(preset, "purity") / 100.0, -1.0, 1.0);
    dynamics.color_per_tip = descriptor_bool(preset, "colorDynamicsPerTip", true);
  }
  dynamics.wet_edges = descriptor_bool(preset, "Wtdg") || descriptor_bool(preset, "wetEdges");
  dynamics.noise = descriptor_bool(preset, "Nose");
  // The static Brush Tip Shape flips live in the tip object (the preset-level ones are jitters).
  dynamics.tip_flip_x = descriptor_bool(primary_brush, "flipX");
  dynamics.tip_flip_y = descriptor_bool(primary_brush, "flipY");
  return dynamics;
}

// True when a descriptor value would change how a brush paints if Patchy honored it: a true
// bool, a non-zero number, text, an enum, a list with items, or an object with such a child. A
// 'brVr' variation counts only with a jitter, minimum, or control (its default fade steps do not).
[[nodiscard]] bool descriptor_value_is_set(const DescriptorValue& value) {
  switch (value.type) {
    case DescriptorValue::Type::Empty:
      return false;
    case DescriptorValue::Type::Bool:
      return value.bool_value;
    case DescriptorValue::Type::Integer:
      return value.integer_value != 0;
    case DescriptorValue::Type::LargeInteger:
      return value.large_integer_value != 0;
    case DescriptorValue::Type::Double:
    case DescriptorValue::Type::UnitFloat:
      return value.double_value != 0.0;
    case DescriptorValue::Type::String:
      return !value.string_value.empty();
    case DescriptorValue::Type::List:
      return !value.list_value.empty();
    case DescriptorValue::Type::Object: {
      if (value.object_value == nullptr) {
        return false;
      }
      const auto& object = *value.object_value;
      if (object.class_id == "brVr") {
        return descriptor_number(object, "jitter") != 0.0 || descriptor_number(object, "Mnm ") != 0.0 ||
               descriptor_number(object, "bVTy") != 0.0;
      }
      return std::any_of(object.values.begin(), object.values.end(),
                         [](const auto& entry) { return descriptor_value_is_set(entry.second); });
    }
    default:
      return true;
  }
}

// Records the Photoshop settings of one preset that are in use but have no Patchy equivalent.
// Named features get a translatable label; anything unrecognized is listed by its raw key, so a
// later Photoshop release's settings never drop silently.
void collect_unmapped_settings(const DescriptorObject& preset, const DescriptorObject& primary_brush,
                               std::set<std::string>& unmapped) {
  // Keys the import maps, reduces on purpose (texture depth dynamics, with its own notice), or
  // that only describe the file (names, sample ids, tool options).
  static constexpr std::array<std::string_view, 45> kHandledPresetKeys{
      "Nm  ",        "Brsh",          "useTipDynamics",     "flipX",          "flipY",
      "minimumDiameter", "minimumRoundness", "szVr",        "angleDynamics",  "roundnessDynamics",
      "useScatter",  "Cnt ",          "bothAxes",           "countDynamics",  "scatterDynamics",
      "dualBrush",   "useTexture",    "TxtC",               "textureBlendMode", "textureDepth",
      "minimumDepth", "textureDepthDynamics", "Txtr",       "textureScale",   "InvT",
      "invertTexture", "textureBrightness", "textureContrast", "usePaintDynamics", "prVr",
      "opVr",        "useColorDynamics", "clVr",            "H   ",           "Strt",
      "Brgh",        "purity",        "colorDynamicsPerTip", "Wtdg",          "wetEdges",
      "Nose",        "Rpt ",          "toolOptions",        "useBrushSize",   "tiltScale"};
  const auto handled = [](std::string_view key) {
    return std::find(kHandledPresetKeys.begin(), kHandledPresetKeys.end(), key) != kHandledPresetKeys.end();
  };
  for (const auto& [key, value] : preset.values) {
    if (key == "brushProjection") {
      if (descriptor_value_is_set(value)) {
        unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Brush Projection"));
      }
    } else if (key == "useBrushPose" || key.starts_with("brushPose") || key.starts_with("overridePose")) {
      if (key == "useBrushPose" && descriptor_value_is_set(value)) {
        unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Brush Pose"));
      }
    } else if (key == "wtVr" || key == "mxVr") {
      if (descriptor_bool(preset, "usePaintDynamics") && descriptor_value_is_set(value)) {
        unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Wetness and Mix jitter"));
      }
    } else if (key == "protectTexture") {
      if (descriptor_value_is_set(value)) {
        unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Protect Texture"));
      }
    } else if (key == "brushGroup") {
      if (value.object_value != nullptr && descriptor_bool(*value.object_value, "useBrushGroup")) {
        unmapped.insert(key);
      }
    } else if (!handled(key) && descriptor_value_is_set(value)) {
      unmapped.insert(key);
    }
  }
  // Tilt Scale only acts while the size control is Pen Tilt.
  if (descriptor_bool(preset, "useTipDynamics") && read_variation(preset, "szVr").control == 3 &&
      descriptor_number(preset, "tiltScale") != 0.0) {
    unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Tilt Scale"));
  }
  if (descriptor_bool(preset, "useTexture")) {
    const auto mode = descriptor_enum(preset, "textureBlendMode");
    if (!mode.empty() && !texture_mode_from_code(mode).has_value()) {
      unmapped.insert("textureBlendMode");
    }
  }
  if (const auto* dual = descriptor_object(preset, "dualBrush");
      dual != nullptr && descriptor_bool(*dual, "useDualBrush")) {
    // Patchy's Dual Brush is one multiplied mask without its own scattering.
    const auto mode = descriptor_enum(*dual, "BlnM");
    if (!mode.empty() && mode != "Mltp" && mode != "multiply") {
      unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Dual Brush mode"));
    }
    const auto dual_count = descriptor_number(*dual, "Cnt ", 1.0);
    const auto* dual_scatter = descriptor_value(*dual, "scatterDynamics");
    const auto* dual_count_jitter = descriptor_value(*dual, "countDynamics");
    if (descriptor_bool(*dual, "Flip") ||
        (descriptor_bool(*dual, "useScatter") &&
         (dual_count > 1.0 || (dual_scatter != nullptr && descriptor_value_is_set(*dual_scatter)) ||
          (dual_count_jitter != nullptr && descriptor_value_is_set(*dual_count_jitter))))) {
      unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Dual Brush scattering and flip"));
    }
  }
  static constexpr std::array<std::string_view, 10> kHandledTipKeys{
      "Dmtr", "Hrdn", "Angl", "Rndn", "Spcn", "Intr", "flipX", "flipY", "Nm  ", "sampledData"};
  for (const auto& [key, value] : primary_brush.values) {
    if (std::find(kHandledTipKeys.begin(), kHandledTipKeys.end(), key) == kHandledTipKeys.end() &&
        descriptor_value_is_set(value)) {
      unmapped.insert("Brsh/" + key);
    }
  }
  // Spacing off makes Photoshop place dabs by input events; Patchy keeps a spatial cadence
  // (docs/brushes.md, Flow and Airbrush), so the preset's spacing value applies instead.
  if (const auto* interval = descriptor_value(primary_brush, "Intr");
      interval != nullptr && interval->type == DescriptorValue::Type::Bool && !interval->bool_value) {
    unmapped.insert(PATCHY_TRANSLATE_NOOP("QObject", "Spacing turned off"));
  }
}

// The 'desc' block is one serialized ActionDescriptor whose "Brsh" list holds every brush preset
// in file order. Sampled presets name their 'samp' entry by uuid ('sampledData'); computed ones
// carry their round tip's parameters.
std::vector<DescBrushInfo> parse_desc_brush_infos(std::span<const std::uint8_t> desc_block,
                                                  std::vector<std::string>& warnings,
                                                  std::set<std::string>& unmapped,
                                                  std::size_t& preset_count) {
  std::vector<DescBrushInfo> infos;
  BigEndianReader reader(desc_block);
  const auto descriptor_version = reader.read_u32();
  if (descriptor_version != 16U) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "Unsupported ABR descriptor version"));
  }
  const auto root = read_descriptor(reader);
  const auto* brush_list = descriptor_value(root, "Brsh");
  if (brush_list == nullptr || brush_list->type != DescriptorValue::Type::List) {
    return infos;
  }
  preset_count = brush_list->list_value.size();
  for (std::size_t desc_index = 0; desc_index < brush_list->list_value.size(); ++desc_index) {
    const auto& entry = brush_list->list_value[desc_index];
    if (entry.type != DescriptorValue::Type::Object || entry.object_value == nullptr) {
      continue;
    }
    const auto& preset = *entry.object_value;
    const auto* brush = descriptor_object(preset, "Brsh");
    DescBrushInfo info;
    info.desc_index = desc_index;
    if (const auto* name = descriptor_value(preset, "Nm  ");
        name != nullptr && name->type == DescriptorValue::Type::String) {
      info.name = name->string_value;
    }
    if (brush == nullptr || (brush->class_id != "sampledBrush" && brush->class_id != "computedBrush")) {
      warnings.push_back("Skipped brush \"" + (info.name.empty() ? std::string("(unnamed)") : info.name) +
                         "\": unsupported tip type");
      continue;
    }
    info.computed = brush->class_id == "computedBrush";
    info.sample_id = descriptor_string(*brush, "sampledData");
    if (const auto* spacing = descriptor_value(*brush, "Spcn");
        spacing != nullptr &&
        (spacing->type == DescriptorValue::Type::UnitFloat || spacing->type == DescriptorValue::Type::Double)) {
      info.spacing = std::clamp(spacing->double_value / 100.0, 0.01, 10.0);
    }
    if (const auto* diameter = descriptor_value(*brush, "Dmtr"); diameter != nullptr) {
      const auto value = descriptor_number(*brush, "Dmtr", 0.0);
      if (value > 0.0) {
        info.diameter = value;
      }
    }
    info.capture_size = descriptor_bool(preset, "useBrushSize", true);
    info.hardness = std::clamp(descriptor_number(*brush, "Hrdn", 100.0) / 100.0, 0.0, 1.0);
    info.base_angle_degrees = descriptor_number(*brush, "Angl", 0.0);
    info.base_roundness = std::clamp(descriptor_number(*brush, "Rndn", 100.0), 1.0, 100.0);
    info.dynamics = parse_brush_dynamics(preset, *brush);
    collect_unmapped_settings(preset, *brush, unmapped);
    // Photoshop 2026 ground-truth capture: Transfer Flow is the 'prVr' variation, the
    // options-bar percentage is toolOptions.flow, and Airbrush is preset 'Rpt ' (the Action
    // Manager names it "repeat"). Only a brush preset that explicitly carries tool options
    // may change Patchy's application-wide Brush settings when selected.
    if (const auto* tool_options = descriptor_object(preset, "toolOptions");
        tool_options != nullptr && descriptor_bool(*tool_options, "brushPreset")) {
      const auto* flow = descriptor_value(*tool_options, "flow");
      if (flow != nullptr &&
          (flow->type == DescriptorValue::Type::Integer ||
           flow->type == DescriptorValue::Type::LargeInteger ||
           flow->type == DescriptorValue::Type::Double ||
           flow->type == DescriptorValue::Type::UnitFloat)) {
        info.tool_flow_percent = std::clamp(
            static_cast<int>(std::lround(descriptor_number(*tool_options, "flow", 100.0))), 1, 100);
      }
      const auto* repeat = descriptor_value(preset, "Rpt ");
      if (repeat != nullptr && repeat->type == DescriptorValue::Type::Bool) {
        info.tool_airbrush = repeat->bool_value;
      }
    }

    if (descriptor_bool(preset, "useTexture")) {
      const auto depth = read_variation(preset, "textureDepthDynamics");
      if (depth.control != 0 || depth.jitter > 0.0) {
        warnings.push_back(
            "Brush \"" + (info.name.empty() ? std::string("(unnamed)") : info.name) +
            "\": input-driven texture depth was imported as a static depth for patent safety");
      }
    }
    infos.push_back(std::move(info));
  }
  return infos;
}

// The 'phry' block: one descriptor whose "hierarchy" list is the Brushes panel's flat folder
// walk, 'Grup' {name} opening a folder, 'groupEnd' closing it, and one 'preset' per desc preset
// in the same order. Returns each preset's folder path, or nothing when the block is unusable.
std::vector<std::vector<std::string>> parse_preset_hierarchy(std::span<const std::uint8_t> block) {
  std::vector<std::vector<std::string>> paths;
  try {
    BigEndianReader reader(block);
    if (reader.read_u32() != 16U) {
      return {};
    }
    const auto root = read_descriptor(reader);
    const auto* hierarchy = descriptor_value(root, "hierarchy");
    if (hierarchy == nullptr || hierarchy->type != DescriptorValue::Type::List) {
      return {};
    }
    std::vector<std::string> folders;
    for (const auto& item : hierarchy->list_value) {
      if (item.type != DescriptorValue::Type::Object || item.object_value == nullptr) {
        continue;
      }
      const auto& object = *item.object_value;
      if (object.class_id == "Grup") {
        folders.push_back(descriptor_string(object, "Nm  "));
      } else if (object.class_id == "groupEnd") {
        if (!folders.empty()) {
          folders.pop_back();
        }
      } else if (object.class_id == "preset") {
        paths.push_back(folders);
      }
    }
  } catch (const std::exception&) {
    return {};
  }
  return paths;
}

// Finds the embedded 'patt' records that imported brushes reference, by id or (when the id is
// missing from the file) by name; a brush matched by name takes the embedded pattern's id. Only
// record headers are read here: decode_abr_pattern decodes the pixels later, one at a time.
std::vector<AbrPatternRecord> find_referenced_patterns(std::span<const std::uint8_t> bytes,
                                                       std::span<const std::uint8_t> block,
                                                       std::vector<AbrBrush>& brushes) {
  std::set<std::string> wanted_ids;
  std::set<std::string> wanted_names;
  for (const auto& brush : brushes) {
    if (brush.dynamics.texture_enabled) {
      if (!brush.dynamics.texture_pattern_id.empty()) {
        wanted_ids.insert(brush.dynamics.texture_pattern_id);
      }
      if (!brush.dynamics.texture_pattern_name.empty()) {
        wanted_names.insert(brush.dynamics.texture_pattern_name);
      }
    }
  }
  std::vector<AbrPatternRecord> records;
  if (wanted_ids.empty() && wanted_names.empty()) {
    return records;
  }
  const auto block_offset = static_cast<std::size_t>(block.data() - bytes.data());
  BigEndianReader reader(block);
  try {
    while (reader.remaining() >= 16U) {
      const auto record_start = reader.position();
      const auto length = reader.read_u32();
      if (length < 16U || length > reader.remaining()) {
        break;
      }
      const auto record_end = reader.position() + length;
      reader.skip(12U);  // version, mode, height, width
      AbrPatternRecord record;
      record.name = read_descriptor_unicode_string(reader);
      const auto id_length = reader.read_u8();
      const auto id_bytes = reader.read_bytes(id_length);
      record.id.assign(id_bytes.begin(), id_bytes.end());
      while (!record.id.empty() && record.id.back() == '\0') {
        record.id.pop_back();
      }
      const auto duplicate = std::any_of(records.begin(), records.end(),
                                         [&record](const AbrPatternRecord& other) { return other.id == record.id; });
      if (!record.id.empty() && !duplicate &&
          (wanted_ids.contains(record.id) || wanted_names.contains(record.name))) {
        record.offset = block_offset + record_start;
        record.length = record_end - record_start;
        records.push_back(std::move(record));
      }
      reader.skip(record_end - reader.position());
      const auto padding = (4U - ((4U + length) % 4U)) % 4U;
      reader.skip(std::min<std::size_t>(padding, reader.remaining()));
    }
  } catch (const std::exception&) {
    // A damaged block keeps the records found so far; the rest fall back to the grain.
  }
  for (auto& brush : brushes) {
    auto& dynamics = brush.dynamics;
    if (!dynamics.texture_enabled || dynamics.texture_pattern_name.empty()) {
      continue;
    }
    const auto by_id = std::any_of(records.begin(), records.end(), [&dynamics](const AbrPatternRecord& record) {
      return record.id == dynamics.texture_pattern_id;
    });
    if (by_id) {
      continue;
    }
    for (const auto& record : records) {
      if (record.name == dynamics.texture_pattern_name) {
        dynamics.texture_pattern_id = record.id;
        break;
      }
    }
  }
  return records;
}

// Reads mask rows (raw or RLE) into an 8-bit mask, converting 16-bit samples down.
std::vector<std::uint8_t> read_mask_rows(BigEndianReader& reader, std::int32_t width, std::int32_t height,
                                         std::int32_t depth, std::uint8_t compression) {
  const auto bytes_per_sample = depth == 16 ? 2 : 1;
  const auto row_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(bytes_per_sample);
  std::vector<std::uint8_t> data;
  data.reserve(row_bytes * static_cast<std::size_t>(height));

  if (compression == 0) {
    data = reader.read_bytes(row_bytes * static_cast<std::size_t>(height));
  } else if (compression == 1) {
    std::vector<std::uint16_t> row_lengths(static_cast<std::size_t>(height));
    for (auto& length : row_lengths) {
      length = reader.read_u16();
    }
    for (const auto length : row_lengths) {
      const auto encoded = reader.read_bytes(length);
      const auto decoded = decode_packbits(encoded, row_bytes);
      data.insert(data.end(), decoded.begin(), decoded.end());
    }
  } else {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "Unknown ABR brush compression mode"));
  }

  if (bytes_per_sample == 1) {
    return data;
  }
  std::vector<std::uint8_t> mask(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  for (std::size_t index = 0; index < mask.size(); ++index) {
    mask[index] = data[index * 2U];  // big-endian: high byte is a fine 16→8 conversion
  }
  return mask;
}

// Crops a mask to its non-empty bounding box; returns false when the mask is entirely empty.
bool crop_mask_to_content(AbrBrush& brush) {
  std::int32_t min_x = brush.width;
  std::int32_t min_y = brush.height;
  std::int32_t max_x = -1;
  std::int32_t max_y = -1;
  for (std::int32_t y = 0; y < brush.height; ++y) {
    const auto* row = brush.mask.data() + static_cast<std::size_t>(y) * brush.width;
    for (std::int32_t x = 0; x < brush.width; ++x) {
      if (row[x] != 0U) {
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
  }
  if (max_x < min_x || max_y < min_y) {
    return false;
  }
  const auto cropped_width = max_x - min_x + 1;
  const auto cropped_height = max_y - min_y + 1;
  if (cropped_width == brush.width && cropped_height == brush.height) {
    return true;
  }
  std::vector<std::uint8_t> cropped(static_cast<std::size_t>(cropped_width) *
                                    static_cast<std::size_t>(cropped_height));
  for (std::int32_t y = 0; y < cropped_height; ++y) {
    const auto* src = brush.mask.data() + static_cast<std::size_t>(y + min_y) * brush.width + min_x;
    std::copy_n(src, cropped_width, cropped.data() + static_cast<std::size_t>(y) * cropped_width);
  }
  brush.width = cropped_width;
  brush.height = cropped_height;
  brush.mask = std::move(cropped);
  return true;
}

void validate_brush_dimensions(std::int32_t width, std::int32_t height, std::int32_t depth) {
  if (width <= 0 || height <= 0) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "brush has empty bounds"));
  }
  if (width > kMaxBrushDimension || height > kMaxBrushDimension) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "brush is larger than 4096px"));
  }
  if (depth != 8 && depth != 16) {
    throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "brush depth is not 8 or 16 bit"));
  }
}

// A computed brush as a rendered round mask; its angle/roundness stay the static tip shape the
// engine applies per dab, so the mask itself is always a circle.
AbrBrush make_computed_brush(double diameter, double hardness) {
  AbrBrush brush;
  brush.computed = true;
  const auto size = std::clamp(static_cast<std::int32_t>(std::lround(diameter)), kMinComputedMaskSize,
                               kMaxComputedMaskSize);
  brush.width = size;
  brush.height = size;
  brush.mask = render_computed_brush_mask(size, hardness);
  return brush;
}

constexpr const char* kNoUsableBrushes = PATCHY_TRANSLATE_NOOP("QObject", "The file contains no usable brushes");

AbrReadResult read_abr_v12(BigEndianReader& reader, std::uint16_t version, std::string& error) {
  AbrReadResult result;
  const auto count = reader.read_u16();
  for (std::uint16_t index = 0; index < count; ++index) {
    const auto type = reader.read_u16();
    const auto size = reader.read_u32();
    if (size > reader.remaining()) {
      throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "ABR brush entry is truncated"));
    }
    const auto entry_bytes = reader.read_bytes(size);
    if (type == 1U) {
      // misc u32, spacing u16 (%), diameter u16, roundness u16 (%), angle i16, hardness u16 (%).
      try {
        BigEndianReader entry(entry_bytes);
        (void)entry.read_u32();
        const auto spacing = entry.read_u16();
        const auto diameter = entry.read_u16();
        const auto roundness = entry.read_u16();
        const auto angle = static_cast<std::int16_t>(entry.read_u16());
        const auto hardness = entry.read_u16();
        if (diameter == 0U) {
          result.warnings.push_back("Skipped computed brush " + std::to_string(index + 1) +
                                    ": it has no diameter");
          continue;
        }
        auto brush = make_computed_brush(diameter, static_cast<double>(hardness) / 100.0);
        brush.spacing = std::clamp(static_cast<double>(spacing) / 100.0, 0.01, 10.0);
        brush.base_roundness = std::clamp(static_cast<double>(roundness), 1.0, 100.0);
        brush.base_angle_degrees = static_cast<double>(angle);
        brush.diameter = diameter;
        result.brushes.push_back(std::move(brush));
      } catch (const std::exception& entry_error) {
        result.warnings.push_back("Skipped unreadable brush " + std::to_string(index + 1) + ": " +
                                  entry_error.what());
      }
      continue;
    }
    if (type != 2U) {
      result.warnings.push_back("Skipped unknown brush type in brush " + std::to_string(index + 1));
      continue;
    }
    try {
      BigEndianReader entry(entry_bytes);
      (void)entry.read_u32();                    // misc
      const auto spacing = entry.read_u16();     // percent of diameter
      AbrBrush brush;
      brush.spacing = std::clamp(static_cast<double>(spacing) / 100.0, 0.01, 10.0);
      if (version == 2U) {
        brush.name = read_descriptor_unicode_string(entry);  // same int32-count UTF-16BE layout
      }
      (void)entry.read_u8();                     // antialiasing
      entry.skip(8);                             // short bounds
      const auto top = static_cast<std::int32_t>(entry.read_u32());
      const auto left = static_cast<std::int32_t>(entry.read_u32());
      const auto bottom = static_cast<std::int32_t>(entry.read_u32());
      const auto right = static_cast<std::int32_t>(entry.read_u32());
      const auto depth = static_cast<std::int32_t>(entry.read_u16());
      const auto compression = entry.read_u8();
      brush.width = right - left;
      brush.height = bottom - top;
      validate_brush_dimensions(brush.width, brush.height, depth);
      brush.mask = read_mask_rows(entry, brush.width, brush.height, depth, compression);
      if (!crop_mask_to_content(brush)) {
        result.warnings.push_back("Skipped empty brush " + std::to_string(index + 1));
        continue;
      }
      result.brushes.push_back(std::move(brush));
    } catch (const std::exception& entry_error) {
      result.warnings.push_back("Skipped unreadable brush " + std::to_string(index + 1) + ": " +
                                entry_error.what());
    }
  }
  if (result.brushes.empty()) {
    error = kNoUsableBrushes;
  }
  return result;
}

// One decoded (or undecodable) 'samp' entry; `brush` is empty when the entry was skipped.
struct SampleEntry {
  std::string id;
  std::optional<AbrBrush> brush;
};

std::vector<SampleEntry> read_samp_entries(std::span<const std::uint8_t> samp_block, std::uint16_t subversion,
                                           std::vector<std::string>& warnings) {
  std::vector<SampleEntry> samples;
  BigEndianReader samp(samp_block);
  // Each entry opens with a fixed-size key: a Pascal uuid string (the id a sampledBrush's
  // 'sampledData' names) followed by fields the reader skips, 47/301 bytes in total.
  const auto key_skip = subversion == 1U ? 47U : 301U;
  std::size_t sample_index = 0;
  while (samp.remaining() >= 4U) {
    const auto brush_size = samp.read_u32();
    auto padded_size = static_cast<std::size_t>(brush_size);
    while (padded_size % 4U != 0U) {
      ++padded_size;
    }
    if (padded_size > samp.remaining()) {
      // A truncated trailing entry: keep what we already parsed and warn.
      warnings.push_back(PATCHY_TRANSLATE_NOOP("QObject", "Ignored a truncated trailing brush entry"));
      break;
    }
    const auto entry_bytes = samp.read_bytes(padded_size);
    ++sample_index;
    SampleEntry sample;
    try {
      BigEndianReader entry(std::span<const std::uint8_t>(entry_bytes.data(), brush_size));
      const auto key = entry.read_bytes(key_skip);
      const auto id_length = std::min<std::size_t>(key[0], key.size() - 1U);
      sample.id.assign(key.begin() + 1, key.begin() + 1 + static_cast<std::ptrdiff_t>(id_length));
      const auto top = static_cast<std::int32_t>(entry.read_u32());
      const auto left = static_cast<std::int32_t>(entry.read_u32());
      const auto bottom = static_cast<std::int32_t>(entry.read_u32());
      const auto right = static_cast<std::int32_t>(entry.read_u32());
      const auto depth = static_cast<std::int32_t>(entry.read_u16());
      const auto compression = entry.read_u8();
      AbrBrush brush;
      brush.width = right - left;
      brush.height = bottom - top;
      validate_brush_dimensions(brush.width, brush.height, depth);
      brush.mask = read_mask_rows(entry, brush.width, brush.height, depth, compression);
      if (!crop_mask_to_content(brush)) {
        warnings.push_back("Skipped empty brush " + std::to_string(sample_index));
      } else {
        sample.brush = std::move(brush);
      }
    } catch (const std::exception& entry_error) {
      warnings.push_back("Skipped unreadable brush " + std::to_string(sample_index) + ": " +
                         entry_error.what());
    }
    samples.push_back(std::move(sample));
  }
  return samples;
}

void apply_desc_info(AbrBrush& brush, const DescBrushInfo& info) {
  brush.name = info.name;
  if (info.spacing.has_value()) {
    brush.spacing = *info.spacing;
  }
  brush.base_angle_degrees = info.base_angle_degrees;
  brush.base_roundness = info.base_roundness;
  brush.dynamics = info.dynamics;
  brush.tool_flow_percent = info.tool_flow_percent;
  brush.tool_airbrush = info.tool_airbrush;
  if (info.capture_size && info.diameter.has_value()) {
    brush.diameter = static_cast<int>(std::lround(*info.diameter));
  }
}

AbrReadResult read_abr_v6(BigEndianReader& reader, std::span<const std::uint8_t> bytes, std::string& error) {
  AbrReadResult result;
  const auto subversion = reader.read_u16();
  if (subversion != 1U && subversion != 2U) {
    throw std::runtime_error("Unsupported ABR subversion " + std::to_string(subversion));
  }

  std::span<const std::uint8_t> samp_block;
  std::span<const std::uint8_t> desc_block;
  std::span<const std::uint8_t> patt_block;
  std::span<const std::uint8_t> phry_block;
  while (reader.remaining() >= 12U) {
    const auto signature = key_string(read_signature(reader));
    if (signature != "8BIM") {
      throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "ABR tagged block has a corrupt signature"));
    }
    const auto key = key_string(read_signature(reader));
    const auto length = reader.read_u32();
    if (length > reader.remaining()) {
      throw std::runtime_error(PATCHY_TRANSLATE_NOOP("QObject", "ABR tagged block is truncated"));
    }
    const auto block = bytes.subspan(reader.position(), length);
    if (key == "samp") {
      samp_block = block;
    } else if (key == "desc") {
      desc_block = block;
    } else if (key == "patt") {
      patt_block = block;
    } else if (key == "phry") {
      phry_block = block;
    }
    // Tagged blocks are padded to 4-byte boundaries; the length field excludes the padding.
    auto padded_length = static_cast<std::size_t>(length);
    while (padded_length % 4U != 0U) {
      ++padded_length;
    }
    reader.skip(std::min(padded_length, reader.remaining()));
  }

  std::vector<DescBrushInfo> infos;
  std::set<std::string> unmapped;
  std::size_t preset_count = 0;
  if (!desc_block.empty()) {
    try {
      infos = parse_desc_brush_infos(desc_block, result.warnings, unmapped, preset_count);
    } catch (const std::exception& desc_error) {
      result.warnings.push_back(std::string("Brush names unavailable: ") + desc_error.what());
    }
  }
  const auto samples = read_samp_entries(samp_block, subversion, result.warnings);
  auto folders = parse_preset_hierarchy(phry_block);
  if (folders.size() != preset_count) {
    folders.clear();  // a hierarchy that does not cover every preset cannot be trusted
  }

  // Sampled presets pair with 'samp' entries by uuid; several presets may share one tip. Tips no
  // preset names are then a Dual Brush's secondary tips and stay out of the list. Files whose
  // uuids do not resolve (none match at all) fall back to pairing in order, as older readers
  // did, and keep their leftover tips as unnamed brushes.
  const auto ids_resolve = std::any_of(infos.begin(), infos.end(), [&samples](const DescBrushInfo& info) {
    return !info.computed && !info.sample_id.empty() &&
           std::any_of(samples.begin(), samples.end(),
                       [&info](const SampleEntry& sample) { return sample.id == info.sample_id; });
  });
  std::vector<bool> used(samples.size(), false);
  std::size_t next_in_order = 0;
  for (const auto& info : infos) {
    std::optional<AbrBrush> brush;
    if (info.computed) {
      brush = make_computed_brush(info.diameter.value_or(kMinComputedMaskSize), info.hardness);
    } else {
      std::optional<std::size_t> sample_index;
      if (ids_resolve) {
        for (std::size_t index = 0; index < samples.size(); ++index) {
          if (samples[index].id == info.sample_id) {
            sample_index = index;
            break;
          }
        }
      } else {
        while (next_in_order < samples.size() && used[next_in_order]) {
          ++next_in_order;
        }
        if (next_in_order < samples.size()) {
          sample_index = next_in_order++;
        }
      }
      if (!sample_index.has_value()) {
        result.warnings.push_back("Skipped brush \"" + (info.name.empty() ? std::string("(unnamed)") : info.name) +
                                  "\": its sampled tip is missing from the file");
        continue;
      }
      used[*sample_index] = true;
      brush = samples[*sample_index].brush;  // empty: its skip was already reported
    }
    if (!brush.has_value()) {
      continue;
    }
    apply_desc_info(*brush, info);
    if (info.desc_index < folders.size()) {
      brush->group_path = folders[info.desc_index];
    }
    result.brushes.push_back(std::move(*brush));
  }
  // Without usable uuids (or a usable 'desc') every remaining tip still imports, unnamed.
  for (std::size_t index = 0; index < samples.size() && !ids_resolve; ++index) {
    if (!used[index] && samples[index].brush.has_value()) {
      result.brushes.push_back(*samples[index].brush);
    }
  }
  if (!patt_block.empty()) {
    result.patterns = find_referenced_patterns(bytes, patt_block, result.brushes);
  }
  result.unmapped_settings.assign(unmapped.begin(), unmapped.end());
  if (result.brushes.empty()) {
    error = kNoUsableBrushes;
  }
  return result;
}

}  // namespace

std::optional<PatternResource> decode_abr_pattern(std::span<const std::uint8_t> bytes,
                                                  const AbrPatternRecord& record) {
  if (record.offset > bytes.size() || record.length > bytes.size() - record.offset) {
    return std::nullopt;
  }
  auto patterns = parse_patterns_block(bytes.subspan(record.offset, record.length), nullptr);
  if (patterns.empty()) {
    return std::nullopt;
  }
  auto pattern = std::move(patterns.front());
  pattern.provenance = PatternProvenance::Authored;
  return pattern;
}

std::vector<std::uint8_t> render_computed_brush_mask(std::int32_t size, double hardness) {
  size = std::max(1, size);
  std::vector<std::uint8_t> mask(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
  const auto radius = static_cast<double>(size) / 2.0;
  const auto inner = radius * std::clamp(hardness, 0.0, 1.0);
  for (std::int32_t y = 0; y < size; ++y) {
    for (std::int32_t x = 0; x < size; ++x) {
      const auto distance = std::hypot(static_cast<double>(x) + 0.5 - radius, static_cast<double>(y) + 0.5 - radius);
      auto coverage = 1.0;
      if (distance > inner) {
        // Smoothstep falloff across the soft band, then the antialiased rim.
        const auto t = std::clamp((distance - inner) / std::max(1e-9, radius - inner), 0.0, 1.0);
        coverage = 1.0 - t * t * (3.0 - 2.0 * t);
      }
      coverage = std::min(coverage, std::clamp(radius + 0.5 - distance, 0.0, 1.0));
      mask[static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)] =
          static_cast<std::uint8_t>(std::lround(coverage * 255.0));
    }
  }
  return mask;
}

std::optional<AbrReadResult> read_abr(std::span<const std::uint8_t> bytes, std::string& error) {
  error.clear();
  try {
    BigEndianReader reader(bytes);
    const auto version = reader.read_u16();
    AbrReadResult result;
    if (version == 1U || version == 2U) {
      result = read_abr_v12(reader, version, error);
    } else if (version >= 6U && version <= 10U) {
      result = read_abr_v6(reader, bytes, error);
    } else {
      error = "Unsupported ABR version " + std::to_string(version);
      return std::nullopt;
    }
    if (!error.empty()) {
      return std::nullopt;
    }
    return result;
  } catch (const std::exception& parse_error) {
    error = parse_error.what();
    return std::nullopt;
  }
}

}  // namespace patchy::psd
