import std;
import fiat;

template<class Operation>
struct transform_probe
{
  static constexpr fiat::stage_class   kind {fiat::stage_class::elementwise};
  static constexpr fiat::skeleton_kind skeleton {
    fiat::skeleton_kind::transform
  };
  static constexpr fiat::stage_schedule schedule {};

  template<fiat::scalar Element>
  using ports = std::tuple<
    fiat::port<Element, fiat::port_access::read>,
    fiat::port<Element, fiat::port_access::write>
  >;

  using splices = std::tuple<
    fiat::splice_operation<fiat::splice_point::operation, Operation>
  >;

  typename Operation::argument_tuple arguments {};
};

template<class Combine>
struct scan_probe
{
  static constexpr fiat::stage_class   kind {fiat::stage_class::scan};
  static constexpr fiat::skeleton_kind skeleton {
    fiat::skeleton_kind::inclusive_scan
  };
  static constexpr fiat::stage_schedule schedule {};

  template<fiat::scalar Element>
  using ports = std::tuple<
    fiat::port<Element, fiat::port_access::read>,
    fiat::port<std::uint32_t, fiat::port_access::read_write>,
    fiat::port<Element, fiat::port_access::write>
  >;

  using splices = std::tuple<
    fiat::splice_operation<fiat::splice_point::operation, fiat::chain<>>,
    fiat::splice_operation<fiat::splice_point::combine, Combine>,
    fiat::splice_operation<fiat::splice_point::epilogue, fiat::identity>
  >;

  std::tuple<> arguments {};
};

using square_transform =
  transform_probe<fiat::chain<fiat::bound_operation<fiat::square>>>;
using plus_scan = scan_probe<fiat::plus>;

constexpr auto transform_layout =
  fiat::dispatch_push_of<square_transform, float>;
constexpr auto scan_layout   = fiat::dispatch_push_of<plus_scan, std::uint32_t>;
constexpr auto legacy_layout = fiat::dispatch_push {
  .ports     = 2U,
  .arguments = 26U,
};

static_assert(transform_layout.size() == 24U, "2 ports, no arguments");
static_assert(scan_layout.size() == 32U, "3 ports, no arguments");
static_assert(fiat::push_budget == 256U, "Vulkan 1.4 maxPushConstantsSize");
static_assert(
  transform_layout.argument_words_within(128U) == fiat::argument_capacity,
  "the 128-byte layout keeps 26 argument words");
static_assert(
  legacy_layout.size() == sizeof(fiat::detail::transform_push),
  "2 ports and 26 arguments match the transform push block");
static_assert(
  std::ranges::equal(
    fiat::compile<float>(square_transform {}),
    fiat::compile<float>(fiat::transform(fiat::square {}))),
  "the stage protocol compiles transform to the same words");
static_assert(
  std::ranges::equal(
    fiat::compile<std::uint32_t>(plus_scan {}),
    fiat::compile<std::uint32_t>(fiat::inclusive_scan(fiat::plus {}, 0U))),
  "the stage protocol compiles scan to the same words");

auto
main() -> int
{
  std::println("transform push: {} bytes", transform_layout.size());
  std::println("scan push: {} bytes", scan_layout.size());
  std::println("push budget: {} bytes", fiat::push_budget);
  std::println(
    "argument capacity at 128 bytes: {} words",
    transform_layout.argument_words_within(128U));
  return 0;
}
