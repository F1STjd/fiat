import std;
import fiat;

constexpr auto minimum_float_words =
  fiat::compile<float>(fiat::transform(fiat::minimum {}, 0.0F));
constexpr auto maximum_float_words =
  fiat::compile<float>(fiat::transform(fiat::maximum {}, 0.0F));
constexpr auto minimum_int_words =
  fiat::compile<std::int32_t>(fiat::transform(fiat::minimum {}, 0));
constexpr auto maximum_int_words =
  fiat::compile<std::int32_t>(fiat::transform(fiat::maximum {}, 0));
constexpr auto minimum_uint_words =
  fiat::compile<std::uint32_t>(fiat::transform(fiat::minimum {}, 0U));
constexpr auto maximum_uint_words =
  fiat::compile<std::uint32_t>(fiat::transform(fiat::maximum {}, 0U));

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(
      output / "selection_minimum_float.spv", minimum_float_words) &&
    fiat::write_spirv(
      output / "selection_maximum_float.spv", maximum_float_words) &&
    fiat::write_spirv(output / "selection_minimum_int.spv", minimum_int_words) &&
    fiat::write_spirv(output / "selection_maximum_int.spv", maximum_int_words) &&
    fiat::write_spirv(
      output / "selection_minimum_uint.spv", minimum_uint_words) &&
    fiat::write_spirv(output / "selection_maximum_uint.spv", maximum_uint_words);

  return written ? 0 : 1;
}
