# fiat

`fiat` turns a captureless C++ lambda into a SPIR-V 1.6 library module at compile time. The module exports one function, `fiat_operation(T) -> T`, which `vkpp::gpu` splices into precompiled Slang skeletons. No Vulkan, no Slang and no runtime code generation are involved.

```cpp
import fiat;

constexpr auto words = fiat::compile<float>([](auto& value) { return value * 2; });
```

`words` is a `constexpr std::array<std::uint32_t, N>`. Supported scalars: `float`, `std::int32_t`, `std::uint32_t`; operations: `+ - * /`, unary `-` (not on `std::uint32_t`), and compound assignment.

License: GPL-3.0.

