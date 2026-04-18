#include "Expr.h"
#include "token.h"

struct RuntimeError : std::runtime_error {
  Token token_;
  RuntimeError(Token token, const std::string &message)
      : token_(token), std::runtime_error(message) {}
};

struct Interpreter : ExprVisitor {
  LiteralValue visit(Literal &expr) override;

  LiteralValue visit(Grouping &expr) override;

  LiteralValue visit(Unary &expr) override;

  LiteralValue visit(Binary &expr) override;

  LiteralValue visit(Ternary &expr) override;

private:
  LiteralValue evaluate(Expr &expr);
  bool isTruthy(const LiteralValue &expr);

  template <typename T>
  T check_operand(const Token &op, const LiteralValue &val,
                  const std::string &message) {
    if (auto *result = std::get_if<T>(&val))
      return *result;
    throw RuntimeError(op, message);
  }

  std::pair<double, double> check_number_operands(const Token &op,
                                                  const LiteralValue &left,
                                                  const LiteralValue &right) {
    return {check_operand<double>(op, left, "Operands must be numbers."),
            check_operand<double>(op, right, "Operands must be numbers.")};
  }
};
