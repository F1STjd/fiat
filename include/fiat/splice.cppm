export module fiat:splice;

import std;

import :spirv;
import :trace;
import :emit;

namespace fiat
{

inline
void
splice_error([[maybe_unused]] std::string_view reason)
{}

constexpr
auto
opcode_of(std::uint32_t word) -> std::uint32_t
{ return word & 0xFF'FFU; }

constexpr
auto
instructions_of(std::span<const std::uint32_t> module)
  -> std::vector<std::span<const std::uint32_t>>
{
  std::vector<std::span<const std::uint32_t>> items {};
  auto rest = module.subspan(spirv::header_size);
  while (!rest.empty())
  {
    const std::size_t count = rest.front() >> 16U;
    if (count == 0UZ || count > rest.size())
    {
      splice_error("truncated SPIR-V instruction in template");
    }
    items.push_back(rest.first(count));
    rest = rest.subspan(count);
  }
  return items;
}

template<scalar Element>
constexpr
auto
declares_element(std::span<const std::uint32_t> item, std::uint32_t type_id)
  -> bool
{
  if constexpr (std::same_as<Element, float>)
  {
    return opcode_of(item.front()) ==
             std::to_underlying(spirv::opcode::type_float) &&
           item[ 1 ] == type_id &&
           item[ 2 ] == 32U;
  }
  else
  {
    return opcode_of(item.front()) ==
             std::to_underlying(spirv::opcode::type_int) &&
           item[ 1 ] == type_id &&
           item[ 2 ] == 32U &&
           item[ 3 ] == (std::same_as<Element, std::int32_t> ? 1U : 0U);
  }
}

template<scalar Element>
constexpr
auto
splice(
  std::span<const std::uint32_t> skeleton, std::span<const instruction> trace)
  -> std::vector<std::uint32_t>
{
  const auto items  = instructions_of(skeleton);
  const auto symbol = spirv::literal_string(operation_symbol);

  const auto is_import = [ & ](std::span<const std::uint32_t> item) -> bool {
    return opcode_of(item.front()) ==
             std::to_underlying(spirv::opcode::decorate) &&
           item.size() == 3UZ + symbol.size() + 1UZ &&
           item[ 2 ] ==
             std::to_underlying(spirv::decoration::linkage_attributes) &&
           std::ranges::equal(item.subspan(3UZ, symbol.size()), symbol) &&
           item.back() == std::to_underlying(spirv::linkage_type::imported);
  };
  const auto linkage = std::ranges::find_if(items, is_import);
  if (linkage == items.end())
  {
    splice_error("template has no fiat_operation import");
  }
  const std::uint32_t function = (*linkage)[ 1 ];

  const auto declaration =
    std::ranges::find_if(items, [ & ](const auto& item) -> bool {
      return opcode_of(item.front()) ==
               std::to_underlying(spirv::opcode::function) &&
             item[ 2 ] == function;
    });
  if (declaration == items.end())
  {
    splice_error("fiat_operation is not declared");
  }

  const function_layout layout {
    .type          = (*declaration)[ 1 ],
    .function_type = (*declaration)[ 4 ],
    .function      = function,
    .offset        = skeleton[ 3 ] - 1U,
  };
  if (!std::ranges::any_of(items, [ & ](const auto& item) -> bool {
        return declares_element<Element>(item, layout.type);
      }))
  {
    splice_error("template element type does not match Element");
  }

  spirv::writer out {label_id(layout, trace) + 1U};
  bool          constants_written {false};
  bool          skipping {false};
  for (const auto& item : items)
  {
    const auto code = opcode_of(item.front());
    if (skipping)
    {
      skipping = code != std::to_underlying(spirv::opcode::function_end);
      continue;
    }
    if (
      code == std::to_underlying(spirv::opcode::capability) &&
      item[ 1 ] == std::to_underlying(spirv::capability::linkage))
    {
      continue;
    }
    if (
      code == std::to_underlying(spirv::opcode::decorate) &&
      item[ 2 ] == std::to_underlying(spirv::decoration::linkage_attributes))
    {
      continue;
    }
    if (code == std::to_underlying(spirv::opcode::function))
    {
      if (!constants_written)
      {
        emit_constants(out, layout, trace);
        constants_written = true;
      }
      if (item[ 2 ] == function)
      {
        emit_definition(out, layout, trace);
        skipping = true;
        continue;
      }
    }
    out.append(item);
  }
  return std::move(out).words();
}

} // namespace fiat
