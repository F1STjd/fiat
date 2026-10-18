export module fiat:spirv;

import std;

namespace fiat::spirv
{

export inline constexpr std::uint32_t magic {0x07'23'02'03U};
export inline constexpr std::uint32_t version_1_6 {0x00'01'06'00U};
export inline constexpr std::uint32_t generator {};

export enum class opcode : std::uint16_t
{
  memory_model       = 14,
  capability         = 17,
  type_int           = 21,
  type_float         = 22,
  type_function      = 33,
  constant           = 43,
  function           = 54,
  function_parameter = 55,
  function_end       = 56,
  decorate           = 71,
  snegate            = 126,
  fnegate            = 127,
  iadd               = 128,
  fadd               = 129,
  isub               = 130,
  fsub               = 131,
  imul               = 132,
  fmul               = 133,
  udiv               = 134,
  sdiv               = 135,
  fdiv               = 136,
  label              = 248,
  return_value       = 254,
};

export enum class capability : std::uint32_t
{
  shader  = 1,
  linkage = 5,
};

export enum class addressing_model : std::uint32_t
{
  logical = 0,
};

export enum class memory_model : std::uint32_t
{
  glsl450 = 1,
};

export enum class decoration : std::uint32_t
{
  linkage_attributes = 41,
};

export enum class linkage_type : std::uint32_t
{
  exported = 0,
};

export enum class function_control : std::uint32_t
{
  none    = 0,
  inlined = 1,
};

export constexpr
auto
literal_string(std::string_view text) -> std::vector<std::uint32_t>
{
  std::vector<std::uint32_t> words((text.size() + 4UZ) / 4UZ, 0U);
  for (auto index : std::views::indices(text.size()))
  {
    const auto byte =
      static_cast<std::uint32_t>(static_cast<unsigned char>(text[ index ]));
    words[ index / 4UZ ] |= byte << (8UZ * (index % 4UZ));
  }
  return words;
}

export class writer
{
public:
  explicit constexpr writer(std::uint32_t bound)
  : words_ {magic, version_1_6, generator, bound, 0U}
  {}

  constexpr
  void
  emit(opcode code, std::span<const std::uint32_t> operands)
  {
    const auto count = static_cast<std::uint32_t>(operands.size()) + 1U;
    words_.push_back((count << 16U) | std::to_underlying(code));
    words_.append_range(operands);
  }

  constexpr
  void
  emit(opcode code, std::initializer_list<std::uint32_t> operands)
  { emit(code, std::span {operands}); }

  [[nodiscard]]
  constexpr
  auto
  words() && -> std::vector<std::uint32_t>
  { return std::move(words_); }

private:
  std::vector<std::uint32_t> words_ {};
};

} // namespace fiat::spirv
