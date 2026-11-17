export module fiat:ops;

import std;

import :trace;

namespace fiat
{

export struct plus
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return left + right; }
};

export struct minus
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return left - right; }
};

export struct multiplies
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return left * right; }
};

export struct divides
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return left / right; }
};

export struct minimum
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return min(left, right); }
};

export struct maximum
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type> left, value<Type> right) const -> value<Type>
  { return max(left, right); }
};

export struct square
{
  template<scalar Type>
  constexpr
  auto
  operator ()(value<Type>& operand) const -> value<Type>
  { return operand * operand; }
};

export struct negate
{
  template<scalar Type>
  constexpr
  auto
  operator ()(const value<Type>& operand) const -> value<Type>
  { return -operand; }
};

export struct absolute
{
  template<scalar Type>
  constexpr
  auto
  operator ()(const value<Type>& operand) const -> value<Type>
  { return abs(operand); }
};

export struct identity
{
  template<scalar Type>
  constexpr
  auto
  operator ()(const value<Type>& operand) const -> value<Type>
  { return operand; }
};

export template<std::uint32_t Bits>
struct digit
{
  static_assert(Bits < 32U, "fiat::digit bits must be less than 32");
  static constexpr std::uint32_t bits {Bits};
  static constexpr std::uint32_t mask {
    static_cast<std::uint32_t>((1ULL << Bits) - 1ULL),
  };

  using argument_tuple = std::tuple<std::uint32_t>;

  constexpr
  auto
  operator ()(
    const value<std::uint32_t>& key, const value<std::uint32_t>& shift)
    -> value<std::uint32_t>
  { return (key >> shift) & mask; }
};

export struct greater {};
export struct greater_equal {};
export struct less {};
export struct less_equal {};
export struct equal {};
export struct not_equal {};

export template<class Compare>
concept compare_operation =
  std::same_as<Compare, greater> ||
  std::same_as<Compare, greater_equal> ||
  std::same_as<Compare, less> ||
  std::same_as<Compare, less_equal> ||
  std::same_as<Compare, equal> ||
  std::same_as<Compare, not_equal>;

} // namespace fiat
