export module fiat:templates;

import std;

import :spirv;
import :trace;

namespace fiat
{

inline
void
template_error([[maybe_unused]] std::string_view reason)
{}

template<std::size_t Size>
  requires (Size % sizeof(std::uint32_t) == 0UZ)
consteval
auto
assemble_words(const std::array<unsigned char, Size>& bytes)
  -> std::array<std::uint32_t, Size / sizeof(std::uint32_t)>
{
  static_assert(
    std::endian::native == std::endian::little,
    "assemble_words reads embedded SPIR-V as little-endian words");
  auto words = std::bit_cast<
    std::array<std::uint32_t, Size / sizeof(std::uint32_t)>
  >(bytes);
  if (words.front() != spirv::magic)
  {
    template_error("an embedded template does not start with the SPIR-V magic");
  }
  return words;
}

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wc23-extensions"
#endif

inline constexpr auto transform_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "transform_float.spv"
  }));

inline constexpr auto transform_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "transform_int.spv"
  }));

inline constexpr auto transform_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "transform_uint.spv"
  }));

inline constexpr auto reduce_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "reduce_float.spv"
  }));

inline constexpr auto reduce_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "reduce_int.spv"
  }));

inline constexpr auto reduce_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "reduce_uint.spv"
  }));

inline constexpr auto scan_inclusive_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_inclusive_float.spv"
  }));

inline constexpr auto scan_inclusive_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_inclusive_int.spv"
  }));

inline constexpr auto scan_inclusive_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_inclusive_uint.spv"
  }));

inline constexpr auto scan_exclusive_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_exclusive_float.spv"
  }));

inline constexpr auto scan_exclusive_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_exclusive_int.spv"
  }));

inline constexpr auto scan_exclusive_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scan_exclusive_uint.spv"
  }));

inline constexpr auto predicate_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "predicate_float.spv"
  }));

inline constexpr auto predicate_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "predicate_int.spv"
  }));

inline constexpr auto predicate_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "predicate_uint.spv"
  }));

inline constexpr auto scatter_float_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scatter_float.spv"
  }));

inline constexpr auto scatter_int_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scatter_int.spv"
  }));

inline constexpr auto scatter_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "scatter_uint.spv"
  }));

inline constexpr auto histogram_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "histogram_uint.spv"
  }));

inline constexpr auto tile_histogram_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "tile_histogram_uint.spv"
  }));

inline constexpr auto stable_scatter_uint_words =
  assemble_words(std::to_array<unsigned char>({
#embed "stable_scatter_uint.spv"
  }));

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

export enum class scan_mode : std::uint8_t
{
  inclusive,
  exclusive,
};

} // namespace fiat
