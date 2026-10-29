import std;
import fiat;

constexpr auto plus   = [](auto left, auto right) { return left + right; };
constexpr auto square = [](auto& value) { return value * value; };

constexpr auto float_words = fiat::compile<float>(fiat::reduce(plus, 0.0F));
constexpr auto int_words   = fiat::compile<std::int32_t>(fiat::reduce(plus, 0));
constexpr auto uint_words =
  fiat::compile<std::uint32_t>(fiat::reduce(plus, 0U));
constexpr auto fused_words =
  fiat::compile<std::int32_t>(fiat::transform(square) | fiat::reduce(plus, 0));

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
