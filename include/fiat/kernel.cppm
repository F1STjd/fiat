export module fiat:kernel;

import std;

import :trace;
import :emit;
import :templates;
import :splice;

namespace fiat
{

export struct transform_push
{
  std::uint64_t input {};
  std::uint64_t output {};
  std::uint32_t count {};
  std::uint32_t first {};
};

static_assert(
  sizeof(transform_push) == 24UZ, "matches the template push block");

export inline constexpr std::string_view    transform_entry {"fiat_transform"};
export inline constexpr std::uint32_t group_size_constant_id {};

export template<class Operation>
struct transform_stage {};

export
template<class Operation>
consteval auto
transform(Operation) -> transform_stage<Operation>
{ return {}; }

export template<scalar Element, class Operation>
  requires traceable<Operation, Element>
inline constexpr std::size_t transform_word_count =
  splice<Element>(transform_template<Element>(), trace<Element, Operation>())
    .size();

export
template<scalar Element, class Operation>
  requires traceable<Operation, Element>
consteval auto
compile(transform_stage<Operation>)
  -> std::array<std::uint32_t, transform_word_count<Element, Operation>>
{
  const std::vector<std::uint32_t> words =
    splice<Element>(transform_template<Element>(), trace<Element, Operation>());
  std::array<std::uint32_t, transform_word_count<Element, Operation>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
