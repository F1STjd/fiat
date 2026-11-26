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
      if (bound(port) || port.initially_available) { continue; }
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

  const auto exchange = std::ranges::find_if(ports, [](const graph_port& port) {
    return port.region == graph_buffer_region::ping_pong;
  });
  if (exchange == ports.end()) { return result; }

  const bool one_exchange =
    std::ranges::all_of(ports, [ & ](const graph_port& port) -> bool {
      return port.region != graph_buffer_region::ping_pong ||
             port.allocation_identity == exchange->allocation_identity;
    });
  if (!one_exchange)
  {
    graph_error("graph uses more than one ping-pong allocation");
  }
  const auto node_ports = [ & ](std::size_t node) {
    return std::span<const graph_port> {ports}.subspan(
      first_ports[ node ], port_counts[ node ]);
  };
  const auto is_source = [ & ](const graph_port& port) -> bool {
    return port.initially_available &&
           port.region == graph_buffer_region::external &&
           reads(port.access) &&
           port.bytes == exchange->bytes;
  };
  const auto writer = std::ranges::find_if(
    result.topological_order, [ & ](std::size_t node) -> bool {
      return std::ranges::any_of(
        node_ports(node), [](const graph_port& port) -> bool {
          return port.region == graph_buffer_region::ping_pong &&
                 writes(port.access);
        });
    });
  std::span<const graph_port> candidates {ports};
  if (
    writer != result.topological_order.end() &&
    std::ranges::any_of(node_ports(*writer), is_source))
  {
    candidates = node_ports(*writer);
  }

  const auto source = std::ranges::find_if(candidates, is_source);
  if (source == candidates.end())
  {
    graph_error("graph ping-pong has no initially available source");
  }

  const bool one_source =
    std::ranges::all_of(candidates, [ & ](const graph_port& port) -> bool {
      return !is_source(port) ||
             port.allocation_identity == source->allocation_identity;
    });
  if (!one_source) { graph_error("graph ping-pong source is ambiguous"); }

  result.ping_pong_pass_count = static_cast<
    std::size_t
  >(std::ranges::count_if(ports, [ & ](const graph_port& port) -> bool {
    return writes(port.access) &&
           ((port.region == graph_buffer_region::external &&
              port.allocation_identity == source->allocation_identity) ||
             (port.region == graph_buffer_region::ping_pong &&
               port.allocation_identity == exchange->allocation_identity));
  }));
  const graph_port& final_port =
    result.ping_pong_pass_count % 2UZ == 0UZ ? *source : *exchange;
  result.final_allocation = graph_allocation {
    .identity = final_port.allocation_identity,
    .region   = final_port.region,
  };

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

} // namespace fiat
