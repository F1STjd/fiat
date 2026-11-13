export module fiat:spirv;

import std;

namespace fiat::spirv
{

export inline constexpr std::uint32_t magic {0x07'23'02'03U};
export inline constexpr std::uint32_t version_1_6 {0x00'01'06'00U};
export inline constexpr std::uint32_t generator {};
export inline constexpr std::size_t   header_size {5UZ};

export enum class opcode : std::uint16_t
{
  ext_inst_import     = 11,
  ext_inst            = 12,
  memory_model        = 14,
  entry_point         = 15,
  capability          = 17,
  type_void           = 19,
  type_bool           = 20,
  type_int            = 21,
  type_float          = 22,
  type_vector         = 23,
  type_array          = 28,
  type_struct         = 30,
  type_pointer        = 32,
  type_function       = 33,
  constant            = 43,
  spec_constant       = 50,
  function            = 54,
  function_parameter  = 55,
  function_end        = 56,
  variable            = 59,
  load                = 61,
  store               = 62,
  access_chain        = 65,
  decorate            = 71,
  member_decorate     = 72,
  composite_extract   = 81,
  uconvert            = 113,
  convert_u_to_ptr    = 120,
  bitcast             = 124,
  snegate             = 126,
  fnegate             = 127,
  iadd                = 128,
  fadd                = 129,
  isub                = 130,
  fsub                = 131,
  imul                = 132,
  fmul                = 133,
  udiv                = 134,
  sdiv                = 135,
  fdiv                = 136,
  uless_than          = 176,
  shift_right_logical = 194,
  shift_left_logical  = 196,
  bitwise_or          = 197,
  bitwise_and         = 199,
  selection_merge     = 247,
  label               = 248,
  branch              = 249,
  branch_conditional  = 250,
  return_void         = 253,
  return_value        = 254,
  execution_mode_id   = 331,
};

export enum class glsl_std_450 : std::uint32_t
{
  fabs = 4,
  sabs = 4,
  fmin = 37,
  umin = 38,
  smin = 39,
  fmax = 40,
  umax = 41,
  smax = 42,
};

export enum class capability : std::uint32_t
{
  shader                            = 1,
  linkage                           = 5,
  int64                             = 11,
  physical_storage_buffer_addresses = 5'347,
};

export enum class addressing_model : std::uint32_t
{
  logical                    = 0,
  physical_storage_buffer_64 = 5'348,
};

export enum class memory_model : std::uint32_t
{
  glsl450 = 1,
};

export enum class storage_class : std::uint32_t
{
  input                   = 1,
  push_constant           = 9,
  physical_storage_buffer = 5'349,
};

export enum class decoration : std::uint32_t
{
  spec_id            = 1,
  block              = 2,
  array_stride       = 6,
  builtin            = 11,
  offset             = 35,
  linkage_attributes = 41,
};

export enum class linkage_type : std::uint32_t
{
  exported = 0,
  imported = 1,
};

export enum class execution_model : std::uint32_t
{
  gl_compute = 5,
};

export enum class execution_mode : std::uint32_t
{
  local_size_id = 38,
};

export enum class builtin : std::uint32_t
{
  global_invocation_id = 28
};

export enum class selection_control : std::uint32_t
{
  none = 0,
};

export inline constexpr std::uint32_t memory_access_aligned {0x2U};

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

  constexpr
  void
  append(std::span<const std::uint32_t> instruction)
  { words_.append_range(instruction); }

  [[nodiscard]]
  constexpr
  auto
  words() && -> std::vector<std::uint32_t>
  { return std::move(words_); }

private:
  std::vector<std::uint32_t> words_ {};
};

} // namespace fiat::spirv

namespace fiat
{

export constexpr
auto
fingerprint(std::span<const std::uint32_t> words) -> std::uint64_t
{
  constexpr auto offset_basis {0xCB'F2'9C'E4'84'22'23'25ULL};
  constexpr auto prime {0x00'00'01'00'00'00'01'B3ULL};
  auto           hash {offset_basis};
  for (const auto word : words)
  {
    for (const auto index : std::views::indices(4U))
    {
      hash ^= (word >> (8U * index)) & 0xFFU;
      hash *= prime;
    }
  }
  return hash;
}

export
auto
write_spirv(
  const std::filesystem::path& path, std::span<const std::uint32_t> words)
  -> bool
{
  std::ofstream file {path, std::ios::binary};
  file.write(
    reinterpret_cast<const char*>(words.data()),
    static_cast<std::streamsize>(words.size_bytes()));
  return static_cast<bool>(file);
}

} // namespace fiat
