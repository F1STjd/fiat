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

namespace detail
{

struct symbol_trace
{
  std::string_view             symbol {};
  std::span<const instruction> instructions {};
};

} // namespace detail

template<scalar Element>
constexpr
auto
splice(
  std::span<const std::uint32_t>        skeleton,
  std::span<const detail::symbol_trace> traces) -> std::vector<std::uint32_t>
{
  const auto items = instructions_of(skeleton);

  const auto uint_type = std::ranges::find_if(
    items, [](std::span<const std::uint32_t> item) -> bool {
      return opcode_of(item.front()) ==
               std::to_underlying(spirv::opcode::type_int) &&
             item[ 2 ] == 32U &&
             item[ 3 ] == 0U;
    });
  if (uint_type == items.end())
  {
    splice_error("template has no 32-bit unsigned integer type");
  }

  const auto uint_pointer = std::ranges::find_if(
    items, [ & ](std::span<const std::uint32_t> item) -> bool {
      return opcode_of(item.front()) ==
               std::to_underlying(spirv::opcode::type_pointer) &&
             item[ 2 ] ==
               std::to_underlying(spirv::storage_class::push_constant) &&
             item[ 3 ] == (*uint_type)[ 1 ];
    });
  if (uint_pointer == items.end())
  {
    splice_error("template has no push block uint pointer type");
  }

  const auto push_variable = std::ranges::find_if(
    items, [](std::span<const std::uint32_t> item) -> bool {
      return opcode_of(item.front()) ==
               std::to_underlying(spirv::opcode::variable) &&
             item[ 3 ] ==
               std::to_underlying(spirv::storage_class::push_constant);
    });
  if (push_variable == items.end())
  {
    splice_error("template has no push block variable");
  }

  const auto layout_of =
    [ & ](const detail::symbol_trace& entry, std::uint32_t first_offset)
    -> function_layout {
    const auto symbol = spirv::literal_string(entry.symbol);

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
      splice_error("template has no import for a traced symbol");
    }
    const std::uint32_t function = (*linkage)[ 1 ];

    const auto declaration = std::ranges::find_if(
      items, [ & ](std::span<const std::uint32_t> item) -> bool {
        return opcode_of(item.front()) ==
                 std::to_underlying(spirv::opcode::function) &&
               item[ 2 ] == function;
      });
    if (declaration == items.end())
    {
      splice_error("a traced symbol is not declared");
    }

    const auto declared = std::ranges::distance(
      std::ranges::subrange {std::next(declaration), items.end()} |
      std::views::take_while([](std::span<const std::uint32_t> item) -> bool {
        return opcode_of(item.front()) ==
               std::to_underlying(spirv::opcode::function_parameter);
      }));
    const auto traced = std::ranges::count(
      entry.instructions, operation::parameter, &instruction::code);
    if (declared != traced)
    {
      splice_error("declared parameter count differs from the trace");
    }

    return {
      .type          = (*declaration)[ 1 ],
      .function_type = (*declaration)[ 4 ],
      .function      = function,
      .offset        = first_offset,
      .element       = kind_of<Element>,
      .uint_type     = (*uint_type)[ 1 ],
      .uint_pointer  = (*uint_pointer)[ 1 ],
      .push          = (*push_variable)[ 2 ],
    };
  };

  std::vector<function_layout> layouts {};
  std::uint32_t                offset = skeleton[ 3 ] - 1U;
  for (const auto& entry : traces)
  {
    layouts.push_back(layout_of(entry, offset));
    offset = label_id(layouts.back(), entry.instructions);
  }
  if (!std::ranges::any_of(
        items, [ & ](std::span<const std::uint32_t> item) -> bool {
          return declares_element<Element>(item, layouts.front().type);
        }))
  {
    splice_error("template element type does not match Element");
  }

  spirv::writer out {offset + 1U};
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
        for (const auto& [ layout, entry ] : std::views::zip(layouts, traces))
        {
          emit_constants(out, layout, entry.instructions);
        }
        constants_written = true;
      }
      for (const auto& [ layout, entry ] : std::views::zip(layouts, traces))
      {
        if (item[ 2 ] == layout.function)
        {
          emit_definition(out, layout, entry.instructions);
          skipping = true;
        }
      }
      if (skipping) { continue; }
    }
    out.append(item);
  }
  return std::move(out).words();
}

} // namespace fiat
