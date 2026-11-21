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

  using plsices = std::tuple<>;
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
  fiat::make_node(fiat::reduce(fiat::plus {}, 0.0F), float_intermediate));
static_assert(graph_one.analysis.dispatch_count == 2UZ);
static_assert(graph_one.analysis.barrier_count == 1UZ);
static_assert(graph_one.analysis.requirements.scratch_bytes == 4'104UZ);

#endif

auto
main() -> int
{}
