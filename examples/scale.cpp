import std;
import fiat;

constexpr auto scale = [](auto& value, auto factor) { return value * factor; };

constexpr auto words = fiat::compile<float>(fiat::transform(scale, float {}));

auto
main() -> int
{ return fiat::write_spirv("scale.spv", words) ? 0 : 1; }
