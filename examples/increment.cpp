import std;
import fiat;

constexpr auto words = fiat::compile<std::uint32_t>([](auto& value) {
  value += 1U;
});

auto
main() -> int
{ return fiat::write_spirv("increment.spv", words) ? 0 : 1; }
