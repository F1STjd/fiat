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

} // namespace fiat
