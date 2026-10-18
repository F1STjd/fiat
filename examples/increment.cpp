import std;
import fiat;

constexpr auto words = fiat::compile<std::uint32_t>([](auto& value) {
  value += 1U;
});

auto
main() -> int
{
  std::ofstream file {"increment.spv", std::ios::binary};
  file.write(
    reinterpret_cast<const char*>(words.data()),
    static_cast<std::streamsize>(std::span {words}.size_bytes()));
  return file ? 0 : 1;
}
