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

export template<scalar Element, port_access Access>
struct port
{
  using element_type = Element;
  static constexpr port_access access {Access};
};

export struct stage_schedule
{
  std::uint32_t group_size {};
  std::uint32_t items_per_invocation {1U};
};

export enum class skeleton_kind : std::uint8_t
{
  transform,
  reduce,
  inclusive_scan,
  exclusive_scan,
  histogram,
  tile_histogram,
  stable_scatter,
};

export enum class splice_point : std::uint8_t
{
  operation,
  combine,
  epilogue,
  key,
};

export template<splice_point Point, class Operation>
struct splice_operation
{
  static constexpr splice_point point {Point};
  using operation_type = Operation;
};

export inline constexpr std::uint32_t push_budget {256U};

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

template<scalar Element, port_access Access>
inline constexpr bool is_port<port<Element, Access>> {true};

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
    ((Splice::point == splice_point::operation ||
       Splice::point == splice_point::epilogue) &&
      traceable<typename Splice::operation_type, Element>));

template<class Element, class Splices>
inline constexpr bool is_splice_tuple {false};

template<class Element, class... Splices>
inline constexpr bool is_splice_tuple<Element, std::tuple<Splices...>> {
  sizeof...(Splices) >= 1UZ && (traceable_splice<Splices, Element> && ...),
};

template<class Splices>
struct splice_arguments;

template<class... Splices>
struct splice_arguments<std::tuple<Splices...>>
{
  using type = decltype(std::tuple_cat(
    stddeclval<
      typename arguments_of<typename Splices::operation_type>::type
    >()...));
};

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
  } &&
  is_port_tuple<typename Stage::template ports<Element>> &&
  is_splice_tuple<Element, typename Stage::splices> &&
  std::same_as<
    std::remove_cvref_t<decltype(Stage::arguments)>,
    typename splice_arguments<typename Stage::splices>::type
  >;

export template<class Stage, class Element>
  requires archetype_stage<Stage, Element>
inline constexpr dispatch_push dispatch_push_of {
  .ports = static_cast<std::uint32_t>(
    std::tuple_size_v<typename Stage::template ports<Element>>),
  .arguments = static_cast<std::uint32_t>(
    std::tuple_size_v<std::remove_cvref_t<decltype(Stage::arguments)>>),
};

template<skeleton_kind Skeleton, scalar Element>
consteval
auto
skeleton_words() -> std::span<const std::uint32_t>
{
  if constexpr (Skeleton == skeleton_kind::transform)
  {
    return transform_template<Element>();
  }
  else if constexpr (Skeleton == skeleton_kind::reduce)
  {
    return reduce_template<Element>();
  }
  else if constexpr (Skeleton == skeleton_kind::inclusive_scan)
  {
    return scan_template<Element, scan_mode::inclusive>();
  }
  else if constexpr (Skeleton == skeleton_kind::exclusive_scan)
  {
    return scan_template<Element, scan_mode::exclusive>();
  }
  else
  {
    static_assert(
      std::same_as<Element, std::uint32_t>,
      "fiat histogram and scatter skeletons take std::uint32_t elements");
    if constexpr (Skeleton == skeleton_kind::histogram)
    {
      return histogram_template();
    }
    else if constexpr (Skeleton == skeleton_kind::tile_histogram)
    {
      return tile_histogram_template();
    }
    else
    {
      return stable_scatter_template();
    }
  }
}

constexpr
auto
symbol_of(splice_point point) -> std::string_view
{
  switch (point)
  {
  case splice_point::operation : return operation_symbol;
  case splice_point::combine   : return combine_symbol;
  case splice_point::epilogue  : return epilogue_symbol;
  case splice_point::key       : return key_symbol;
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

template<scalar Element, class Splice, std::size_t FirstSlot>
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
    return detail::trace_from_slot<std::uint32_t, operation_type, FirstSlot>();
  }
  else
  {
    return detail::trace_from_slot<Element, operation_type, FirstSlot>();
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
  const std::array<std::vector<instruction>, sizeof...(Positions)> traced {
    trace_splice<
      Element,
      std::tuple_element_t<Positions, splices>,
      first_slot<splices>(std::make_index_sequence<Positions> {})
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
    Stage::schedule.group_size,
    "fiat stage group_size must be 0 (the context's) or a power of two");
  static_assert(
    Stage::schedule.items_per_invocation == 1U,
    "fiat stage items_per_invocation must be 1");
  static_assert(
    dispatch_push_of<Stage, Element>.size() <= push_budget,
    "fiat stage and arguments must fit the 256-byte push budget");
  const std::vector<std::uint32_t> words =
    stage_module<Element, Stage>(splice_positions<Stage> {});
  std::array<std::uint32_t, stage_word_count<Element, Stage>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
