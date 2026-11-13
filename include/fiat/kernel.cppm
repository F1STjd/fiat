export module fiat:kernel;

import std;

import :trace;
import :emit;
import :templates;
import :splice;
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

export struct reduce_push
{
  std::uint64_t                                input {};
  std::uint64_t                                scratch {};
  std::uint32_t                                count {};
  std::uint32_t                                identity {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

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
static_assert(
  sizeof(detail::reduce_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(detail::reduce_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

export inline constexpr std::size_t scan_argument_capacity {
  argument_capacity - 2UZ
};
static_assert(
  scan_argument_capacity == 24UZ,
  "the scan output pointer takes the last two of the 26 argument words");

namespace detail
{

export struct scan_push
{
  std::uint64_t                                     input {};
  std::uint64_t                                     scratch {};
  std::uint32_t                                     count {};
  std::uint32_t                                     identity {};
  std::array<std::uint32_t, scan_argument_capacity> arguments {};
  std::uint64_t                                     output {};
};

} // namespace detail

static_assert(
  detail::argument_offset + scan_argument_capacity * sizeof(std::uint32_t) ==
    120UZ,
  "the scan output pointer is push member 5 at offset 120");
static_assert(
  sizeof(detail::scan_push) == 128UZ,
  "fits the 128 bytes every Vulkan device guarantees");

namespace detail
{

export struct scatter_push
{
  std::uint64_t input {};
  std::uint64_t output {};
  std::uint32_t count {};
  std::uint32_t first {};
  std::uint64_t flags {};
  std::uint64_t indices {};
  std::uint64_t count_out {};
};

} // namespace detail

static_assert(
  sizeof(detail::scatter_push) == 48UZ,
  "fits the 128 bytes every Vulkan device guarantees");

namespace detail
{

export struct histogram_push
{
  std::uint64_t                                input {};
  std::uint64_t                                counts {};
  std::uint32_t                                count {};
  std::uint32_t                                first {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

} // namespace detail

static_assert(
  sizeof(detail::histogram_push) ==
    detail::argument_offset + argument_capacity * sizeof(std::uint32_t),
  "matches the template push block");
static_assert(
  sizeof(detail::histogram_push) == 128UZ,
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

export template<class Operation>
struct transform_stage
{
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
  typename arguments_of<Operation>::type arguments {};
  Identity                               identity {};
  typename arguments_of<Epilogue>::type  epilogue_arguments {};
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
  typename arguments_of<Operation>::type arguments {};
  Identity                               identity {};
  typename arguments_of<Epilogue>::type  epilogue_arguments {};
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

export struct scatter_stage {};

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

export template<std::uint32_t Buckets, histogram_mode Mode, class Key>
struct histogram_stage
{
  static constexpr std::uint32_t  buckets {Buckets};
  static constexpr histogram_mode mode {Mode};
  using key_type = Key;
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
  static_assert(Buckets >= 1U, "fiat::histogram needs at least one bucket");
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
  return {
    .arguments          = {},
    .identity           = identity,
    .epilogue_arguments = {},
  };
}

export
template<class... Left, class Combine, class Identity>
constexpr auto
operator |(
  transform_stage<chain<Left...>>          left,
  reduce_stage<chain<>, Combine, Identity> right)
  -> reduce_stage<chain<Left...>, Combine, Identity>
{
  return {
    .arguments          = left.arguments,
    .identity           = right.identity,
    .epilogue_arguments = {},
  };
}

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
  return {
    .arguments          = {},
    .identity           = identity,
    .epilogue_arguments = {},
  };
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
  return {
    .arguments          = {},
    .identity           = identity,
    .epilogue_arguments = {},
  };
}

export
template<class... Left, class Combine, class Identity, scan_mode Mode>
constexpr auto
operator |(
  transform_stage<chain<Left...>>              left,
  scan_stage<chain<>, Combine, Identity, Mode> right)
  -> scan_stage<chain<Left...>, Combine, Identity, Mode>
{
  static_assert(
    std::tuple_size_v<typename chain<Left...>::argument_tuple> <=
      scan_argument_capacity,
    "a fiat::transform fused into a scan takes at most 24 runtime arguments");
  return {
    .arguments          = left.arguments,
    .identity           = right.identity,
    .epilogue_arguments = {},
  };
}

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

template<
  scalar Element,
  class Operation,
  class Combine,
  class Epilogue = identity
>
  requires (
    traceable<Operation, Element> &&
    combinable<Combine, Element> &&
    traceable<Epilogue, Element>)
constexpr
auto
reduce_module() -> std::vector<std::uint32_t>
{
  constexpr auto map_slots =
    std::tuple_size_v<typename arguments_of<Operation>::type>;
  constexpr auto epilogue_slots =
    std::tuple_size_v<typename arguments_of<Epilogue>::type>;
  static_assert(
    map_slots + epilogue_slots <= argument_capacity,
    "fiat::reduce map and epilogue arguments together take at most 26 runtime "
    "slots");

  const std::vector<instruction> map_instructions = trace<Element, Operation>();
  const std::vector<instruction> combine_instructions =
    trace_binary<Element, Combine>();
  const std::vector<instruction> epilogue_instructions =
    trace_from_slot<Element, Epilogue, map_slots>();
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
      symbol_trace {
        .symbol       = epilogue_symbol,
        .instructions = epilogue_instructions,
      },
    });
}

} // namespace detail

export template<
  scalar Element,
  class Operation,
  class Combine,
  class Epilogue = identity
>
  requires (traceable<Operation, Element> &&
             combinable<Combine, Element> &&
             traceable<Epilogue, Element>)
inline constexpr std::size_t reduce_word_count =
  detail::reduce_module<Element, Operation, Combine, Epilogue>().size();

export
template<
  scalar Element,
  class Operation,
  class Combine,
  class Identity,
  class Epilogue = identity
>
  requires (
    traceable<Operation, Element> &&
    combinable<Combine, Element> &&
    std::same_as<Identity, Element> &&
    traceable<Epilogue, Element>)
consteval auto
compile(reduce_stage<Operation, Combine, Identity, Epilogue>) -> std::array<
  std::uint32_t,
  reduce_word_count<Element, Operation, Combine, Epilogue>
>
{
  const std::vector<std::uint32_t> words =
    detail::reduce_module<Element, Operation, Combine, Epilogue>();
  std::array<
    std::uint32_t,
    reduce_word_count<Element, Operation, Combine, Epilogue>
  >
    result {};
  std::ranges::copy(words, result.begin());
  return result;
}

namespace detail
{

template<
  scalar Element,
  class Operation,
  class Combine,
  scan_mode Mode,
  class Epilogue = fiat::identity
>
  requires (
    traceable<Operation, Element> &&
    combinable<Combine, Element> &&
    traceable<Epilogue, Element>)
constexpr
auto
scan_module() -> std::vector<std::uint32_t>
{
  constexpr auto map_slots =
    std::tuple_size_v<typename arguments_of<Operation>::type>;
  constexpr auto epilogue_slots =
    std::tuple_size_v<typename arguments_of<Epilogue>::type>;
  static_assert(
    map_slots + epilogue_slots <= scan_argument_capacity,
    "fiat scan map and epilogue arguments together take at most 24 runtime "
    "slots");

  const std::vector<instruction> map_instructions = trace<Element, Operation>();
  const std::vector<instruction> combine_instructions =
    trace_binary<Element, Combine>();
  const std::vector<instruction> epilogue_instructions =
    trace_from_slot<Element, Epilogue, map_slots>();

  return splice<Element>(
    scan_template<Element, Mode>(),
    std::array {
      symbol_trace {
        .symbol       = operation_symbol,
        .instructions = map_instructions,
      },
      symbol_trace {
        .symbol       = combine_symbol,
        .instructions = combine_instructions,
      },
      symbol_trace {
        .symbol       = epilogue_symbol,
        .instructions = epilogue_instructions,
      },
    });
}

} // namespace detail

export template<
  scalar Element,
  class Operation,
  class Combine,
  scan_mode Mode,
  class Epilogue = fiat::identity
>
  requires (traceable<Operation, Element> &&
             combinable<Combine, Element> &&
             traceable<Epilogue, Element>)
inline constexpr std::size_t scan_word_count =
  detail::scan_module<Element, Operation, Combine, Mode, Epilogue>().size();

export
template<
  scalar Element,
  class Operation,
  class Combine,
  class Identity,
  scan_mode Mode,
  class Epilogue
>
  requires (
    traceable<Operation, Element> &&
    combinable<Combine, Element> &&
    std::same_as<Identity, Element> &&
    traceable<Epilogue, Element>)
consteval auto
compile(scan_stage<Operation, Combine, Identity, Mode, Epilogue>) -> std::array<
  std::uint32_t,
  scan_word_count<Element, Operation, Combine, Mode, Epilogue>
>
{
  const std::vector<std::uint32_t> words =
    detail::scan_module<Element, Operation, Combine, Mode, Epilogue>();
  std::array<
    std::uint32_t,
    scan_word_count<Element, Operation, Combine, Mode, Epilogue>
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

template<scalar Element>
consteval
auto
scatter_words() -> std::span<const std::uint32_t>
{
  if constexpr (std::same_as<Element, float>) { return scatter_float_words; }
  else if constexpr (std::same_as<Element, std::int32_t>)
  {
    return scatter_int_words;
  }
  else
  {
    return scatter_uint_words;
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

export template<scalar Element>
inline constexpr std::size_t scatter_word_count =
  detail::scatter_words<Element>().size();

export
template<scalar Element>
consteval auto
compile(scatter_stage) -> std::array<std::uint32_t, scatter_word_count<Element>>
{
  const auto words = detail::scatter_words<Element>();
  std::array<std::uint32_t, scatter_word_count<Element>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

namespace detail
{

template<class Key>
  requires traceable<Key, std::uint32_t>
constexpr
auto
histogram_module() -> std::vector<std::uint32_t>
{
  const auto instructions = trace<std::uint32_t, Key>();
  return splice<std::uint32_t>(
    histogram_template(),
    std::array {
      symbol_trace {key_symbol, instructions},
    });
}

} // namespace detail

export template<class Key>
  requires traceable<Key, std::uint32_t>
inline constexpr std::size_t histogram_word_count =
  detail::histogram_module<Key>().size();

export
template<scalar Element, std::uint32_t Buckets, histogram_mode Mode, class Key>
  requires (
    std::same_as<Element, std::uint32_t> &&
    traceable<Key, Element> &&
    (Buckets >= 1U))
consteval auto
compile(histogram_stage<Buckets, Mode, Key>)
  -> std::array<std::uint32_t, histogram_word_count<Key>>
{
  const auto words = detail::histogram_module<Key>();
  std::array<std::uint32_t, histogram_word_count<Key>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
