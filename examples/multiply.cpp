import std;
import fiat;

constexpr auto words = fiat::compile<float>([](auto& value) {
  return value * 2;
});

auto
main() -> int
{
  return fiat::write_spirv(
           std::filesystem::path {FIAT_SPV_OUTPUT_DIR} / "multiply.spv", words)
           ? 0
           : 1;
}
