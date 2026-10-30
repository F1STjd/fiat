import std;
import fiat;

namespace
{

struct stub_scalar
{
  using element_type = float;

  std::uint64_t address_value {0x10'00'00'00ULL};

  [[nodiscard]]
  constexpr
  auto
  address() const noexcept -> std::uint64_t
  { return address_value; }
};

constexpr auto divide_by = [](auto x, auto total) { return x / total; };

constexpr auto words =
  fiat::compile<float>(fiat::transform_with(stub_scalar {}, divide_by));

} // namespace

auto
main() -> int
{
  return fiat::write_spirv(
           std::filesystem::path {FIAT_SPV_OUTPUT_DIR} / "transform_with.spv",
           words)
           ? 0
           : 1;
}
