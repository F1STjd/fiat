export module fiat:graph;

import std;

import :trace;
import :stage;

namespace fiat
{

export enum class graph_buffer_region : std::uint8_t
{
  external,
  table,
  ping_pong,
  scratch,
};

export template<scalar Element>
struct graph_buffer
{
  using element_type = Element;

  std::size_t         identity {};
  std::size_t         allocation_identity {};
  std::size_t         bytes {};
  graph_buffer_region region {graph_buffer_region::external};
  bool                initially_available {true};
};

template<std::size_t Capacity>
struct graph_message
{
  std::array<char, Capacity> characters {};
  std::size_t                length {};

  constexpr
  auto
  data() const -> const char*
  { return characters.data(); }

  constexpr
  auto
  size() const -> std::size_t
  { return length; }
};

template<scalar Element>
inline constexpr std::string_view scalar_name =
  std::same_as<Element, float> ? "float"
  : std::same_as<Element, std::int32_t>
    ? "std::int32_t"
    : "std::uint32_t";

template<std::size_t Position, scalar Expected, scalar Actual>
consteval
auto
port_mismatch_message() -> graph_message<96UZ>
{
  graph_message<96> result {};
  auto              destination = result.characters.begin();
  const auto        append      = [ & ](std::string_view text) {
    destination = std::ranges::copy(text, destination).out;
  };
  append("fiat graph port ");
  [[maybe_unused]] const auto [ end, conversion_error ] =
    std::to_chars(destination, result.characters.end(), Position);
  destination = end;
  append(" expects ");
  append(scalar_name<Expected>);
  append(" but received ");
  append(scalar_name<Actual>);
  result.length =
    static_cast<std::size_t>(destination - result.characters.begin());
  return result;
}

template<std::size_t Position, class Port, class Buffer>
consteval
void
validate_port()
{
  static_assert(
    std::same_as<typename Port::element_type, typename Buffer::element_type>,
    port_mismatch_message<
      Position,
      typename Port::element_type,
      typename Buffer::element_type
    >());
}

template<class Stage, scalar Default>
using graph_element = std::conditional_t<
  std::size_t {archetype_stage<Stage, float>} +
      std::size_t {archetype_stage<Stage, std::int32_t>} +
      std::size_t {archetype_stage<Stage, std::uint32_t>} ==
    1UZ,
  typename std::conditional_t<
    archetype_stage<Stage, float>,
    float,
    std::conditional_t<
      archetype_stage<Stage, std::int32_t>,
      std::int32_t,
      std::uint32_t
    >
  >,
  Default
>;

export struct graph_port
{
  std::size_t         identity {};
  std::size_t         allocation_identity {};
  std::size_t         bytes {};
  std::size_t         element_bytes {};
  std::uint32_t       size {};
  graph_buffer_region region {};
  port_access         access {};
  bool                initially_available {};
};

export template<class Stage, scalar Element>
struct graph_node
{
  using stage_type = Stage;
  using ports      = typename Stage::template ports<Element>;

  static constexpr std::size_t port_count {std::tuple_size_v<ports>};

  Stage                              stage;
  std::array<graph_port, port_count> bound_ports;
};

} // namespace fiat
