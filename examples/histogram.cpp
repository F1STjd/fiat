import std;
import fiat;

constexpr auto identity_words =
  fiat::compile<std::uint32_t>(fiat::histogram<256>(fiat::identity {}));
constexpr auto digit8_words =
  fiat::compile<std::uint32_t>(fiat::histogram<256>(fiat::digit<8> {}, 0U));
constexpr auto digit4_words =
  fiat::compile<std::uint32_t>(fiat::histogram<16>(fiat::digit<4> {}, 8U));

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(output / "histogram_identity_uint.spv", identity_words) &&
    fiat::write_spirv(output / "histogram_digit8_uint.spv", digit8_words) &&
    fiat::write_spirv(output / "histogram_digit4_uint.spv", digit4_words);
  return written ? 0 : 1;
}
