import std;
import fiat;

namespace
{

constexpr auto words =
  fiat::compile<float>(fiat::transform_with<float>(fiat::divides {}));

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
