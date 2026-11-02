import std;
import fiat;

constexpr auto float_words =
  fiat::compile<float>(fiat::reduce(fiat::plus {}, 0.0F));
constexpr auto int_words =
  fiat::compile<std::int32_t>(fiat::reduce(fiat::plus {}, 0));
constexpr auto uint_words =
  fiat::compile<std::uint32_t>(fiat::reduce(fiat::plus {}, 0U));
constexpr auto fused_words = fiat::compile<std::int32_t>(
  fiat::transform(fiat::square {}) | fiat::reduce(fiat::plus {}, 0));

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(output / "reduce_float.spv", float_words) &&
    fiat::write_spirv(output / "reduce_int.spv", int_words) &&
    fiat::write_spirv(output / "reduce_uint.spv", uint_words) &&
    fiat::write_spirv(output / "reduce_fused.spv", fused_words);

  return written ? 0 : 1;
}
