// Gradient Map adjustment: render math, metadata round trip, and the native
// Photoshop 'grdm' block (write, read, byte preservation of imported payloads).

#include "core_test_support.hpp"
#include "psd_test_support.hpp"
#include "test_groups.hpp"

namespace {

using patchy::test::psd_layer_block_payload;

bool same_pixels(const patchy::PixelBuffer& lhs, const patchy::PixelBuffer& rhs) {
  return std::ranges::equal(lhs.data(), rhs.data());
}

using patchy::test::psd_layer_extra_data;
using patchy::test::single_adjustment_layer_psd;

patchy::GradientDefinition two_stop_gradient(patchy::RgbColor left, patchy::RgbColor right) {
  auto gradient = patchy::default_gradient_map_definition(left, right);
  gradient.name = "Test Ramp";
  return gradient;
}

// A 258x1 document: a gray ramp 0..255, one pure red pixel, and one fully
// transparent pixel, under a single Gradient Map adjustment layer.
patchy::Document gradient_map_ramp_document(const patchy::GradientMapAdjustment& gradient_map) {
  patchy::Document document(258, 1, patchy::PixelFormat::rgba8());
  patchy::PixelBuffer pixels(258, 1, patchy::PixelFormat::rgba8());
  for (std::int32_t x = 0; x < 256; ++x) {
    auto* px = pixels.pixel(x, 0);
    px[0] = px[1] = px[2] = static_cast<std::uint8_t>(x);
    px[3] = 255;
  }
  auto* red = pixels.pixel(256, 0);
  red[0] = 255;
  red[1] = 0;
  red[2] = 0;
  red[3] = 255;
  auto* clear = pixels.pixel(257, 0);
  clear[0] = 10;
  clear[1] = 20;
  clear[2] = 30;
  clear[3] = 0;
  document.add_layer(patchy::Layer(document.allocate_layer_id(), "Ramp", std::move(pixels)));

  patchy::AdjustmentSettings settings;
  settings.kind = patchy::AdjustmentKind::GradientMap;
  settings.gradient_map = gradient_map;
  patchy::Layer layer(document.allocate_layer_id(), "Gradient Map", patchy::LayerKind::Adjustment);
  layer.set_bounds(patchy::Rect::from_size(document.width(), document.height()));
  patchy::configure_adjustment_layer(layer, settings);
  document.add_layer(std::move(layer));
  return document;
}

void adjustment_gradient_map_renders_gray_ramp_reverse_and_transparency() {
  // Classic with zero smoothness is a straight per-channel ramp, so a
  // black-to-white map is the identity and a reversed one is the negative.
  patchy::GradientMapAdjustment identity;
  identity.gradient = two_stop_gradient({0, 0, 0}, {255, 255, 255});
  identity.gradient.smoothness = 0;
  identity.method = patchy::GradientInterpolationMethod::Classic;

  std::vector<std::uint8_t> alpha;
  const auto flat = patchy::Compositor{}.flatten_rgb8(gradient_map_ramp_document(identity), &alpha);
  for (std::int32_t x = 0; x < 256; ++x) {
    const auto* px = flat.pixel(x, 0);
    CHECK(px[0] == x && px[1] == x && px[2] == x);
  }
  // Pure red reads the shared Blend If gray: (299 * 255 + 500) / 1000 = 76.
  CHECK(patchy::gradient_map_luminance(patchy::RgbColor{255, 0, 0}) == 76);
  CHECK(flat.pixel(256, 0)[0] == 76 && flat.pixel(256, 0)[1] == 76 && flat.pixel(256, 0)[2] == 76);
  // The transparent pixel stays transparent.
  CHECK(alpha[256] == 255);
  CHECK(alpha[257] == 0);

  auto reversed = identity;
  reversed.reverse = true;
  const auto reversed_flat = patchy::Compositor{}.flatten_rgb8(gradient_map_ramp_document(reversed));
  for (std::int32_t x = 0; x < 256; ++x) {
    CHECK(reversed_flat.pixel(x, 0)[0] == 255 - x);
  }

  // A duotone maps the ends exactly onto its stop colors.
  patchy::GradientMapAdjustment duotone;
  duotone.gradient = two_stop_gradient({20, 10, 90}, {255, 190, 40});
  const auto duotone_flat = patchy::Compositor{}.flatten_rgb8(gradient_map_ramp_document(duotone));
  CHECK(duotone_flat.pixel(0, 0)[0] == 20 && duotone_flat.pixel(0, 0)[1] == 10 && duotone_flat.pixel(0, 0)[2] == 90);
  CHECK(duotone_flat.pixel(255, 0)[0] == 255 && duotone_flat.pixel(255, 0)[1] == 190 &&
        duotone_flat.pixel(255, 0)[2] == 40);

  // Dither nudges each channel by -1..+2 and never changes between renders.
  auto dithered = duotone;
  dithered.dither = true;
  const auto dithered_flat = patchy::Compositor{}.flatten_rgb8(gradient_map_ramp_document(dithered));
  const auto dithered_again = patchy::Compositor{}.flatten_rgb8(gradient_map_ramp_document(dithered));
  CHECK(same_pixels(dithered_flat, dithered_again));
  bool any_dithered = false;
  for (std::int32_t x = 0; x < 256; ++x) {
    for (int channel = 0; channel < 3; ++channel) {
      const auto delta = static_cast<int>(dithered_flat.pixel(x, 0)[channel]) -
                         static_cast<int>(duotone_flat.pixel(x, 0)[channel]);
      CHECK(delta >= -1 && delta <= 2);
      any_dithered = any_dithered || delta != 0;
    }
  }
  CHECK(any_dithered);
}

void adjustment_gradient_map_metadata_round_trip_is_exact() {
  patchy::AdjustmentSettings settings;
  settings.kind = patchy::AdjustmentKind::GradientMap;
  auto& gradient = settings.gradient_map.gradient;
  gradient.name = "Sunset \xE2\x80\x94 ; : test";
  gradient.smoothness = 3000;
  gradient.color_stops = {patchy::GradientColorStop{0.0F, {12, 34, 56}},
                          patchy::GradientColorStop{0.333F, {250, 100, 0}, 0.3F},
                          patchy::GradientColorStop{1.0F, {255, 255, 255}, 0.5F,
                                                    patchy::GradientColorStop::Kind::Background}};
  gradient.alpha_stops = {patchy::GradientAlphaStop{0.0F, 1.0F}, patchy::GradientAlphaStop{0.7F, 0.25F, 0.4F}};
  settings.gradient_map.reverse = true;
  settings.gradient_map.dither = true;
  settings.gradient_map.method = patchy::GradientInterpolationMethod::Linear;

  patchy::Layer layer(1, "Gradient Map", patchy::LayerKind::Adjustment);
  patchy::configure_adjustment_layer(layer, settings);
  const auto restored = patchy::adjustment_settings_from_layer(layer);
  CHECK(restored.has_value());
  CHECK(restored->kind == patchy::AdjustmentKind::GradientMap);
  CHECK(restored->gradient_map == settings.gradient_map);
  CHECK(restored->gradient_map.lut != nullptr);
  CHECK(patchy::adjustment_kind_from_key(patchy::adjustment_kind_key(patchy::AdjustmentKind::GradientMap)) ==
        patchy::AdjustmentKind::GradientMap);

  // A Noise definition carries no stops; the empty lists must survive too.
  settings.gradient_map.gradient.form = patchy::GradientDefinitionForm::Noise;
  settings.gradient_map.gradient.color_stops.clear();
  settings.gradient_map.gradient.alpha_stops.clear();
  settings.gradient_map.gradient.noise.seed = 4242;
  settings.gradient_map.gradient.noise.color_model = patchy::GradientNoiseColorModel::HSB;
  patchy::configure_adjustment_layer(layer, settings);
  const auto noise = patchy::adjustment_settings_from_layer(layer);
  CHECK(noise.has_value() && noise->gradient_map == settings.gradient_map);

  // Switching the layer to another kind drops the gradient keys.
  settings.kind = patchy::AdjustmentKind::Invert;
  patchy::configure_adjustment_layer(layer, settings);
  CHECK(!layer.metadata().contains(patchy::kLayerMetadataAdjustmentGradientMapColorStops));
}

void psd_gradient_map_writes_native_grdm_and_round_trips() {
  patchy::GradientMapAdjustment gradient_map;
  gradient_map.gradient = two_stop_gradient({30, 0, 60}, {255, 220, 120});
  gradient_map.gradient.color_stops.insert(gradient_map.gradient.color_stops.begin() + 1,
                                           patchy::GradientColorStop{0.25F, {200, 40, 90}, 0.3F});
  gradient_map.reverse = true;
  gradient_map.method = patchy::GradientInterpolationMethod::Linear;
  const auto document = gradient_map_ramp_document(gradient_map);

  const auto bytes = patchy::psd::DocumentIo::write_layered_rgb8(document);
  const auto extra = psd_layer_extra_data(bytes, 1);
  const auto grdm = psd_layer_block_payload(extra, "grdm");
  CHECK(grdm.has_value());
  CHECK(!psd_layer_block_payload(extra, "plAD").has_value());
  CHECK(grdm->size() % 4U == 0U);
  // Version 3, reversed, not dithered, then the method key.
  CHECK((*grdm)[0] == 0 && (*grdm)[1] == 3 && (*grdm)[2] == 1 && (*grdm)[3] == 0);
  CHECK(std::string(grdm->begin() + 4, grdm->begin() + 8) == "Lnr ");

  const auto read = patchy::psd::DocumentIo::read(bytes);
  CHECK(read.layers().size() == 2U);
  CHECK(read.layers()[1].kind() == patchy::LayerKind::Adjustment);
  const auto restored = patchy::adjustment_settings_from_layer(read.layers()[1]);
  CHECK(restored.has_value());
  CHECK(restored->kind == patchy::AdjustmentKind::GradientMap);
  CHECK(restored->gradient_map == gradient_map);
  const auto expected = patchy::Compositor{}.flatten_rgb8(document);
  const auto actual = patchy::Compositor{}.flatten_rgb8(read);
  CHECK(same_pixels(expected, actual));

  // An unedited Patchy-written layer re-saves byte-identically.
  const auto resaved = patchy::psd::DocumentIo::write_layered_rgb8(read);
  CHECK(psd_layer_block_payload(psd_layer_extra_data(resaved, 1), "grdm") == grdm);
}

// Hand-built from the published layout (with the 20-byte color stops real
// files carry): version 1, two RGB stops with 16-bit colors that are not
// byte multiples, a foreground color type, 70% smoothness, and a nondefault
// noise block that Patchy must never rewrite.
std::vector<std::uint8_t> authored_grdm_payload(std::uint16_t first_stop_space = 0, bool noise = false) {
  patchy::psd::BigEndianWriter writer;
  writer.write_u16(1);
  writer.write_u8(0);  // reverse
  writer.write_u8(0);  // dither
  const std::u16string name = u"Imported";
  writer.write_u32(static_cast<std::uint32_t>(name.size() + 1U));
  for (const auto unit : name) {
    writer.write_u16(static_cast<std::uint16_t>(unit));
  }
  writer.write_u16(0);
  writer.write_u16(noise ? 0 : 2);
  const auto stop = [&writer](std::uint32_t location, std::uint16_t space, std::array<std::uint16_t, 3> color,
                              std::uint16_t type) {
    writer.write_u32(location);
    writer.write_u32(50);
    writer.write_u16(space);
    for (const auto value : color) {
      writer.write_u16(value);
    }
    writer.write_u16(0);
    writer.write_u16(type);
  };
  if (!noise) {
    stop(0, first_stop_space, {0x1234, 0x0000, 0x8000}, 1);
    stop(4096, 0, {0xFFFF, 0xEEEE, 0x0101}, 0);
  }
  writer.write_u16(2);
  for (const std::uint32_t location : {0U, 4096U}) {
    writer.write_u32(location);
    writer.write_u32(50);
    writer.write_u16(255);
  }
  writer.write_u16(2);     // expansion count
  writer.write_u16(2867);  // smoothness (70%)
  writer.write_u16(32);    // noise block length
  writer.write_u16(noise ? 1 : 0);  // mode
  writer.write_u32(0xCAFE);
  writer.write_u16(0);
  writer.write_u16(1);
  writer.write_u32(2048);
  writer.write_u16(0);
  for (const std::uint16_t value : {0, 0, 0, 0, 100, 100, 100, 100}) {
    writer.write_u16(value);
  }
  writer.write_u16(0);  // dummy
  return writer.bytes();
}

void psd_imported_grdm_payload_is_preserved_until_the_gradient_changes() {
  const auto payload = authored_grdm_payload();
  auto document = patchy::psd::DocumentIo::read(single_adjustment_layer_psd({{{'g', 'r', 'd', 'm'}, payload}}));
  CHECK(document.layers().size() == 1U);
  auto& layer = document.layers()[0];
  CHECK(layer.kind() == patchy::LayerKind::Adjustment);
  const auto imported = patchy::adjustment_settings_from_layer(layer);
  CHECK(imported.has_value() && imported->kind == patchy::AdjustmentKind::GradientMap);
  const auto& gradient = imported->gradient_map.gradient;
  CHECK(gradient.name == "Imported");
  CHECK(gradient.smoothness == 2867);
  CHECK(gradient.form == patchy::GradientDefinitionForm::Solid);
  CHECK(gradient.color_stops.size() == 2U);
  CHECK(gradient.color_stops[0].color.red == 18 && gradient.color_stops[0].color.blue == 128);
  CHECK(gradient.color_stops[0].kind == patchy::GradientColorStop::Kind::Foreground);
  CHECK(imported->gradient_map.method == patchy::GradientInterpolationMethod::Classic);

  const auto resaved_payload = [&document] {
    const auto bytes = patchy::psd::DocumentIo::write_layered_rgb8(document);
    return psd_layer_block_payload(psd_layer_extra_data(bytes, 0), "grdm").value_or(std::vector<std::uint8_t>{});
  };
  // Unedited: every byte survives, including the unmodeled 16-bit colors.
  auto expected = payload;
  while (expected.size() % 2U != 0U) {
    expected.push_back(0);
  }
  CHECK(resaved_payload() == expected);

  // Reverse only: one byte changes. Method: the version-1 block becomes
  // version 3 with the method key inserted; everything else is kept.
  auto settings = *imported;
  settings.gradient_map.reverse = true;
  settings.gradient_map.method = patchy::GradientInterpolationMethod::Perceptual;
  patchy::configure_adjustment_layer(layer, settings);
  auto patched = expected;
  patched[1] = 3;
  patched[2] = 1;
  const std::array<std::uint8_t, 4> perc{'P', 'e', 'r', 'c'};
  patched.insert(patched.begin() + 4, perc.begin(), perc.end());
  CHECK(resaved_payload() == patched);

  // A gradient edit regenerates the block from the model.
  settings.gradient_map.gradient.color_stops[1].color = patchy::RgbColor{0, 128, 255};
  patchy::configure_adjustment_layer(layer, settings);
  const auto regenerated = resaved_payload();
  CHECK(regenerated != patched);
  const auto reread = patchy::psd::DocumentIo::read(single_adjustment_layer_psd({{{'g', 'r', 'd', 'm'}, regenerated}}));
  const auto reread_settings = patchy::adjustment_settings_from_layer(reread.layers()[0]);
  CHECK(reread_settings.has_value());
  CHECK(reread_settings->gradient_map.gradient.color_stops[1].color.blue == 255);
  CHECK(reread_settings->gradient_map.reverse);

  // Stops in a color space Patchy does not model (CMYK) keep the pre-model
  // path: a plain layer that re-emits the raw block untouched.
  const auto cmyk = authored_grdm_payload(2);
  const auto cmyk_document = patchy::psd::DocumentIo::read(single_adjustment_layer_psd({{{'g', 'r', 'd', 'm'}, cmyk}}));
  CHECK(cmyk_document.layers()[0].kind() == patchy::LayerKind::Pixel);
  const auto cmyk_bytes = patchy::psd::DocumentIo::write_layered_rgb8(cmyk_document);
  CHECK(psd_layer_block_payload(psd_layer_extra_data(cmyk_bytes, 0), "grdm") == cmyk);

  // A Noise gradient (mode 1, no color stops) models as Noise, renders a
  // non-gray ramp through the shared noise renderer, and round-trips intact.
  const auto noise = authored_grdm_payload(0, true);
  const auto noise_document =
      patchy::psd::DocumentIo::read(single_adjustment_layer_psd({{{'g', 'r', 'd', 'm'}, noise}}));
  const auto noise_settings = patchy::adjustment_settings_from_layer(noise_document.layers()[0]);
  CHECK(noise_settings.has_value());
  CHECK(noise_settings->gradient_map.gradient.form == patchy::GradientDefinitionForm::Noise);
  CHECK(noise_settings->gradient_map.gradient.noise.seed == 0xCAFEU);
  const auto lut = patchy::build_gradient_map_lut(noise_settings->gradient_map);
  CHECK(std::any_of(lut.begin(), lut.end(), [](patchy::RgbColor color) {
    return color.red != color.green || color.green != color.blue;
  }));
  const auto noise_bytes = patchy::psd::DocumentIo::write_layered_rgb8(noise_document);
  CHECK(psd_layer_block_payload(psd_layer_extra_data(noise_bytes, 0), "grdm") == noise);
}

}  // namespace

std::vector<patchy::test::TestCase> adjustments_gradient_map_tests() {
  return {
      {"adjustment_gradient_map_renders_gray_ramp_reverse_and_transparency",
       adjustment_gradient_map_renders_gray_ramp_reverse_and_transparency},
      {"adjustment_gradient_map_metadata_round_trip_is_exact", adjustment_gradient_map_metadata_round_trip_is_exact},
      {"psd_gradient_map_writes_native_grdm_and_round_trips", psd_gradient_map_writes_native_grdm_and_round_trips},
      {"psd_imported_grdm_payload_is_preserved_until_the_gradient_changes",
       psd_imported_grdm_payload_is_preserved_until_the_gradient_changes},
  };
}
