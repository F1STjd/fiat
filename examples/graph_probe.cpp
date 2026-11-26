import std;
import fiat;

template<fiat::stage_class Kind, class... Ports>
struct probe_stage
{
  static constexpr fiat::stage_class   kind {Kind};
  static constexpr fiat::skeleton_kind skeleton {
    fiat::skeleton_kind::transform,
  };
  static constexpr fiat::stage_schedule schedule {};
  static constexpr std::array           dispatches {
    fiat::dispatch_rule {.entry = fiat::transform_entry},
  };

  template<fiat::scalar>
  using ports = std::tuple<Ports...>;

  using splices = std::tuple<>;
  std::tuple<> arguments {};
};

using branch_reduce_probe = probe_stage<
  fiat::stage_class::sink,
  fiat::port<float, fiat::port_access::read>,
  fiat::port<float, fiat::port_access::write>
>;
using binary_transform_probe = probe_stage<
  fiat::stage_class::elementwise,
  fiat::port<float, fiat::port_access::read>,
  fiat::port<float, fiat::port_access::read>,
  fiat::port<float, fiat::port_access::write>
>;

#if defined(FIAT_GRAPH_BASELINE)

struct baseline_result
{
  std::size_t dispatches {};
  std::size_t barriers {};

  constexpr
  auto
  operator ==(const baseline_result&) const -> bool = default;
};

consteval
void
record_stage(baseline_result& result, const auto& stage, bool barrier_before)
{
  result.dispatches += stage.dispatches.size();
  result.barriers +=
    std::size_t {barrier_before} + stage.dispatches.size() - 1UZ;
}

consteval
auto
graph_one_baseline() -> baseline_result
{
  baseline_result result {};
  record_stage(result, fiat::transform(fiat::square {}), false);
  record_stage(result, fiat::reduce(fiat::plus {}, 0.0F), true);
  return result;
}

consteval
auto
graph_two_baseline() -> baseline_result
{
  baseline_result result {};
  record_stage(result, fiat::transform(fiat::square {}), false);
  record_stage(result, branch_reduce_probe {}, true);
  record_stage(result, fiat::inclusive_scan(fiat::plus {}, 0.0F), false);
  record_stage(result, binary_transform_probe {}, true);
  return result;
}

consteval
auto
graph_three_baseline() -> baseline_result
{
  baseline_result result {};
  record_stage(result, fiat::predicate(fiat::not_equal {}, 0.0F), false);
  record_stage(result, fiat::exclusive_scan(fiat::plus {}, 0U), true);
  record_stage(result, fiat::scatter(), true);
  return result;
}

consteval
void
record_radix_pass(baseline_result& result, bool barrier_before)
{
  record_stage(
    result, fiat::tile_histogram<256U>(fiat::digit<8U> {}, 0U), barrier_before);
  record_stage(result, fiat::exclusive_scan(fiat::plus {}, 0U), true);
  record_stage(
    result, fiat::stable_scatter<256U>(fiat::digit<8U> {}, 0U), true);
}

consteval
auto
graph_four_baseline() -> baseline_result
{
  baseline_result result {};
  record_radix_pass(result, false);
  record_radix_pass(result, true);
  record_radix_pass(result, true);
  record_radix_pass(result, true);
  return result;
}

static_assert(graph_one_baseline() == baseline_result {2UZ, 1UZ});
static_assert(graph_two_baseline() == baseline_result {5UZ, 3UZ});
static_assert(graph_three_baseline() == baseline_result {4UZ, 3UZ});
static_assert(graph_four_baseline() == baseline_result {16UZ, 15UZ});

#else

constexpr std::size_t element_count {1'024UZ};
constexpr std::size_t element_bytes {element_count * sizeof(std::uint32_t)};

constexpr fiat::graph_buffer<float> float_source {
  .identity            = 0UZ,
  .allocation_identity = 0UZ,
  .bytes               = element_bytes,
};
constexpr fiat::graph_buffer<float> float_intermediate {
  .identity            = 1UZ,
  .allocation_identity = 1UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};
constexpr fiat::graph_buffer<float> float_reduction {
  .identity            = 2UZ,
  .allocation_identity = 2UZ,
  .bytes               = sizeof(float),
  .initially_available = false,
};
constexpr fiat::graph_buffer<float> float_scan {
  .identity            = 3UZ,
  .allocation_identity = 3UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};
constexpr fiat::graph_buffer<float> float_join {
  .identity            = 4UZ,
  .allocation_identity = 4UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};

constexpr auto graph_one = fiat::make_graph(
  fiat::make_node(
    fiat::transform(fiat::square {}), float_source, float_intermediate),
  fiat::make_node(
    fiat::reduce(fiat::plus {}, 0.0F), float_intermediate, float_reduction));
static_assert(graph_one.analysis.dispatch_count == 2UZ);
static_assert(graph_one.analysis.barrier_count == 1UZ);
static_assert(graph_one.analysis.requirements.scratch_bytes == 4'104UZ);
static_assert(!graph_one.analysis.final_allocation.has_value());

constexpr auto graph_two = fiat::make_graph(
  fiat::make_node(branch_reduce_probe {}, float_intermediate, float_reduction),
  fiat::make_node(
    binary_transform_probe {}, float_reduction, float_scan, float_join),
  fiat::make_node(
    fiat::transform(fiat::square {}), float_source, float_intermediate),
  fiat::make_node(
    fiat::inclusive_scan(fiat::plus {}, 0.0F), float_intermediate, float_scan));
static_assert(graph_two.analysis.stage_barrier_count == 2UZ);
static_assert(
  graph_two.analysis.topological_order ==
  std::array<std::size_t, 4UZ> {2UZ, 0UZ, 3UZ, 1UZ});

constexpr fiat::graph_buffer<std::uint32_t> flags {
  .identity            = 5UZ,
  .allocation_identity = 5UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};
constexpr fiat::graph_buffer<std::uint32_t> indices {
  .identity            = 6UZ,
  .allocation_identity = 6UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};
constexpr fiat::graph_buffer<std::uint32_t> kept_count {
  .identity            = 7UZ,
  .allocation_identity = 7UZ,
  .bytes               = sizeof(std::uint32_t),
  .region              = fiat::graph_buffer_region::table,
  .initially_available = false,
};
constexpr auto graph_three = fiat::make_graph(
  fiat::make_node(
    fiat::predicate(fiat::not_equal {}, 0.0F), float_source, flags),
  fiat::make_node(fiat::exclusive_scan(fiat::plus {}, 0U), flags, indices),
  fiat::make_node(
    fiat::scatter(), float_source, float_join, flags, indices, kept_count));
static_assert(graph_three.analysis.dispatch_count == 4UZ);
static_assert(graph_three.analysis.barrier_count == 3UZ);

constexpr fiat::graph_buffer<std::uint32_t> unsigned_source_zero {
  .identity            = 20UZ,
  .allocation_identity = 0UZ,
  .bytes               = element_bytes,
};
constexpr fiat::graph_buffer<std::uint32_t> unsigned_exchange_one {
  .identity            = 21UZ,
  .allocation_identity = 1UZ,
  .bytes               = element_bytes,
  .region              = fiat::graph_buffer_region::ping_pong,
  .initially_available = false,
};
constexpr fiat::graph_buffer<std::uint32_t> unsigned_source_two {
  .identity            = 22UZ,
  .allocation_identity = 0UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};
constexpr fiat::graph_buffer<std::uint32_t> unsigned_exchange_three {
  .identity            = 23UZ,
  .allocation_identity = 1UZ,
  .bytes               = element_bytes,
  .region              = fiat::graph_buffer_region::ping_pong,
  .initially_available = false,
};
constexpr fiat::graph_buffer<std::uint32_t> unsigned_source_four {
  .identity            = 24UZ,
  .allocation_identity = 0UZ,
  .bytes               = element_bytes,
  .initially_available = false,
};

template<std::size_t Pass>
consteval
auto
radix_pass(auto input, auto output)
{
  constexpr fiat::graph_buffer<std::uint32_t> histogram {
    .identity            = 30UZ + 3UZ * Pass,
    .allocation_identity = 2UZ,
    .bytes               = 256UZ * sizeof(std::uint32_t),
    .region              = fiat::graph_buffer_region::table,
    .initially_available = false,
  };
  constexpr fiat::graph_buffer<std::uint32_t> offsets {
    .identity            = 31UZ + 3UZ * Pass,
    .allocation_identity = 3UZ,
    .bytes               = 256UZ * sizeof(std::uint32_t),
    .region              = fiat::graph_buffer_region::table,
    .initially_available = false,
  };
  return std::tuple {
    fiat::make_node(
      fiat::tile_histogram<256U>(fiat::digit<8U> {}, 0U), input, histogram),
    fiat::make_node(fiat::exclusive_scan(fiat::plus {}, 0U), histogram, offsets),
    fiat::make_node(
      fiat::stable_scatter<256U>(fiat::digit<8U> {}, 0U),
      input,
      output,
      offsets,
      fiat::unbound_buffer<std::uint32_t>,
      fiat::unbound_buffer<std::uint32_t>),
  };
}

constexpr auto graph_four_nodes = std::tuple_cat(
  radix_pass<0UZ>(unsigned_source_zero, unsigned_exchange_one),
  radix_pass<1UZ>(unsigned_exchange_one, unsigned_source_two),
  radix_pass<2UZ>(unsigned_source_two, unsigned_exchange_three),
  radix_pass<3UZ>(unsigned_exchange_three, unsigned_source_four));
constexpr auto graph_four = std::apply(
  [](auto... nodes) { return fiat::make_graph(nodes...); }, graph_four_nodes);
static_assert(graph_four.analysis.dispatch_count == 16UZ);
static_assert(graph_four.analysis.barrier_count == 15UZ);
static_assert(graph_four.analysis.edges[ 0UZ ][ 5UZ ]);
static_assert(graph_four.analysis.ping_pong_pass_count == 4UZ);
static_assert(
  graph_four.analysis.final_allocation ==
  fiat::graph_allocation {
    .identity = 0UZ,
    .region   = fiat::graph_buffer_region::external,
  });
static_assert(
  graph_four.analysis.requirements.ping_pong_bytes == element_bytes);
static_assert(graph_four.analysis.buffer_offsets[ 8UZ ] == 0UZ);
static_assert(graph_four.analysis.workspace_offsets[ 1UZ ].scratch == 0UZ);

  #if 0
constexpr auto graph_five = fiat::make_node(
  fiat::reduce(fiat::plus {}, 0U), float_source);
  #endif

#endif

auto
main() -> int
{}
