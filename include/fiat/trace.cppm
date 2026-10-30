export module fiat:trace;

import std;

namespace fiat
{

export template<class Type>
concept scalar =
  std::same_as<Type, float> ||
  std::same_as<Type, std::int32_t> ||
  std::same_as<Type, std::uint32_t>;

export enum class scalar_kind : std::uint8_t
{
  float32,
  int32,
  uint32,
};

export template<scalar Type>
inline constexpr scalar_kind kind_of =
  std::same_as<Type, float> ? scalar_kind::float32
  : std::same_as<Type, std::int32_t>
    ? scalar_kind::int32
    : scalar_kind::uint32;

export enum class operation : std::uint8_t
{
  parameter,
  constant,
  add,
  subtract,
  multiply,
  divide,
  negate,
  result,
  argument_address,
  argument_load,
  argument_cast,
  uniform_load,
};

inline constexpr std::uint32_t argument_member {4U};

export struct instruction
{
  operation     code {};
  scalar_kind   kind {};
  std::uint32_t result {};
  std::uint32_t left {};
  std::uint32_t right {};
  std::uint32_t literal {};
};

namespace detail
{

class recorder
{
public:
  constexpr
  auto
  emit_parameter(scalar_kind kind) -> std::uint32_t
  { return push(operation::parameter, kind, 0U, 0U, 0U); }

  constexpr
  auto
  emit_argument(scalar_kind kind, std::uint32_t slot) -> std::uint32_t
  {
    const auto member = emit_constant(scalar_kind::uint32, argument_member);
    const auto index  = emit_constant(scalar_kind::uint32, slot);
    const auto address =
      push(operation::argument_address, scalar_kind::uint32, member, index, 0U);
    const auto bits =
      push(operation::argument_load, scalar_kind::uint32, address, 0U, 0U);
    if (kind == scalar_kind::uint32) { return bits; }
    return push(operation::argument_cast, kind, bits, 0U, 0U);
  }

  constexpr
  auto
  emit_uniform(scalar_kind kind, std::uint32_t slot) -> std::uint32_t
  {
    const auto member     = emit_constant(scalar_kind::uint32, argument_member);
    const auto lo_index   = emit_constant(scalar_kind::uint32, slot);
    const auto hi_index   = emit_constant(scalar_kind::uint32, slot + 1U);
    const auto shift      = emit_constant(scalar_kind::uint32, 32U);
    const auto lo_address = push(
      operation::argument_address, scalar_kind::uint32, member, lo_index, 0U);
    const auto lo =
      push(operation::argument_load, scalar_kind::uint32, lo_address, 0U, 0U);
    const auto hi_address = push(
      operation::argument_address, scalar_kind::uint32, member, hi_index, 0U);
    const auto hi =
      push(operation::argument_load, scalar_kind::uint32, hi_address, 0U, 0U);
    next_id_          += 5U;
    const auto result  = push(operation::uniform_load, kind, lo, hi, shift);
    return result;
  }

  constexpr
  auto
  emit_constant(scalar_kind kind, std::uint32_t literal) -> std::uint32_t
  {
    const auto found = std::ranges::find_if(
      instructions_, [ & ](const instruction& item) -> bool {
        return item.code == operation::constant &&
               item.kind == kind &&
               item.literal == literal;
      });
    if (found != instructions_.end()) { return found->result; }
    return push(operation::constant, kind, 0U, 0U, literal);
  }

  constexpr
  auto
  emit_unary(operation code, scalar_kind kind, std::uint32_t operand)
    -> std::uint32_t
  { return push(code, kind, operand, 0U, 0U); }

  constexpr
  auto
  emit_binary(
    operation code, scalar_kind kind, std::uint32_t left, std::uint32_t right)
    -> std::uint32_t
  { return push(code, kind, left, right, 0U); }

  constexpr
  void
  emit_result(scalar_kind kind, std::uint32_t operand)
  {
    instructions_.push_back({
      .code    = operation::result,
      .kind    = kind,
      .result  = 0U,
      .left    = operand,
      .right   = 0U,
      .literal = 0U,
    });
  }

  [[nodiscard]]
  constexpr
  auto
  instructions() && -> std::vector<instruction>
  { return std::move(instructions_); }

private:
  constexpr
  auto
  push(
    operation     code,
    scalar_kind   kind,
    std::uint32_t left,
    std::uint32_t right,
    std::uint32_t literal) -> std::uint32_t
  {
    const std::uint32_t result = next_id_++;
    instructions_.push_back({
      .code    = code,
      .kind    = kind,
      .result  = result,
      .left    = left,
      .right   = right,
      .literal = literal,
    });
    return result;
  }

  std::vector<instruction> instructions_ {};
  std::uint32_t            next_id_ {1U};
};

} // namespace detail

export template<scalar Type>
class value;

export template<class Literal, class Type>
concept literal_for =
  std::is_arithmetic_v<Literal> &&
  !std::same_as<Literal, bool> &&
  std::same_as<std::common_type_t<Type, Literal>, Type>;

export template<class Operand, class Type>
concept operand_for =
  std::same_as<Operand, value<Type>> || literal_for<Operand, Type>;

template<scalar Type, class Left, class Right>
constexpr
auto
combine(operation code, const Left& left, const Right& right) -> value<Type>;

export template<scalar Type>
class value
{
public:
  constexpr value(detail::recorder& owner, std::uint32_t result)
  : recorder_ {owner}, id_ {result}
  {}

  constexpr value(const value&) = default;

  constexpr
  auto
  operator =(const value& other) -> value&
  {
    id_ = other.id_;
    return *this;
  }

  [[nodiscard]]
  constexpr
  auto
  id() const noexcept -> std::uint32_t
  { return id_; }

  [[nodiscard]]
  constexpr
  auto
  target() const noexcept -> detail::recorder&
  { return recorder_; }

  explicit
  operator bool() const = delete(
    "a GPU value has no host truth value; branches arrive in the future");

  template<operand_for<Type> Right>
  constexpr
  auto
  operator +=(const Right& right) -> value&
  {
    *this = combine<Type>(operation::add, *this, right);
    return *this;
  }

  template<operand_for<Type> Right>
  constexpr
  auto
  operator -=(const Right& right) -> value&
  {
    *this = combine<Type>(operation::subtract, *this, right);
    return *this;
  }

  template<operand_for<Type> Right>
  constexpr
  auto
  operator *=(const Right& right) -> value&
  {
    *this = combine<Type>(operation::multiply, *this, right);
    return *this;
  }

  template<operand_for<Type> Right>
  constexpr
  auto
  operator /=(const Right& right) -> value&
  {
    *this = combine<Type>(operation::divide, *this, right);
    return *this;
  }

private:
  detail::recorder& recorder_;
  std::uint32_t     id_;
};

inline
void
literal_out_of_range([[maybe_unused]] std::string_view reason)
{}

template<std::integral Literal>
constexpr
auto
exact_in_float(Literal literal) -> bool
{
  const auto magnitude = [ & ] -> std::uint64_t {
    if constexpr (std::signed_integral<Literal>)
    {
      if (literal < 0)
      {
        return std::uint64_t {} - static_cast<std::uint64_t>(literal);
      }
    }
    return static_cast<std::uint64_t>(literal);
  }();
  return magnitude == 0U ||
         std::bit_width(magnitude) - std::countr_zero(magnitude) <=
           std::numeric_limits<float>::digits;
}

template<class Type, class Literal>
constexpr
auto
to_scalar(Literal literal) -> Type
{
  if constexpr (std::unsigned_integral<Type> && std::signed_integral<Literal>)
  {
    if (literal < 0)
    {
      literal_out_of_range("negative literal for unsigned fiat value");
    }
  }
  if constexpr (std::integral<Type> && std::integral<Literal>)
  {
    if (!std::in_range<Type>(+literal))
    {
      literal_out_of_range("literal does not fit the element_type");
    }
  }
  if constexpr (std::same_as<Type, float> && std::integral<Literal>)
  {
    if (!exact_in_float(literal))
    {
      literal_out_of_range("integer literal loses precision as float");
    }
  }
  if constexpr (std::same_as<Type, Literal>) { return literal; }
  else
  {
    return static_cast<Type>(literal);
  }
}

template<class Type, class Operand>
constexpr
auto
operand_id(detail::recorder& target, const Operand& operand) -> std::uint32_t
{
  if constexpr (std::same_as<Operand, value<Type>>) { return operand.id(); }
  else
  {
    return target.emit_constant(
      kind_of<Type>, std::bit_cast<std::uint32_t>(to_scalar<Type>(operand)));
  }
}

template<scalar Type, class Left, class Right>
constexpr
auto
combine(operation code, const Left& left, const Right& right) -> value<Type>
{
  detail::recorder& target = [ & ] -> detail::recorder& {
    if constexpr (std::same_as<Left, value<Type>>) { return left.target(); }
    else
    {
      return right.target();
    }
  }();
  const std::uint32_t left_id  = operand_id<Type>(target, left);
  const std::uint32_t right_id = operand_id<Type>(target, right);
  return value<Type> {
    target,
    target.emit_binary(code, kind_of<Type>, left_id, right_id),
  };
}

export
template<scalar Type, operand_for<Type> Right>
constexpr auto
operator +(const value<Type>& left, const Right& right) -> value<Type>
{ return combine<Type>(operation::add, left, right); }

export
template<scalar Type, literal_for<Type> Left>
constexpr auto
operator +(const Left& left, const value<Type>& right) -> value<Type>
{ return combine<Type>(operation::add, left, right); }

export
template<scalar Type, operand_for<Type> Right>
constexpr auto
operator -(const value<Type>& left, const Right& right) -> value<Type>
{ return combine<Type>(operation::subtract, left, right); }

export
template<scalar Type, literal_for<Type> Left>
constexpr auto
operator -(const Left& left, const value<Type>& right) -> value<Type>
{ return combine<Type>(operation::subtract, left, right); }

export
template<scalar Type, operand_for<Type> Right>
constexpr auto
operator *(const value<Type>& left, const Right& right) -> value<Type>
{ return combine<Type>(operation::multiply, left, right); }

export
template<scalar Type, literal_for<Type> Left>
constexpr auto
operator *(const Left& left, const value<Type>& right) -> value<Type>
{ return combine<Type>(operation::multiply, left, right); }

export
template<scalar Type, operand_for<Type> Right>
constexpr auto
operator /(const value<Type>& left, const Right& right) -> value<Type>
{ return combine<Type>(operation::divide, left, right); }

export
template<scalar Type, literal_for<Type> Left>
constexpr auto
operator /(const Left& left, const value<Type>& right) -> value<Type>
{ return combine<Type>(operation::divide, left, right); }

export
template<scalar Type>
  requires (!std::same_as<Type, std::uint32_t>)
constexpr auto
operator -(const value<Type>& operand) -> value<Type>
{
  detail::recorder& target = operand.target();
  return value<Type> {
    target,
    target.emit_unary(operation::negate, kind_of<Type>, operand.id()),
  };
}

export
auto
operator -(const value<std::uint32_t>&) -> value<std::uint32_t> = delete(
  "unary minus on std::uint32_t has no SPIR-V opcode; subtract from 0U "
  "instead");

} // namespace fiat
