#if defined(__cpp_impl_reflection)
  #include <meta>
#endif

import std;
import fiat;

#if defined(__cpp_impl_reflection)

struct probe_leaves;

consteval
{
  std::array<std::meta::info, 2UZ> members {
    std::meta::data_member_spec(^^fiat::value<float>, {.name = "x"}),
    std::meta::data_member_spec(^^fiat::value<float>, {.name = "y"}),
  };
  std::meta::define_aggregate(^^probe_leaves, members);
}

auto
main() -> int
{
  constexpr auto bytes = sizeof(probe_leaves);
  return bytes >= 2UZ * sizeof(fiat::value<float>) ? 0 : 1;
}

#else

auto
main() -> int
{
  std::println("reflection unavailable (__cpp_impl_reflection unset)");
  return 2;
}

#endif
