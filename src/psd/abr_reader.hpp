#pragma once

#include "core/brush_dynamics.hpp"
#include "core/pattern_resource.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Reader for Adobe Photoshop .abr brush files. Extracts sampled (bitmap) brushes as 8-bit
// grayscale coverage masks plus name, spacing, static tip shape (angle/roundness/flips), and the
// supported Shape Dynamics / Scattering / Transfer / Texture / Dual Brush / Color Dynamics /
// Wet Edges / Noise settings. Computed (round/elliptical) brushes become rendered disc masks at
// their hardness, with their angle and roundness as the static tip shape. Supports the legacy
// v1/v2 layout and the v6/v7/v10 8BIM tagged-block layout (subversions 1 and 2). Texture
// patterns embedded in the file are decoded only when a brush references them; no Adobe pattern
// pixels are bundled with Patchy. See docs/abr-import.md.
namespace patchy::psd {

struct AbrBrush {
  std::string name;               // empty when the file carries no name; caller assigns a fallback
  double spacing{0.25};           // dab spacing as a fraction of the brush diameter
  double base_angle_degrees{0.0};   // static tip rotation ('Angl'), v6+ descriptors only
  double base_roundness{100.0};     // static tip roundness percent ('Rndn'), 1-100
  BrushDynamics dynamics{};       // supported dynamics/effects, default = off
  std::optional<int> tool_flow_percent;  // included Photoshop tool setting, 1-100
  std::optional<bool> tool_airbrush;     // included Photoshop Airbrush/repeat setting
  std::optional<int> diameter;    // the preset's brush size in pixels ('Dmtr'), when the file has one
  bool computed{false};           // the mask was rendered from a computed (non-sampled) brush
  std::vector<std::string> group_path;  // Photoshop preset folders ('phry'), outermost first
  std::int32_t width{0};
  std::int32_t height{0};
  std::vector<std::uint8_t> mask; // row-major coverage, width * height bytes, 255 = opaque
};

// One texture pattern embedded in the file ('patt') that an imported brush references. Records
// point into the bytes given to read_abr; decode them one at a time with decode_abr_pattern, so
// a file with many large patterns never holds them all decoded at once.
struct AbrPatternRecord {
  std::string id;
  std::string name;
  std::size_t offset{0};  // the record's length field, from the start of the file
  std::size_t length{0};  // the whole record including its length field
};

struct AbrReadResult {
  std::vector<AbrBrush> brushes;
  std::vector<std::string> warnings;  // per-brush skips or compatibility reductions
  std::vector<AbrPatternRecord> patterns;
  // Photoshop settings that were present and in use but have no Patchy equivalent, sorted and
  // unique: English feature labels (marked for translation) or raw descriptor keys.
  std::vector<std::string> unmapped_settings;
};

// Parses an in-memory .abr file. Returns std::nullopt and sets `error` when the file as a whole
// is unusable (bad header, truncation, or no usable brushes at all); individual undecodable
// brushes are skipped with a warning instead of failing the file.
[[nodiscard]] std::optional<AbrReadResult> read_abr(std::span<const std::uint8_t> bytes, std::string& error);

// Decodes one embedded pattern record from the same bytes read_abr parsed, as an Authored
// resource for the caller's Pattern library. nullopt when the record cannot be decoded.
[[nodiscard]] std::optional<PatternResource> decode_abr_pattern(std::span<const std::uint8_t> bytes,
                                                                const AbrPatternRecord& record);

// The coverage mask a computed brush renders to: a disc `size` pixels across whose edge falls
// off over the outer (1 - hardness) of its radius. Exposed for tests.
[[nodiscard]] std::vector<std::uint8_t> render_computed_brush_mask(std::int32_t size, double hardness);

}  // namespace patchy::psd
