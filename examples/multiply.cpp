import std;
import fiat;

constexpr auto words = fiat::compile<float>([](auto& value) {
  return value * 2;
});

auto
main() -> int
{ return fiat::write_spirv("multiply.spv", words) ? 0 : 1; }
