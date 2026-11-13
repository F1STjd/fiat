export module fiat:templates;

import std;

import :spirv;
import :trace;

namespace fiat
{

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wc23-extensions"
#endif

inline constexpr auto transform_float_bytes = std::to_array<unsigned char>({
#embed "transform_float.spv"
});

inline constexpr auto transform_int_bytes = std::to_array<unsigned char>({
#embed "transform_int.spv"
});

inline constexpr auto transform_uint_bytes = std::to_array<unsigned char>({
#embed "transform_uint.spv"
});

inline constexpr auto reduce_float_bytes = std::to_array<unsigned char>({
#embed "reduce_float.spv"
});

inline constexpr auto reduce_int_bytes = std::to_array<unsigned char>({
#embed "reduce_int.spv"
});

inline constexpr auto reduce_uint_bytes = std::to_array<unsigned char>({
#embed "reduce_uint.spv"
});

inline constexpr auto scan_inclusive_float_bytes = std::to_array<unsigned char>({
#embed "scan_inclusive_float.spv"
});

inline constexpr auto scan_inclusive_int_bytes = std::to_array<unsigned char>({
#embed "scan_inclusive_int.spv"
});

inline constexpr auto scan_inclusive_uint_bytes = std::to_array<unsigned char>({
#embed "scan_inclusive_uint.spv"
});

inline constexpr auto scan_exclusive_float_bytes = std::to_array<unsigned char>({
#embed "scan_exclusive_float.spv"
});

inline constexpr auto scan_exclusive_int_bytes = std::to_array<unsigned char>({
#embed "scan_exclusive_int.spv"
});

inline constexpr auto scan_exclusive_uint_bytes = std::to_array<unsigned char>({
#embed "scan_exclusive_uint.spv"
});

inline constexpr auto predicate_greater_float_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_float.spv"
});

inline constexpr auto predicate_greater_int_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_int.spv"
});

inline constexpr auto predicate_greater_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_uint.spv"
});

inline constexpr auto predicate_greater_equal_float_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_equal_float.spv"
});

inline constexpr auto predicate_greater_equal_int_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_equal_int.spv"
});

inline constexpr auto predicate_greater_equal_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_greater_equal_uint.spv"
});

inline constexpr auto predicate_less_float_bytes = std::to_array<unsigned char>({
#embed "predicate_less_float.spv"
});

inline constexpr auto predicate_less_int_bytes = std::to_array<unsigned char>({
#embed "predicate_less_int.spv"
});

inline constexpr auto predicate_less_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_less_uint.spv"
});

inline constexpr auto predicate_less_equal_float_bytes = std::to_array<unsigned char>({
#embed "predicate_less_equal_float.spv"
});

inline constexpr auto predicate_less_equal_int_bytes = std::to_array<unsigned char>({
#embed "predicate_less_equal_int.spv"
});

inline constexpr auto predicate_less_equal_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_less_equal_uint.spv"
});

inline constexpr auto predicate_equal_float_bytes = std::to_array<unsigned char>({
#embed "predicate_equal_float.spv"
});

inline constexpr auto predicate_equal_int_bytes = std::to_array<unsigned char>({
#embed "predicate_equal_int.spv"
});

inline constexpr auto predicate_equal_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_equal_uint.spv"
});

inline constexpr auto predicate_not_equal_float_bytes = std::to_array<unsigned char>({
#embed "predicate_not_equal_float.spv"
});

inline constexpr auto predicate_not_equal_int_bytes = std::to_array<unsigned char>({
#embed "predicate_not_equal_int.spv"
});

inline constexpr auto predicate_not_equal_uint_bytes = std::to_array<unsigned char>({
#embed "predicate_not_equal_uint.spv"
});

inline constexpr auto scatter_float_bytes = std::to_array<unsigned char>({
#embed "scatter_float.spv"
});

inline constexpr auto scatter_int_bytes = std::to_array<unsigned char>({
#embed "scatter_int.spv"
});

inline constexpr auto scatter_uint_bytes = std::to_array<unsigned char>({
#embed "scatter_uint.spv"
});

inline constexpr auto histogram_uint_bytes = std::to_array<unsigned char>({
#embed "histogram_uint.spv"
});

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif
template<std::size_t Size>
  requires (Size % sizeof(std::uint32_t) == 0UZ)
consteval
auto
assemble_words(const std::array<unsigned char, Size>& bytes)
  -> std::array<std::uint32_t, Size / sizeof(std::uint32_t)>
{
  std::array<std::uint32_t, Size / sizeof(std::uint32_t)> words {};
  for (auto index : std::views::indices(words.size()))
  {
    const auto base = index * sizeof(std::uint32_t);
    words[ index ] =
      static_cast<std::uint32_t>(bytes[ base ]) |
      static_cast<std::uint32_t>(bytes[ base + 1UZ ]) << 8U |
      static_cast<std::uint32_t>(bytes[ base + 2UZ ]) << 16U |
      static_cast<std::uint32_t>(bytes[ base + 3UZ ]) << 24U;
  }
  return words;
}

inline constexpr auto transform_float_words =
  assemble_words(transform_float_bytes);
inline constexpr auto transform_int_words = assemble_words(transform_int_bytes);
inline constexpr auto transform_uint_words =
  assemble_words(transform_uint_bytes);

inline constexpr auto reduce_float_words = assemble_words(reduce_float_bytes);
inline constexpr auto reduce_int_words   = assemble_words(reduce_int_bytes);
inline constexpr auto reduce_uint_words  = assemble_words(reduce_uint_bytes);

inline constexpr auto scan_inclusive_float_words =
  assemble_words(scan_inclusive_float_bytes);
inline constexpr auto scan_inclusive_int_words =
  assemble_words(scan_inclusive_int_bytes);
inline constexpr auto scan_inclusive_uint_words =
  assemble_words(scan_inclusive_uint_bytes);
inline constexpr auto scan_exclusive_float_words =
  assemble_words(scan_exclusive_float_bytes);
inline constexpr auto scan_exclusive_int_words =
  assemble_words(scan_exclusive_int_bytes);
inline constexpr auto scan_exclusive_uint_words =
  assemble_words(scan_exclusive_uint_bytes);

inline constexpr auto predicate_greater_float_words =
  assemble_words(predicate_greater_float_bytes);
inline constexpr auto predicate_greater_int_words =
  assemble_words(predicate_greater_int_bytes);
inline constexpr auto predicate_greater_uint_words =
  assemble_words(predicate_greater_uint_bytes);
inline constexpr auto predicate_greater_equal_float_words =
  assemble_words(predicate_greater_equal_float_bytes);
inline constexpr auto predicate_greater_equal_int_words =
  assemble_words(predicate_greater_equal_int_bytes);
inline constexpr auto predicate_greater_equal_uint_words =
  assemble_words(predicate_greater_equal_uint_bytes);
inline constexpr auto predicate_less_float_words =
  assemble_words(predicate_less_float_bytes);
inline constexpr auto predicate_less_int_words =
  assemble_words(predicate_less_int_bytes);
inline constexpr auto predicate_less_uint_words =
  assemble_words(predicate_less_uint_bytes);
inline constexpr auto predicate_less_equal_float_words =
  assemble_words(predicate_less_equal_float_bytes);
inline constexpr auto predicate_less_equal_int_words =
  assemble_words(predicate_less_equal_int_bytes);
inline constexpr auto predicate_less_equal_uint_words =
  assemble_words(predicate_less_equal_uint_bytes);
inline constexpr auto predicate_equal_float_words =
  assemble_words(predicate_equal_float_bytes);
inline constexpr auto predicate_equal_int_words =
  assemble_words(predicate_equal_int_bytes);
inline constexpr auto predicate_equal_uint_words =
  assemble_words(predicate_equal_uint_bytes);
inline constexpr auto predicate_not_equal_float_words =
  assemble_words(predicate_not_equal_float_bytes);
inline constexpr auto predicate_not_equal_int_words =
  assemble_words(predicate_not_equal_int_bytes);
inline constexpr auto predicate_not_equal_uint_words =
  assemble_words(predicate_not_equal_uint_bytes);

inline constexpr auto scatter_float_words = assemble_words(scatter_float_bytes);
inline constexpr auto scatter_int_words   = assemble_words(scatter_int_bytes);
inline constexpr auto scatter_uint_words  = assemble_words(scatter_uint_bytes);

inline constexpr auto histogram_uint_words =
  assemble_words(histogram_uint_bytes);

static_assert(transform_float_words.front() == spirv::magic);
static_assert(transform_int_words.front() == spirv::magic);
static_assert(transform_uint_words.front() == spirv::magic);

static_assert(reduce_float_words.front() == spirv::magic);
static_assert(reduce_int_words.front() == spirv::magic);
static_assert(reduce_uint_words.front() == spirv::magic);

static_assert(scan_inclusive_float_words.front() == spirv::magic);
static_assert(scan_inclusive_int_words.front() == spirv::magic);
static_assert(scan_inclusive_uint_words.front() == spirv::magic);
static_assert(scan_exclusive_float_words.front() == spirv::magic);
static_assert(scan_exclusive_int_words.front() == spirv::magic);
static_assert(scan_exclusive_uint_words.front() == spirv::magic);

static_assert(predicate_greater_float_words.front() == spirv::magic);
static_assert(predicate_greater_int_words.front() == spirv::magic);
static_assert(predicate_greater_uint_words.front() == spirv::magic);
static_assert(predicate_greater_equal_float_words.front() == spirv::magic);
static_assert(predicate_greater_equal_int_words.front() == spirv::magic);
static_assert(predicate_greater_equal_uint_words.front() == spirv::magic);
static_assert(predicate_less_float_words.front() == spirv::magic);
static_assert(predicate_less_int_words.front() == spirv::magic);
static_assert(predicate_less_uint_words.front() == spirv::magic);
static_assert(predicate_less_equal_float_words.front() == spirv::magic);
static_assert(predicate_less_equal_int_words.front() == spirv::magic);
static_assert(predicate_less_equal_uint_words.front() == spirv::magic);
static_assert(predicate_equal_float_words.front() == spirv::magic);
static_assert(predicate_equal_int_words.front() == spirv::magic);
static_assert(predicate_equal_uint_words.front() == spirv::magic);
static_assert(predicate_not_equal_float_words.front() == spirv::magic);
static_assert(predicate_not_equal_int_words.front() == spirv::magic);
static_assert(predicate_not_equal_uint_words.front() == spirv::magic);

static_assert(scatter_float_words.front() == spirv::magic);
static_assert(scatter_int_words.front() == spirv::magic);
static_assert(scatter_uint_words.front() == spirv::magic);

static_assert(histogram_uint_words.front() == spirv::magic);

template<scalar Element>
consteval
auto
transform_template() -> std::span<const std::uint32_t>
{
  if constexpr (std::same_as<Element, float>) { return transform_float_words; }
  else if constexpr (std::same_as<Element, std::int32_t>)
  {
    return transform_int_words;
  }
  else
  {
    return transform_uint_words;
  }
}

template<scalar Element>
consteval
auto
reduce_template() -> std::span<const std::uint32_t>
{
  if constexpr (std::same_as<Element, float>) { return reduce_float_words; }
  else if constexpr (std::same_as<Element, std::int32_t>)
  {
    return reduce_int_words;
  }
  else
  {
    return reduce_uint_words;
  }
}

export enum class scan_mode : std::uint8_t
{
  inclusive,
  exclusive,
};

template<scalar Element, scan_mode Mode>
consteval
auto
scan_template() -> std::span<const std::uint32_t>
{
  if constexpr (Mode == scan_mode::inclusive)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return scan_inclusive_float_words;
    }
    else if constexpr (std::same_as<Element, std::uint32_t>)
    {
      return scan_inclusive_uint_words;
    }
    else
    {
      return scan_inclusive_int_words;
    }
  }
  else
  {
    if constexpr (std::same_as<Element, float>)
    {
      return scan_exclusive_float_words;
    }
    else if constexpr (std::same_as<Element, std::uint32_t>)
    {
      return scan_exclusive_uint_words;
    }
    else
    {
      return scan_exclusive_int_words;
    }
  }
}

consteval
auto
histogram_template() -> std::span<const std::uint32_t>
{ return histogram_uint_words; }

} // namespace fiat
