import std;
import fiat;

constexpr auto tile8_words = fiat::compile<std::uint32_t>(
  fiat::tile_histogram<256>(fiat::digit<8> {}, 0U));
constexpr auto scatter8_words = fiat::compile<std::uint32_t>(
  fiat::stable_scatter<256>(fiat::digit<8> {}, 0U));
constexpr auto tile4_words =
  fiat::compile<std::uint32_t>(fiat::tile_histogram<16>(fiat::digit<4> {}, 0U));
constexpr auto scatter4_words =
  fiat::compile<std::uint32_t>(fiat::stable_scatter<16>(fiat::digit<4> {}, 0U));

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(output / "tile_histogram_digit8_uint.spv", tile8_words) &&
    fiat::write_spirv(
      output / "stable_scatter_digit8_uint.spv", scatter8_words) &&
    fiat::write_spirv(output / "tile_histogram_digit4_uint.spv", tile4_words) &&
    fiat::write_spirv(output / "stable_scatter_digit4_uint.spv", scatter4_words);
  return written ? 0 : 1;
}
