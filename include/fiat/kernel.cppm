export module fiat:kernel;

import std;

import :trace;
import :emit;
import :templates;
import :splice;

namespace fiat
{

export inline constexpr std::size_t argument_offset {24UZ};
export inline constexpr std::size_t argument_capacity {26UZ};

export struct transform_push
{
  std::uint64_t                                input {};
  std::uint64_t                                output {};
  std::uint32_t                                count {};
  std::uint32_t                                first {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

static_assert(
  sizeof(transform_push) ==
    argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(transform_push) == 128UZ,
  "fits the 12 bytes every Vulkan device guarantees");

export inline constexpr std::string_view transform_entry {"fiat_transform"};
export inline constexpr std::uint32_t    group_size_constant_id {};

export template<class Operation>
struct transform_stage
{
  typename arguments_of<Operation>::type arguments {};
};

export template<class Operation, class... Arguments>
struct bound_operation
{
  using argument_tuple = std::tuple<Arguments...>;

  template<scalar Element, class... Values>
    requires (
      (std::same_as<Values, value<Arguments>> && ...) &&
      traceable_call<Operation, Element, Arguments...>)
  constexpr
  auto
  operator ()(value<Element>& operand, const Values&... arguments) const
    -> value<Element>
  {
    if constexpr (
      std::is_void_v<
        std::invoke_result_t<Operation, value<Element>&, const Values&...>
      >)
    {
      Operation {}(operand, arguments...);
      return operand;
    }
    else
    {
      return Operation {}(operand, arguments...);
    }
  }
};

template<
  class Operation,
  std::size_t First,
  scalar      Element,
  class Arguments,
  std::size_t... Slots
>
constexpr
void
apply_operation(
  value<Element>&                   operand,
  [[maybe_unused]] const Arguments& arguments,
  std::index_sequence<Slots...>)
{ operand = Operation {}(operand, std::get<First + Slots>(arguments)...); }

template<std::size_t Count>
consteval
auto
first_slots(const std::array<std::size_t, Count>& counts)
  -> std::array<std::size_t, Count>
{
  std::array<std::size_t, Count> result {};
  std::exclusive_scan(counts.begin(), counts.end(), result.begin(), 0UZ);
  return result;
}

export template<class... Operations>
struct chain
{
  using argument_tuple = decltype(std::tuple_cat(
    std::declval<typename Operations::argument_tuple>()...));

  static constexpr std::array counts {
    std::tuple_size_v<typename Operations::argument_tuple>...,
  };

  static constexpr std::array firsts = first_slots(counts);

  template<scalar Element, class... Arguments>
    requires (
      std::same_as<std::tuple<Arguments...>, argument_tuple> &&
      (traceable<Operations, Element> && ...))
  constexpr
  auto
  operator ()(value<Element>& operand, const value<Arguments>&... arguments)
    const -> value<Element>
  {
    const std::tuple<const value<Arguments>&...> all {arguments...};
    [ & ]<std::size_t... Positions>(std::index_sequence<Positions...>) {
      (apply_operation<Operations, firsts[ Positions ]>(
         operand, all, std::make_index_sequence<counts[ Positions ]> {}),
        ...);
    }(std::index_sequence_for<Operations...> {});
    return operand;
  }
};

export
template<class Operation, class... Arguments>
constexpr auto
transform(Operation, Arguments... arguments)
  -> transform_stage<chain<bound_operation<Operation>, Arguments...>>
{
  static_assert(
    std::is_empty_v<Operation>,
    "fiat::transform cannot read lambda captures; pass runtime values as "
    "arguments: fiat::transform(f, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fiat::transform arguments must be float, std::int32_t or std::uint32_t");
  static_assert(
    sizeof...(Arguments) <= argument_capacity,
    "fiat::transform takes at most 26 runtime arguments");
  return {.arguments = {arguments...}};
}

export
template<class... Left, class... Right>
constexpr auto
fuse(
  transform_stage<chain<Left...>> left, transform_stage<chain<Right...>> right)
  -> transform_stage<chain<Left..., Right...>>
{
  static_assert(
    std::tuple_size_v<typename chain<Left..., Right...>::argument_tuple> <=
      argument_capacity,
    "a fused fiat::transform chain takes at most 26 runtime arguments");
  return {.arguments = std::tuple_cat(left.arguments, right.arguments)};
}

export
template<class... Left, class... Right>
constexpr auto
operator |(
  transform_stage<chain<Left...>> left, transform_stage<chain<Right...>> right)
  -> transform_stage<chain<Left..., Right...>>
{ return fuse(left, right); }

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
