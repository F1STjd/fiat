export module fiat:kernel;

import std;

import :trace;
import :emit;
import :templates;
import :splice;

namespace fiat
{

export inline constexpr std::size_t argument_capacity {26UZ};

namespace detail
{

export inline constexpr std::size_t argument_offset {24UZ};

export struct transform_push
{
  std::uint64_t                                input {};
  std::uint64_t                                output {};
  std::uint32_t                                count {};
  std::uint32_t                                first {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

export inline constexpr std::uint32_t reduce_group_limit {1'024U};
static_assert(
  reduce_group_limit <= 65'535U,
  "65535 is the Vulkan minimum of maxComputeWorkGroupCount[0]");

export inline constexpr std::size_t reduce_scratch_bytes {
  8UZ + 4UZ * reduce_group_limit
};
static_assert(reduce_scratch_bytes == 4'104UZ, "counter + result + partials");

export struct reduce_push
{
  std::uint64_t                                input {};
  std::uint64_t                                scratch {};
  std::uint32_t                                count {};
  std::uint32_t                                identity {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

} // namespace detail

static_assert(
  sizeof(detail::transform_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(detail::transform_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");
static_assert(
  sizeof(detail::reduce_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(detail::reduce_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

export inline constexpr std::string_view transform_entry {"fiat_transform"};
export inline constexpr std::string_view reduce_entry {"fiat_reduce"};
export inline constexpr std::uint32_t    group_size_constant_id {};

export template<class Operation>
struct transform_stage
{
  typename arguments_of<Operation>::type arguments {};
};

export template<class Operation, class Combine, class Identity>
struct reduce_stage
{
  typename arguments_of<Operation>::type arguments {};
  Identity                               identity {};
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

  static constexpr std::array<std::size_t, sizeof...(Operations)> counts {
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
  -> transform_stage<chain<bound_operation<Operation, Arguments...>>>
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

export
template<class Combine, class Identity>
constexpr auto
reduce(Combine, Identity identity) -> reduce_stage<chain<>, Combine, Identity>
{
  static_assert(
    std::is_empty_v<Combine>, "fiat::reduce cannot read lambda captures");
  static_assert(
    (scalar<Identity>),
    "fiat::reduce identity must be float, std::int32_t or std::uint32_t");
  static_assert(
    std::tuple_size_v<typename arguments_of<Combine>::type> == 0UZ,
    "fiat::reduce combine operations take no runtime arguments; fuse a "
    "fiat::transform before the reduce instead");
  return {.arguments = {}, .identity = identity};
}

export
template<class... Left, class Combine, class Identity>
constexpr auto
operator |(
  transform_stage<chain<Left...>>          left,
  reduce_stage<chain<>, Combine, Identity> right)
  -> reduce_stage<chain<Left...>, Combine, Identity>
{ return {.arguments = left.arguments, .identity = right.identity}; }

namespace detail
{

template<scalar Element, class Operation>
  requires traceable<Operation, Element>
constexpr
auto
transform_module() -> std::vector<std::uint32_t>
{
  const std::vector<instruction> instructions = trace<Element, Operation>();
  return splice<Element>(
    transform_template<Element>(),
    std::array {symbol_trace {operation_symbol, instructions}});
}

} // namespace detail

export template<scalar Element, class Operation>
  requires traceable<Operation, Element>
inline constexpr std::size_t transform_word_count =
  detail::transform_module<Element, Operation>().size();

export
template<scalar Element, class Operation>
  requires traceable<Operation, Element>
consteval auto
compile(transform_stage<Operation>)
  -> std::array<std::uint32_t, transform_word_count<Element, Operation>>
{
  const std::vector<std::uint32_t> words =
    detail::transform_module<Element, Operation>();
  std::array<std::uint32_t, transform_word_count<Element, Operation>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

namespace detail
{

template<scalar Element, class Operation, class Combine>
  requires (traceable<Operation, Element> && combinable<Combine, Element>)
constexpr
auto
reduce_module() -> std::vector<std::uint32_t>
{
  const std::vector<instruction> map_instructions = trace<Element, Operation>();
  const std::vector<instruction> combine_instructions =
    trace_binary<Element, Combine>();
  return splice<Element>(
    reduce_template<Element>(),
    std::array {
      symbol_trace {
        .symbol       = operation_symbol,
        .instructions = map_instructions,
      },
      symbol_trace {
        .symbol       = combine_symbol,
        .instructions = combine_instructions,
      },
    });
}

} // namespace detail

export template<scalar Element, class Operation, class Combine>
  requires (traceable<Operation, Element> && combinable<Combine, Element>)
inline constexpr std::size_t reduce_word_count =
  detail::reduce_module<Element, Operation, Combine>().size();

export
template<scalar Element, class Operation, class Combine, class Identity>
  requires (
    traceable<Operation, Element> &&
    combinable<Combine, Element> &&
    std::same_as<Identity, Element>)
consteval auto
compile(reduce_stage<Operation, Combine, Identity>)
  -> std::array<std::uint32_t, reduce_word_count<Element, Operation, Combine>>
{
  const std::vector<std::uint32_t> words =
    detail::reduce_module<Element, Operation, Combine>();
  std::array<std::uint32_t, reduce_word_count<Element, Operation, Combine>>
    result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
