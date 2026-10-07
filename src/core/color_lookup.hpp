#pragma once

#include "core/layer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patchy {

// Photoshop's Color Lookup adjustment in its 3DLUT File mode: an RGB-to-RGB table
// loaded from a .cube file. The Abstract and Device Link (ICC profile) modes are not
// modeled. See docs/color-lookup.md.

// The Cube LUT specification allows 2..256 nodes per axis. Files are untrusted, so
// their size is capped too: 64 MiB of text holds every grid up to about 140.
inline constexpr int kColorLookupMinGrid = 2;
inline constexpr int kColorLookupMaxGrid = 256;
inline constexpr std::size_t kColorLookupMaxFileBytes = std::size_t{64} * 1024U * 1024U;

// A 3D table of grid^3 RGB nodes, 16-bit (0..65535), red varying fastest (the .cube
// order).
struct ColorLookupTable {
  int grid{0};
  std::vector<std::uint16_t> nodes;

  [[nodiscard]] bool valid() const noexcept;
  // One color through the table by tetrahedral interpolation, in integer math (every
  // toolchain agrees). `rounding` (0..65534) is the offset added before the final
  // division to 8 bits: kColorLookupRoundNearest rounds, a dither threshold from
  // color_lookup_dither_threshold dithers. Grays only ever read nodes on the gray
  // diagonal, so a table that keeps neutrals neutral keeps them exactly.
  [[nodiscard]] RgbColor apply(RgbColor color, std::uint32_t rounding) const noexcept;
};

inline constexpr std::uint32_t kColorLookupRoundNearest = 32767U;

// The rounding offset of the Dither option at document pixel (x, y): a splitmix64 hash
// of the position mapped onto 128..65407. The bounds keep a table node that is exact
// to half a 16-bit step on its byte (an identity table stays an exact no-op).
[[nodiscard]] std::uint32_t color_lookup_dither_threshold(std::int32_t x, std::int32_t y) noexcept;

// Parses a .cube file (Adobe/Resolve Cube LUT format): optional TITLE, LUT_3D_SIZE,
// DOMAIN_MIN/DOMAIN_MAX or LUT_3D_INPUT_RANGE at their 0..1 default, then exactly
// grid^3 data lines. Rejects 1D and shaper LUTs, other domains, missing, extra or
// non-numeric data, and files over kColorLookupMaxFileBytes. Values clamp to 0..1.
[[nodiscard]] std::optional<ColorLookupTable> parse_cube_lut(std::span<const std::uint8_t> bytes);

// A loaded LUT file: its bytes (embedded in the PSD so documents stay portable) and
// the parsed table. `id` is a hash of the bytes.
struct ColorLookupData {
  std::string id;
  std::vector<std::uint8_t> file_bytes;
  ColorLookupTable table;
};

// nullptr when the bytes are not a usable .cube file.
[[nodiscard]] std::shared_ptr<const ColorLookupData> make_color_lookup_data(std::vector<std::uint8_t> file_bytes);

// Reads and parses a .cube file; nullptr when it cannot be read, is over the size cap,
// or is not a usable .cube file.
[[nodiscard]] std::shared_ptr<const ColorLookupData> read_color_lookup_file(const std::filesystem::path& path);

// The LUTs used in this process, by id. Layer metadata carries only the id;
// configure_adjustment_layer registers the table a layer uses, and saving embeds the
// bytes in the PSD again, so a reopened document needs nothing outside the file.
void register_color_lookup(std::shared_ptr<const ColorLookupData> data);
[[nodiscard]] std::shared_ptr<const ColorLookupData> find_color_lookup(std::string_view id);

}  // namespace patchy
