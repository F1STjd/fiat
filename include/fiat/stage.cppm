export module fiat:stage;

import std;

import :trace;
import :emit;
import :templates;
import :splice;

namespace fiat
{

export enum class stage_class : std::uint8_t
{
  elementwise,
  sink,
  scan,
  permute,
  contraction,
  opaque,
};

export enum class port_access : std::uint8_t
{
  read,
  write,
  read_write,
};

export template<scalar Element, port_access Access, std::uint32_t Size = 0U>
struct port
{
  using element_type = Element;
  static constexpr port_access   access {Access};
  static constexpr std::uint32_t size {Size};
};

export struct stage_schedule
{
  std::uint32_t group_size {};
  std::uint32_t items_per_invocation {1U};
};

export enum class skeleton_kind : std::uint8_t
{
  transform,
  transform_with,
  reduce,
  inclusive_scan,
  exclusive_scan,
  histogram,
  tile_histogram,
  stable_scatter,
  scatter,
  predicate,
};

export enum class splice_point : std::uint8_t
{
  operation,
  combine,
  epilogue,
  key,
  predicate,
  uniform_operation,
};

export template<splice_point Point, class Operation>
struct splice_operation
{
  static constexpr splice_point point {Point};
  using operation_type = Operation;
};

export enum class dispatch_extent : std::uint8_t
{
  per_element,
  grid_stride,
};

export struct dispatch_rule
{
  std::string_view entry {};
  dispatch_extent  extent {dispatch_extent::per_element};
  std::uint32_t    maximum_groups {};
};

export struct workspace_sizes
{
  std::size_t table_bytes {};
  std::size_t ping_pong_bytes {};
  std::size_t scratch_bytes {};

  [[nodiscard]]
  constexpr
  auto
  regions() const -> std::array<std::size_t, 3UZ>
  { return {table_bytes, ping_pong_bytes, scratch_bytes}; }

  [[nodiscard]]
  constexpr
  auto
  region_count() const -> std::size_t
  {
    return std::size_t {table_bytes != 0UZ} +
           std::size_t {ping_pong_bytes != 0UZ} +
           std::size_t {scratch_bytes != 0UZ};
  }
};

export inline constexpr std::uint32_t push_budget {256U};

export inline constexpr std::uint32_t skeleton_push_bytes {128U};

export struct dispatch_push
{
  static constexpr std::uint32_t address_bytes {8U};
  static constexpr std::uint32_t word_bytes {4U};

  std::uint32_t ports {};
  std::uint32_t arguments {};

  [[nodiscard]]
  constexpr
  auto
  address_offset(std::uint32_t port_index) const -> std::uint32_t
  { return address_bytes * port_index; }

  [[nodiscard]]
  constexpr
  auto
  count_offset() const -> std::uint32_t
  { return address_bytes * ports; }

  [[nodiscard]]
  constexpr
  auto
  first_offset() const -> std::uint32_t
  { return count_offset() + word_bytes; }

  [[nodiscard]]
  constexpr
  auto
  argument_offset() const -> std::uint32_t
  { return first_offset() + word_bytes; }

  [[nodiscard]]
  constexpr
  auto
  argument_member() const -> std::uint32_t
  { return ports + 2U; }

  [[nodiscard]]
  constexpr
  auto
  size() const -> std::uint32_t
  { return argument_offset() + word_bytes * arguments; }

  [[nodiscard]]
  constexpr
  auto
  argument_words_within(std::uint32_t budget) const -> std::uint32_t
  { return (budget - argument_offset()) / word_bytes; }
};

template<class Type>
inline constexpr bool is_port {false};

template<scalar Element, port_access Access, std::uint32_t Size>
inline constexpr bool is_port<port<Element, Access, Size>> {true};

template<class Ports>
inline constexpr bool is_port_tuple {false};

template<class... Ports>
inline constexpr bool is_port_tuple<std::tuple<Ports...>> {
  sizeof...(Ports) >= 1UZ && (is_port<Ports> && ...),
};

template<class Splice, class Element>
concept traceable_splice =
  requires {
    { Splice::point } -> std::convertible_to<splice_point>;
    typename Splice::operation_type;
  } &&
  ((Splice::point == splice_point::combine &&
     combinable<typename Splice::operation_type, Element>) ||
    (Splice::point == splice_point::key &&
      traceable<typename Splice::operation_type, std::uint32_t>) ||
    (Splice::point == splice_point::predicate &&
      traceable_predicate<typename Splice::operation_type, Element>) ||
    (Splice::point == splice_point::uniform_operation &&
      uniform_traceable<
        typename Splice::operation_type,
        Element,
        typename Splice::operation_type::uniform_type
      >) ||
    ((Splice::point == splice_point::operation ||
       Splice::point == splice_point::epilogue) &&
      traceable<typename Splice::operation_type, Element>));

template<class Element, class Splices>
inline constexpr bool is_splice_tuple {false};

template<class Element, class... Splices>
inline constexpr bool is_splice_tuple<Element, std::tuple<Splices...>> {
  (traceable_splice<Splices, Element> && ...),
};

template<class Dispatches>
inline constexpr bool is_dispatch_array {false};

template<std::size_t Count>
inline constexpr bool is_dispatch_array<std::array<dispatch_rule, Count>> {
  Count >= 1UZ,
};

template<class Splices>
struct splice_arguments;

template<class... Splices>
struct splice_arguments<std::tuple<Splices...>>
{
  using type = decltype(std::tuple_cat(
    std::declval<
      typename arguments_of<typename Splices::operation_type>::type
    >()...));
};

template<class Stage>
struct leading_arguments_of
{
  using type = std::tuple<>;
};

template<class Stage>
  requires requires { typename Stage::leading_arguments; }
struct leading_arguments_of<Stage>
{
  using type = typename Stage::leading_arguments;
};

template<class Stage>
inline constexpr std::size_t leading_slots =
  std::tuple_size_v<typename leading_arguments_of<Stage>::type>;

export template<class Stage>
inline constexpr workspace_sizes workspace_of {};

export template<class Stage>
  requires requires { Stage::workspace; }
inline constexpr workspace_sizes workspace_of<Stage> {Stage::workspace};

export template<class Stage, class Element>
concept archetype_stage =
  scalar<Element> &&
  requires {
    { Stage::kind } -> std::convertible_to<stage_class>;
    { Stage::skeleton } -> std::convertible_to<skeleton_kind>;
    { Stage::schedule } -> std::convertible_to<stage_schedule>;
    typename Stage::template ports<Element>;
    typename Stage::splices;
    typename std::remove_cvref_t<decltype(Stage::arguments)>;
    typename std::remove_cvref_t<decltype(Stage::dispatches)>;
  } &&
  is_port_tuple<typename Stage::template ports<Element>> &&
  is_splice_tuple<Element, typename Stage::splices> &&
  is_dispatch_array<std::remove_cvref_t<decltype(Stage::dispatches)>> &&
  std::same_as<
    std::remove_cvref_t<decltype(Stage::arguments)>,
    decltype(std::tuple_cat(
      std::declval<typename leading_arguments_of<Stage>::type>(),
      std::declval<typename splice_arguments<typename Stage::splices>::type>()))
  >;

export template<class Stage, class Element>
  requires archetype_stage<Stage, Element>
inline constexpr dispatch_push dispatch_push_of {
  .ports = static_cast<std::uint32_t>(
    std::tuple_size_v<typename Stage::template ports<Element>> +
    workspace_of<Stage>.region_count()),
  .arguments = static_cast<std::uint32_t>(
    std::tuple_size_v<std::remove_cvref_t<decltype(Stage::arguments)>>),
};

export template<class Stage, class Element>
  requires archetype_stage<Stage, Element>
inline constexpr dispatch_push skeleton_push_of {
  .ports = dispatch_push_of<Stage, Element>.ports,
  .arguments =
    dispatch_push_of<Stage, Element>.argument_words_within(skeleton_push_bytes),
};

template<scalar Element>
consteval
auto
element_words(
  std::span<const std::uint32_t> float_words,
  std::span<const std::uint32_t> int_words,
  std::span<const std::uint32_t> uint_words) -> std::span<const std::uint32_t>
{
  if constexpr (std::same_as<Element, float>) { return float_words; }
  else if constexpr (std::same_as<Element, std::int32_t>) { return int_words; }
  else
  {
    return uint_words;
  }
}

template<skeleton_kind Skeleton, scalar Element>
consteval
auto
skeleton_words() -> std::span<const std::uint32_t>
{
  if constexpr (Skeleton == skeleton_kind::transform)
  {
    return element_words<Element>(
      transform_float_words, transform_int_words, transform_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::transform_with)
  {
    return element_words<Element>(
      transform_with_float_words,
      transform_with_int_words,
      transform_with_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::reduce)
  {
    return element_words<Element>(
      reduce_float_words, reduce_int_words, reduce_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::inclusive_scan)
  {
    return element_words<Element>(
      scan_inclusive_float_words,
      scan_inclusive_int_words,
      scan_inclusive_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::exclusive_scan)
  {
    return element_words<Element>(
      scan_exclusive_float_words,
      scan_exclusive_int_words,
      scan_exclusive_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::scatter)
  {
    return element_words<Element>(
      scatter_float_words, scatter_int_words, scatter_uint_words);
  }
  else if constexpr (Skeleton == skeleton_kind::predicate)
  {
    return element_words<Element>(
      predicate_float_words, predicate_int_words, predicate_uint_words);
  }
  else
  {
    static_assert(
      std::same_as<Element, std::uint32_t>,
      "fiat histogram and scatter skeletons take std::uint32_t elements");
    if constexpr (Skeleton == skeleton_kind::histogram)
    {
      return histogram_uint_words;
    }
    else if constexpr (Skeleton == skeleton_kind::tile_histogram)
    {
      return tile_histogram_uint_words;
    }
    else
    {
      return stable_scatter_uint_words;
    }
  }
}

constexpr
auto
symbol_of(splice_point point) -> std::string_view
{
  switch (point)
  {
  case splice_point::operation         : return operation_symbol;
  case splice_point::combine           : return combine_symbol;
  case splice_point::epilogue          : return epilogue_symbol;
  case splice_point::key               : return key_symbol;
  case splice_point::predicate         : return predicate_symbol;
  case splice_point::uniform_operation : return operation_symbol;
  }
  std::unreachable();
}

template<class Splice>
inline constexpr std::size_t splice_slots = std::tuple_size_v<
  typename arguments_of<typename Splice::operation_type>::type
>;

template<class Splices, std::size_t... Earlier>
consteval
auto
first_slot(std::index_sequence<Earlier...>) -> std::size_t
{ return (0UZ + ... + splice_slots<std::tuple_element_t<Earlier, Splices>>); }

template<
  scalar Element,
  class Splice,
  std::size_t   FirstSlot,
  std::uint32_t ArgumentMember
>
constexpr
auto
trace_splice() -> std::vector<instruction>
{
  using operation_type = typename Splice::operation_type;
  if constexpr (Splice::point == splice_point::combine)
  {
    return detail::trace_binary<Element, operation_type>();
  }
  else if constexpr (Splice::point == splice_point::key)
  {
    return detail::trace_from_slot<
      std::uint32_t,
      operation_type,
      FirstSlot,
      ArgumentMember
    >();
  }
  else if constexpr (Splice::point == splice_point::predicate)
  {
    return trace_predicate<Element, operation_type, FirstSlot, ArgumentMember>();
  }
  else if constexpr (Splice::point == splice_point::uniform_operation)
  {
    static_assert(
      FirstSlot == 0UZ && ArgumentMember == uniform_argument_member,
      "a uniform operation reads its uniform from push_member 2 and its "
      "arguments from push member 5");
    return detail::trace_with_uniform<
      Element,
      typename operation_type::uniform_type,
      operation_type,
      ArgumentMember
    >();
  }
  else
  {
    return detail::trace_from_slot<
      Element,
      operation_type,
      FirstSlot,
      ArgumentMember
    >();
  }
}

template<class Stage>
using splice_positions =
  std::make_index_sequence<std::tuple_size_v<typename Stage::splices>>;

template<scalar Element, class Stage, std::size_t... Positions>
constexpr
auto
stage_module(std::index_sequence<Positions...>) -> std::vector<std::uint32_t>
{
  using splices = typename Stage::splices;
  [[maybe_unused]]
  const std::array<std::vector<instruction>, sizeof...(Positions)> traced {
    trace_splice<
      Element,
      std::tuple_element_t<Positions, splices>,
      leading_slots<Stage> +
        first_slot<splices>(std::make_index_sequence<Positions> {}),
      dispatch_push_of<Stage, Element>.argument_member()
    >()...,
  };
  const std::array<detail::symbol_trace, sizeof...(Positions)> traces {
    detail::symbol_trace {
      .symbol = symbol_of(std::tuple_element_t<Positions, splices>::point),
      .instructions = traced[ Positions ],
    }...,
  };
  return splice<Element>(skeleton_words<Stage::skeleton, Element>(), traces);
}

export template<scalar Element, archetype_stage<Element> Stage>
inline constexpr std::size_t stage_word_count =
  stage_module<Element, Stage>(splice_positions<Stage> {}).size();

export
template<scalar Element, archetype_stage<Element> Stage>
consteval auto
compile(const Stage&)
  -> std::array<std::uint32_t, stage_word_count<Element, Stage>>
{
  static_assert(
    Stage::schedule.group_size == 0U ||
      std::has_single_bit(Stage::schedule.group_size),
    "fiat stage group_size must be 0 (the context's) or a power of two");
  static_assert(
    Stage::schedule.items_per_invocation == 1U,
    "fiat stage items_per_invocation must be 1");
  static_assert(
    std::ranges::all_of(
      Stage::dispatches,
      [](const dispatch_rule& rule) -> bool {
        return rule.extent != dispatch_extent::grid_stride ||
               (rule.maximum_groups >= 1U && rule.maximum_groups <= 65'535U);
      }),
    "fiat grid_stride dispatches need maximum_groups in [1, 65535]");
  static_assert(
    dispatch_push_of<Stage, Element>.size() <= push_budget,
    "fiat stage ports and arguments must fit the 256-byte push budget");
  static_assert(
    dispatch_push_of<Stage, Element>.size() <= skeleton_push_bytes,
    "fiat stage ports and arguments must fit the 128-byte skeleton push block");
  const std::vector<std::uint32_t> words =
    stage_module<Element, Stage>(splice_positions<Stage> {});
  std::array<std::uint32_t, stage_word_count<Element, Stage>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
