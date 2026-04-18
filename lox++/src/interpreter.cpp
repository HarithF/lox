#include "interpreter.h"

LiteralValue Interpreter::visit(Literal &expr) { return expr.value; }

LiteralValue Interpreter::visit(Grouping &expr) {
  return evaluate(std::move(expr.expression));
}

LiteralValue Interpreter::visit(Unary &expr) {
  auto right = evaluate(std::move(expr.right));

  switch (expr.operator_.type) {
  case TokenType::MINUS:
    return -get<double>(right);
  case TokenType::BANG:
    return !isTruthy(right);
  default:
    return std::monostate();
  }
}

LiteralValue Interpreter::visit(Binary &expr) {
  auto right = evaluate(std::move(expr.right));
  auto left = evaluate(std::move(expr.left));

  switch (expr.operator_.type) {
  case TokenType::MINUS:
    return std::get<double>(left) - std::get<double>(right);
  case TokenType::SLASH:
    return std::get<double>(left) / std::get<double>(right);
  case TokenType::STAR:
    return std::get<double>(left) * std::get<double>(right);
  case TokenType::PLUS:
    return std::visit(
        [](auto left, auto right) -> LiteralValue {
          using Tl = std::decay_t<decltype(left)>;
          using Tr = std::decay_t<decltype(right)>;
          if constexpr (std::is_same_v<Tl, double> &&
                        std::is_same_v<Tr, double>) {
            return left + right;
          } else if constexpr (std::is_same_v<Tl, std::string> &&
                               std::is_same_v<Tr, std::string>) {
            return left + right;
          } else
            return std::monostate();
        },
        left, right);
  default:
    std::cerr << "Invalid Binary op";
    return std::monostate();
  }
}

LiteralValue Interpreter::evaluate(ExprPtr expr) { return expr->accept(*this); }

bool Interpreter::isTruthy(const LiteralValue &expr) {
  return std::visit(
      [](auto val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return false;
        } else if constexpr (std::is_same_v<T, bool>) {
          return val;
        } else
          return true;
      },
      expr);
}
