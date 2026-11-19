export module fiat:kernel;

import std;

import :trace;
import :emit;
import :templates;
import :splice;
import :stage;
import :ops;

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

} // namespace detail

export inline constexpr std::size_t reduce_result_offset {4UZ};
static_assert(
  reduce_result_offset == 4UZ, "scratch member 1 holds the reduced value");

static_assert(
  sizeof(detail::transform_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(detail::transform_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

export inline constexpr std::size_t stable_scatter_argument_capacity {20UZ};

static_assert(
  stable_scatter_argument_capacity == 20UZ,
  "leaves 24 bytes after member 4 for scanned and the two value pointers");

namespace detail
{

export struct tile_histogram_push
{
  std::uint64_t                                input {};
  std::uint64_t                                histogram {};
  std::uint32_t                                count {};
  std::uint32_t                                first {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

} // namespace detail

static_assert(
  sizeof(detail::tile_histogram_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the tile-histogram template push block");
static_assert(
  sizeof(detail::tile_histogram_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

namespace detail
{

export struct stable_scatter_push
{
  std::uint64_t                                               input_keys {};
  std::uint64_t                                               output_keys {};
  std::uint32_t                                               count {};
  std::uint32_t                                               first {};
  std::array<std::uint32_t, stable_scatter_argument_capacity> arguments {};
  std::uint64_t                                               scanned {};
  std::uint64_t                                               input_values {};
  std::uint64_t                                               output_values {};
};

} // namespace detail

static_assert(
  sizeof(detail::stable_scatter_push) ==
    detail::argument_offset +
      stable_scatter_argument_capacity * sizeof(std::uint32_t) +
      24UZ,
  "matches the stable-scatter template push block");
static_assert(
  sizeof(detail::stable_scatter_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

export inline constexpr std::string_view transform_entry {"fiat_transform"};

export inline constexpr std::string_view reduce_entry {"fiat_reduce"};

export inline constexpr std::string_view scan_upsweep_entry {
  "fiat_scan_upsweep"
};
export inline constexpr std::string_view scan_downsweep_entry {
  "fiat_scan_downsweep"
};
export inline constexpr std::uint32_t group_size_constant_id {};

export inline constexpr std::string_view predicate_entry {"fiat_predicate"};

export inline constexpr std::string_view scatter_entry {"fiat_scatter"};

export inline constexpr std::string_view histogram_entry {"fiat_histogram"};
export inline constexpr std::string_view histogram_global_entry {
  "fiat_histogram_global"
};
export inline constexpr std::uint32_t buckets_constant_id {1U};

export inline constexpr std::string_view tile_histogram_entry {
  "fiat_tile_histogram"
};

export inline constexpr std::string_view stable_scatter_entry {
  "fiat_stable_scatter"
};
export inline constexpr std::string_view stable_scatter_pairs_entry {
  "fiat_stable_scatter_pairs"
};

export template<class Operation>
struct transform_stage
{
  static constexpr stage_class    kind {stage_class::elementwise};
  static constexpr skeleton_kind  skeleton {skeleton_kind::transform};
  static constexpr stage_schedule schedule {};
  static constexpr std::array     dispatches {
    dispatch_rule {.entry = transform_entry},
  };

  template<scalar Element>
  using ports = std::tuple<
    port<Element, port_access::read>,
    port<Element, port_access::write>
  >;

  using splices =
    std::tuple<splice_operation<splice_point::operation, Operation>>;

  typename arguments_of<Operation>::type arguments {};
};

export template<
  class Operation,
  class Combine,
  class Identity,
  class Epilogue = identity
>
struct reduce_stage
{
  static constexpr stage_class    kind {stage_class::sink};
  static constexpr skeleton_kind  skeleton {skeleton_kind::reduce};
  static constexpr stage_schedule schedule {};
  static constexpr std::array     dispatches {
    dispatch_rule {
      .entry          = reduce_entry,
      .extent         = dispatch_extent::grid_stride,
      .maximum_groups = detail::reduce_group_limit,
    },
  };
  static constexpr workspace_sizes workspace {
    .scratch_bytes = detail::reduce_scratch_bytes,
  };

  template<std::same_as<Identity> Element>
  using ports = std::tuple<port<Element, port_access::read>>;

  using leading_arguments = std::tuple<Identity>;

  using splices = std::tuple<
    splice_operation<splice_point::operation, Operation>,
    splice_operation<splice_point::combine, Combine>,
    splice_operation<splice_point::epilogue, Epilogue>
  >;

  decltype(std::tuple_cat(
    std::declval<leading_arguments>(),
    std::declval<typename arguments_of<Operation>::type>(),
    std::declval<typename arguments_of<Epilogue>::type>())) arguments {};
};

export template<
  class Operation,
  class Combine,
  class Identity,
  scan_mode Mode,
  class Epilogue = identity
>
struct scan_stage
{
  static constexpr stage_class   kind {stage_class::scan};
  static constexpr skeleton_kind skeleton {
    Mode == scan_mode::inclusive
      ? skeleton_kind::inclusive_scan
      : skeleton_kind::exclusive_scan,
  };
  static constexpr stage_schedule schedule {};
  static constexpr std::array     dispatches {
    dispatch_rule {
      .entry          = scan_upsweep_entry,
      .extent         = dispatch_extent::grid_stride,
      .maximum_groups = detail::reduce_group_limit,
    },
    dispatch_rule {
      .entry          = scan_downsweep_entry,
      .extent         = dispatch_extent::grid_stride,
      .maximum_groups = detail::reduce_group_limit,
    },
  };
  static constexpr workspace_sizes workspace {
    .scratch_bytes = detail::reduce_scratch_bytes,
  };

  template<std::same_as<Identity> Element>
  using ports = std::tuple<
    port<Element, port_access::read>,
    port<Element, port_access::write>
  >;

  using leading_arguments = std::tuple<Identity>;

  using splices = std::tuple<
    splice_operation<splice_point::operation, Operation>,
    splice_operation<splice_point::combine, Combine>,
    splice_operation<splice_point::epilogue, Epilogue>
  >;

  decltype(std::tuple_cat(
    std::declval<leading_arguments>(),
    std::declval<typename arguments_of<Operation>::type>(),
    std::declval<typename arguments_of<Epilogue>::type>())) arguments {};
};

export template<class Compare, class Threshold>
struct predicate_stage
{
  Threshold threshold {};
};

export template<class Compare, class Threshold>
struct compact_stage
{
  Threshold threshold {};
};

export struct scatter_stage
{
  static constexpr stage_class    kind {stage_class::permute};
  static constexpr skeleton_kind  skeleton {skeleton_kind::scatter};
  static constexpr stage_schedule schedule {};
  static constexpr std::array     dispatches {
    dispatch_rule {.entry = scatter_entry},
  };

  template<scalar Element>
  using ports = std::tuple<
    port<Element, port_access::read>,
    port<Element, port_access::write>,
    port<std::uint32_t, port_access::read>,
    port<std::uint32_t, port_access::read>,
    port<std::uint32_t, port_access::write, 1U>
  >;

  using splices = std::tuple<>;

  std::tuple<> arguments {};
};

export
template<class Compare, class Threshold>
constexpr auto
predicate(Compare, Threshold threshold) -> predicate_stage<Compare, Threshold>
{
  static_assert(
    compare_operation<Compare>,
    "fiat::predicate compare must be greater, greater_equal, less, less_equal, "
    "or not_equal");
  static_assert(
    scalar<Threshold>,
    "fiat::predicate threshold must be float, std::int32_t or std::uint32_t");
  return {.threshold = threshold};
}

export
template<class Compare, class Threshold>
constexpr auto
compact(Compare, Threshold threshold) -> compact_stage<Compare, Threshold>
{
  static_assert(
    compare_operation<Compare>,
    "fiat::compact compare must be greater, greater_equal, less, less_equal, "
    "or not_equal");
  static_assert(
    scalar<Threshold>,
    "fiat::compact threshold must be float, std::int32_t or std::uint32_t");
  return {.threshold = threshold};
}

export constexpr
auto
scatter() -> scatter_stage
{ return {}; }

export enum class histogram_mode : std::uint8_t
{
  privatized,
  global,
};

inline constexpr std::uint32_t privatized_bucket_limit {4'096U};

export template<std::uint32_t Buckets, histogram_mode Mode, class Key>
struct histogram_stage
{
  static_assert(Buckets >= 1U, "fiat::histogram needs at least one bucket");
  static_assert(
    Mode == histogram_mode::global || Buckets <= privatized_bucket_limit,
    "a privatized fiat::histogram keeps at most 4096 buckets in shared "
    "memory; use histogram_mode::global for more");

  static constexpr stage_class    kind {stage_class::sink};
  static constexpr skeleton_kind  skeleton {skeleton_kind::histogram};
  static constexpr stage_schedule schedule {};
  static constexpr std::array     dispatches {
    dispatch_rule {
      .entry =
        Mode == histogram_mode::privatized
          ? histogram_entry
          : histogram_global_entry,
    },
  };
  static constexpr std::uint32_t buckets {Buckets};

  template<std::same_as<std::uint32_t> Element>
  using ports = std::tuple<
    port<Element, port_access::read>,
    port<Element, port_access::read_write, Buckets>
  >;

  using splices = std::tuple<splice_operation<splice_point::key, Key>>;

  typename arguments_of<Key>::type arguments {};
};

export
template<
  std::uint32_t  Buckets,
  histogram_mode Mode = histogram_mode::privatized,
  class Key,
  class... Arguments
>
constexpr auto
histogram(Key, Arguments... arguments) -> histogram_stage<Buckets, Mode, Key>
{
  static_assert(
    std::is_empty_v<Key> && std::default_initializable<Key>,
    "fiat::histogram cannot read lambda captures; pass runtime values as "
    "arguments: fiat::histogram<Buckets>(key, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fiat::histogram arguments must be float, std::int32_t or std::uint32_t");
  static_assert(
    sizeof...(Arguments) <= argument_capacity,
    "fiat::histogram takes at most 26 runtime arguments");
  static_assert(
    std::same_as<std::tuple<Arguments...>, typename arguments_of<Key>::type>,
    "fiat::histogram arguments must match the key");
  if constexpr (requires { Key::bits; })
  {
    static_assert(Key::bits < 32U, "fiat::digit bits must be less than 32");
    static_assert(
      Buckets == (Key::bits < 32U ? (1U << Key::bits) : 0U),
      "fiat::histogram buckets must equal 1U << bits");
  }
  static_assert(
    traceable<Key, std::uint32_t>,
    "fiat::histogram key must be traceable on std::uint32_t");
  return {.arguments = {arguments...}};
}

static_assert(
    dispatch_push_of<
      histogram_stage<16U, histogram_mode::global, identity>,
      std::uint32_t
    >.argument_member() == 4U,
    "the histogram template reads its argument words from push member 4");
static_assert(
    skeleton_push_of<
      histogram_stage<16U, histogram_mode::global, identity>,
      std::uint32_t
    >.arguments == argument_capacity,
    "the histogram template keeps 26 argument words after the input and counts");
static_assert(
  !archetype_stage<histogram_stage<16U, histogram_mode::global, identity>, float>,
  "fiat histogram stages take std::uint32_t elements");

export template<std::uint32_t Buckets, class Key>
struct tile_histogram_stage
{
  static constexpr std::uint32_t buckets {Buckets};
  using key_type = Key;
  typename arguments_of<Key>::type arguments {};
};

export
template<std::uint32_t Buckets, class Key, class... Arguments>
constexpr auto
tile_histogram(Key, Arguments... arguments)
  -> tile_histogram_stage<Buckets, Key>
{
  static_assert(
    Buckets >= 1U, "fiat::tile_histogram needs at least one bucket");
  static_assert(
    std::is_empty_v<Key> && std::default_initializable<Key>,
    "fiat::tile_histogram cannot read lambda captures; pass runtime values as "
    "arguments: fiat::tile_histogram<Buckets>(key, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fiat::tile_histogram arguments must be float, std::int32_t or "
    "std::uint32_t");
  static_assert(
    sizeof...(Arguments) <= argument_capacity,
    "fiat::tile_histogram takes at most 26 runtime arguments");
  static_assert(
    std::same_as<std::tuple<Arguments...>, typename arguments_of<Key>::type>,
    "fiat::tile_histogram arguments must match the key");
  if constexpr (requires { Key::bits; })
  {
    static_assert(
      Key::bits >= 1U && Key::bits <= 8U,
      "fiat::tile_histogram bits must be between 1 and 8");
    static_assert(
      Buckets == (Key::bits <= 8U ? (1U << Key::bits) : 0U),
      "fiat::tile_histogram buckets must equal 1U << bits");
  }
  static_assert(
    traceable<Key, std::uint32_t>,
    "fiat::tile_histogram key must be traceable on std::uint32_t");
  return {.arguments = {arguments...}};
}

export template<std::uint32_t Buckets, class Key>
struct stable_scatter_stage
{
  static constexpr std::uint32_t buckets {Buckets};
  using key_type = Key;
  typename arguments_of<Key>::type arguments {};
};

export
template<std::uint32_t Buckets, class Key, class... Arguments>
constexpr auto
stable_scatter(Key, Arguments... arguments)
  -> stable_scatter_stage<Buckets, Key>
{
  static_assert(
    Buckets >= 1U, "fiat::stable_scatter needs at least one bucket");
  static_assert(
    std::is_empty_v<Key> && std::default_initializable<Key>,
    "fiat::stable_scatter cannot read lambda captures; pass runtime values as "
    "arguments: fiat::stable_scatter<Buckets>(key, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fiat::stable_scatter arguments must be float, std::int32_t or "
    "std::uint32_t");
  static_assert(
    sizeof...(Arguments) <= stable_scatter_argument_capacity,
    "fiat::stable_scatter takes at most 20 runtime arguments");
  static_assert(
    std::same_as<std::tuple<Arguments...>, typename arguments_of<Key>::type>,
    "fiat::stable_scatter arguments must match the key");
  if constexpr (requires { Key::bits; })
  {
    static_assert(
      Key::bits >= 1U && Key::bits <= 8U,
      "fiat::stable_scatter bits must be between 1 and 8");
    static_assert(
      Buckets == (Key::bits <= 8U ? (1U << Key::bits) : 0U),
      "fiat::stable_scatter buckets must equal 1U << bits");
  }
  static_assert(
    traceable<Key, std::uint32_t>,
    "fiat::stable_scatter key must be traceable on std::uint32_t");
  return {.arguments = {arguments...}};
}

export template<std::uint32_t Bits = 8U>
struct radix_sort_stage
{
  static constexpr std::uint32_t bits {Bits};
  static constexpr std::uint32_t buckets {1U << Bits};
};

export
template<std::uint32_t Bits = 8U>
constexpr auto
radix_sort() -> radix_sort_stage<Bits>
{
  static_assert(
    Bits >= 1U && Bits <= 8U, "fiat::radix_sort bits must be between 1 and 8");
  return {};
}

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

template<scalar Element, class Operation>
consteval
auto
matches_transform_splice(transform_stage<Operation> stage) -> bool
{
  const std::vector<instruction> instructions =
    detail::trace<Element, Operation>();
  return std::ranges::equal(
    compile<Element>(stage),
    splice<Element>(
      transform_template<Element>(),
      std::array {
        detail::symbol_trace {operation_symbol, instructions},
      }));
}

static_assert(
  matches_transform_splice<float>(transform(square {})),
  "the stage protocol compiles fiat::transform to the spliced template words");
static_assert(
  matches_transform_splice<std::int32_t>(
    transform(multiplies {}, 3) | transform(plus {}, 7)),
  "the stage protocol compiles fused arguments to the spliced template words");
static_assert(
  matches_transform_splice<std::uint32_t>(transform(minus {}, 2U)),
  "the stage protocol compiles fiat::transform to the spliced template words");
static_assert(
  skeleton_push_of<transform_stage<chain<>>, float>.size() ==
    sizeof(detail::transform_push),
  "the transform skeleton push block is the 128-byte transform_push");
static_assert(
  skeleton_push_of<transform_stage<chain<>>, float>.argument_offset() ==
    detail::argument_offset,
  "transform arguments start after two ports, count and first");

export template<scalar Uniform>
struct uniform_tag {};

export template<scalar Uniform, class Operation, class... Arguments>
struct bound_uniform_operation
{
  using argument_tuple = std::tuple<Arguments...>;
  using uniform_type   = Uniform;

  template<scalar Element, class... Values>
    requires (
      std::same_as<Element, Uniform> &&
      (std::same_as<Values, value<Arguments>> && ...) &&
      uniform_traceable_call<Operation, Element, Uniform, Arguments...>)
  constexpr
  auto
  operator ()(
    value<Element>&       operand,
    const value<Uniform>& uniform,
    const Values&... arguments) const -> value<Element>
  {
    if constexpr (
      std::is_void_v<std::invoke_result_t<
        Operation,
        value<Element>&,
        const value<Uniform>&,
        const Values&...
      >>)
    {
      Operation {}(operand, uniform, arguments...);
      return operand;
    }
    else
    {
      return Operation {}(operand, uniform, arguments...);
    }
  }
};

export template<scalar Uniform, class Operation>
struct transform_with_stage
{
  std::uint64_t                          address {};
  typename arguments_of<Operation>::type arguments {};
};

export
template<class UniformSource, class Operation, class... Arguments>
  requires (requires(const UniformSource& uniform) {
    typename UniformSource::element_type;
    requires scalar<typename UniformSource::element_type>;
    { uniform.address() } -> std::convertible_to<std::uint64_t>;
  } && scalar<typename UniformSource::element_type>)
constexpr auto
transform_with(const UniformSource& uniform, Operation, Arguments... arguments)
  -> transform_with_stage<
    typename UniformSource::element_type,
    bound_uniform_operation<
      typename UniformSource::element_type,
      Operation,
      Arguments...
    >
  >
{
  static_assert(
    std::is_empty_v<Operation>,
    "fait::transform_with cannot read lambda captures; pass runtime values as "
    "arguments: fiat::transform_with(scalar, f, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fait::transform_with arguments must be float std::int32_t or "
    "std::uint32_t");
  static_assert(
    sizeof...(Arguments) <= argument_capacity - 2UZ,
    "fait::transform_with takes at most 24 further runtime arguments");
  return {
    .address   = static_cast<std::uint64_t>(uniform.address()),
    .arguments = {arguments...},
  };
}

export
template<class Combine, class Identity>
constexpr auto
reduce(Combine, Identity identity) -> reduce_stage<chain<>, Combine, Identity>
{
  static_assert(
    std::is_empty_v<Combine>, "fiat::reduce cannot read lambda captures");
  static_assert(
    scalar<Identity>,
    "fiat::reduce identity must be float, std::int32_t or std::uint32_t");
  static_assert(
    std::tuple_size_v<typename arguments_of<Combine>::type> == 0UZ,
    "fiat::reduce combine operations take no runtime arguments; fuse a "
    "fiat::transform before the reduce instead");
  return {.arguments = {identity}};
}

export
template<class... Left, class Combine, class Identity>
constexpr auto
operator |(
  transform_stage<chain<Left...>>          left,
  reduce_stage<chain<>, Combine, Identity> right)
  -> reduce_stage<chain<Left...>, Combine, Identity>
{ return {.arguments = std::tuple_cat(right.arguments, left.arguments)}; }

template<class Epilogue, class... Next>
struct extended_epilogue;

template<class... Next>
struct extended_epilogue<identity, Next...>
{
  using type = chain<Next...>;
};

template<class... Current, class... Next>
struct extended_epilogue<chain<Current...>, Next...>
{
  using type = chain<Current..., Next...>;
};

export
template<
  class Operation,
  class Combine,
  class Identity,
  class Epilogue,
  class... Next
>
constexpr auto
operator |(
  reduce_stage<Operation, Combine, Identity, Epilogue> left,
  transform_stage<chain<Next...>>                      right)
  -> reduce_stage<
    Operation,
    Combine,
    Identity,
    typename extended_epilogue<Epilogue, Next...>::type
  >
{ return {.arguments = std::tuple_cat(left.arguments, right.arguments)}; }

static_assert(
  skeleton_push_of<reduce_stage<chain<>, plus, float>, float>.argument_offset() ==
    detail::argument_offset,
  "reduce arguments start after the input, the scratch, count and first");
static_assert(
  skeleton_push_of<reduce_stage<chain<>, plus, float>, float>.arguments ==
    argument_capacity,
  "reduce keeps 26 argument words; the identity takes the first");
static_assert(
  !archetype_stage<reduce_stage<chain<>, plus, std::uint32_t>, float>,
  "the reduce identity has the array's element type");

export
template<class Combine, class Identity>
constexpr auto
inclusive_scan(Combine, Identity identity)
  -> scan_stage<chain<>, Combine, Identity, scan_mode::inclusive>
{
  static_assert(
    std::is_empty_v<Combine>,
    "fiat::inclusive_scan cannot read lambda captures");
  static_assert(
    scalar<Identity>,
    "fiat::inclusive_scan identity must be float, std::int32_t or "
    "std::uint32_t");
  static_assert(
    std::tuple_size_v<typename arguments_of<Combine>::type> == 0UZ,
    "fiat::inclusive_scan combine operations take no runtime arguments; fuse a "
    "fiat::transform before the scan instead");
  return {.arguments = {identity}};
}

export
template<class Combine, class Identity>
constexpr auto
exclusive_scan(Combine, Identity identity)
  -> scan_stage<chain<>, Combine, Identity, scan_mode::exclusive>
{
  static_assert(
    std::is_empty_v<Combine>,
    "fiat::exclusive_scan cannot read lambda captures");
  static_assert(
    scalar<Identity>,
    "fiat::exclusive_scan identity must be float, std::int32_t or "
    "std::uint32_t");
  static_assert(
    std::tuple_size_v<typename arguments_of<Combine>::type> == 0UZ,
    "fiat::exclusive_scan combine operations take no runtime arguments; fuse a "
    "fiat::transform before the scan instead");
  return {.arguments = {identity}};
}

export
template<class... Left, class Combine, class Identity, scan_mode Mode>
constexpr auto
operator |(
  transform_stage<chain<Left...>>              left,
  scan_stage<chain<>, Combine, Identity, Mode> right)
  -> scan_stage<chain<Left...>, Combine, Identity, Mode>
{ return {.arguments = std::tuple_cat(right.arguments, left.arguments)}; }

export
template<
  class Operation,
  class Combine,
  class Identity,
  scan_mode Mode,
  class Epilogue,
  class... Next
>
constexpr auto
operator |(
  scan_stage<Operation, Combine, Identity, Mode, Epilogue> left,
  transform_stage<chain<Next...>>                          right)
  -> scan_stage<
    Operation,
    Combine,
    Identity,
    Mode,
    typename extended_epilogue<Epilogue, Next...>::type
  >
{ return {.arguments = std::tuple_cat(left.arguments, right.arguments)}; }

static_assert(
    skeleton_push_of<scan_stage<chain<>, plus, float, scan_mode::inclusive>, float>
        .argument_offset() == 32U,
    "scan arguments start after the input, the output, the scratch, count and "
    "first");
static_assert(
    skeleton_push_of<scan_stage<chain<>, plus, float, scan_mode::inclusive>, float>
        .arguments == 24U,
    "scan keeps 24 argument words; the identity takes the first");
static_assert(
    dispatch_push_of<scan_stage<chain<>, plus, float, scan_mode::inclusive>, float>
        .argument_member() == 5U,
    "the scan template reads its argument words from push member 5");
static_assert(
  !archetype_stage<
    scan_stage<chain<>, plus, std::uint32_t, scan_mode::inclusive>,
    float
  >,
  "the scan identity has the array's element type");

namespace detail
{

template<scalar Element, scalar Uniform, class Operation>
  requires uniform_traceable<Operation, Element, Uniform>
constexpr
auto
transform_with_module() -> std::vector<std::uint32_t>
{
  const auto instructions = trace_with_uniform<Element, Uniform, Operation>();
  return splice<Element>(
    transform_template<Element>(),
    std::array {
      symbol_trace {
        .symbol       = operation_symbol,
        .instructions = instructions,
      },
    });
}

} // namespace detail

export template<scalar Element, scalar Uniform, class Operation>
  requires uniform_traceable<Operation, Element, Uniform>
inline constexpr std::size_t transform_with_word_count =
  detail::transform_with_module<Element, Uniform, Operation>().size();

export
template<scalar Element, scalar Uniform, class Operation>
  requires uniform_traceable<Operation, Element, Uniform>
consteval auto
compile(transform_with_stage<Uniform, Operation>) -> std::array<
  std::uint32_t,
  transform_with_word_count<Element, Uniform, Operation>
>
{
  const auto words =
    detail::transform_with_module<Element, Uniform, Operation>();
  std::array<
    std::uint32_t,
    transform_with_word_count<Element, Uniform, Operation>
  >
    result {};
  std::ranges::copy(words, result.begin());
  return result;
}

namespace detail
{

template<scalar Element, class Compare>
  requires compare_operation<Compare>
consteval
auto
predicate_words() -> std::span<const std::uint32_t>
{
  if constexpr (std::same_as<Compare, greater>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_greater_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_greater_int_words;
    }
    else
    {
      return predicate_greater_uint_words;
    }
  }
  else if constexpr (std::same_as<Compare, greater_equal>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_greater_equal_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_greater_equal_int_words;
    }
    else
    {
      return predicate_greater_equal_uint_words;
    }
  }
  else if constexpr (std::same_as<Compare, less>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_less_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_less_int_words;
    }
    else
    {
      return predicate_less_uint_words;
    }
  }
  else if constexpr (std::same_as<Compare, less_equal>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_less_equal_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_less_equal_int_words;
    }
    else
    {
      return predicate_less_equal_uint_words;
    }
  }
  else if constexpr (std::same_as<Compare, equal>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_equal_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_equal_int_words;
    }
    else
    {
      return predicate_equal_uint_words;
    }
  }
  else if constexpr (std::same_as<Compare, not_equal>)
  {
    if constexpr (std::same_as<Element, float>)
    {
      return predicate_not_equal_float_words;
    }
    else if constexpr (std::same_as<Element, std::int32_t>)
    {
      return predicate_not_equal_int_words;
    }
    else
    {
      return predicate_not_equal_uint_words;
    }
  }
}

} // namespace detail

export template<scalar Element, class Compare>
  requires compare_operation<Compare>
inline constexpr std::size_t predicate_word_count =
  detail::predicate_words<Element, Compare>().size();

export
template<scalar Element, class Compare, class Threshold>
  requires (compare_operation<Compare> && std::same_as<Threshold, Element>)
consteval auto
compile(predicate_stage<Compare, Threshold>)
  -> std::array<std::uint32_t, predicate_word_count<Element, Compare>>
{
  auto words = detail::predicate_words<Element, Compare>();
  std::array<std::uint32_t, predicate_word_count<Element, Compare>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

namespace detail
{

template<class Key>
  requires traceable<Key, std::uint32_t>
constexpr
auto
tile_histogram_module() -> std::vector<std::uint32_t>
{
  const auto instructions = trace<std::uint32_t, Key>();
  return splice<std::uint32_t>(
    tile_histogram_template(),
    std::array {
      symbol_trace {key_symbol, instructions},
    });
}

template<class Key>
  requires traceable<Key, std::uint32_t>
constexpr
auto
stable_scatter_module() -> std::vector<std::uint32_t>
{
  const auto instructions = trace<std::uint32_t, Key>();
  return splice<std::uint32_t>(
    stable_scatter_template(),
    std::array {
      symbol_trace {key_symbol, instructions},
    });
}

} // namespace detail

export template<class Key>
  requires traceable<Key, std::uint32_t>
inline constexpr std::size_t tile_histogram_word_count =
  detail::tile_histogram_module<Key>().size();

export
template<scalar Element, std::uint32_t Buckets, class Key>
  requires (
    std::same_as<Element, std::uint32_t> &&
    traceable<Key, Element> &&
    (Buckets >= 1U))
consteval auto
compile(tile_histogram_stage<Buckets, Key>)
  -> std::array<std::uint32_t, tile_histogram_word_count<Key>>
{
  const auto words = detail::tile_histogram_module<Key>();
  std::array<std::uint32_t, tile_histogram_word_count<Key>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

export template<class Key>
  requires traceable<Key, std::uint32_t>
inline constexpr std::size_t stable_scatter_word_count =
  detail::stable_scatter_module<Key>().size();

export
template<scalar Element, std::uint32_t Buckets, class Key>
  requires (
    std::same_as<Element, std::uint32_t> &&
    traceable<Key, Element> &&
    (Buckets >= 1U))
consteval auto
compile(stable_scatter_stage<Buckets, Key>)
  -> std::array<std::uint32_t, stable_scatter_word_count<Key>>
{
  const auto words = detail::stable_scatter_module<Key>();
  std::array<std::uint32_t, stable_scatter_word_count<Key>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
