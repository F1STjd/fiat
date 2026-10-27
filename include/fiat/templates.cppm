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

static_assert(transform_float_words.front() == spirv::magic);
static_assert(transform_int_words.front() == spirv::magic);
static_assert(transform_uint_words.front() == spirv::magic);
static_assert(reduce_float_words.front() == spirv::magic);
static_assert(reduce_int_words.front() == spirv::magic);
static_assert(reduce_uint_words.front() == spirv::magic);

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

} // namespace fiat
