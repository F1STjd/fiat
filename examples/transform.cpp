import std;
import fiat;

constexpr auto words = fiat::compile<float>(fiat::transform([](auto& value) {
  return value * 2;
}));

auto
main() -> int
{
  std::ofstream file {"transform.spv", std::ios::binary};
  file.write(
    reinterpret_cast<const char*>(words.data()),
    static_cast<std::streamsize>(std::span {words}.size_bytes()));
  return file ? 0 : 1;
}
