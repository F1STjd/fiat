export module fiat:emit;

import std;
import :spirv;
import :trace;

namespace fiat
{

export inline constexpr std::string_view operation_symbol {"fiat_operation"};
export inline constexpr std::string_view combine_symbol {"fiat_combine"};

template<class Operation>
struct arguments_of
{
  using type = std::tuple<>;
};

template<class Operation>
  requires requires { typename Operation::argument_tuple; }
struct arguments_of<Operation>
{
  using type = typename Operation::argument_tuple;
};

export template<class Operation, class Element, class... Arguments>
concept traceable_call =
  scalar<Element> &&
  std::is_empty_v<Operation> &&
  std::default_initializable<Operation> &&
  (std::same_as<Arguments, Element> && ...) &&
  std::invocable<Operation, value<Element>&, const value<Arguments>&...> &&
  (std::same_as<
     std::invoke_result_t<Operation, value<Element>&, const value<Arguments>&...>,
     value<Element>
   > ||
    std::is_void_v<
      std::invoke_result_t<Operation, value<Element>&, const value<Arguments>&...>
    >);

template<class Operation, class Element, class Arguments>
inline constexpr bool traceable_arguments {false};

template<class Operation, class Element, class... Arguments>
inline constexpr bool
  traceable_arguments<Operation, Element, std::tuple<Arguments...>> =
    traceable_call<Operation, Element, Arguments...>;

export template<class Operation, class Element>
concept traceable =
  scalar<Element> &&
  traceable_arguments<Operation, Element, typename arguments_of<Operation>::type>;

export template<class Operation, class Element, class Uniform, class... Arguments>
concept uniform_traceable_call =
  scalar<Element> &&
  scalar<Uniform> &&
  std::same_as<Uniform, Element> &&
  std::is_empty_v<Operation> &&
  std::default_initializable<Operation> &&
  (std::same_as<Arguments, Element> && ...) &&
  std::invocable<
    Operation,
    value<Element>&,
    const value<Uniform>&,
    const value<Arguments>&...
  > &&
  (std::same_as<
     std::invoke_result_t<
       Operation,
       value<Element>&,
       const value<Uniform>&,
       const value<Arguments>&...
     >,
     value<Element>
   > ||
    std::is_void_v<std::invoke_result_t<
      Operation,
      value<Element>&,
      const value<Uniform>&,
      const value<Arguments>&...
    >>);

template<class Operation, class Element, class Uniform, class Arguments>
inline constexpr bool uniform_traceable_arguments {false};

template<class Operation, class Element, class Uniform, class... Arguments>
inline constexpr bool uniform_traceable_arguments<
  Operation,
  Element,
  Uniform,
  std::tuple<Arguments...>
> = uniform_traceable_call<Operation, Element, Uniform, Arguments...>;

export template<class Operation, class Element, class Uniform>
concept uniform_traceable =
  scalar<Element> &&
  scalar<Uniform> &&
  std::same_as<Uniform, Element> &&
  requires { typename Operation::uniform_type; } &&
  std::same_as<typename Operation::uniform_type, Uniform> &&
  uniform_traceable_arguments<
    Operation,
    Element,
    Uniform,
    typename arguments_of<Operation>::type
  >;

export template<class Operation, class Element>
concept combinable =
  scalar<Element> &&
  std::is_empty_v<Operation> &&
  std::default_initializable<Operation> &&
  std::invocable<Operation, value<Element>, value<Element>> &&
  std::same_as<
    std::invoke_result_t<Operation, value<Element>, value<Element>>,
    value<Element>
  >;

template<
  class Operation,
  scalar Element,
  class... Arguments,
  std::size_t... Slots
>
constexpr
void
trace_call(
  [[maybe_unused]] detail::recorder& target,
  value<Element>&                    parameter,
  std::type_identity<std::tuple<Arguments...>>,
  std::index_sequence<Slots...>)
{
  [[maybe_unused]]
  const std::tuple<value<Arguments>...> arguments {
    value<Arguments> {
      target,
      target.emit_argument(
        kind_of<Arguments>, static_cast<std::uint32_t>(Slots)),
    }...,
  };
  if constexpr (
    std::is_void_v<
      std::invoke_result_t<Operation, value<Element>&, const value<Arguments>&...>
    >)
  {
    Operation {}(parameter, std::get<Slots>(arguments)...);
  }
  else
  {
    parameter = Operation {}(parameter, std::get<Slots>(arguments)...);
  }
}

template<
  class Operation,
  scalar Element,
  scalar Uniform,
  class... Arguments,
  std::size_t... Slots
>
constexpr
void
trace_uniform_call(
  [[maybe_unused]] detail::recorder& target,
  value<Element>&                    parameter,
  const value<Uniform>&              uniform,
  [[maybe_unused]] std::type_identity<std::tuple<Arguments...>>,
  std::index_sequence<Slots...>)
{
  [[maybe_unused]]
  const std::tuple<value<Arguments>...> arguments {
    value<Arguments> {
      target,
      target.emit_argument(
        kind_of<Arguments>, static_cast<std::uint32_t>(Slots + 2U)),
    }...,
  };

  if constexpr (
    std::is_void_v<std::invoke_result_t<
      Operation,
      value<Element>&,
      const value<Uniform>&,
      const value<Arguments>&...
    >>)
  {
    Operation {}(parameter, uniform, std::get<Slots>(arguments)...);
  }
  else
  {
    parameter = Operation {}(parameter, uniform, std::get<Slots>(arguments)...);
  }
}

namespace detail
{

template<scalar Element, class Operation>
  requires traceable<Operation, Element>
constexpr
auto
trace() -> std::vector<instruction>
{
  using argument_tuple = typename arguments_of<Operation>::type;

  recorder       target {};
  value<Element> parameter {target, target.emit_parameter(kind_of<Element>)};
  trace_call<Operation>(
    target,
    parameter,
    std::type_identity<argument_tuple> {},
    std::make_index_sequence<std::tuple_size_v<argument_tuple>> {});
  target.emit_result(kind_of<Element>, parameter.id());
  return std::move(target).instructions();
}

template<scalar Element, class Combine>
  requires combinable<Combine, Element>
constexpr
auto
trace_binary() -> std::vector<instruction>
{
  recorder             target {};
  value<Element>       left {target, target.emit_parameter(kind_of<Element>)};
  value<Element>       right {target, target.emit_parameter(kind_of<Element>)};
  const value<Element> merged = Combine {}(left, right);
  target.emit_result(kind_of<Element>, merged.id());
  return std::move(target).instructions();
}

template<scalar Element, scalar Uniform, class Operation>
  requires uniform_traceable<Operation, Element, Uniform>
constexpr
auto
trace_with_uniform() -> std::vector<instruction>
{
  using argument_tuple = typename arguments_of<Operation>::type;

  recorder       target {};
  value<Element> parameter {target, target.emit_parameter(kind_of<Element>)};
  value<Uniform> uniform {target, target.emit_uniform(kind_of<Uniform>, 0U)};
  trace_uniform_call<Operation>(
    target,
    parameter,
    uniform,
    std::type_identity<argument_tuple> {},
    std::make_index_sequence<std::tuple_size_v<argument_tuple>> {});
  target.emit_result(kind_of<Element>, parameter.id());
  return std::move(target).instructions();
}

} // namespace detail

constexpr
auto
opcode_for(operation code, scalar_kind kind) -> spirv::opcode
{
  const bool floating = kind == scalar_kind::float32;
  switch (code)
  {
  case operation::add :
    return floating ? spirv::opcode::fadd : spirv::opcode::iadd;
  case operation::subtract :
    return floating ? spirv::opcode::fsub : spirv::opcode::isub;
  case operation::multiply :
    return floating ? spirv::opcode::fmul : spirv::opcode::imul;
  case operation::divide :
  {
    if (floating) { return spirv::opcode::fdiv; }
    return kind == scalar_kind::int32 ? spirv::opcode::sdiv : spirv::opcode::udiv;
  }
  case operation::negate :
    return floating ? spirv::opcode::fnegate : spirv::opcode::snegate;
  case operation::parameter        :
  case operation::constant         :
  case operation::result           :
  case operation::argument_address :
  case operation::argument_load    :
  case operation::argument_cast    :
  case operation::uniform_load     : break;
  }
  std::unreachable();
}

struct function_layout
{
  std::uint32_t type {};
  std::uint32_t function_type {};
  std::uint32_t function {};
  std::uint32_t offset {};
  scalar_kind   element {};
  std::uint32_t uint_type {};
  std::uint32_t uint_pointer {};
  std::uint32_t push {};
  std::uint32_t element_pointer {};
  std::uint32_t ulong_type {};
};

constexpr
auto
constant_type(const function_layout& layout, scalar_kind kind) -> std::uint32_t
{ return kind == layout.element ? layout.type : layout.uint_type; }

constexpr
auto
module_id(const function_layout& layout, std::uint32_t trace_id)
  -> std::uint32_t
{ return trace_id + layout.offset; }

constexpr
auto
label_id(
  const function_layout& layout, std::span<const instruction> instructions)
  -> std::uint32_t
{
  return module_id(
           layout,
           std::ranges::max(
             instructions | std::views::transform(&instruction::result))) +
         1U;
}

constexpr
void
emit_constants(
  spirv::writer&               out,
  const function_layout&       layout,
  std::span<const instruction> instructions)
{
  for (const instruction& item : instructions)
  {
    if (item.code == operation::constant)
    {
      out.emit(
        spirv::opcode::constant,
        {
          constant_type(layout, item.kind),
          module_id(layout, item.result),
          item.literal,
        });
    }
  }
}

constexpr
void
emit_definition(
  spirv::writer&               out,
  const function_layout&       layout,
  std::span<const instruction> instructions)
{
  out.emit(
    spirv::opcode::function,
    {
      layout.type,
      layout.function,
      std::to_underlying(spirv::function_control::inlined),
      layout.function_type,
    });
  const auto is_parameter = [](const instruction& item) -> bool {
    return item.code == operation::parameter;
  };
  for (const auto& item : instructions | std::views::take_while(is_parameter))
  {
    out.emit(
      spirv::opcode::function_parameter,
      {layout.type, module_id(layout, item.result)});
  }
  out.emit(spirv::opcode::label, {label_id(layout, instructions)});

  for (const auto& item : instructions)
  {
    switch (item.code)
    {
    case operation::parameter :
    case operation::constant  : break;
    case operation::add       :
    case operation::subtract  :
    case operation::multiply  :
    case operation::divide :
    {
      out.emit(
        opcode_for(item.code, item.kind),
        {
          layout.type,
          module_id(layout, item.result),
          module_id(layout, item.left),
          module_id(layout, item.right),
        });
      break;
    }
    case operation::negate :
    {
      out.emit(
        opcode_for(item.code, item.kind),
        {
          layout.type,
          module_id(layout, item.result),
          module_id(layout, item.left),
        });
      break;
    }
    case operation::result :
    {
      out.emit(spirv::opcode::return_value, {module_id(layout, item.left)});
      break;
    }
    case operation::argument_address :
    {
      out.emit(
        spirv::opcode::access_chain,
        {
          layout.uint_pointer,
          module_id(layout, item.result),
          layout.push,
          module_id(layout, item.left),
          module_id(layout, item.right),
        });
      break;
    }
    case operation::argument_load :
    {
      out.emit(
        spirv::opcode::load,
        {
          layout.uint_type,
          module_id(layout, item.result),
          module_id(layout, item.left),
        });
      break;
    }
    case operation::argument_cast :
    {
      out.emit(
        spirv::opcode::bitcast,
        {
          layout.type,
          module_id(layout, item.result),
          module_id(layout, item.left),
        });
      break;
    }
    case operation::uniform_load :
    {
      const auto lo      = module_id(layout, item.left);
      const auto hi      = module_id(layout, item.right);
      const auto shift   = module_id(layout, item.literal);
      const auto base    = item.result - 5U;
      const auto hi64    = module_id(layout, base);
      const auto lo64    = module_id(layout, base + 1U);
      const auto shifted = module_id(layout, base + 2U);
      const auto address = module_id(layout, base + 3U);
      const auto pointer = module_id(layout, base + 4U);
      out.emit(spirv::opcode::uconvert, {layout.ulong_type, hi64, hi});
      out.emit(spirv::opcode::uconvert, {layout.ulong_type, lo64, lo});
      out.emit(
        spirv::opcode::shift_left_logical,
        {
          layout.ulong_type,
          shifted,
          hi64,
          shift,
        });
      out.emit(
        spirv::opcode::bitwise_or,
        {
          layout.ulong_type,
          address,
          shifted,
          lo64,
        });
      out.emit(
        spirv::opcode::convert_u_to_ptr,
        {
          layout.element_pointer,
          pointer,
          address,
        });
      out.emit(
        spirv::opcode::load,
        {
          layout.type,
          module_id(layout, item.result),
          pointer,
          0x2U,
          4U,
        });
      break;
    }
    }
  }
  out.emit(spirv::opcode::function_end, {});
}

namespace detail
{

template<scalar Element>
constexpr
auto
emit_function_module(std::span<const instruction> instructions)
  -> std::vector<std::uint32_t>
{
  constexpr function_layout layout {
    .type          = 1U,
    .function_type = 2U,
    .function      = 3U,
    .offset        = 3U,
    .element       = kind_of<Element>,
  };

  spirv::writer out {label_id(layout, instructions) + 1U};
  out.emit(
    spirv::opcode::capability, {std::to_underlying(spirv::capability::shader)});
  out.emit(
    spirv::opcode::capability,
    {std::to_underlying(spirv::capability::linkage)});
  out.emit(
    spirv::opcode::memory_model,
    {
      std::to_underlying(spirv::addressing_model::logical),
      std::to_underlying(spirv::memory_model::glsl450),
    });

  std::vector<std::uint32_t> decorate {
    layout.function,
    std::to_underlying(spirv::decoration::linkage_attributes),
  };
  decorate.append_range(spirv::literal_string(operation_symbol));
  decorate.push_back(std::to_underlying(spirv::linkage_type::exported));
  out.emit(spirv::opcode::decorate, decorate);

  if constexpr (std::same_as<Element, float>)
  {
    out.emit(spirv::opcode::type_float, {layout.type, 32U});
  }
  else
  {
    out.emit(
      spirv::opcode::type_int,
      {
        layout.type,
        32U,
        std::same_as<Element, std::int32_t> ? 1U : 0U,
      });
  }
  out.emit(
    spirv::opcode::type_function,
    {layout.function_type, layout.type, layout.type});

  emit_constants(out, layout, instructions);
  emit_definition(out, layout, instructions);
  return std::move(out).words();
}

} // namespace detail

export template<scalar Element, class Operation>
  requires (traceable<Operation, Element> &&
             std::tuple_size_v<typename arguments_of<Operation>::type> == 0UZ)
inline constexpr std::size_t word_count =
  detail::emit_function_module<Element>(detail::trace<Element, Operation>())
    .size();

export
template<scalar Element, class Operation>
  requires (
    traceable<Operation, Element> &&
    std::tuple_size_v<typename arguments_of<Operation>::type> == 0UZ)
consteval auto
compile(Operation) -> std::array<std::uint32_t, word_count<Element, Operation>>
{
  const std::vector<std::uint32_t> words =
    detail::emit_function_module<Element>(detail::trace<Element, Operation>());
  std::array<std::uint32_t, word_count<Element, Operation>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
