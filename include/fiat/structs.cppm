module;

#include <cstddef>

#if defined(__cpp_impl_reflection)
  #include <meta>
#endif

export module fiat:structs;

import std;
import :trace;
import :emit;
import :spirv;
import :kernel;

namespace fiat
{

inline constexpr std::size_t soa_alignment {256UZ};
inline constexpr std::size_t soa_leaf_limit {8UZ};

export template<class T>
concept soa_element = std::is_aggregate_v<T> && !std::is_polymorphic_v<T>;

namespace detail
{

consteval
auto
align_up(std::size_t value, std::size_t alignment) -> std::size_t
{ return (value + alignment - 1UZ) / alignment * alignment; }

#if defined(__cpp_impl_reflection)

template<soa_element T>
inline constexpr std::size_t leaf_count_v = [] consteval {
  std::size_t count {};
  template for (
    constexpr auto member : std::meta::nonstatic_data_members_of(
      ^^T, std::meta::access_context::current()))
  {
    using leaf = [:std::meta::type_of(member):];
    static_assert(scalar<leaf>);
    ++count;
  }
  return count;
}();

template<soa_element T>
inline constexpr std::array<scalar_kind, leaf_count_v<T>> leaf_kinds_v =
  [] consteval {
    std::array<scalar_kind, leaf_count_v<T>> kinds {};
    std::size_t                              index {};
    template for (
      constexpr auto member : std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current()))
    {
      using leaf       = [:std::meta::type_of(member):];
      kinds[ index++ ] = kind_of<leaf>;
    }
    return kinds;
  }();

template<soa_element T>
consteval
auto
region_offset(std::uint32_t count, std::size_t leaf) -> std::uint64_t
{
  const auto stride =
    static_cast<std::uint64_t>(align_up(count * 4UZ, soa_alignment));
  return stride * static_cast<std::uint64_t>(leaf);
}

export
template<soa_element T>
consteval auto
soa_bytes(std::uint32_t count) -> std::uint64_t
{ return region_offset<T>(count, leaf_count_v<T>); }

#else

template<soa_element T>
inline constexpr std::size_t leaf_count_v {};

template<soa_element T>
inline constexpr std::array<scalar_kind, 0UZ> leaf_kinds_v {};

template<soa_element T>
consteval
auto
region_offset(std::uint32_t, std::size_t) -> std::uint64_t
{ return {}; }

export
template<soa_element T>
consteval auto
soa_bytes(std::uint32_t) -> std::uint64_t
{ return {}; }

#endif

static_assert(soa_leaf_limit >= 1UZ);

export struct foreach_push
{
  std::uint64_t                                base {};
  std::uint64_t                                unused {};
  std::uint32_t                                count {};
  std::uint32_t                                first {};
  std::array<std::uint32_t, argument_capacity> arguments {};
};

static_assert(sizeof(foreach_push) == 128UZ, "same budget as transform_push");
static_assert(
  offsetof(foreach_push, arguments) == argument_offset,
  "argument block stays at byte 24");

} // namespace detail

export template<class Operation>
struct foreach_stage
{
  typename arguments_of<Operation>::type arguments {};
};

#if defined(__cpp_impl_reflection)

namespace detail
{

template<soa_element T>
struct traced_struct_impl
{
  struct type;

  consteval
  {
    std::vector<std::meta::info> members {};
    template for (
      constexpr auto member : std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unchecked()))
    {
      using leaf_type = [:std::meta::type_of(member):];
      members.push_back(
        std::meta::data_member_spec(
          ^^value<leaf_type>, {.name = std::meta::identifier_of(member)}));
    }
    std::meta::define_aggregate(^^type, members);
  }
};

} // namespace detail

export template<soa_element T>
using traced_struct = typename detail::traced_struct_impl<T>::type;

#else

namespace detail
{

template<soa_element T>
struct traced_struct_impl
{
  struct type {};
};

} // namespace detail

export template<soa_element T>
using traced_struct = typename detail::traced_struct_impl<T>::type;

#endif

export template<class Operation, class Element, class... Arguments>
concept foreach_traceable_call =
  soa_element<Element> &&
  std::is_empty_v<Operation> &&
  std::default_initializable<Operation> &&
  (scalar<Arguments> && ...) &&
  std::invocable<Operation, traced_struct<Element>&, const value<Arguments>&...> &&
  (std::same_as<
     std::invoke_result_t<
       Operation,
       traced_struct<Element>&,
       const value<Arguments>&...
     >,
     traced_struct<Element>
   > ||
    std::is_void_v<std::invoke_result_t<
      Operation,
      traced_struct<Element>&,
      const value<Arguments>&...
    >>);

template<class Operation, class Element, class Arguments>
inline constexpr bool foreach_traceable_arguments {false};

template<class Operation, class Element, class... Arguments>
inline constexpr bool
  foreach_traceable_arguments<Operation, Element, std::tuple<Arguments...>> =
    foreach_traceable_call<Operation, Element, Arguments...>;

export template<class Operation, class Element>
concept foreach_traceable =
  soa_element<Element> &&
  foreach_traceable_arguments<
    Operation,
    Element,
    typename arguments_of<Operation>::type
  >;

export template<class Operation, class... Arguments>
struct bound_foreach
{
  using argument_tuple = std::tuple<Arguments...>;

  template<soa_element Element>
    requires foreach_traceable_call<Operation, Element, Arguments...>
  constexpr
  auto
  operator ()(
    [[maybe_unused]] traced_struct<Element>& operand,
    [[maybe_unused]] const value<Arguments>&... arguments) const
  {
#if defined(__cpp_impl_reflection)
    if constexpr (
      std::is_void_v<std::invoke_result_t<
        Operation,
        traced_struct<Element>&,
        const value<Arguments>&...
      >>)
    {
      Operation {}(operand, arguments...);
    }
    else
    {
      operand = Operation {}(operand, arguments...);
    }
#endif
  }
};

export
template<class Operation, class... Arguments>
  requires (
    (scalar<Arguments> && ...) && sizeof...(Arguments) <= argument_capacity)
constexpr auto
for_each(Operation, Arguments... arguments)
  -> foreach_stage<bound_foreach<Operation, Arguments...>>
{
  static_assert(
    std::is_empty_v<Operation> && std::default_initializable<Operation>,
    "fiat::for_each cannot read lambda captures; pass runtime values as "
    "arguments: fiat::for_each(f, value)");
  static_assert(
    (scalar<Arguments> && ...),
    "fiat::for_each arguments must be float, std::int32_t or std::uint32_t");
  return {
    .arguments =
      typename arguments_of<bound_foreach<Operation, Arguments...>>::type {
        arguments...,
      },
  };
}

namespace detail
{

#if defined(__cpp_impl_reflection)

template<soa_element Element, std::size_t Index>
consteval
auto
bind_leaf(recorder& target, std::uint32_t id)
{
  constexpr auto member = std::meta::nonstatic_data_members_of(
    ^^Element, std::meta::access_context::unchecked())[ Index ];
  using leaf = [:std::meta::type_of(member):];
  return value<leaf> {target, id};
}

template<soa_element Element, std::size_t... Indices>
consteval
auto
bind_traced_struct_from(
  recorder&                                      target,
  std::span<const std::uint32_t, soa_leaf_limit> leaf_ids,
  std::index_sequence<Indices...>) -> traced_struct<Element>
{
  return traced_struct<Element> {
    bind_leaf<Element, Indices>(target, leaf_ids[ Indices ])...,
  };
}

template<soa_element Element>
consteval
auto
bind_traced_struct(
  recorder& target, std::span<const std::uint32_t, soa_leaf_limit> leaf_ids)
  -> traced_struct<Element>
{
  return bind_traced_struct_from<Element>(
    target, leaf_ids, std::make_index_sequence<leaf_count_v<Element>> {});
}

template<class... Arguments, std::size_t... Slots>
consteval
auto
bind_traced_arguments_from(
  recorder& target,
  std::type_identity<std::tuple<Arguments...>>,
  std::index_sequence<Slots...>) -> std::tuple<value<Arguments>...>
{
  return std::tuple<value<Arguments>...> {
    value<Arguments> {
      target,
      target.emit_argument(kind_of<Arguments>, static_cast<std::uint32_t>(Slots))
    }...,
  };
}

template<class... Arguments>
consteval
auto
bind_traced_arguments(
  recorder& target, std::type_identity<std::tuple<Arguments...>> identity)
  -> std::tuple<value<Arguments>...>
{
  return bind_traced_arguments_from(
    target, identity, std::index_sequence_for<Arguments...> {});
}

template<soa_element Element>
consteval
auto
traced_leaf_id(const traced_struct<Element>& operand, std::size_t leaf)
  -> std::uint32_t
{
  std::size_t index {};
  template for (
    constexpr auto member : std::meta::nonstatic_data_members_of(
      ^^typename traced_struct_impl<Element>::type,
      std::meta::access_context::unchecked()))
  {
    if (index == leaf) { return operand.[:member:].id(); }
    ++index;
  }
  return {};
}

template<soa_element Element, class Operation>
  requires foreach_traceable<Operation, Element>
consteval
auto
trace_foreach() -> std::vector<instruction>
{
  using argument_tuple  = typename arguments_of<Operation>::type;
  constexpr auto leaves = leaf_count_v<Element>;

  recorder                                  target {};
  std::array<std::uint32_t, soa_leaf_limit> leaf_ids {};
  for (auto leaf : std::views::indices(leaves))
  {
    leaf_ids[ leaf ] = target.emit_parameter(leaf_kinds_v<Element>[ leaf ]);
  }
  auto       operand = bind_traced_struct<Element>(target, leaf_ids);
  const auto arguments =
    bind_traced_arguments(target, std::type_identity<argument_tuple> {});
  std::apply(
    [ & ](const auto&... argument) {
      if constexpr (
        std::is_void_v<std::invoke_result_t<
          Operation,
          traced_struct<Element>&,
          decltype(argument)...
        >>)
      {
        Operation {}(operand, argument...);
      }
      else
      {
        operand = Operation {}(operand, argument...);
      }
    },
    arguments);

  for (auto leaf : std::views::indices(leaves))
  {
    target.emit_result(
      leaf_kinds_v<Element>[ leaf ], traced_leaf_id<Element>(operand, leaf));
  }
  return std::move(target).instructions();
}

template<soa_element Element, class Operation>
  requires foreach_traceable<Operation, Element>
consteval
auto
foreach_module() -> std::vector<std::uint32_t>
{
  constexpr auto leaves = leaf_count_v<Element>;
  static_assert(leaves > 0UZ && leaves <= soa_leaf_limit);

  const auto instructions = trace_foreach<Element, Operation>();

  std::uint32_t next_id {1U};
  const auto    advance_id = [ & ] -> std::uint32_t { return next_id++; };

  const std::uint32_t id_void            = advance_id();
  const std::uint32_t id_bool            = advance_id();
  const std::uint32_t id_float           = advance_id();
  const std::uint32_t id_int             = advance_id();
  const std::uint32_t id_uint            = advance_id();
  const std::uint32_t id_ulong           = advance_id();
  const std::uint32_t id_uint3           = advance_id();
  const std::uint32_t id_arg_count       = advance_id();
  const std::uint32_t id_arg_array       = advance_id();
  const std::uint32_t id_push_type       = advance_id();
  const std::uint32_t id_push_ptr        = advance_id();
  const std::uint32_t id_push_ulong_ptr  = advance_id();
  const std::uint32_t id_push_uint_ptr   = advance_id();
  const std::uint32_t id_float_psb_ptr   = advance_id();
  const std::uint32_t id_int_psb_ptr     = advance_id();
  const std::uint32_t id_uint_psb_ptr    = advance_id();
  const std::uint32_t id_input_uint3_ptr = advance_id();
  const std::uint32_t id_main_type       = advance_id();
  const std::uint32_t id_zero            = advance_id();
  const std::uint32_t id_one             = advance_id();
  const std::uint32_t id_two             = advance_id();
  const std::uint32_t id_three           = advance_id();
  const std::uint32_t id_four            = advance_id();
  const std::uint32_t id_c255            = advance_id();
  const std::uint32_t id_align_mask      = advance_id();
  const std::uint32_t id_group_size      = advance_id();
  const std::uint32_t id_global_id       = advance_id();
  const std::uint32_t id_push            = advance_id();
  const std::uint32_t id_main            = advance_id();
  const std::uint32_t id_entry           = advance_id();
  const std::uint32_t id_body            = advance_id();
  const std::uint32_t id_done            = advance_id();
  const std::uint32_t id_glsl            = advance_id();

  std::array<std::uint32_t, soa_leaf_limit> id_leaf_ptr {};
  std::array<std::uint32_t, soa_leaf_limit> id_addr {};
  std::array<std::uint32_t, soa_leaf_limit> id_index_x4 {};
  std::array<std::uint32_t, soa_leaf_limit> id_index_x4_ul {};
  std::array<std::uint32_t, soa_leaf_limit> id_region_u {};
  std::array<std::uint32_t, soa_leaf_limit> id_region_ul {};
  std::array<std::uint32_t, soa_leaf_limit> id_base_plus {};
  std::array<std::uint32_t, soa_leaf_limit> id_leaf_const {};

  for (auto leaf : std::views::indices(leaves))
  {
    id_leaf_ptr[ leaf ]    = advance_id();
    id_addr[ leaf ]        = advance_id();
    id_index_x4[ leaf ]    = advance_id();
    id_index_x4_ul[ leaf ] = advance_id();
    id_region_u[ leaf ]    = advance_id();
    id_region_ul[ leaf ]   = advance_id();
    id_base_plus[ leaf ]   = advance_id();
    id_leaf_const[ leaf ]  = advance_id();
  }

  const std::uint32_t id_invocation   = advance_id();
  const std::uint32_t id_invocation_x = advance_id();
  const std::uint32_t id_first_member = advance_id();
  const std::uint32_t id_first        = advance_id();
  const std::uint32_t id_index        = advance_id();
  const std::uint32_t id_count_member = advance_id();
  const std::uint32_t id_count        = advance_id();
  const std::uint32_t id_in_range     = advance_id();
  const std::uint32_t id_base_member  = advance_id();
  const std::uint32_t id_base         = advance_id();
  const std::uint32_t id_count_x4     = advance_id();
  const std::uint32_t id_count_x4_255 = advance_id();
  const std::uint32_t id_stride       = advance_id();

  std::uint32_t constant_temps =
    std::ranges::count_if(instructions, [](const instruction& item) -> bool {
      return item.code == operation::constant &&
             item.kind == scalar_kind::float32;
    });

  const std::uint32_t id_const_bits_base = advance_id();

  for (auto _ : std::views::iota(1U, constant_temps)) { advance_id(); }

  std::uint32_t trace_high {1U};
  for (const instruction& item : instructions)
  {
    trace_high = std::max({trace_high, item.result, item.left, item.literal});
  }
  const std::uint32_t layout_offset {next_id - 1U};
  const std::uint32_t bound {next_id + trace_high};
  spirv::writer       out {bound};

  const auto mapped_id =
    [ layout_offset ](std::uint32_t trace_id) -> std::uint32_t {
    return trace_id + layout_offset;
  };

  const auto type_of = [ & ](scalar_kind kind) -> std::uint32_t {
    switch (kind)
    {
    case scalar_kind::float32 : return id_float;
    case scalar_kind::int32   : return id_int;
    case scalar_kind::uint32  : return id_uint;
    }
    return id_uint;
  };

  const auto ptr_of = [ & ](scalar_kind kind) -> std::uint32_t {
    switch (kind)
    {
    case scalar_kind::float32 : return id_float_psb_ptr;
    case scalar_kind::int32   : return id_int_psb_ptr;
    case scalar_kind::uint32  : return id_uint_psb_ptr;
    }
    return id_uint_psb_ptr;
  };

  out.emit(
    spirv::opcode::capability, {std::to_underlying(spirv::capability::shader)});
  out.emit(
    spirv::opcode::capability,
    {std::to_underlying(spirv::capability::physical_storage_buffer_addresses)});
  out.emit(
    spirv::opcode::capability, {std::to_underlying(spirv::capability::int64)});

  {
    auto                       name = spirv::literal_string("GLSL.std.450");
    std::vector<std::uint32_t> operands {id_glsl};
    operands.append_range(name);
    out.emit(spirv::opcode::ext_inst_import, operands);
  }

  out.emit(
    spirv::opcode::memory_model,
    {
      std::to_underlying(spirv::addressing_model::physical_storage_buffer_64),
      std::to_underlying(spirv::memory_model::glsl450),
    });

  {
    auto                       name = spirv::literal_string("fiat_foreach");
    std::vector<std::uint32_t> operands {
      std::to_underlying(spirv::execution_model::gl_compute),
      id_main,
    };
    operands.append_range(name);
    operands.push_back(id_global_id);
    operands.push_back(id_push);
    out.emit(spirv::opcode::entry_point, operands);
  }

  out.emit(
    spirv::opcode::execution_mode_id,
    {
      id_main,
      std::to_underlying(spirv::execution_mode::local_size_id),
      id_group_size,
      id_one,
      id_one,
    });

  out.emit(
    spirv::opcode::decorate,
    {
      id_global_id,
      std::to_underlying(spirv::decoration::builtin),
      std::to_underlying(spirv::builtin::global_invocation_id),
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_group_size,
      std::to_underlying(spirv::decoration::spec_id),
      0U,
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_arg_array,
      std::to_underlying(spirv::decoration::array_stride),
      4U,
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_float_psb_ptr,
      std::to_underlying(spirv::decoration::array_stride),
      4U,
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_int_psb_ptr,
      std::to_underlying(spirv::decoration::array_stride),
      4U,
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_uint_psb_ptr,
      std::to_underlying(spirv::decoration::array_stride),
      4U,
    });
  out.emit(
    spirv::opcode::decorate,
    {
      id_push_type,
      std::to_underlying(spirv::decoration::block),
    });
  out.emit(
    spirv::opcode::member_decorate,
    {
      id_push_type,
      0U,
      std::to_underlying(spirv::decoration::offset),
      0U,
    });
  out.emit(
    spirv::opcode::member_decorate,
    {
      id_push_type,
      1U,
      std::to_underlying(spirv::decoration::offset),
      8U,
    });
  out.emit(
    spirv::opcode::member_decorate,
    {
      id_push_type,
      2U,
      std::to_underlying(spirv::decoration::offset),
      16U,
    });
  out.emit(
    spirv::opcode::member_decorate,
    {
      id_push_type,
      3U,
      std::to_underlying(spirv::decoration::offset),
      20U,
    });
  out.emit(
    spirv::opcode::member_decorate,
    {
      id_push_type,
      4U,
      std::to_underlying(spirv::decoration::offset),
      24U,
    });

  out.emit(spirv::opcode::type_void, {id_void});
  out.emit(spirv::opcode::type_bool, {id_bool});
  out.emit(spirv::opcode::type_float, {id_float, 32U});
  out.emit(spirv::opcode::type_int, {id_int, 32U, 1U});
  out.emit(spirv::opcode::type_int, {id_uint, 32U, 0U});
  out.emit(spirv::opcode::type_int, {id_ulong, 64U, 0U});
  out.emit(spirv::opcode::type_vector, {id_uint3, id_uint, 3U});
  out.emit(spirv::opcode::constant, {id_uint, id_arg_count, 26U});
  out.emit(spirv::opcode::type_array, {id_arg_array, id_uint, id_arg_count});
  out.emit(
    spirv::opcode::type_struct,
    {id_push_type, id_ulong, id_ulong, id_uint, id_uint, id_arg_array});
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_push_ptr,
      std::to_underlying(spirv::storage_class::push_constant),
      id_push_type,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_push_ulong_ptr,
      std::to_underlying(spirv::storage_class::push_constant),
      id_ulong,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_push_uint_ptr,
      std::to_underlying(spirv::storage_class::push_constant),
      id_uint,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_float_psb_ptr,
      std::to_underlying(spirv::storage_class::physical_storage_buffer),
      id_float,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_int_psb_ptr,
      std::to_underlying(spirv::storage_class::physical_storage_buffer),
      id_int,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_uint_psb_ptr,
      std::to_underlying(spirv::storage_class::physical_storage_buffer),
      id_uint,
    });
  out.emit(
    spirv::opcode::type_pointer,
    {
      id_input_uint3_ptr,
      std::to_underlying(spirv::storage_class::input),
      id_uint3,
    });
  out.emit(spirv::opcode::type_function, {id_main_type, id_void});

  out.emit(spirv::opcode::constant, {id_uint, id_zero, 0U});
  out.emit(spirv::opcode::constant, {id_uint, id_one, 1U});
  out.emit(spirv::opcode::constant, {id_uint, id_two, 2U});
  out.emit(spirv::opcode::constant, {id_uint, id_three, 3U});
  out.emit(spirv::opcode::constant, {id_uint, id_four, 4U});
  out.emit(spirv::opcode::constant, {id_uint, id_c255, 255U});
  out.emit(spirv::opcode::constant, {id_uint, id_align_mask, 0xFF'FF'FF'00U});
  out.emit(spirv::opcode::spec_constant, {id_uint, id_group_size, 256U});

  for (auto leaf : std::views::indices(leaves))
  {
    out.emit(
      spirv::opcode::constant,
      {id_uint, id_leaf_const[ leaf ], static_cast<std::uint32_t>(leaf)});
  }

  out.emit(
    spirv::opcode::variable,
    {
      id_input_uint3_ptr,
      id_global_id,
      std::to_underlying(spirv::storage_class::input),
    });
  out.emit(
    spirv::opcode::variable,
    {
      id_push_ptr,
      id_push,
      std::to_underlying(spirv::storage_class::push_constant),
    });

  out.emit(
    spirv::opcode::function,
    {
      id_void,
      id_main,
      std::to_underlying(spirv::function_control::none),
      id_main_type,
    });
  out.emit(spirv::opcode::label, {id_entry});

  out.emit(spirv::opcode::load, {id_uint3, id_invocation, id_global_id});
  out.emit(
    spirv::opcode::composite_extract,
    {id_uint, id_invocation_x, id_invocation, 0U});
  out.emit(
    spirv::opcode::access_chain,
    {id_push_uint_ptr, id_first_member, id_push, id_three});
  out.emit(spirv::opcode::load, {id_uint, id_first, id_first_member});
  out.emit(spirv::opcode::iadd, {id_uint, id_index, id_first, id_invocation_x});
  out.emit(
    spirv::opcode::access_chain,
    {id_push_uint_ptr, id_count_member, id_push, id_two});
  out.emit(spirv::opcode::load, {id_uint, id_count, id_count_member});
  out.emit(
    spirv::opcode::uless_than, {id_bool, id_in_range, id_index, id_count});
  out.emit(
    spirv::opcode::selection_merge,
    {id_done, std::to_underlying(spirv::selection_control::none)});
  out.emit(spirv::opcode::branch_conditional, {id_in_range, id_body, id_done});

  out.emit(spirv::opcode::label, {id_body});
  out.emit(
    spirv::opcode::access_chain,
    {id_push_ulong_ptr, id_base_member, id_push, id_zero});
  out.emit(spirv::opcode::load, {id_ulong, id_base, id_base_member});

  out.emit(spirv::opcode::imul, {id_uint, id_count_x4, id_count, id_four});
  out.emit(
    spirv::opcode::iadd, {id_uint, id_count_x4_255, id_count_x4, id_c255});
  out.emit(
    spirv::opcode::bitwise_and,
    {id_uint, id_stride, id_count_x4_255, id_align_mask});

  std::size_t                               leaf_param {};
  std::array<std::uint32_t, soa_leaf_limit> id_store_ptr {};
  for (const instruction& item : instructions)
  {
    if (item.code != operation::parameter) { continue; }
    if (leaf_param >= leaves) { continue; }
    const std::size_t   leaf {leaf_param++};
    const scalar_kind   kind {leaf_kinds_v<Element>[ leaf ]};
    const std::uint32_t dest {mapped_id(item.result)};

    out.emit(
      spirv::opcode::imul,
      {id_uint, id_region_u[ leaf ], id_stride, id_leaf_const[ leaf ]});
    out.emit(
      spirv::opcode::uconvert,
      {id_ulong, id_region_ul[ leaf ], id_region_u[ leaf ]});
    out.emit(
      spirv::opcode::imul, {id_uint, id_index_x4[ leaf ], id_index, id_four});
    out.emit(
      spirv::opcode::uconvert,
      {id_ulong, id_index_x4_ul[ leaf ], id_index_x4[ leaf ]});
    out.emit(
      spirv::opcode::iadd,
      {id_ulong, id_base_plus[ leaf ], id_base, id_region_ul[ leaf ]});
    out.emit(
      spirv::opcode::iadd,
      {id_ulong, id_addr[ leaf ], id_base_plus[ leaf ], id_index_x4_ul[ leaf ]});
    out.emit(
      spirv::opcode::convert_u_to_ptr,
      {ptr_of(kind), id_leaf_ptr[ leaf ], id_addr[ leaf ]});
    id_store_ptr[ leaf ] = id_leaf_ptr[ leaf ];
    out.emit(
      spirv::opcode::load,
      {
        type_of(kind),
        dest,
        id_leaf_ptr[ leaf ],
        spirv::memory_access_aligned,
        4U,
      });
  }

  std::size_t                               result_leaf {};
  std::array<std::uint32_t, soa_leaf_limit> id_result {};
  std::uint32_t next_const_bits {id_const_bits_base};
  for (const instruction& item : instructions)
  {
    switch (item.code)
    {
    case operation::parameter : break;
    case operation::constant :
    {
      if (item.kind == scalar_kind::float32)
      {
        const std::uint32_t bits {next_const_bits++};
        out.emit(spirv::opcode::constant, {id_uint, bits, item.literal});
        out.emit(
          spirv::opcode::bitcast, {id_float, mapped_id(item.result), bits});
      }
      else
      {
        out.emit(
          spirv::opcode::constant,
          {type_of(item.kind), mapped_id(item.result), item.literal});
      }
      break;
    }
    case operation::add      :
    case operation::subtract :
    case operation::multiply :
    case operation::divide :
    {
      out.emit(
        detail::opcode_for(item.code, item.kind),
        {
          type_of(item.kind),
          mapped_id(item.result),
          mapped_id(item.left),
          mapped_id(item.right),
        });
      break;
    }
    case operation::negate :
    {
      out.emit(
        detail::opcode_for(item.code, item.kind),
        {type_of(item.kind), mapped_id(item.result), mapped_id(item.left)});
      break;
    }
    case operation::minimum :
    {
      const auto instruction = [ & ] {
        if (item.kind == scalar_kind::float32)
        {
          return spirv::glsl_std_450::fmin;
        }
        if (item.kind == scalar_kind::int32)
        {
          return spirv::glsl_std_450::smin;
        }
        return spirv::glsl_std_450::umin;
      }();
      out.emit(
        spirv::opcode::ext_inst,
        {
          /* type_of(item.kind),
          module_id(item.result),
          id_glsl,
          std::to_underlying(instruction),
          module_id(item.left),
          module_id(item.right), */
        });
      break;
    }
    case operation::maximum :
    {
      const auto instruction = [ & ] {
        if (item.kind == scalar_kind::float32)
        {
          return spirv::glsl_std_450::fmax;
        }
        if (item.kind == scalar_kind::int32)
        {
          return spirv::glsl_std_450::smax;
        }
        return spirv::glsl_std_450::umax;
      }();
      out.emit(
        spirv::opcode::ext_inst,
        {
          /* type_of(item.kind),
          module_id(item.result),
          id_glsl,
          std::to_underlying(instruction),
          module_id(item.left),
          module_id(item.right), */
        });
      break;
    }
    case operation::abs :
    {
      const auto instruction =
        item.kind == scalar_kind::float32
          ? spirv::glsl_std_450::fabs
          : spirv::glsl_std_450::sabs;
      out.emit(
        spirv::opcode::ext_inst,
        {
          /* type_of(item.kind),
          module_id(item.result),
          id_glsl,
          std::to_underlying(instruction),
          module_id(item.left), */
        });
      break;
    }
    case operation::result :
    {
      id_result[ result_leaf++ ] = mapped_id(item.left);
      break;
    }
    case operation::argument_address :
    {
      out.emit(
        spirv::opcode::access_chain,
        {
          id_push_uint_ptr,
          mapped_id(item.result),
          id_push,
          mapped_id(item.left),
          mapped_id(item.right),
        });
      break;
    }
    case operation::argument_load :
    {
      out.emit(
        spirv::opcode::load,
        {id_uint, mapped_id(item.result), mapped_id(item.left)});
      break;
    }
    case operation::argument_cast :
    {
      out.emit(
        spirv::opcode::bitcast,
        {type_of(item.kind), mapped_id(item.result), mapped_id(item.left)});
      break;
    }
    case operation::uniform_load : break;
    }
  }

  for (auto leaf : std::views::indices(leaves))
  {
    out.emit(
      spirv::opcode::store,
      {
        id_store_ptr[ leaf ],
        id_result[ leaf ],
        spirv::memory_access_aligned,
        4U,
      });
  }

  out.emit(spirv::opcode::branch, {id_done});
  out.emit(spirv::opcode::label, {id_done});
  out.emit(spirv::opcode::return_void, {});
  out.emit(spirv::opcode::function_end, {});

  return std::move(out).words();
}

#else

template<soa_element Element>
consteval
auto
bind_traced_struct(recorder&, std::span<const std::uint32_t, soa_leaf_limit>)
  -> traced_struct<Element>
{ return {}; }

template<class... Arguments>
consteval
auto
bind_traced_arguments(
  recorder& target, std::type_identity<std::tuple<Arguments...>>)
  -> std::tuple<value<Arguments>...>
{
  return std::tuple<value<Arguments>...> {
    value<Arguments> {target, 0U}...,
  };
}

template<soa_element Element>
consteval
auto
traced_leaf_id(const traced_struct<Element>&, std::size_t) -> std::uint32_t
{ return 0U; }

template<soa_element Element, class Operation>
  requires foreach_traceable<Operation, Element>
consteval
auto
foreach_module() -> std::vector<std::uint32_t>
{ return {}; }

#endif

} // namespace detail

export template<soa_element Element, class Operation>
  requires foreach_traceable<Operation, Element>
inline constexpr std::size_t foreach_word_count =
  detail::foreach_module<Element, Operation>().size();

export
template<soa_element Element, class Operation>
  requires foreach_traceable<Operation, Element>
consteval auto
compile(foreach_stage<Operation>)
  -> std::array<std::uint32_t, foreach_word_count<Element, Operation>>
{
  const auto words = detail::foreach_module<Element, Operation>();
  std::array<std::uint32_t, foreach_word_count<Element, Operation>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

export inline constexpr std::string_view foreach_entry {"fiat_foreach"};

} // namespace fiat
