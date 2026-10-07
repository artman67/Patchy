#include "core/color_lookup.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <system_error>
#include <map>
#include <mutex>
#include <utility>

namespace patchy {

namespace {

std::mutex& registry_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::map<std::string, std::shared_ptr<const ColorLookupData>, std::less<>>& registry() {
  static std::map<std::string, std::shared_ptr<const ColorLookupData>, std::less<>> tables;
  return tables;
}

// Where an 8-bit value falls on the grid: the lower node and the fraction toward the
// next one in 1/255 steps. The top value lands on the last cell at fraction 255.
struct GridPosition {
  int lower{0};
  std::uint32_t fraction{0};  // 0..255
};

GridPosition grid_position(std::uint8_t value, int grid) noexcept {
  const auto scaled = static_cast<std::uint32_t>(value) * static_cast<std::uint32_t>(grid - 1);
  const auto lower = static_cast<int>(scaled / 255U);
  if (lower >= grid - 1) {
    return {grid - 2, 255U};
  }
  return {lower, scaled % 255U};
}

// A .cube line split into whitespace-separated tokens.
std::vector<std::string_view> tokens_of(std::string_view line) {
  std::vector<std::string_view> tokens;
  std::size_t index = 0;
  while (index < line.size()) {
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
      ++index;
    }
    const auto start = index;
    while (index < line.size() && line[index] != ' ' && line[index] != '\t') {
      ++index;
    }
    if (index > start) {
      tokens.push_back(line.substr(start, index - start));
    }
  }
  return tokens;
}

// A whole token as a decimal number ("0.5", "-1e-3", ".25", "1."). Hand-written so no
// locale or toolchain difference can change a LUT: the table keeps 16 bits, far less
// than this parse's precision.
std::optional<double> parse_number(std::string_view text) {
  std::size_t index = 0;
  bool negative = false;
  if (index < text.size() && (text[index] == '+' || text[index] == '-')) {
    negative = text[index] == '-';
    ++index;
  }
  std::uint64_t mantissa = 0;
  int exponent = 0;
  int digits = 0;
  const auto take_digit = [&](char digit, bool fractional) {
    if (mantissa < 100000000000000000ULL) {
      mantissa = mantissa * 10U + static_cast<std::uint64_t>(digit - '0');
      if (fractional) {
        --exponent;
      }
    } else if (!fractional) {
      ++exponent;  // digits past 17 significant ones only scale the value
    }
    ++digits;
  };
  while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
    take_digit(text[index++], false);
  }
  if (index < text.size() && text[index] == '.') {
    ++index;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
      take_digit(text[index++], true);
    }
  }
  if (digits == 0) {
    return std::nullopt;
  }
  if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
    ++index;
    bool exponent_negative = false;
    if (index < text.size() && (text[index] == '+' || text[index] == '-')) {
      exponent_negative = text[index] == '-';
      ++index;
    }
    int value = 0;
    int exponent_digits = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
      value = std::min(value * 10 + (text[index++] - '0'), 1000);
      ++exponent_digits;
    }
    if (exponent_digits == 0) {
      return std::nullopt;
    }
    exponent += exponent_negative ? -value : value;
  }
  if (index != text.size()) {
    return std::nullopt;
  }
  // One multiply or divide by a power of ten, exact up to 1e22, so the usual
  // six-decimal values come out correctly rounded. Larger exponents only matter to
  // the 0..1 clamp the caller applies.
  exponent = std::clamp(exponent, -40, 40);
  double scale = 1.0;
  for (int step = 0; step < std::abs(exponent); ++step) {
    scale *= 10.0;
  }
  const auto magnitude =
      exponent >= 0 ? static_cast<double>(mantissa) * scale : static_cast<double>(mantissa) / scale;
  return negative ? -magnitude : magnitude;
}

bool is_default_domain(const std::vector<std::string_view>& tokens, double expected) {
  for (std::size_t index = 1; index < tokens.size(); ++index) {
    const auto value = parse_number(tokens[index]);
    if (!value.has_value() || *value != expected) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool ColorLookupTable::valid() const noexcept {
  if (grid < kColorLookupMinGrid || grid > kColorLookupMaxGrid) {
    return false;
  }
  const auto count = static_cast<std::size_t>(grid) * static_cast<std::size_t>(grid) * static_cast<std::size_t>(grid);
  return nodes.size() == count * 3U;
}

RgbColor ColorLookupTable::apply(RgbColor color, std::uint32_t rounding) const noexcept {
  const auto red = grid_position(color.red, grid);
  const auto green = grid_position(color.green, grid);
  const auto blue = grid_position(color.blue, grid);
  const auto stride_g = static_cast<std::size_t>(grid);
  const auto stride_b = stride_g * stride_g;
  const auto base = static_cast<std::size_t>(blue.lower) * stride_b + static_cast<std::size_t>(green.lower) * stride_g +
                    static_cast<std::size_t>(red.lower);
  // Each corner of the cell as a node offset: bit 0 = red + 1, bit 1 = green, bit 2 = blue.
  const auto corner = [&](int bits) {
    return base + ((bits & 1) != 0 ? 1U : 0U) + ((bits & 2) != 0 ? stride_g : 0U) + ((bits & 4) != 0 ? stride_b : 0U);
  };
  const auto fr = red.fraction;
  const auto fg = green.fraction;
  const auto fb = blue.fraction;
  // The cube splits into six tetrahedra along its gray diagonal (000 to 111); the
  // ordering of the three fractions picks one. The four weights sum to 255.
  std::array<std::size_t, 4> node{};
  std::array<std::uint32_t, 4> weight{};
  if (fr >= fg) {
    if (fg >= fb) {
      node = {corner(0), corner(1), corner(3), corner(7)};
      weight = {255U - fr, fr - fg, fg - fb, fb};
    } else if (fr >= fb) {
      node = {corner(0), corner(1), corner(5), corner(7)};
      weight = {255U - fr, fr - fb, fb - fg, fg};
    } else {
      node = {corner(0), corner(4), corner(5), corner(7)};
      weight = {255U - fb, fb - fr, fr - fg, fg};
    }
  } else if (fb >= fg) {
    node = {corner(0), corner(4), corner(6), corner(7)};
    weight = {255U - fb, fb - fg, fg - fr, fr};
  } else if (fb >= fr) {
    node = {corner(0), corner(2), corner(6), corner(7)};
    weight = {255U - fg, fg - fb, fb - fr, fr};
  } else {
    node = {corner(0), corner(2), corner(3), corner(7)};
    weight = {255U - fg, fg - fr, fr - fb, fb};
  }
  std::array<std::uint8_t, 3> out{};
  for (std::size_t channel = 0; channel < 3U; ++channel) {
    std::uint32_t sum = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
      sum += weight[index] * nodes[node[index] * 3U + channel];
    }
    // sum / (255 * 65535) is the output on 0..1, so sum / 65535 is the output byte.
    out[channel] = static_cast<std::uint8_t>(std::min<std::uint32_t>((sum + rounding) / 65535U, 255U));
  }
  return RgbColor{out[0], out[1], out[2]};
}

std::uint32_t color_lookup_dither_threshold(std::int32_t x, std::int32_t y) noexcept {
  auto z = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) ^
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
  z += 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  z ^= z >> 31U;
  return 128U + static_cast<std::uint32_t>(z % 65280U);
}

std::optional<ColorLookupTable> parse_cube_lut(std::span<const std::uint8_t> bytes) {
  if (bytes.empty() || bytes.size() > kColorLookupMaxFileBytes) {
    return std::nullopt;
  }
  std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (text.starts_with("\xEF\xBB\xBF")) {
    text.remove_prefix(3);
  }
  ColorLookupTable table;
  std::size_t expected = 0;
  std::size_t filled = 0;
  std::size_t position = 0;
  while (position < text.size()) {
    auto end = text.find('\n', position);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    auto line = text.substr(position, end - position);
    position = end + 1;
    if (const auto comment = line.find('#'); comment != std::string_view::npos) {
      line = line.substr(0, comment);
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    const auto tokens = tokens_of(line);
    if (tokens.empty()) {
      continue;
    }
    const auto first = tokens.front().front();
    const auto numeric = (first >= '0' && first <= '9') || first == '-' || first == '+' || first == '.';
    if (!numeric) {
      // Keywords come before the data.
      if (expected != 0 && filled != 0) {
        return std::nullopt;
      }
      const auto keyword = tokens.front();
      if (keyword == "LUT_3D_SIZE") {
        if (tokens.size() != 2 || table.grid != 0) {
          return std::nullopt;
        }
        const auto size = parse_number(tokens[1]);
        if (!size.has_value() || *size < kColorLookupMinGrid || *size > kColorLookupMaxGrid ||
            *size != std::floor(*size)) {
          return std::nullopt;
        }
        table.grid = static_cast<int>(*size);
        const auto grid = static_cast<std::size_t>(table.grid);
        expected = grid * grid * grid;
        // A data line takes at least six bytes ("0 0 0\n"): refuse a size the file
        // cannot hold before allocating for it.
        if (expected > bytes.size() / 6U + 1U) {
          return std::nullopt;
        }
        table.nodes.reserve(expected * 3U);
      } else if (keyword == "LUT_1D_SIZE") {
        return std::nullopt;  // 1D and shaper LUTs are not Color Lookup tables
      } else if (keyword == "DOMAIN_MIN") {
        if (tokens.size() != 4 || !is_default_domain(tokens, 0.0)) {
          return std::nullopt;
        }
      } else if (keyword == "DOMAIN_MAX") {
        if (tokens.size() != 4 || !is_default_domain(tokens, 1.0)) {
          return std::nullopt;
        }
      } else if (keyword == "LUT_3D_INPUT_RANGE") {
        if (tokens.size() != 3) {
          return std::nullopt;
        }
        const auto low = parse_number(tokens[1]);
        const auto high = parse_number(tokens[2]);
        if (!low.has_value() || !high.has_value() || *low != 0.0 || *high != 1.0) {
          return std::nullopt;
        }
      }
      // TITLE and other producers' keywords carry nothing the table needs.
      continue;
    }
    if (expected == 0 || filled >= expected || tokens.size() != 3) {
      return std::nullopt;
    }
    for (const auto token : tokens) {
      const auto value = parse_number(token);
      if (!value.has_value() || !std::isfinite(*value)) {
        return std::nullopt;
      }
      table.nodes.push_back(static_cast<std::uint16_t>(std::lround(std::clamp(*value, 0.0, 1.0) * 65535.0)));
    }
    ++filled;
  }
  if (expected == 0 || filled != expected) {
    return std::nullopt;
  }
  return table;
}

std::shared_ptr<const ColorLookupData> make_color_lookup_data(std::vector<std::uint8_t> file_bytes) {
  auto table = parse_cube_lut(file_bytes);
  if (!table.has_value()) {
    return nullptr;
  }
  std::uint64_t hash = 1469598103934665603ULL;  // FNV-1a
  for (const auto byte : file_bytes) {
    hash = (hash ^ byte) * 1099511628211ULL;
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string id = "cube-";
  for (int shift = 60; shift >= 0; shift -= 4) {
    id.push_back(kHex[(hash >> static_cast<unsigned>(shift)) & 0xFU]);
  }
  auto data = std::make_shared<ColorLookupData>();
  data->id = std::move(id);
  data->file_bytes = std::move(file_bytes);
  data->table = std::move(*table);
  return data;
}

std::shared_ptr<const ColorLookupData> read_color_lookup_file(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > kColorLookupMaxFileBytes) {
    return nullptr;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return nullptr;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
    return nullptr;
  }
  return make_color_lookup_data(std::move(bytes));
}

void register_color_lookup(std::shared_ptr<const ColorLookupData> data) {
  if (data == nullptr || data->id.empty() || !data->table.valid()) {
    return;
  }
  const std::lock_guard lock(registry_mutex());
  registry().try_emplace(data->id, std::move(data));
}

std::shared_ptr<const ColorLookupData> find_color_lookup(std::string_view id) {
  if (id.empty()) {
    return nullptr;
  }
  const std::lock_guard lock(registry_mutex());
  const auto found = registry().find(id);
  return found != registry().end() ? found->second : nullptr;
}

}  // namespace patchy
