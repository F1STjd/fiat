import std;
import fiat;

constexpr auto twice    = [](auto& value) { return value * 2; };
constexpr auto plus_one = [](auto& value) { return value + 1; };

constexpr auto words =
  fiat::compile<float>(fiat::transform(twice) | fiat::transform(plus_one));

auto
main() -> int
{ return fiat::write_spirv("transform.spv", words) ? 0 : 1; }
