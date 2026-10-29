import std;
import fiat;

constexpr auto words = fiat::compile<std::uint32_t>([](auto& value) {
  value += 1U;
});

auto
main() -> int
{
  return fiat::write_spirv(
           std::filesystem::path {FIAT_SPV_OUTPUT_DIR} / "increment.spv", words)
           ? 0
           : 1;
}
