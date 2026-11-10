import std;
import fiat;

constexpr auto greater_float_words =
  fiat::compile<float>(fiat::predicate(fiat::greater {}, 0.0F));
constexpr auto greater_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::greater {}, 0));
constexpr auto greater_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::greater {}, 0U));
constexpr auto greater_equal_float_words =
  fiat::compile<float>(fiat::predicate(fiat::greater_equal {}, 0.0F));
constexpr auto greater_equal_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::greater_equal {}, 0));
constexpr auto greater_equal_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::greater_equal {}, 0U));
constexpr auto less_float_words =
  fiat::compile<float>(fiat::predicate(fiat::less {}, 0.0F));
constexpr auto less_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::less {}, 0));
constexpr auto less_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::less {}, 0U));
constexpr auto less_equal_float_words =
  fiat::compile<float>(fiat::predicate(fiat::less_equal {}, 0.0F));
constexpr auto less_equal_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::less_equal {}, 0));
constexpr auto less_equal_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::less_equal {}, 0U));
constexpr auto equal_float_words =
  fiat::compile<float>(fiat::predicate(fiat::equal {}, 0.0F));
constexpr auto equal_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::equal {}, 0));
constexpr auto equal_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::equal {}, 0U));
constexpr auto not_equal_float_words =
  fiat::compile<float>(fiat::predicate(fiat::not_equal {}, 0.0F));
constexpr auto not_equal_int_words =
  fiat::compile<std::int32_t>(fiat::predicate(fiat::not_equal {}, 0));
constexpr auto not_equal_uint_words =
  fiat::compile<std::uint32_t>(fiat::predicate(fiat::not_equal {}, 0U));
constexpr auto scatter_float_words = fiat::compile<float>(fiat::scatter());
constexpr auto scatter_int_words = fiat::compile<std::int32_t>(fiat::scatter());
constexpr auto scatter_uint_words =
  fiat::compile<std::uint32_t>(fiat::scatter());

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(
      output / "predicate_greater_float.spv", greater_float_words) &&
    fiat::write_spirv(output / "predicate_greater_int.spv", greater_int_words) &&
    fiat::write_spirv(
      output / "predicate_greater_uint.spv", greater_uint_words) &&
    fiat::write_spirv(
      output / "predicate_greater_equal_float.spv", greater_equal_float_words) &&
    fiat::write_spirv(
      output / "predicate_greater_equal_int.spv", greater_equal_int_words) &&
    fiat::write_spirv(
      output / "predicate_greater_equal_uint.spv", greater_equal_uint_words) &&
    fiat::write_spirv(output / "predicate_less_float.spv", less_float_words) &&
    fiat::write_spirv(output / "predicate_less_int.spv", less_int_words) &&
    fiat::write_spirv(output / "predicate_less_uint.spv", less_uint_words) &&
    fiat::write_spirv(
      output / "predicate_less_equal_float.spv", less_equal_float_words) &&
    fiat::write_spirv(
      output / "predicate_less_equal_int.spv", less_equal_int_words) &&
    fiat::write_spirv(
      output / "predicate_less_equal_uint.spv", less_equal_uint_words) &&
    fiat::write_spirv(output / "predicate_equal_float.spv", equal_float_words) &&
    fiat::write_spirv(output / "predicate_equal_int.spv", equal_int_words) &&
    fiat::write_spirv(output / "predicate_equal_uint.spv", equal_uint_words) &&
    fiat::write_spirv(
      output / "predicate_not_equal_float.spv", not_equal_float_words) &&
    fiat::write_spirv(
      output / "predicate_not_equal_int.spv", not_equal_int_words) &&
    fiat::write_spirv(
      output / "predicate_not_equal_uint.spv", not_equal_uint_words) &&
    fiat::write_spirv(output / "scatter_float.spv", scatter_float_words) &&
    fiat::write_spirv(output / "scatter_int.spv", scatter_int_words) &&
    fiat::write_spirv(output / "scatter_uint.spv", scatter_uint_words);

  return written ? 0 : 1;
}
