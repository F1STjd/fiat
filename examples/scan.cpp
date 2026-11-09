import std;
import fiat;

constexpr auto inclusive_float_words =
  fiat::compile<float>(fiat::inclusive_scan(fiat::plus {}, 0.0F));
constexpr auto exclusive_float_words =
  fiat::compile<float>(fiat::exclusive_scan(fiat::plus {}, 0.0F));
constexpr auto inclusive_int_words =
  fiat::compile<std::int32_t>(fiat::inclusive_scan(fiat::plus {}, 0));
constexpr auto exclusive_int_words =
  fiat::compile<std::int32_t>(fiat::exclusive_scan(fiat::plus {}, 0));
constexpr auto inclusive_uint_words =
  fiat::compile<std::uint32_t>(fiat::inclusive_scan(fiat::plus {}, 0U));
constexpr auto exclusive_uint_words =
  fiat::compile<std::uint32_t>(fiat::exclusive_scan(fiat::plus {}, 0U));
constexpr auto maximum_int_words =
  fiat::compile<std::int32_t>(fiat::inclusive_scan(
    fiat::maximum {}, std::numeric_limits<std::int32_t>::min()));

using fused_float_stage = fiat::scan_stage<
  fiat::chain<fiat::bound_operation<fiat::square>>,
  fiat::plus,
  float,
  fiat::scan_mode::exclusive,
  fiat::chain<fiat::bound_operation<fiat::multiplies, float>>
>;

constexpr auto fused_float_words = fiat::compile<float>(fused_float_stage {
  .arguments          = {},
  .identity           = 0.0F,
  .epilogue_arguments = {2.0F},
});

auto
main() -> int
{
  const auto output = std::filesystem::path {FIAT_SPV_OUTPUT_DIR};
  const bool written =
    fiat::write_spirv(
      output / "scan_inclusive_float.spv", inclusive_float_words) &&
    fiat::write_spirv(
      output / "scan_exclusive_float.spv", exclusive_float_words) &&
    fiat::write_spirv(output / "scan_inclusive_int.spv", inclusive_int_words) &&
    fiat::write_spirv(output / "scan_exclusive_int.spv", exclusive_int_words) &&
    fiat::write_spirv(
      output / "scan_inclusive_uint.spv", inclusive_uint_words) &&
    fiat::write_spirv(
      output / "scan_exclusive_uint.spv", exclusive_uint_words) &&
    fiat::write_spirv(output / "scan_maximum_int.spv", maximum_int_words) &&
    fiat::write_spirv(output / "scan_fused_float.spv", fused_float_words);

  return written ? 0 : 1;
}
