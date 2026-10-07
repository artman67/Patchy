// Adjustment-layer codecs for the PSD reader/writer: the Photoshop levl /
// curv / hue2 payloads (hue2 patches in place, curv preserves imported bytes
// exactly), plus read-only legacy support for the private plAD adjustment
// block and its CRV2 curves extension (never written since 2026-07: Photoshop
// reported the unknown key as "unknown data" on every open). Split out of
// psd_document_io.cpp as a pure move.

#include "psd/psd_document_io.hpp"
#include "psd/psd_io_internal.hpp"

#include "color/color_management.hpp"
#include "core/adjustment_layer.hpp"
#include "core/blend_math.hpp"
#include "core/layer_metadata.hpp"
#include "core/pattern_resource.hpp"
#include "core/smart_object.hpp"
#include "core/style_contour.hpp"
#include "core/text_warp.hpp"
#include "formats/acv_curves_io.hpp"
#include "psd/psd_binary.hpp"
#include "psd/psd_descriptor.hpp"
#include "psd/psd_filter_effects.hpp"
#include "psd/psd_patterns.hpp"
#include "psd/psd_smart_objects.hpp"
#include "render/compositor.hpp"
#include "support/string_utils.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite.h>
#include <wrl/client.h>
#endif

namespace patchy::psd {

namespace {

AdjustmentKind adjustment_kind_from_value(std::uint8_t value) {
  switch (value) {
    case 1U:
      return AdjustmentKind::Curves;
    case 2U:
      return AdjustmentKind::HueSaturation;
    case 3U:
      return AdjustmentKind::ColorBalance;
    default:
      return AdjustmentKind::Levels;
  }
}

std::optional<CurvesChannel> curves_channel_from_value(std::uint8_t value) {
  switch (value) {
    case 0U:
      return CurvesChannel::Rgb;
    case 1U:
      return CurvesChannel::Red;
    case 2U:
      return CurvesChannel::Green;
    case 3U:
      return CurvesChannel::Blue;
    default:
      return std::nullopt;
  }
}

LevelsChannel levels_channel_from_value(int value) {
  switch (value) {
    case 1:
      return LevelsChannel::Red;
    case 2:
      return LevelsChannel::Green;
    case 3:
      return LevelsChannel::Blue;
    default:
      return LevelsChannel::Rgb;
  }
}

// clamp_levels_record / levels_master_record / set_levels_master_record come
// from core/adjustment_layer.hpp (single source of truth for the clamp ranges).

LevelsRecord levels_record_for_photoshop_index(LevelsAdjustment settings, int index) {
  switch (index) {
    case 0:
      return levels_master_record(settings);
    case 1:
      return clamp_levels_record(settings.red);
    case 2:
      return clamp_levels_record(settings.green);
    case 3:
      return clamp_levels_record(settings.blue);
    default:
      // Index 4 included: Patchy writes RGB files, and Photoshop 2026 silently turns a
      // Levels layer into a plain empty layer (mask gone too) when an RGB document's
      // fifth record is not the identity. The black ink's record stays in the model
      // (LevelsAdjustment::black_ink) and is never written.
      return {};
  }
}

void set_levels_record_for_photoshop_index(LevelsAdjustment& settings, int index, LevelsRecord record) {
  record = clamp_levels_record(record);
  switch (index) {
    case 0:
      set_levels_master_record(settings, record);
      return;
    case 1:
      settings.red = record;
      return;
    case 2:
      settings.green = record;
      return;
    case 3:
      settings.blue = record;
      return;
    case 4:
      // A CMYK document's black ink. RGB documents leave this record at its identity.
      settings.black_ink = record;
      return;
    default:
      return;
  }
}

void write_i16(BigEndianWriter& writer, int value) {
  writer.write_u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
}

int read_i16(BigEndianReader& reader) {
  return static_cast<int>(static_cast<std::int16_t>(reader.read_u16()));
}

LevelsRecord read_levels_record_i32(BigEndianReader& reader) {
  return clamp_levels_record(
      LevelsRecord{read_i32(reader), read_i32(reader), read_i32(reader), read_i32(reader), read_i32(reader)});
}

void write_photoshop_levels_record(BigEndianWriter& writer, LevelsRecord record) {
  record = clamp_levels_record(record);
  writer.write_u16(static_cast<std::uint16_t>(record.black_input));
  writer.write_u16(static_cast<std::uint16_t>(record.white_input));
  writer.write_u16(static_cast<std::uint16_t>(record.black_output));
  writer.write_u16(static_cast<std::uint16_t>(record.white_output));
  writer.write_u16(static_cast<std::uint16_t>(record.gamma_percent));
}

LevelsRecord read_photoshop_levels_record(BigEndianReader& reader) {
  const auto black_input = static_cast<int>(reader.read_u16());
  const auto white_input = static_cast<int>(reader.read_u16());
  const auto black_output = static_cast<int>(reader.read_u16());
  const auto white_output = static_cast<int>(reader.read_u16());
  const auto gamma_percent = static_cast<int>(reader.read_u16());
  return clamp_levels_record(LevelsRecord{black_input, white_input, gamma_percent, black_output, white_output});
}

// Photoshop's hue2 hue fields store -180..180; the model keeps 0..360 (UI convention).
int hue2_file_hue_to_model(int hue) {
  return ((hue % 360) + 360) % 360;
}

int hue2_model_hue_to_file(int hue) {
  const auto normalized = ((hue % 360) + 360) % 360;
  return normalized > 180 ? normalized - 360 : normalized;
}

// The six per-hextant band records plus the undocumented 36-byte trailer exactly as
// Photoshop 2026 writes them for a fresh Hue/Saturation layer (COM byte capture, July
// 2026; identical for colorize on/off). Bands are preserved but not rendered.
constexpr std::array<std::uint8_t, 120> kPhotoshopHueSaturationDefaultTail = {
    0x01, 0x3B, 0x01, 0x59, 0x00, 0x0F, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F, 0x00, 0x2D, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x87, 0x00, 0xA5,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87, 0x00, 0xA5, 0x00, 0xC3,
    0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x00, 0xE1,
    0x00, 0xFF, 0x01, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0x01, 0x1D, 0x01, 0x3B, 0x01, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x64, 0x00, 0x32, 0x00, 0x3C, 0x00, 0x64, 0x00, 0x32,
    0x00, 0x78, 0x00, 0x64, 0x00, 0x32, 0x00, 0xB4, 0x00, 0x64, 0x00, 0x32,
    0x00, 0xF0, 0x00, 0x64, 0x00, 0x32, 0x01, 0x2C, 0x00, 0x64, 0x00, 0x32,
};

std::optional<CurvesAdjustment> parse_patchy_curves_extension(std::span<const std::uint8_t> payload) {
  if (payload.size() > kPatchyCurvesExtensionMaxPayloadSize) {
    return std::nullopt;
  }
  try {
    BigEndianReader reader(payload);
    if (reader.read_u16() != kPatchyCurvesExtensionVersion ||
        reader.read_u16() != kPatchyCurvesExtensionChannelCount) {
      return std::nullopt;
    }

    CurvesAdjustment curves;
    std::array<bool, kPatchyCurvesExtensionChannelCount> seen{};
    for (std::uint16_t index = 0; index < kPatchyCurvesExtensionChannelCount; ++index) {
      const auto channel_value = reader.read_u8();
      const auto channel = curves_channel_from_value(channel_value);
      if (!channel.has_value() || reader.read_u8() != 0U || seen[channel_value]) {
        return std::nullopt;
      }
      seen[channel_value] = true;
      const auto count = reader.read_u16();
      if (count < 2U || count > 19U || reader.remaining() < static_cast<std::size_t>(count) * 4U) {
        return std::nullopt;
      }
      CurveControlPoints points;
      points.reserve(count);
      for (std::uint16_t point_index = 0; point_index < count; ++point_index) {
        points.push_back(CurveControlPoint{static_cast<int>(reader.read_u16()),
                                           static_cast<int>(reader.read_u16())});
      }
      if (normalized_curve_control_points(points) != points) {
        return std::nullopt;
      }
      set_curve_points_for_channel(curves, *channel, std::move(points));
    }
    if (reader.remaining() != 0U || std::any_of(seen.begin(), seen.end(), [](bool value) { return !value; })) {
      return std::nullopt;
    }
    return curves;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool curve_points_are_exact_identity(const CurveControlPoints& points) {
  return points.size() == 2U && points[0] == CurveControlPoint{0, 0} &&
         points[1] == CurveControlPoint{255, 255};
}

}  // namespace

void write_i32(BigEndianWriter& writer, int value) {
  writer.write_u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
}

int read_i32(BigEndianReader& reader) {
  return static_cast<int>(static_cast<std::int32_t>(reader.read_u32()));
}

std::vector<std::uint8_t> photoshop_levels_payload(LevelsAdjustment settings) {
  BigEndianWriter writer;
  writer.write_u16(kPhotoshopLevelsAdjustmentVersion);
  for (int index = 0; index < kPhotoshopLevelsRecordCount; ++index) {
    write_photoshop_levels_record(writer, levels_record_for_photoshop_index(settings, index));
  }
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_levels_adjustment(std::span<const std::uint8_t> payload) {
  try {
    BigEndianReader reader(payload);
    if (reader.read_u16() != kPhotoshopLevelsAdjustmentVersion ||
        reader.remaining() < static_cast<std::size_t>(kPhotoshopLevelsRecordCount) * 10U) {
      return std::nullopt;
    }
    AdjustmentSettings settings;
    settings.kind = AdjustmentKind::Levels;
    for (int index = 0; index < kPhotoshopLevelsRecordCount; ++index) {
      const auto record = read_photoshop_levels_record(reader);
      if (index < 5) {
        set_levels_record_for_photoshop_index(settings.levels, index, record);
      }
    }
    return settings;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<AdjustmentSettings> parse_photoshop_hue2_adjustment(std::span<const std::uint8_t> payload) {
  try {
    BigEndianReader reader(payload);
    if (reader.read_u16() != kPhotoshopHueSaturationVersion ||
        reader.remaining() < kPhotoshopHueSaturationHeaderSize - 2U) {
      return std::nullopt;
    }
    AdjustmentSettings settings;
    settings.kind = AdjustmentKind::HueSaturation;
    settings.hue_saturation.colorize = reader.read_u8() != 0;
    reader.skip(1);  // padding
    settings.hue_saturation.colorize_hue = hue2_file_hue_to_model(read_i16(reader));
    settings.hue_saturation.colorize_saturation = std::clamp(read_i16(reader), 0, 100);
    settings.hue_saturation.colorize_lightness = std::clamp(read_i16(reader), -100, 100);
    settings.hue_saturation.hue_shift = std::clamp(read_i16(reader), -180, 180);
    settings.hue_saturation.saturation_delta = std::clamp(read_i16(reader), -100, 100);
    settings.hue_saturation.lightness_delta = std::clamp(read_i16(reader), -100, 100);
    // Six per-hue-range band records: four i16 range stops in wheel order then
    // an i16 hue/saturation/lightness triple. Files that stop after the header
    // (the legacy 16-byte shape) keep Photoshop's default hextants.
    settings.hue_saturation.bands = default_hue_saturation_bands();
    if (reader.remaining() >= kPhotoshopHueSaturationBandRecordSize * settings.hue_saturation.bands.size()) {
      const auto degrees = [](int value) { return ((value % 360) + 360) % 360; };
      for (auto& band : settings.hue_saturation.bands) {
        band.outer_start = degrees(read_i16(reader));
        band.inner_start = degrees(read_i16(reader));
        band.inner_end = degrees(read_i16(reader));
        band.outer_end = degrees(read_i16(reader));
        band.hue_shift = std::clamp(read_i16(reader), -180, 180);
        band.saturation_delta = std::clamp(read_i16(reader), -100, 100);
        band.lightness_delta = std::clamp(read_i16(reader), -100, 100);
      }
    }
    return settings;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::vector<std::uint8_t> photoshop_hue2_payload(const HueSaturationAdjustment& settings,
                                                 const UnknownPsdBlock* original) {
  BigEndianWriter header;
  header.write_u16(kPhotoshopHueSaturationVersion);
  header.write_u8(settings.colorize ? 1 : 0);
  header.write_u8(0);  // padding
  write_i16(header, hue2_model_hue_to_file(settings.colorize_hue));
  write_i16(header, std::clamp(settings.colorize_saturation, 0, 100));
  write_i16(header, std::clamp(settings.colorize_lightness, -100, 100));
  write_i16(header, std::clamp(settings.hue_shift, -180, 180));
  write_i16(header, std::clamp(settings.saturation_delta, -100, 100));
  write_i16(header, std::clamp(settings.lightness_delta, -100, 100));
  for (const auto& band : settings.bands) {
    write_i16(header, ((band.outer_start % 360) + 360) % 360);
    write_i16(header, ((band.inner_start % 360) + 360) % 360);
    write_i16(header, ((band.inner_end % 360) + 360) % 360);
    write_i16(header, ((band.outer_end % 360) + 360) % 360);
    write_i16(header, std::clamp(band.hue_shift, -180, 180));
    write_i16(header, std::clamp(band.saturation_delta, -100, 100));
    write_i16(header, std::clamp(band.lightness_delta, -100, 100));
  }

  auto bytes = header.bytes();
  if (original != nullptr && original->payload.size() >= kPhotoshopHueSaturationHeaderSize &&
      original->payload[0] == 0x00 && original->payload[1] == kPhotoshopHueSaturationVersion) {
    // Patch-in-place: the header and the six band records come from the model,
    // the undocumented 36-byte trailer stays byte-identical to the imported
    // payload, so an unedited layer still round-trips exactly.
    std::vector<std::uint8_t> patched(original->payload.begin(), original->payload.end());
    const auto copied = std::min(bytes.size(), patched.size());
    std::copy_n(bytes.begin(), copied, patched.begin());
    return patched;
  }
  // A fresh layer already carries its band records from the model, so only the
  // undocumented 36-byte trailer is appended from Photoshop's template.
  bytes.insert(bytes.end(), kPhotoshopHueSaturationDefaultTail.begin() + kPhotoshopHueSaturationBandBlockSize,
               kPhotoshopHueSaturationDefaultTail.end());
  return bytes;
}

std::optional<AdjustmentSettings> parse_photoshop_curves_adjustment(
    std::span<const std::uint8_t> payload) {
  // Photoshop's curv adjustment block begins with one zero byte, followed by
  // the documented Curves-file body. Photoshop 2026 writes a version-1 bitmap
  // body plus its indexed `Crv ` version-4 extension and pads the payload to a
  // four-byte boundary. The shared ACV reader handles both sections and gives
  // the richer indexed extension authority when it is present.
  if (payload.empty() || payload.front() != 0U) {
    return std::nullopt;
  }
  try {
    AdjustmentSettings settings;
    settings.kind = AdjustmentKind::Curves;
    settings.curves = acv::read(payload.subspan(1U));
    return settings;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::vector<std::uint8_t> photoshop_curves_payload(const CurvesAdjustment& curves,
                                                   const UnknownPsdBlock* original) {
  if (original != nullptr) {
    // (Not when the original carries a black-ink curve: that payload came from a CMYK
    // document, and Patchy writes RGB, where a fifth channel does not belong. See the
    // Levels note in levels_record_for_photoshop_index.)
    if (const auto parsed = parse_photoshop_curves_adjustment(original->payload);
        parsed.has_value() && parsed->curves == curves &&
        curve_points_are_exact_identity(normalized_curve_control_points(curves.black_ink))) {
      // The imported payload may contain compatibility details Patchy does not
      // model. Keep every byte until the modeled control points actually change.
      return original->payload;
    }
  }

  constexpr std::array channels{CurvesChannel::Rgb, CurvesChannel::Red,
                                CurvesChannel::Green, CurvesChannel::Blue};
  struct ActiveCurve {
    std::uint16_t channel{0};
    CurveControlPoints points;
  };
  std::vector<ActiveCurve> active;
  std::uint32_t bitmap = 0U;
  for (std::size_t index = 0; index < channels.size(); ++index) {
    auto points = normalized_curve_control_points(curve_points_for_channel(curves, channels[index]));
    if (curve_points_are_exact_identity(points)) {
      continue;
    }
    bitmap |= 1U << static_cast<unsigned>(index);
    active.push_back(ActiveCurve{static_cast<std::uint16_t>(index), std::move(points)});
  }

  const auto write_curve = [](BigEndianWriter& writer, const CurveControlPoints& points) {
    writer.write_u16(static_cast<std::uint16_t>(points.size()));
    for (const auto point : points) {
      // Photoshop stores each control point as output first, then input.
      writer.write_u16(static_cast<std::uint16_t>(point.output));
      writer.write_u16(static_cast<std::uint16_t>(point.input));
    }
  };

  BigEndianWriter writer;
  writer.write_u8(0U);  // curv adjustment-block prefix
  writer.write_u16(1U);
  // Photoshop 2026 writes this bitmap as four bytes even though Adobe's table
  // labels the field as two. Real captures use 0x0000000f for RGB+R+G+B.
  writer.write_u32(bitmap);
  for (const auto& curve : active) {
    write_curve(writer, curve.points);
  }

  write_signature(writer, kPhotoshopCurvesExtraMarker);
  writer.write_u16(4U);
  writer.write_u32(static_cast<std::uint32_t>(active.size()));
  for (const auto& curve : active) {
    writer.write_u16(curve.channel);
    write_curve(writer, curve.points);
  }
  while ((writer.bytes().size() % 4U) != 0U) {
    writer.write_u8(0U);
  }
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_color_balance_adjustment(
    std::span<const std::uint8_t> payload) {
  if (payload.size() < 12) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  reader.skip(6);  // shadows: preserved via patch-in-place, not modeled
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::ColorBalance;
  settings.color_balance.cyan_red =
      std::clamp(static_cast<int>(static_cast<std::int16_t>(reader.read_u16())), -100, 100);
  settings.color_balance.magenta_green =
      std::clamp(static_cast<int>(static_cast<std::int16_t>(reader.read_u16())), -100, 100);
  settings.color_balance.yellow_blue =
      std::clamp(static_cast<int>(static_cast<std::int16_t>(reader.read_u16())), -100, 100);
  return settings;
}

std::vector<std::uint8_t> photoshop_color_balance_payload(const ColorBalanceAdjustment& settings,
                                                          const UnknownPsdBlock* original) {
  std::vector<std::uint8_t> payload;
  if (original != nullptr && original->payload.size() >= 12) {
    payload = original->payload;  // keep shadows/highlights/preserve-luminosity bytes
  } else {
    payload.assign(20, 0);  // PS 2026's fresh midtones-only shape
  }
  const auto write_i16_at = [&payload](std::size_t offset, int value) {
    const auto encoded = static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(value, -100, 100)));
    payload[offset] = static_cast<std::uint8_t>(encoded >> 8U);
    payload[offset + 1] = static_cast<std::uint8_t>(encoded & 0xFFU);
  };
  write_i16_at(6, settings.cyan_red);
  write_i16_at(8, settings.magenta_green);
  write_i16_at(10, settings.yellow_blue);
  return payload;
}

bool photoshop_color_balance_payload_has_unrendered_data(std::span<const std::uint8_t> payload) {
  for (std::size_t index = 0; index < payload.size(); ++index) {
    const auto in_shadows = index < 6;
    const auto in_highlights = index >= 12 && index < 18;
    const auto is_preserve_luminosity = index == 18;
    if ((in_shadows || in_highlights || is_preserve_luminosity) && payload[index] != 0) {
      return true;
    }
  }
  return false;
}

std::optional<AdjustmentSettings> parse_photoshop_posterize_adjustment(std::span<const std::uint8_t> payload) {
  if (payload.size() < 2) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::Posterize;
  settings.posterize.levels = std::clamp(static_cast<int>(reader.read_u16()), 2, 255);
  return settings;
}

std::vector<std::uint8_t> photoshop_posterize_payload(const PosterizeAdjustment& settings,
                                                      const UnknownPsdBlock* original) {
  const auto levels = std::clamp(settings.levels, 2, 255);
  if (original != nullptr) {
    // Unedited imported payloads re-emit byte-for-byte (curv-style guard) so
    // any undocumented trailing bytes Photoshop may add survive untouched.
    const auto parsed = parse_photoshop_posterize_adjustment(original->payload);
    if (parsed.has_value() && parsed->posterize.levels == levels) {
      return original->payload;
    }
  }
  BigEndianWriter writer;
  writer.write_u16(static_cast<std::uint16_t>(levels));
  writer.write_u16(0);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_brightness_contrast_adjustment(
    std::span<const std::uint8_t> payload) {
  if (payload.size() < 4) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::BrightnessContrast;
  // 'brit' alone (no CgEd descriptor) is the CS-era legacy record.
  settings.brightness_contrast.use_legacy = true;
  settings.brightness_contrast.brightness =
      std::clamp(static_cast<int>(static_cast<std::int16_t>(reader.read_u16())), -100, 100);
  settings.brightness_contrast.contrast =
      std::clamp(static_cast<int>(static_cast<std::int16_t>(reader.read_u16())), -100, 100);
  return settings;
}

std::optional<BrightnessContrastDescriptorParse> parse_photoshop_brightness_contrast_descriptor(
    std::span<const std::uint8_t> payload) {
  if (payload.size() < 4) {
    return std::nullopt;
  }
  try {
    BigEndianReader reader(payload);
    if (reader.read_u32() != 16) {
      return std::nullopt;
    }
    const auto descriptor = read_descriptor(reader);
    const auto* brightness = descriptor_value(descriptor, "Brgh");
    const auto* contrast = descriptor_value(descriptor, "Cntr");
    if (brightness == nullptr || brightness->type != DescriptorValue::Type::Integer ||
        contrast == nullptr || contrast->type != DescriptorValue::Type::Integer) {
      return std::nullopt;
    }
    BrightnessContrastDescriptorParse parsed;
    parsed.settings.kind = AdjustmentKind::BrightnessContrast;
    if (const auto* legacy = descriptor_value(descriptor, "useLegacy");
        legacy != nullptr && legacy->type == DescriptorValue::Type::Bool) {
      parsed.use_legacy = legacy->bool_value;
    }
    parsed.settings.brightness_contrast.use_legacy = parsed.use_legacy;
    if (parsed.use_legacy) {
      parsed.settings.brightness_contrast.brightness = std::clamp(brightness->integer_value, -100, 100);
      parsed.settings.brightness_contrast.contrast = std::clamp(contrast->integer_value, -100, 100);
    } else {
      parsed.settings.brightness_contrast.brightness =
          std::clamp(brightness->integer_value, -kModernBrightnessRange, kModernBrightnessRange);
      parsed.settings.brightness_contrast.contrast =
          std::clamp(contrast->integer_value, kModernContrastMin, kModernContrastMax);
    }
    return parsed;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

namespace {

// The imported state the current settings are compared against for the
// unedited-round-trip guards: a parseable CgEd wins over brit.
std::optional<BrightnessContrastAdjustment> original_brightness_contrast_state(const Layer& layer) {
  const UnknownPsdBlock* brit = nullptr;
  const UnknownPsdBlock* descriptor = nullptr;
  for (const auto& block : layer.unknown_psd_blocks()) {
    if (block.key == "brit") {
      brit = &block;
    } else if (block.key == "CgEd") {
      descriptor = &block;
    }
  }
  if (descriptor != nullptr) {
    if (const auto parsed = parse_photoshop_brightness_contrast_descriptor(descriptor->payload);
        parsed.has_value()) {
      return parsed->settings.brightness_contrast;
    }
  }
  if (brit != nullptr) {
    if (const auto parsed = parse_photoshop_brightness_contrast_adjustment(brit->payload); parsed.has_value()) {
      return parsed->brightness_contrast;
    }
  }
  return std::nullopt;
}

bool brightness_contrast_settings_match(const BrightnessContrastAdjustment& a,
                                        const BrightnessContrastAdjustment& b) {
  return a.brightness == b.brightness && a.contrast == b.contrast && a.use_legacy == b.use_legacy;
}

}  // namespace

std::vector<std::uint8_t> photoshop_brightness_contrast_payload(const BrightnessContrastAdjustment& settings,
                                                                const Layer& layer) {
  const auto clamped = clamp_brightness_contrast(settings);
  const auto original = original_brightness_contrast_state(layer);
  if (original.has_value() && brightness_contrast_settings_match(*original, clamped)) {
    for (const auto& block : layer.unknown_psd_blocks()) {
      if (block.key == "brit") {
        return block.payload;  // unedited: byte-identical round trip
      }
    }
  }
  BigEndianWriter writer;
  if (!clamped.use_legacy) {
    // Photoshop writes an all-zero compatibility 'brit' beside a modern CgEd
    // (byte-verified on PS 2026 files); the descriptor carries the values.
    writer.write_u16(0);
    writer.write_u16(0);
    writer.write_u16(0);
    writer.write_u8(0);
    writer.write_u8(0);
    return writer.bytes();
  }
  writer.write_u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(clamped.brightness)));
  writer.write_u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(clamped.contrast)));
  writer.write_u16(127);  // mean, Photoshop's fixed midpoint
  writer.write_u8(0);     // lab
  writer.write_u8(0);     // pad
  return writer.bytes();
}

std::optional<std::vector<std::uint8_t>> photoshop_brightness_contrast_descriptor_payload(
    const BrightnessContrastAdjustment& settings, const Layer& layer) {
  const auto clamped = clamp_brightness_contrast(settings);
  const UnknownPsdBlock* original_block = nullptr;
  for (const auto& block : layer.unknown_psd_blocks()) {
    if (block.key == "CgEd") {
      original_block = &block;
    }
  }
  const auto original = original_brightness_contrast_state(layer);
  if (original.has_value() && brightness_contrast_settings_match(*original, clamped)) {
    if (original_block != nullptr) {
      return original_block->payload;  // unedited: byte-identical round trip
    }
    // Legacy brit-only file, untouched: keep it descriptor-free.
    return std::nullopt;
  }
  if (clamped.use_legacy && original_block == nullptr) {
    // Edited legacy settings on a file that never had a descriptor stay
    // brit-only, the historical Patchy output Photoshop reads as legacy.
    return std::nullopt;
  }
  // Regenerate Photoshop 2026's native 7-item shape: null descriptor with
  // Vrsn/Brgh/Cntr/means/Lab/useLegacy/Auto ('means' and 'useLegacy' are
  // stringIDs, the rest charIDs). 'means' is inert at render time (dark- and
  // light-context COM probes render identically); preserve an imported value,
  // else write Photoshop's default 127.
  auto means = 127;
  auto lab = false;
  auto auto_flag = false;
  if (original_block != nullptr) {
    try {
      BigEndianReader reader(original_block->payload);
      if (reader.read_u32() == 16) {
        const auto descriptor = read_descriptor(reader);
        if (const auto* value = descriptor_value(descriptor, "means");
            value != nullptr && value->type == DescriptorValue::Type::Integer) {
          means = value->integer_value;
        }
        lab = descriptor_bool(descriptor, "Lab ", false);
        auto_flag = descriptor_bool(descriptor, "Auto", false);
      }
    } catch (const std::exception&) {
    }
  }
  DescriptorObject descriptor;
  descriptor.name = "";
  descriptor.class_id = "null";
  const auto add_integer = [&descriptor](const std::string& key, bool long_form, int value) {
    DescriptorValue entry;
    entry.type = DescriptorValue::Type::Integer;
    entry.integer_value = value;
    descriptor.values.emplace(key, std::move(entry));
    descriptor.key_order.push_back({key, long_form});
  };
  const auto add_bool = [&descriptor](const std::string& key, bool long_form, bool value) {
    DescriptorValue entry;
    entry.type = DescriptorValue::Type::Bool;
    entry.bool_value = value;
    descriptor.values.emplace(key, std::move(entry));
    descriptor.key_order.push_back({key, long_form});
  };
  add_integer("Vrsn", false, 1);
  add_integer("Brgh", false, clamped.brightness);
  add_integer("Cntr", false, clamped.contrast);
  add_integer("means", true, means);
  add_bool("Lab ", false, lab);
  add_bool("useLegacy", true, clamped.use_legacy);
  add_bool("Auto", false, auto_flag);
  BigEndianWriter writer;
  writer.write_u32(16);
  write_descriptor(writer, descriptor);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_exposure_adjustment(std::span<const std::uint8_t> payload) {
  if (payload.size() < 14) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  if (reader.read_u16() != 1) {
    return std::nullopt;
  }
  const auto exposure = std::bit_cast<float>(reader.read_u32());
  const auto offset = std::bit_cast<float>(reader.read_u32());
  const auto gamma = std::bit_cast<float>(reader.read_u32());
  if (!std::isfinite(exposure) || !std::isfinite(offset) || !std::isfinite(gamma)) {
    return std::nullopt;
  }
  // Clamp as doubles first: a wild float must not overflow the integer conversion.
  const auto scaled = [](float value, double scale, int low, int high) {
    return static_cast<int>(
        std::lround(std::clamp(static_cast<double>(value) * scale, static_cast<double>(low), static_cast<double>(high))));
  };
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::Exposure;
  settings.exposure = ExposureAdjustment{scaled(exposure, 100.0, -kExposureValueRange, kExposureValueRange),
                                         scaled(offset, 10000.0, -kExposureOffsetRange, kExposureOffsetRange),
                                         scaled(gamma, 100.0, kExposureGammaMin, kExposureGammaMax)};
  return settings;
}

std::vector<std::uint8_t> photoshop_exposure_payload(const ExposureAdjustment& settings,
                                                     const UnknownPsdBlock* original) {
  const auto clamped = clamp_exposure(settings);
  if (original != nullptr) {
    // Unedited imported payloads re-emit byte-for-byte, which also keeps Photoshop's
    // exact float32 values instead of the rounded fields.
    const auto parsed = parse_photoshop_exposure_adjustment(original->payload);
    if (parsed.has_value() && parsed->exposure.exposure_hundredths == clamped.exposure_hundredths &&
        parsed->exposure.offset_ten_thousandths == clamped.offset_ten_thousandths &&
        parsed->exposure.gamma_hundredths == clamped.gamma_hundredths) {
      return original->payload;
    }
  }
  BigEndianWriter writer;
  writer.write_u16(1);
  writer.write_u32(std::bit_cast<std::uint32_t>(static_cast<float>(clamped.exposure_hundredths / 100.0)));
  writer.write_u32(std::bit_cast<std::uint32_t>(static_cast<float>(clamped.offset_ten_thousandths / 10000.0)));
  writer.write_u32(std::bit_cast<std::uint32_t>(static_cast<float>(clamped.gamma_hundredths / 100.0)));
  writer.write_u16(0);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_vibrance_adjustment(std::span<const std::uint8_t> payload) {
  if (payload.size() < 4) {
    return std::nullopt;
  }
  try {
    BigEndianReader reader(payload);
    if (reader.read_u32() != 16) {
      return std::nullopt;
    }
    const auto descriptor = read_descriptor(reader);
    // A missing key reads as 0, Photoshop's default for both sliders.
    const auto slider = [&descriptor](std::string_view key) {
      const auto* value = descriptor_value(descriptor, key);
      if (value == nullptr) {
        return 0;
      }
      if (value->type == DescriptorValue::Type::Integer) {
        return std::clamp(value->integer_value, -kVibranceRange, kVibranceRange);
      }
      if (value->type == DescriptorValue::Type::Double && std::isfinite(value->double_value)) {
        return static_cast<int>(std::lround(std::clamp(value->double_value, static_cast<double>(-kVibranceRange),
                                                       static_cast<double>(kVibranceRange))));
      }
      return 0;
    };
    AdjustmentSettings settings;
    settings.kind = AdjustmentKind::Vibrance;
    settings.vibrance = VibranceAdjustment{slider("vibrance"), slider("Strt")};
    return settings;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::vector<std::uint8_t> photoshop_vibrance_payload(const VibranceAdjustment& settings,
                                                     const UnknownPsdBlock* original) {
  const auto clamped = clamp_vibrance(settings);
  if (original != nullptr) {
    // Unedited imported payloads re-emit byte-for-byte.
    const auto parsed = parse_photoshop_vibrance_adjustment(original->payload);
    if (parsed.has_value() && parsed->vibrance.vibrance == clamped.vibrance &&
        parsed->vibrance.saturation == clamped.saturation) {
      return original->payload;
    }
  }
  DescriptorObject descriptor;
  descriptor.name = "";
  descriptor.class_id = "null";
  const auto add_integer = [&descriptor](const std::string& key, bool long_form, int value) {
    DescriptorValue entry;
    entry.type = DescriptorValue::Type::Integer;
    entry.integer_value = value;
    descriptor.values.emplace(key, std::move(entry));
    descriptor.key_order.push_back({key, long_form});
  };
  add_integer("vibrance", true, clamped.vibrance);
  add_integer("Strt", false, clamped.saturation);
  BigEndianWriter writer;
  writer.write_u32(16);
  write_descriptor(writer, descriptor);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_selective_color_adjustment(
    std::span<const std::uint8_t> payload) {
  constexpr std::size_t kRecordCount = kSelectiveColorRangeCount + 1U;
  if (payload.size() < 4U + kRecordCount * 8U) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  if (reader.read_u16() != 1) {
    return std::nullopt;
  }
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::SelectiveColor;
  settings.selective_color.absolute = reader.read_u16() != 0;
  // The first record is reserved: skipped on read, written as zeros.
  reader.skip(8);
  for (auto& correction : settings.selective_color.corrections) {
    correction.cyan = read_i16(reader);
    correction.magenta = read_i16(reader);
    correction.yellow = read_i16(reader);
    correction.black = read_i16(reader);
  }
  settings.selective_color = clamp_selective_color(settings.selective_color);
  return settings;
}

std::vector<std::uint8_t> photoshop_selective_color_payload(const SelectiveColorAdjustment& settings,
                                                            const UnknownPsdBlock* original) {
  const auto clamped = clamp_selective_color(settings);
  if (original != nullptr) {
    // Unedited imported payloads re-emit byte-for-byte, reserved record included.
    const auto parsed = parse_photoshop_selective_color_adjustment(original->payload);
    if (parsed.has_value() && parsed->selective_color == clamped) {
      return original->payload;
    }
  }
  BigEndianWriter writer;
  writer.write_u16(1);
  writer.write_u16(clamped.absolute ? 1 : 0);
  writer.write_u64(0);  // the reserved first record
  for (const auto& correction : clamped.corrections) {
    write_i16(writer, correction.cyan);
    write_i16(writer, correction.magenta);
    write_i16(writer, correction.yellow);
    write_i16(writer, correction.black);
  }
  return writer.bytes();
}

namespace {

// The six weight items in the dialog's order (Reds through Magentas), as charIDs.
constexpr std::array<const char*, kBlackWhiteColorCount> kBlackWhiteWeightKeys{"Rd  ", "Yllw", "Grn ",
                                                                               "Cyn ", "Bl  ", "Mgnt"};

std::optional<DescriptorObject> read_black_white_descriptor(std::span<const std::uint8_t> payload) {
  if (payload.size() < 4) {
    return std::nullopt;
  }
  try {
    BigEndianReader reader(payload);
    if (reader.read_u32() != 16) {
      return std::nullopt;
    }
    return read_descriptor(reader);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

BlackWhiteAdjustment black_white_from_descriptor(const DescriptorObject& descriptor) {
  // A missing item keeps Photoshop's default; a 'doub' item rounds.
  BlackWhiteAdjustment settings;
  for (std::size_t index = 0; index < kBlackWhiteColorCount; ++index) {
    const auto* value = descriptor_value(descriptor, kBlackWhiteWeightKeys[index]);
    if (value != nullptr && value->type == DescriptorValue::Type::Integer) {
      settings.weights[index] = value->integer_value;
    } else if (value != nullptr && value->type == DescriptorValue::Type::Double &&
               std::isfinite(value->double_value)) {
      settings.weights[index] = static_cast<int>(std::lround(std::clamp(
          value->double_value, static_cast<double>(kBlackWhiteWeightMin), static_cast<double>(kBlackWhiteWeightMax))));
    }
  }
  settings.tint = descriptor_bool(descriptor, "useTint", false);
  if (const auto* color = descriptor_object(descriptor, "tintColor"); color != nullptr && color->class_id == "RGBC") {
    const auto channel = [color](std::string_view key) {
      const auto value = descriptor_number(*color, key, 0.0);
      return std::isfinite(value) ? std::clamp(value, 0.0, 255.0) : 0.0;
    };
    black_white_tint_from_color(channel("Rd  "), channel("Grn "), channel("Bl  "), settings.tint_hue,
                                settings.tint_saturation);
  }
  return clamp_black_white(settings);
}

// Replaces an item in place (keeping its position and id form) or appends it.
void set_descriptor_item(DescriptorObject& descriptor, const std::string& key, bool long_form,
                         DescriptorValue value) {
  if (const auto found = descriptor.values.find(key); found != descriptor.values.end()) {
    found->second = std::move(value);
    return;
  }
  descriptor.values.emplace(key, std::move(value));
  descriptor.key_order.push_back({key, long_form});
}

}  // namespace

std::optional<AdjustmentSettings> parse_photoshop_black_white_adjustment(std::span<const std::uint8_t> payload) {
  const auto descriptor = read_black_white_descriptor(payload);
  if (!descriptor.has_value()) {
    return std::nullopt;
  }
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::BlackWhite;
  settings.black_white = black_white_from_descriptor(*descriptor);
  return settings;
}

std::vector<std::uint8_t> photoshop_black_white_payload(const BlackWhiteAdjustment& settings,
                                                        const UnknownPsdBlock* original) {
  const auto clamped = clamp_black_white(settings);
  DescriptorObject descriptor;
  descriptor.name = "";
  descriptor.class_id = "null";
  std::optional<BlackWhiteAdjustment> imported;
  if (original != nullptr) {
    if (auto parsed = read_black_white_descriptor(original->payload); parsed.has_value()) {
      imported = black_white_from_descriptor(*parsed);
      if (*imported == clamped) {
        return original->payload;  // unedited: byte-identical round trip
      }
      // An edit patches the imported descriptor, so Photoshop's other items (the
      // preset choice) and the item order stay as they were.
      descriptor = std::move(*parsed);
    }
  }
  for (std::size_t index = 0; index < kBlackWhiteColorCount; ++index) {
    DescriptorValue weight;
    weight.type = DescriptorValue::Type::Integer;
    weight.integer_value = clamped.weights[index];
    set_descriptor_item(descriptor, kBlackWhiteWeightKeys[index], false, std::move(weight));
  }
  DescriptorValue use_tint;
  use_tint.type = DescriptorValue::Type::Bool;
  use_tint.bool_value = clamped.tint;
  set_descriptor_item(descriptor, "useTint", true, std::move(use_tint));
  // An imported tint color whose hue and saturation were not edited stays exact.
  if (!imported.has_value() || imported->tint_hue != clamped.tint_hue ||
      imported->tint_saturation != clamped.tint_saturation) {
    const auto tint = black_white_tint_color(clamped.tint_hue, clamped.tint_saturation);
    auto color = std::make_shared<DescriptorObject>();
    color->name = "";
    color->class_id = "RGBC";
    for (const auto& [key, channel] : {std::pair<const char*, std::uint8_t>{"Rd  ", tint.red},
                                       std::pair<const char*, std::uint8_t>{"Grn ", tint.green},
                                       std::pair<const char*, std::uint8_t>{"Bl  ", tint.blue}}) {
      DescriptorValue component;
      component.type = DescriptorValue::Type::Double;
      component.double_value = static_cast<double>(channel);
      set_descriptor_item(*color, key, false, std::move(component));
    }
    DescriptorValue tint_color;
    tint_color.type = DescriptorValue::Type::Object;
    tint_color.object_value = std::move(color);
    set_descriptor_item(descriptor, "tintColor", true, std::move(tint_color));
  }
  BigEndianWriter writer;
  writer.write_u32(16);
  write_descriptor(writer, descriptor);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_channel_mixer_adjustment(std::span<const std::uint8_t> payload) {
  // Version, monochrome, then the red, green and blue output records; a fourth (CMYK
  // black output) record and any tail are not modeled.
  constexpr std::size_t kModeledRecords = 3;
  if (payload.size() < 4U + kModeledRecords * 10U) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  if (reader.read_u16() != 1) {
    return std::nullopt;
  }
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::ChannelMixer;
  auto& mixer = settings.channel_mixer;
  mixer.monochrome = reader.read_u16() != 0;
  for (auto& output : mixer.outputs) {
    output.red = read_i16(reader);
    output.green = read_i16(reader);
    output.blue = read_i16(reader);
    reader.skip(2);  // the black source, CMYK only
    output.constant = read_i16(reader);
  }
  // A monochrome mixer keeps its Gray mix in the first record.
  if (mixer.monochrome) {
    mixer.gray = mixer.outputs[0];
  }
  mixer = clamp_channel_mixer(mixer);
  return settings;
}

std::vector<std::uint8_t> photoshop_channel_mixer_payload(const ChannelMixerAdjustment& settings,
                                                          const UnknownPsdBlock* original) {
  const auto clamped = clamp_channel_mixer(settings);
  if (original != nullptr) {
    // Unedited imported payloads re-emit byte-for-byte, CMYK black data included.
    // The Gray mix of a color mixer never reaches the file, so it cannot mark an edit
    // (a monochrome mixer's outputs are its gray).
    const auto parsed = parse_photoshop_channel_mixer_adjustment(original->payload);
    if (parsed.has_value() && parsed->channel_mixer.monochrome == clamped.monochrome &&
        parsed->channel_mixer.outputs == clamped.outputs) {
      return original->payload;
    }
  }
  BigEndianWriter writer;
  writer.write_u16(1);
  writer.write_u16(clamped.monochrome ? 1 : 0);
  // A monochrome mixer's outputs all equal its Gray mix (clamp_channel_mixer), so
  // the first record carries the gray either way.
  for (const auto& output : clamped.outputs) {
    write_i16(writer, output.red);
    write_i16(writer, output.green);
    write_i16(writer, output.blue);
    write_i16(writer, 0);
    write_i16(writer, output.constant);
  }
  // The black output record, unused in RGB.
  writer.write_u64(0);
  writer.write_u16(0);
  return writer.bytes();
}

std::optional<AdjustmentSettings> parse_photoshop_threshold_adjustment(std::span<const std::uint8_t> payload) {
  if (payload.size() < 2) {
    return std::nullopt;
  }
  BigEndianReader reader(payload);
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::Threshold;
  settings.threshold.level = std::clamp(static_cast<int>(reader.read_u16()), 1, 255);
  return settings;
}

std::vector<std::uint8_t> photoshop_threshold_payload(const ThresholdAdjustment& settings,
                                                      const UnknownPsdBlock* original) {
  const auto level = std::clamp(settings.level, 1, 255);
  if (original != nullptr) {
    const auto parsed = parse_photoshop_threshold_adjustment(original->payload);
    if (parsed.has_value() && parsed->threshold.level == level) {
      return original->payload;
    }
  }
  BigEndianWriter writer;
  writer.write_u16(static_cast<std::uint16_t>(level));
  writer.write_u16(0);
  return writer.bytes();
}

// --- Gradient Map ('grdm') ---
//
// Layout per the Adobe Photoshop File Formats Specification, "Gradient
// settings (Photoshop 6.0)": https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
// Two details the published table does not show come from real files as read
// by the open-source psd-tools project: version 3 inserts a four-byte method
// key (Gcls/Perc/Lnr /Smoo) after the reverse and dither bytes, and each color
// stop carries a trailing u16 color type (0 user, 1 foreground, 2 background),
// making it 20 bytes instead of the table's 18. The reader accepts both stop
// sizes. None of this has been checked against Photoshop itself yet; see
// docs/file-formats.md.
namespace {

constexpr std::uint16_t kGradientMapExpansionCount = 2;
constexpr std::uint16_t kGradientMapNoiseLength = 32;
constexpr std::size_t kGradientMapMaxStops = 4096;

std::array<char, 4> gradient_map_method_key(GradientInterpolationMethod method) {
  switch (method) {
    case GradientInterpolationMethod::Perceptual:
      return {'P', 'e', 'r', 'c'};
    case GradientInterpolationMethod::Linear:
      return {'L', 'n', 'r', ' '};
    case GradientInterpolationMethod::Classic:
      break;
  }
  return {'G', 'c', 'l', 's'};
}

GradientInterpolationMethod gradient_map_method_from_key(std::string_view key) {
  // "Smoo" reads as Perceptual, like the descriptor reader in
  // psd_layer_styles.cpp; other unmodeled methods render as Classic and are
  // preserved byte-for-byte until the gradient is edited.
  if (key == "Perc" || key == "Smoo") {
    return GradientInterpolationMethod::Perceptual;
  }
  if (key == "Lnr ") {
    return GradientInterpolationMethod::Linear;
  }
  return GradientInterpolationMethod::Classic;
}

std::uint8_t unit16_to_byte(std::uint16_t value) {
  return static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) * 255U + 32767U) / 65535U);
}

// Photoshop color-space ids as used by swatch files: 0 RGB, 1 HSB, 8 gray
// (0..10000, read the way Patchy's .aco reader does). CMYK and Lab stops are
// not modeled; such a payload stays on the preserved-bytes path.
std::optional<RgbColor> gradient_map_stop_color(std::uint16_t space, const std::array<std::uint16_t, 4>& values) {
  switch (space) {
    case 0:
      return RgbColor{unit16_to_byte(values[0]), unit16_to_byte(values[1]), unit16_to_byte(values[2])};
    case 1: {
      const auto hue = static_cast<double>(values[0]) / 65535.0 * 6.0;
      const auto saturation = static_cast<double>(values[1]) / 65535.0;
      const auto brightness = static_cast<double>(values[2]) / 65535.0;
      const auto sector = std::min(static_cast<int>(std::floor(hue)), 5);
      const auto fraction = hue - static_cast<double>(sector);
      const auto p = brightness * (1.0 - saturation);
      const auto q = brightness * (1.0 - saturation * fraction);
      const auto t = brightness * (1.0 - saturation * (1.0 - fraction));
      const std::array<std::array<double, 3>, 6> rgb{{{brightness, t, p},
                                                      {q, brightness, p},
                                                      {p, brightness, t},
                                                      {p, q, brightness},
                                                      {t, p, brightness},
                                                      {brightness, p, q}}};
      const auto& channels = rgb[static_cast<std::size_t>(sector)];
      const auto byte = [](double value) {
        return static_cast<std::uint8_t>(std::clamp(std::lround(value * 255.0), 0L, 255L));
      };
      return RgbColor{byte(channels[0]), byte(channels[1]), byte(channels[2])};
    }
    case 8: {
      const auto gray = static_cast<std::uint8_t>(
          std::lround(static_cast<double>(std::min<std::uint16_t>(values[0], 10000)) * 255.0 / 10000.0));
      return RgbColor{gray, gray, gray};
    }
    default:
      return std::nullopt;
  }
}

struct ParsedGradientMap {
  GradientMapAdjustment settings;
  std::uint16_t version{1};
};

std::optional<ParsedGradientMap> parse_gradient_map_payload(std::span<const std::uint8_t> payload,
                                                            bool stops_carry_type) {
  try {
    BigEndianReader reader(payload);
    ParsedGradientMap parsed;
    parsed.version = reader.read_u16();
    if (parsed.version != 1 && parsed.version != 3) {
      return std::nullopt;
    }
    auto& settings = parsed.settings;
    auto& gradient = settings.gradient;
    settings.reverse = reader.read_u8() != 0;
    settings.dither = reader.read_u8() != 0;
    settings.method = GradientInterpolationMethod::Classic;
    if (parsed.version == 3) {
      const auto key = reader.read_span(4);
      settings.method =
          gradient_map_method_from_key(std::string_view(reinterpret_cast<const char*>(key.data()), key.size()));
    }
    const auto name_units = reader.read_u32();
    if (name_units > reader.remaining() / 2U) {
      return std::nullopt;
    }
    reader.skip(static_cast<std::size_t>(name_units) * 2U);
    const auto name_bytes = payload.subspan(reader.position() - static_cast<std::size_t>(name_units) * 2U - 4U,
                                            static_cast<std::size_t>(name_units) * 2U + 4U);
    gradient.name = read_unicode_string_payload(name_bytes).value_or(std::string{});

    const auto color_count = reader.read_u16();
    if (color_count > kGradientMapMaxStops) {
      return std::nullopt;
    }
    gradient.color_stops.clear();
    for (std::uint16_t index = 0; index < color_count; ++index) {
      GradientColorStop stop;
      stop.location = std::clamp(static_cast<float>(reader.read_u32()) / 4096.0F, 0.0F, 1.0F);
      stop.midpoint = std::clamp(static_cast<float>(reader.read_u32()) / 100.0F, 0.0F, 1.0F);
      const auto space = reader.read_u16();
      std::array<std::uint16_t, 4> values{};
      for (auto& value : values) {
        value = reader.read_u16();
      }
      const auto color = gradient_map_stop_color(space, values);
      if (!color.has_value()) {
        return std::nullopt;
      }
      stop.color = *color;
      if (stops_carry_type) {
        const auto type = reader.read_u16();
        stop.kind = type == 1U   ? GradientColorStop::Kind::Foreground
                    : type == 2U ? GradientColorStop::Kind::Background
                                 : GradientColorStop::Kind::User;
      }
      gradient.color_stops.push_back(stop);
    }
    const auto alpha_count = reader.read_u16();
    if (alpha_count > kGradientMapMaxStops) {
      return std::nullopt;
    }
    gradient.alpha_stops.clear();
    for (std::uint16_t index = 0; index < alpha_count; ++index) {
      GradientAlphaStop stop;
      stop.location = std::clamp(static_cast<float>(reader.read_u32()) / 4096.0F, 0.0F, 1.0F);
      stop.midpoint = std::clamp(static_cast<float>(reader.read_u32()) / 100.0F, 0.0F, 1.0F);
      stop.opacity = std::clamp(static_cast<float>(reader.read_u16()) / 255.0F, 0.0F, 1.0F);
      gradient.alpha_stops.push_back(stop);
    }
    // The expansion count and the noise-block length pin the stop size: a
    // wrong guess lands these reads on stop bytes.
    if (reader.read_u16() != kGradientMapExpansionCount) {
      return std::nullopt;
    }
    gradient.smoothness = static_cast<std::uint16_t>(std::min<std::uint16_t>(reader.read_u16(), 4096));
    if (reader.read_u16() != kGradientMapNoiseLength) {
      return std::nullopt;
    }
    const auto mode = reader.read_u16();
    auto& noise = gradient.noise;
    noise.seed = reader.read_u32();
    noise.add_transparency = reader.read_u16() != 0;
    noise.restrict_colors = reader.read_u16() != 0;
    noise.roughness = static_cast<std::uint16_t>(std::min<std::uint32_t>(reader.read_u32(), 4096U));
    const auto color_model = reader.read_u16();
    noise.color_model = color_model == 1U                       ? GradientNoiseColorModel::HSB
                        : color_model == 2U || color_model == 7U ? GradientNoiseColorModel::Lab
                                                                 : GradientNoiseColorModel::RGB;
    std::array<std::uint16_t, 8> ranges{};
    for (auto& value : ranges) {
      value = reader.read_u16();
    }
    // The spec gives no scale for the channel ranges; descriptors use 0..100,
    // so anything larger is read as a full 16-bit range.
    const bool sixteen_bit = std::any_of(ranges.begin(), ranges.end(), [](std::uint16_t value) { return value > 100U; });
    for (std::size_t channel = 0; channel < 4U; ++channel) {
      const auto scale = [sixteen_bit](std::uint16_t value) {
        return sixteen_bit ? static_cast<std::uint16_t>((static_cast<std::uint32_t>(value) * 100U + 32767U) / 65535U)
                           : value;
      };
      noise.minimum[channel] = scale(ranges[channel]);
      noise.maximum[channel] = scale(ranges[4U + channel]);
    }
    // The spec does not define the mode values. 1 is taken as Noise (Photoshop
    // lists the gradient types Solid, Noise), and only for a gradient without
    // color stops, so a wrong guess cannot turn an ordinary map into noise.
    gradient.form = mode == 1U && gradient.color_stops.empty() ? GradientDefinitionForm::Noise
                                                               : GradientDefinitionForm::Solid;
    return parsed;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<ParsedGradientMap> parse_gradient_map_any_layout(std::span<const std::uint8_t> payload) {
  if (auto parsed = parse_gradient_map_payload(payload, true); parsed.has_value()) {
    return parsed;
  }
  return parse_gradient_map_payload(payload, false);
}

void write_gradient_map_ranges(BigEndianWriter& writer, const std::array<std::uint16_t, 4>& values) {
  for (const auto value : values) {
    writer.write_u16(std::min<std::uint16_t>(value, 100));
  }
}

}  // namespace

std::optional<AdjustmentSettings> parse_photoshop_gradient_map_adjustment(std::span<const std::uint8_t> payload) {
  auto parsed = parse_gradient_map_any_layout(payload);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  AdjustmentSettings settings;
  settings.kind = AdjustmentKind::GradientMap;
  settings.gradient_map = std::move(parsed->settings);
  prepare_gradient_map_lut(settings.gradient_map);
  return settings;
}

std::vector<std::uint8_t> photoshop_gradient_map_payload(const GradientMapAdjustment& settings,
                                                         const UnknownPsdBlock* original) {
  if (original != nullptr) {
    if (const auto parsed = parse_gradient_map_any_layout(original->payload);
        parsed.has_value() && parsed->settings.gradient == settings.gradient) {
      // Same gradient: keep every imported byte (unmodeled color types, noise
      // fields, method keys) and patch only the three option fields.
      auto bytes = original->payload;
      if (parsed->settings.reverse != settings.reverse) {
        bytes[2] = settings.reverse ? 1U : 0U;
      }
      if (parsed->settings.dither != settings.dither) {
        bytes[3] = settings.dither ? 1U : 0U;
      }
      if (parsed->settings.method != settings.method) {
        const auto key = gradient_map_method_key(settings.method);
        if (parsed->version == 3) {
          std::copy(key.begin(), key.end(), bytes.begin() + 4);
        } else {
          bytes[0] = 0U;
          bytes[1] = 3U;
          bytes.insert(bytes.begin() + 4, key.begin(), key.end());
        }
      }
      return bytes;
    }
  }

  // Fresh or edited: regenerate a version-3 solid gradient. A Noise definition
  // is written as sampled solid stops, the same flattening the Gradient tool
  // applies, so Patchy never authors the unverified noise fields.
  const auto solid = sampled_solid_gradient(settings.gradient);
  auto color_stops = solid.color_stops;
  auto alpha_stops = solid.alpha_stops;
  if (color_stops.empty()) {
    color_stops = default_gradient_map_definition().color_stops;
  }
  if (alpha_stops.empty()) {
    alpha_stops = default_gradient_map_definition().alpha_stops;
  }
  const auto sort_by_location = [](const auto& lhs, const auto& rhs) { return lhs.location < rhs.location; };
  std::stable_sort(color_stops.begin(), color_stops.end(), sort_by_location);
  std::stable_sort(alpha_stops.begin(), alpha_stops.end(), sort_by_location);
  const auto location_units = [](float location) {
    return static_cast<std::uint32_t>(std::lround(std::clamp(location, 0.0F, 1.0F) * 4096.0F));
  };
  const auto midpoint_percent = [](float midpoint) {
    return static_cast<std::uint32_t>(std::lround(std::clamp(midpoint, 0.0F, 1.0F) * 100.0F));
  };

  BigEndianWriter writer;
  writer.write_u16(3U);
  writer.write_u8(settings.reverse ? 1U : 0U);
  writer.write_u8(settings.dither ? 1U : 0U);
  write_signature(writer, gradient_map_method_key(settings.method));
  const auto name_units = utf8_to_utf16(settings.gradient.name);
  writer.write_u32(checked_u32(name_units.size() + 1U, "gradient map name length"));
  for (const auto unit : name_units) {
    writer.write_u16(unit);
  }
  writer.write_u16(0U);
  writer.write_u16(static_cast<std::uint16_t>(std::min(color_stops.size(), kGradientMapMaxStops)));
  for (std::size_t index = 0; index < color_stops.size() && index < kGradientMapMaxStops; ++index) {
    const auto& stop = color_stops[index];
    writer.write_u32(location_units(stop.location));
    writer.write_u32(midpoint_percent(stop.midpoint));
    writer.write_u16(0U);  // RGB
    writer.write_u16(static_cast<std::uint16_t>(stop.color.red * 257U));
    writer.write_u16(static_cast<std::uint16_t>(stop.color.green * 257U));
    writer.write_u16(static_cast<std::uint16_t>(stop.color.blue * 257U));
    writer.write_u16(0U);
    writer.write_u16(stop.kind == GradientColorStop::Kind::Foreground   ? 1U
                     : stop.kind == GradientColorStop::Kind::Background ? 2U
                                                                        : 0U);
  }
  writer.write_u16(static_cast<std::uint16_t>(std::min(alpha_stops.size(), kGradientMapMaxStops)));
  for (std::size_t index = 0; index < alpha_stops.size() && index < kGradientMapMaxStops; ++index) {
    const auto& stop = alpha_stops[index];
    writer.write_u32(location_units(stop.location));
    writer.write_u32(midpoint_percent(stop.midpoint));
    writer.write_u16(static_cast<std::uint16_t>(std::lround(std::clamp(stop.opacity, 0.0F, 1.0F) * 255.0F)));
  }
  const auto& noise = settings.gradient.noise;
  writer.write_u16(kGradientMapExpansionCount);
  writer.write_u16(std::min<std::uint16_t>(settings.gradient.smoothness, 4096));
  writer.write_u16(kGradientMapNoiseLength);
  writer.write_u16(0U);  // solid
  writer.write_u32(noise.seed);
  writer.write_u16(noise.add_transparency ? 1U : 0U);
  writer.write_u16(noise.restrict_colors ? 1U : 0U);
  writer.write_u32(std::min<std::uint16_t>(noise.roughness, 4096));
  writer.write_u16(noise.color_model == GradientNoiseColorModel::HSB   ? 1U
                   : noise.color_model == GradientNoiseColorModel::Lab ? 7U
                                                                       : 0U);
  write_gradient_map_ranges(writer, noise.minimum);
  write_gradient_map_ranges(writer, noise.maximum);
  writer.write_u16(0U);  // dummy
  while ((writer.bytes().size() % 4U) != 0U) {
    writer.write_u8(0U);
  }
  return writer.bytes();
}

// Read-only since 2026-07: no adjustment kind writes plAD anymore (Photoshop
// reported the unknown key as "unknown data" on every open). The v4 layout
// stays parseable for legacy imports: 'PLAD' signature, u16 version 4, kind u8
// (0 Levels, 1 Curves, 2 HueSat, 3 ColorBalance; newer kinds were never
// written because old builds read unknown kind bytes as Levels), 4 levels
// records of 5 i32, levels channel i32, 3 legacy curve outputs, 3 hue/sat,
// 3 color balance, optional 4-i32 colorize tail, optional CRV2 curves tail.
std::optional<AdjustmentSettings> parse_patchy_adjustment(std::span<const std::uint8_t> payload) {
  try {
    BigEndianReader reader(payload);
    if (read_signature(reader) != kPatchyAdjustmentPayloadSignature) {
      return std::nullopt;
    }
    if (reader.read_u16() != kPatchyAdjustmentVersion) {
      return std::nullopt;
    }
    constexpr auto expected_i32_count = 30U;
    if (reader.remaining() < 1U + expected_i32_count * 4U) {
      return std::nullopt;
    }

    AdjustmentSettings settings;
    settings.kind = adjustment_kind_from_value(reader.read_u8());
    for (int index = 0; index < 4; ++index) {
      set_levels_record_for_photoshop_index(settings.levels, index, read_levels_record_i32(reader));
    }
    settings.levels.channel = levels_channel_from_value(read_i32(reader));
    const auto legacy_curve_shadow = read_i32(reader);
    const auto legacy_curve_midtone = read_i32(reader);
    const auto legacy_curve_highlight = read_i32(reader);
    settings.curves =
        curves_adjustment_from_legacy_outputs(legacy_curve_shadow, legacy_curve_midtone, legacy_curve_highlight);
    settings.hue_saturation.hue_shift = read_i32(reader);
    settings.hue_saturation.saturation_delta = read_i32(reader);
    settings.hue_saturation.lightness_delta = read_i32(reader);
    settings.color_balance.cyan_red = read_i32(reader);
    settings.color_balance.magenta_green = read_i32(reader);
    settings.color_balance.yellow_blue = read_i32(reader);
    if (reader.remaining() >= 16U) {
      // Version-4 trailing colorize extension; absent in pre-July-2026 files.
      settings.hue_saturation.colorize = read_i32(reader) != 0;
      settings.hue_saturation.colorize_hue = std::clamp(read_i32(reader), 0, 360) % 360;
      settings.hue_saturation.colorize_saturation = std::clamp(read_i32(reader), 0, 100);
      settings.hue_saturation.colorize_lightness = std::clamp(read_i32(reader), -100, 100);
    }
    if (settings.kind == AdjustmentKind::Curves && reader.remaining() >= 8U &&
        read_signature(reader) == kPatchyCurvesExtensionSignature) {
      const auto extension_length = static_cast<std::size_t>(reader.read_u32());
      if (extension_length <= kPatchyCurvesExtensionMaxPayloadSize && extension_length <= reader.remaining()) {
        const auto extension = reader.read_bytes(extension_length);
        if (const auto rich_curves = parse_patchy_curves_extension(extension); rich_curves.has_value()) {
          settings.curves = *rich_curves;
        }
      }
      // A malformed or unknown rich tail never invalidates the legacy plAD
      // fields above. This is the compatibility escape hatch for old files and
      // future extensions that retain version 4.
    }
    return settings;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace patchy::psd
