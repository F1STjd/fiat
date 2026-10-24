export module fiat:emit;

import std;
import :spirv;
import :trace;

namespace fiat
{

export inline constexpr std::string_view operation_symbol {"fiat_operation"};

export template<class Operation, class Element>
concept traceable =
  scalar<Element> &&
  std::is_empty_v<Operation> &&
  std::default_initializable<Operation> &&
  std::invocable<Operation, value<Element>&> &&
  (std::same_as<
     std::invoke_result_t<Operation, value<Element>&>,
     value<Element>
   > ||
    std::is_void_v<std::invoke_result_t<Operation, value<Element>&>>);

export
template<scalar Element, class Operation>
  requires traceable<Operation, Element>
constexpr auto
trace() -> std::vector<instruction>
{
  recorder       target {};
  value<Element> parameter {target, target.emit_parameter(kind_of<Element>)};
  if constexpr (std::is_void_v<std::invoke_result_t<Operation, value<Element>&>>)
  {
    Operation {}(parameter);
  }
  else
  {
    parameter = Operation {}(parameter);
  }
  target.emit_result(kind_of<Element>, parameter.id());
  return std::move(target).instructions();
}

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
  case operation::argument_cast    : break;
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
  out.emit(
    spirv::opcode::function_parameter,
    {layout.type, module_id(layout, instructions.front().result)});
  out.emit(spirv::opcode::label, {label_id(layout, instructions)});

  for (const instruction& item : instructions)
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
    }
  }
  out.emit(spirv::opcode::function_end, {});
}

export
template<scalar Element>
constexpr auto
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

export template<scalar Element, class Operation>
  requires traceable<Operation, Element>
inline constexpr std::size_t word_count =
  emit_function_module<Element>(trace<Element, Operation>()).size();

export
template<scalar Element, class Operation>
  requires traceable<Operation, Element>
consteval auto
compile(Operation) -> std::array<std::uint32_t, word_count<Element, Operation>>
{
  const std::vector<std::uint32_t> words =
    emit_function_module<Element>(trace<Element, Operation>());
  std::array<std::uint32_t, word_count<Element, Operation>> result {};
  std::ranges::copy(words, result.begin());
  return result;
}

} // namespace fiat
