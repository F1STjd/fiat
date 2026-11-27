export module fiat:graph;

import std;

import :trace;
import :stage;

namespace fiat
{

inline
void
graph_error([[maybe_unused]] std::string_view reason)
{}

export enum class graph_buffer_region : std::uint8_t
{
  external,
  table,
  ping_pong,
  scratch,
  unbound,
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

export template<scalar Element>
inline constexpr graph_buffer<Element> unbound_buffer {
  .region              = graph_buffer_region::unbound,
  .initially_available = false,
};

template<std::size_t Capacity>
class graph_message
{
public:
  constexpr
  void
  append(std::string_view text)
  {
    const auto copied = text.substr(0UZ, Capacity - size_);
    std::ranges::copy(copied, characters_.begin() + size_);
    size_ += copied.size();
  }

  constexpr
  void
  append_number(std::size_t number)
  {
    std::array<char, 20UZ> digits {};
    if (
      const auto [ end, _ ] =
        std::to_chars(digits.data(), digits.data() + digits.size(), number))
    {
      append(std::string_view {digits.data(), end});
    }
  }

  constexpr
  auto
  data() const -> const char*
  { return characters_.data(); }

  constexpr
  auto
  size() const -> std::size_t
  { return size_; }

private:
  std::array<char, Capacity> characters_ {};
  std::size_t                size_ {};
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
port_mismatch_message() -> graph_message<160UZ>
{
  graph_message<160> result {};
  result.append("fiat graph port ");
  result.append_number(Position);
  result.append(" expects ");
  result.append(scalar_name<Expected>);
  result.append(" but_received ");
  result.append(scalar_name<Actual>);
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

template<scalar Element, class Stage, class Buffers, std::size_t... Positions>
consteval
auto
make_node_impl(
  Stage stage, const Buffers& buffers, std::index_sequence<Positions...>)
  -> graph_node<Stage, Element>
{
  using ports = typename Stage::template ports<Element>;
  static_assert(
    archetype_stage<Stage, Element>,
    "fiat graph nodes need an archetype stage");
  static_assert(
    sizeof...(Positions) == std::tuple_size_v<ports>,
    "fiat graph node buffer count must equal its stage port count");
  (validate_port<
     Positions,
     std::tuple_element_t<Positions, ports>,
     std::tuple_element_t<Positions, Buffers>
   >(),
    ...);
  return {
    .stage       = std::move(stage),
    .bound_ports = {
      graph_port {
        .identity            = std::get<Positions>(buffers).identity,
        .allocation_identity = std::get<Positions>(buffers).allocation_identity,
        .bytes               = std::get<Positions>(buffers).bytes,
        .element_bytes =
          sizeof(typename std::tuple_element_t<Positions, ports>::element_type),
        .size                = std::tuple_element_t<Positions, ports>::size,
        .region              = std::get<Positions>(buffers).region,
        .access              = std::tuple_element_t<Positions, ports>::access,
        .initially_available = std::get<Positions>(buffers).initially_available,
      }...,
    },
  };
}

export
template<class Stage, class... Buffers>
  requires (sizeof...(Buffers) >= 1UZ)
consteval auto
make_node(Stage stage, Buffers... buffers)
{
  using buffer_types = std::tuple<Buffers...>;
  using element      = graph_element<
    Stage,
    typename std::tuple_element_t<0UZ, buffer_types>::element_type
  >;
  return make_node_impl<element>(
    std::move(stage),
    std::tuple {buffers...},
    std::make_index_sequence<sizeof...(Buffers)> {});
}

export struct graph_workspace_offsets
{
  std::size_t table {};
  std::size_t ping_pong {};
  std::size_t scratch {};

  constexpr
  auto
  operator ==(const graph_workspace_offsets&) const -> bool = default;
};

export struct graph_allocation
{
  std::size_t         identity {};
  graph_buffer_region region {};

  constexpr
  auto
  operator ==(const graph_allocation&) const -> bool = default;
};

export template<std::size_t NodeCount, std::size_t PortCount>
struct graph_analysis
{
  std::array<std::array<bool, NodeCount>, NodeCount> edges {};
  std::array<std::size_t, NodeCount>                 topological_order {};
  std::array<std::size_t, NodeCount>                 barrier_levels {};
  std::array<graph_workspace_offsets, NodeCount>     workspace_offsets {};
  std::array<std::size_t, PortCount>                 buffer_offsets {};
  workspace_sizes                                    requirements {};
  std::size_t                                        dispatch_count {};
  std::size_t                                        stage_barrier_count {};
  std::size_t                                        barrier_count {};
  std::size_t                                        ping_pong_pass_count {};
  std::optional<graph_allocation>                    final_allocation {};
  std::size_t                                        pass_count {1UZ};
  std::size_t pass_node_count {NodeCount};

  constexpr
  auto
  operator ==(const graph_analysis&) const -> bool = default;
};

constexpr
auto
reads(port_access access) -> bool
{ return access != port_access::write; }

constexpr
auto
writes(port_access access) -> bool
{ return access != port_access::read; }

constexpr
auto
bound(const graph_port& port) -> bool
{ return port.region != graph_buffer_region::unbound; }

template<std::size_t NodeCount, std::size_t PortCount>
consteval
void
analyze_dependencies(
  graph_analysis<NodeCount, PortCount>&     result,
  const std::array<std::size_t, NodeCount>& first_ports,
  const std::array<std::size_t, NodeCount>& port_counts,
  const std::array<std::size_t, NodeCount>& dispatch_counts,
  const std::array<graph_port, PortCount>&  ports)
{
  const auto node_ports =
    [ & ](std::size_t node) -> std::span<const graph_port> {
    return std::span {ports}.subspan(first_ports[ node ], port_counts[ node ]);
  };

  constexpr auto node_indices = std::views::indices(NodeCount);

  for (const auto node : node_indices)
  {
    for (const auto& port : node_ports(node))
    {
      if (!bound(port) || port.initially_available) { continue; }
      const auto writes_buffer = [ & ](const graph_port& other) -> bool {
        return bound(other) &&
               other.identity == port.identity &&
               writes(other.access);
      };
      if (writes(port.access) && std::ranges::count_if(ports, writes_buffer) > 1)
      {
        graph_error("graph buffer has more than one writer");
      }
      if (
        reads(port.access) &&
        std::ranges::none_of(node_indices, [ & ](std::size_t producer) -> bool {
          return producer != node &&
                 std::ranges::any_of(node_ports(producer), writes_buffer);
        }))
      {
        graph_error("graph reads a buffer that no other node writes");
      }
    }
  }

  for (const auto source : node_indices)
  {
    for (const auto target : node_indices)
    {
      if (source == target) { continue; }
      result.edges[ source ][ target ] = std::ranges::any_of(
        node_ports(source), [ & ](const graph_port& first) -> bool {
          if (!bound(first)) { return false; }
          return std::ranges::any_of(
            node_ports(target), [ & ](const graph_port& second) -> bool {
              if (!bound(second)) { return false; }
              const bool logical_dependency =
                first.identity == second.identity &&
                ((!first.initially_available &&
                   writes(first.access) &&
                   reads(second.access)) ||
                  (first.initially_available &&
                    source < target &&
                    (writes(first.access) || writes(second.access))));
              const bool physical_write_after_read =
                source < target &&
                first.identity != second.identity &&
                first.allocation_identity == second.allocation_identity &&
                first.region == second.region &&
                reads(first.access) &&
                writes(second.access);
              return logical_dependency || physical_write_after_read;
            });
        });
    }
  }

  std::array<bool, NodeCount> emitted {};
  for (const auto position : node_indices)
  {
    const auto found =
      std::ranges::find_if(node_indices, [ & ](std::size_t candidate) -> bool {
        return !emitted[ candidate ] &&
               std::ranges::all_of(
                 node_indices, [ & ](std::size_t predecessor) -> bool {
                   return !result.edges[ predecessor ][ candidate ] ||
                          emitted[ predecessor ];
                 });
      });
    if (found == node_indices.end())
    {
      graph_error("graph contains a dependency cycle");
    }
    result.topological_order[ position ] = *found;
    emitted[ *found ]                    = true;
  }

  for (const auto node : result.topological_order)
  {
    for (const auto predecessor : node_indices)
    {
      if (result.edges[ predecessor ][ node ])
      {
        result.barrier_levels[ node ] = std::max(
          result.barrier_levels[ node ],
          result.barrier_levels[ predecessor ] + 1UZ);
      }
    }
  }
  result.stage_barrier_count = std::ranges::max(result.barrier_levels);
  result.dispatch_count =
    std::ranges::fold_left(dispatch_counts, 0UZ, std::plus {});
  result.barrier_count =
    std::ranges::fold_left(
      dispatch_counts,
      0UZ,
      [](std::size_t count, std::size_t dispatches) -> std::size_t {
        return count + dispatches - 1UZ;
      }) +
    result.stage_barrier_count;
}

struct ping_pong_lane
{
  graph_allocation source {};
  graph_allocation exchange {};
};

template<std::size_t PortCount>
struct ping_pong_lanes
{
  std::array<ping_pong_lane, PortCount> lanes {};
  std::size_t                           count {};
};

constexpr
auto
in_lane(const ping_pong_lane& lane, const graph_port& port) -> bool
{
  return (port.region == lane.source.region &&
           port.allocation_identity == lane.source.identity) ||
         (port.region == lane.exchange.region &&
           port.allocation_identity == lane.exchange.identity);
}

template<std::size_t NodeCount, std::size_t PortCount>
consteval
auto
pair_ping_pong_lanes(
  const std::array<graph_port, PortCount>&  ports,
  const std::array<std::size_t, NodeCount>& topological_order,
  const std::array<std::size_t, NodeCount>& first_ports,
  const std::array<std::size_t, NodeCount>& port_counts)
  -> ping_pong_lanes<PortCount>
{
  std::array<std::size_t, PortCount> exchange_ports {};
  std::size_t                        lane_count {};
  for (const auto position : std::views::indices(PortCount))
  {
    const auto& port = ports[ position ];
    if (port.region != graph_buffer_region::ping_pong) { continue; }
    const bool seen = std::ranges::any_of(
      std::span {exchange_ports}.first(lane_count),
      [ & ](std::size_t existing) -> bool {
        return ports[ existing ].allocation_identity == port.allocation_identity;
      });
    if (!seen) { exchange_ports[ lane_count++ ] = position; }
  }

  const auto node_ports =
    [ & ](std::size_t node) -> std::span<const graph_port> {
    return std::span<const graph_port> {ports}.subspan(
      first_ports[ node ], port_counts[ node ]);
  };
  ping_pong_lanes<PortCount> result {.count = lane_count};
  for (const auto lane_index : std::views::indices(lane_count))
  {
    const graph_port& exchange  = ports[ exchange_ports[ lane_index ] ];
    const auto        is_source = [ & ](const graph_port& port) -> bool {
      return port.initially_available &&
             port.region == graph_buffer_region::external &&
             reads(port.access) &&
             port.bytes == exchange.bytes;
    };
    const auto writer =
      std::ranges::find_if(topological_order, [ & ](std::size_t node) -> bool {
        return std::ranges::any_of(
          node_ports(node), [ & ](const graph_port& port) -> bool {
            return port.region == graph_buffer_region::ping_pong &&
                   port.allocation_identity == exchange.allocation_identity &&
                   writes(port.access);
          });
      });

    std::optional<graph_port> paired_source {};
    if (
      writer != topological_order.end() &&
      std::ranges::any_of(node_ports(*writer), is_source))
    {
      const auto  writer_ports = node_ports(*writer);
      std::size_t exchange_rank {};
      for (const auto& port : writer_ports)
      {
        if (port.region == graph_buffer_region::ping_pong && writes(port.access))
        {
          if (port.allocation_identity == exchange.allocation_identity)
          {
            break;
          }
          ++exchange_rank;
        }
      }

      std::size_t source_rank {};
      for (const auto& port : writer_ports)
      {
        if (is_source(port))
        {
          if (source_rank == exchange_rank)
          {
            paired_source = port;
            break;
          }
          ++source_rank;
        }
      }
    }
    else
    {
      const auto source = std::ranges::find_if(ports, is_source);
      if (source != ports.end())
      {
        const bool one_source =
          std::ranges::all_of(ports, [ & ](const graph_port& port) -> bool {
            return !is_source(port) ||
                   port.allocation_identity == source->allocation_identity;
          });
        if (!one_source) { graph_error("graph ping-pong source is ambiguous"); }
        paired_source = *source;
      }
    }
    if (!paired_source.has_value())
    {
      graph_error("graph ping-pong lane has no initially available source");
    }

    result.lanes[ lane_index ] = ping_pong_lane {
      .source =
        graph_allocation {
          .identity = paired_source->allocation_identity,
          .region   = graph_buffer_region::external,
        },
      .exchange = graph_allocation {
        .identity = exchange.allocation_identity,
        .region   = graph_buffer_region::ping_pong,
      },
    };
  }
  return result;
}

template<class... Nodes>
consteval
auto
analyze(const std::tuple<Nodes...>& all_nodes)
  -> graph_analysis<sizeof...(Nodes), (Nodes::port_count + ...)>
{
  constexpr std::size_t                  node_count {sizeof...(Nodes)};
  constexpr std::size_t                  port_count {(Nodes::port_count + ...)};
  graph_analysis<node_count, port_count> result {};
  std::array<std::size_t, node_count>    first_ports {};
  std::array<std::size_t, node_count>    port_counts {};
  std::array<std::size_t, node_count>    dispatch_counts {};
  std::array<workspace_sizes, node_count> workspaces {};
  std::array<graph_port, port_count>      ports {};
  std::size_t                             node_position {};
  std::size_t                             port_position {};
  std::apply(
    [ & ](const Nodes&... node) {
      ((first_ports[ node_position ]  = port_position,
         port_counts[ node_position ] = Nodes::port_count,
         dispatch_counts[ node_position ] = Nodes::stage_type::dispatches.size(),
         workspaces[ node_position++ ] =
           workspace_of<typename Nodes::stage_type>,
         std::ranges::copy(node.bound_ports, ports.begin() + port_position),
         port_position += Nodes::port_count),
        ...);
    },
    all_nodes);
  analyze_dependencies(
    result, first_ports, port_counts, dispatch_counts, ports);

  std::array<std::size_t, 3UZ> explicit_bytes {};
  for (const auto position : std::views::indices(port_count))
  {
    const auto& port = ports[ position ];
    if (!bound(port)) { continue; }
    const auto prior_ports = std::span {ports}.first(position);
    if (port.size != 0U && port.bytes != port.size * port.element_bytes)
    {
      graph_error("graph port bytes differ from its fixed stage size");
    }
    if (std::ranges::any_of(prior_ports, [ & ](graph_port prior) -> bool {
          return bound(prior) &&
                 prior.identity == port.identity &&
                 prior.bytes != port.bytes;
        }))
    {
      graph_error("graph ports of one logical buffer differ in bytes");
    }
    if (port.region != graph_buffer_region::external && port.initially_available)
    {
      graph_error("graph workspace buffers cannot be initially available");
    }
    const auto found =
      std::ranges::find_if(prior_ports, [ & ](graph_port prior) -> bool {
        return bound(prior) &&
               prior.allocation_identity == port.allocation_identity &&
               prior.region == port.region;
      });
    if (found != prior_ports.end())
    {
      const auto prior = static_cast<std::size_t>(
        std::ranges::distance(prior_ports.begin(), found));
      result.buffer_offsets[ position ] = result.buffer_offsets[ prior ];
    }
    else if (port.region != graph_buffer_region::external)
    {
      const auto region = std::to_underlying(port.region) - 1UZ;
      auto       matching_ports =
        ports | std::views::filter([ & ](const graph_port& candidate) -> bool {
          return bound(candidate) &&
                 candidate.allocation_identity == port.allocation_identity &&
                 candidate.region == port.region;
        });
      const auto allocation_bytes = std::ranges::max(
        matching_ports | std::views::transform(&graph_port::bytes));
      const auto alignment = std::max(
        1UZ,
        std::ranges::max(
          matching_ports | std::views::transform(&graph_port::element_bytes)));
      const auto padded =
        std::saturating_add(explicit_bytes[ region ], alignment - 1UZ);
      const auto aligned_offset         = padded - padded % alignment;
      result.buffer_offsets[ position ] = aligned_offset;
      explicit_bytes[ region ] =
        std::saturating_add(aligned_offset, allocation_bytes);
    }
  }

  std::array<std::array<std::size_t, 3UZ>, node_count> level_bytes {};
  for (const auto node : result.topological_order)
  {
    const auto level                 = result.barrier_levels[ node ];
    const auto regions               = workspaces[ node ].regions();
    result.workspace_offsets[ node ] = {
      .table     = explicit_bytes[ 0UZ ] + level_bytes[ level ][ 0UZ ],
      .ping_pong = explicit_bytes[ 1UZ ] + level_bytes[ level ][ 1UZ ],
      .scratch   = explicit_bytes[ 2UZ ] + level_bytes[ level ][ 2UZ ],
    };
    for (const auto region : std::views::indices(3UZ))
    {
      level_bytes[ level ][ region ] += regions[ region ];
    }
  }

  std::array<std::size_t, 3UZ> stage_bytes {};
  for (const auto& bytes : level_bytes)
  {
    for (const auto region : std::views::indices(3UZ))
    {
      stage_bytes[ region ] = std::max(stage_bytes[ region ], bytes[ region ]);
    }
  }
  result.requirements = {
    .table_bytes     = explicit_bytes[ 0UZ ] + stage_bytes[ 0UZ ],
    .ping_pong_bytes = explicit_bytes[ 1UZ ] + stage_bytes[ 1UZ ],
    .scratch_bytes   = explicit_bytes[ 2UZ ] + stage_bytes[ 2UZ ],
  };

  const auto lanes = pair_ping_pong_lanes(
    ports, result.topological_order, first_ports, port_counts);
  if (lanes.count == 0UZ) { return result; }

  const auto lane_writes = [ & ](const ping_pong_lane& lane) -> std::size_t {
    return static_cast<
      std::size_t
    >(std::ranges::count_if(ports, [ & ](const graph_port& port) -> bool {
      return writes(port.access) && in_lane(lane, port);
    }));
  };
  const auto& first_lane      = lanes.lanes[ 0UZ ];
  result.ping_pong_pass_count = lane_writes(first_lane);
  const bool ends_on_source   = result.ping_pong_pass_count % 2UZ == 0UZ;
  result.final_allocation =
    ends_on_source ? first_lane.source : first_lane.exchange;
  for (
    const auto& lane : std::span {lanes.lanes}.subspan(1UZ, lanes.count - 1UZ))
  {
    if ((lane_writes(lane) % 2UZ == 0UZ) != ends_on_source)
    {
      graph_error("graph ping-pong lanes end on different sides");
    }
  }

  return result;
}

export template<class... Nodes>
struct graph
{
  std::tuple<Nodes...>                                        nodes;
  graph_analysis<sizeof...(Nodes), (Nodes::port_count + ...)> analysis;
};

export
template<class... Nodes>
  requires (sizeof...(Nodes) >= 1UZ)
consteval auto
make_graph(Nodes... nodes) -> graph<Nodes...>
{
  const std::tuple all_nodes {std::move(nodes)...};
  return {.nodes = all_nodes, .analysis = analyze(all_nodes)};
}

template<class Type>
inline constexpr bool is_graph_node {false};

template<class Stage, scalar Element>
inline constexpr bool is_graph_node<graph_node<Stage, Element>> {true};

template<class Type>
inline constexpr bool is_pass_node_tuple {false};

template<class... PassNodes>
  requires (sizeof...(PassNodes) >= 1UZ && (is_graph_node<PassNodes> && ...))
inline constexpr bool is_pass_node_tuple<std::tuple<PassNodes...>> {true};

template<std::size_t Count, class... PassNodes>
consteval
auto
flatten_passes(const std::array<std::tuple<PassNodes...>, Count>& passes)
{
  constexpr std::size_t pass_node_count {sizeof...(PassNodes)};
  return
    [ & ]<std::size_t... FlatIndices>(
      std::index_sequence<FlatIndices...>) -> auto {
      return std::tuple<std::tuple_element_t<
        FlatIndices % pass_node_count,
        std::tuple<PassNodes...>
      >...> {std::get<FlatIndices % pass_node_count>(passes
          [ FlatIndices / pass_node_count ])...};
    }(std::make_index_sequence<Count * pass_node_count> {});
}

template<std::size_t Count, class... PassNodes>
consteval
auto
collect_pass_ports(const std::array<std::tuple<PassNodes...>, Count>& passes)
  -> std::array<graph_port, Count*(PassNodes::port_count + ...)>
{
  constexpr std::size_t pass_port_count {(PassNodes::port_count + ...)};
  std::array<graph_port, Count * pass_port_count> ports {};
  std::size_t                                     offset {};
  for (const auto pass : std::views::indices(Count))
  {
    std::apply(
      [ & ](const PassNodes&... node) -> void {
        ((std::ranges::copy(node.bound_ports, ports.begin() + offset),
           offset += PassNodes::port_count),
          ...);
      },
      passes[ pass ]);
  }
  return ports;
}

template<std::size_t Count, std::size_t PassPortCount>
consteval
void
validate_repeat_psses(
  const std::array<graph_port, Count * PassPortCount>& ports)
{
  if constexpr (Count <= 1UZ) { return; }

  const auto pass_port =
    [ & ](std::size_t pass, std::size_t port_index) -> const graph_port& {
    return ports[ pass * PassPortCount + port_index ];
  };

  bool        has_stride {false};
  std::size_t stride {};
  for (const auto port_index : std::views::indices(PassPortCount))
  {
    const auto& first  = pass_port(0UZ, port_index);
    const auto& second = pass_port(1UZ, port_index);
    if (bound(first) != bound(second))
    {
      graph_error("graph repeat passes do not repeat with period two");
    }
    if (bound(first))
    {
      if (second.identity <= first.identity)
      {
        graph_error("graph repeat passes use different identity strides");
      }
      stride     = second.identity - first.identity;
      has_stride = true;
      break;
    }
  }

  if (has_stride)
  {
    std::size_t lowest {std::numeric_limits<std::size_t>::max()};
    std::size_t highest {};
    for (const auto port_index : std::views::indices(PassPortCount))
    {
      const auto& port = pass_port(0UZ, port_index);
      if (!bound(port)) { continue; }
      lowest  = std::min(lowest, port.identity);
      highest = std::max(highest, port.identity);
    }
    if (highest - lowest >= std::saturating_mul(2UZ, stride))
    {
      graph_error("graph repeat pass identities reach beyond the next pass");
    }
  }

  for (const auto pass : std::views::iota(1UZ, Count))
  {
    for (const auto port_index : std::views::indices(PassPortCount))
    {
      const auto& previous = pass_port(pass - 1UZ, port_index);
      const auto& current  = pass_port(pass, port_index);
      if (bound(previous) != bound(current))
      {
        graph_error("graph repeat passes do not repeat with period two");
      }
      if (
        has_stride &&
        bound(current) &&
        (current.identity <= previous.identity ||
          current.identity - previous.identity != stride))
      {
        graph_error("graph repeat passes use different identity strides");
      }
      if (pass >= 2UZ)
      {
        const auto& prior_two = pass_port(pass - 2UZ, port_index);
        const bool  same_period_two =
          current.allocation_identity == prior_two.allocation_identity &&
          current.region == prior_two.region &&
          current.bytes == prior_two.bytes &&
          current.access == prior_two.access &&
          current.element_bytes == prior_two.element_bytes &&
          current.size == prior_two.size &&
          (pass == 2UZ ||
            current.initially_available == prior_two.initially_available);
        if (!same_period_two)
        {
          graph_error("graph repeat passes do not repeat with period two");
        }
      }
    }
  }
}

template<std::size_t Count, class... PassNodes>
  requires (Count > 6UZ)
consteval
auto
analyze_repeated(const std::array<std::tuple<PassNodes...>, Count>& passes)
  -> graph_analysis<
    Count * sizeof...(PassNodes),
    Count*(PassNodes::port_count + ...)
  >
{
  constexpr std::size_t window_pass_count {6UZ};
  constexpr std::size_t pass_node_count {sizeof...(PassNodes)};
  constexpr std::size_t pass_port_count {(PassNodes::port_count + ...)};
  constexpr std::size_t pass_dispatch_count {(
    PassNodes::stage_type::dispatches.size() + ...)};
  constexpr std::size_t pass_intra_barrier_count {(
    (PassNodes::stage_type::dispatches.size() - 1UZ) + ...)};

  const std::array<std::tuple<PassNodes...>, window_pass_count> window_passes {
    passes[ 0UZ ],
    passes[ 1UZ ],
    passes[ 2UZ ],
    passes[ 3UZ ],
    passes[ 4UZ ],
    passes[ 5UZ ],
  };
  const auto window = analyze(flatten_passes(window_passes));

  for (const auto source_pass : std::views::indices(window_pass_count))
  {
    for (const auto target_pass : std::views::indices(source_pass))
    {
      for (const auto source_local : std::views::indices(pass_node_count))
      {
        for (const auto target_local : std::views::indices(pass_node_count))
        {
          if (
            window.edges
              [ source_pass * pass_node_count + source_local ]
              [ target_pass * pass_node_count + target_local ])
          {
            graph_error("graph repeat pass depends on a later pass");
          }
        }
      }
    }
  }

  const std::size_t level_delta =
    window.barrier_levels[ pass_node_count ] - window.barrier_levels[ 0UZ ];
  for (const auto pass : std::views::iota(1UZ, window_pass_count))
  {
    std::size_t previous_maximum {};
    std::size_t current_minimum {std::numeric_limits<std::size_t>::max()};
    for (const auto local : std::views::indices(pass_node_count))
    {
      const std::size_t previous_level =
        window.barrier_levels[ (pass - 1UZ) * pass_node_count + local ];
      const std::size_t current_level =
        window.barrier_levels[ pass * pass_node_count + local ];
      previous_maximum = std::max(previous_maximum, previous_level);
      current_minimum  = std::min(current_minimum, current_level);
      if (
        current_level <= previous_level ||
        current_level - previous_level != level_delta)
      {
        graph_error("graph repeat passes overlap in barrier levels");
      }
    }
    if (current_minimum <= previous_maximum)
    {
      graph_error("graph repeat passes overlap in barrier levels");
    }
  }

  graph_analysis<Count * pass_node_count, Count * pass_port_count> result {};
  result.pass_count      = Count;
  result.pass_node_count = pass_node_count;
  for (const auto source_pass : std::views::indices(Count))
  {
    const std::size_t class_source =
      source_pass == 0UZ ? 0UZ : (source_pass % 2UZ == 1UZ ? 1UZ : 2UZ);
    for (const auto target_pass : std::views::iota(source_pass, Count))
    {
      const std::size_t distance = target_pass - source_pass;
      const std::size_t class_target =
        distance == 0UZ
          ? class_source
          : (distance == 1UZ
                ? class_source + 1UZ
                : class_source + (distance % 2UZ == 0UZ ? 2UZ : 3UZ));
      for (const auto source_local : std::views::indices(pass_node_count))
      {
        for (const auto target_local : std::views::indices(pass_node_count))
        {
          result.edges
            [ source_pass * pass_node_count + source_local ]
            [ target_pass * pass_node_count + target_local ] =
            window.edges
              [ class_source * pass_node_count + source_local ]
              [ class_target * pass_node_count + target_local ];
        }
      }
    }
  }

  std::array<std::array<std::size_t, pass_node_count>, 3UZ> class_orders {};
  for (const auto class_pass : std::views::indices(3UZ))
  {
    std::size_t count {};
    for (const auto node : window.topological_order)
    {
      if (node / pass_node_count == class_pass)
      {
        class_orders[ class_pass ][ count++ ] = node % pass_node_count;
      }
    }
  }

  for (const auto pass : std::views::indices(Count))
  {
    const std::size_t class_pass =
      pass == 0UZ ? 0UZ : (pass % 2UZ == 1UZ ? 1UZ : 2UZ);
    for (const auto local_position : std::views::indices(pass_node_count))
    {
      result.topological_order[ pass * pass_node_count + local_position ] =
        pass * pass_node_count + class_orders[ class_pass ][ local_position ];
    }
    for (const auto local : std::views::indices(pass_node_count))
    {
      result.barrier_levels[ pass * pass_node_count + local ] =
        window.barrier_levels[ class_pass * pass_node_count + local ] +
        (pass - class_pass) * level_delta;
      result.workspace_offsets[ pass * pass_node_count + local ] =
        window.workspace_offsets[ class_pass * pass_node_count + local ];
    }
    for (const auto port_index : std::views::indices(pass_port_count))
    {
      result.buffer_offsets[ pass * pass_port_count + port_index ] =
        window.buffer_offsets[ class_pass * pass_port_count + port_index ];
    }
  }

  result.requirements        = window.requirements;
  result.stage_barrier_count = std::ranges::max(result.barrier_levels);
  result.dispatch_count      = Count * pass_dispatch_count;
  result.barrier_count =
    Count * pass_intra_barrier_count + result.stage_barrier_count;

  const auto            window_ports = collect_pass_ports(window_passes);
  constexpr std::size_t window_node_count {window_pass_count * pass_node_count};
  constexpr std::array<std::size_t, pass_node_count> pass_port_counts {
    PassNodes::port_count...,
  };
  std::array<std::size_t, window_node_count> window_first_ports {};
  std::array<std::size_t, window_node_count> window_port_counts {};
  std::size_t                                window_port_position {};
  for (const auto node : std::views::indices(window_node_count))
  {
    window_first_ports[ node ]  = window_port_position;
    window_port_counts[ node ]  = pass_port_counts[ node % pass_node_count ];
    window_port_position       += window_port_counts[ node ];
  }

  const auto lanes = pair_ping_pong_lanes(
    window_ports,
    window.topological_order,
    window_first_ports,
    window_port_counts);
  if (lanes.count == 0UZ) { return result; }

  const auto lane_writes = [ & ](const ping_pong_lane& lane) -> std::size_t {
    const auto pass_writes = [ & ](std::size_t pass) -> std::size_t {
      const auto span = std::span {window_ports}.subspan(
        pass * pass_port_count, pass_port_count);
      return static_cast<
        std::size_t
      >(std::ranges::count_if(span, [ & ](const graph_port& port) -> bool {
        return writes(port.access) && in_lane(lane, port);
      }));
    };
    return ((Count + 1UZ) / 2UZ) * pass_writes(0UZ) +
           (Count / 2UZ) * pass_writes(1UZ);
  };
  const auto& first_lane      = lanes.lanes[ 0UZ ];
  result.ping_pong_pass_count = lane_writes(first_lane);
  const bool ends_on_source   = result.ping_pong_pass_count % 2UZ == 0UZ;
  result.final_allocation =
    ends_on_source ? first_lane.source : first_lane.exchange;

  for (
    const auto& lane : std::span {lanes.lanes}.subspan(1UZ, lanes.count - 1UZ))
  {
    if ((lane_writes(lane) % 2UZ) != ends_on_source)
    {
      graph_error("graph ping-pong lanes end on differnet sides");
    }
  }
  return result;
}

export
template<std::size_t Count, class PassBuilder>
  requires (Count >= 1UZ && Count <= std::numeric_limits<std::uint32_t>::max())
consteval auto
repeat(PassBuilder build_pass)
{
  using pass_tuple = std::remove_cvref_t<decltype(build_pass(0U))>;
  static_assert(
    is_pass_node_tuple<pass_tuple>,
    "fiat::repeat pass builder must return a non-empty std::tuple of "
    "graph_node");

  std::array<pass_tuple, Count> passes {};
  for (const auto pass : std::views::indices(static_cast<std::uint32_t>(Count)))
  {
    passes[ pass ] = build_pass(pass);
  }

  const auto all_ports = collect_pass_ports(passes);
  validate_repeat_psses<Count, std::tuple_size_v<decltype(all_ports)> / Count>(
    all_ports);

  auto all_nodes = flatten_passes(passes);
  if constexpr (Count <= 6UZ)
  {
    auto analysis            = analyze(all_nodes);
    analysis.pass_count      = Count;
    analysis.pass_node_count = std::tuple_size_v<pass_tuple>;
    return graph {
      .nodes    = std::move(all_nodes),
      .analysis = std::move(analysis),
    };
  }
  else
  {
    return graph {
      .nodes    = std::move(all_nodes),
      .analysis = analyze_repeated(passes),
    };
  }
}

} // namespace fiat
