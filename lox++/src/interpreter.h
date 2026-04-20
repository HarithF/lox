#include "Expr.h"
#include "Stmt.h"
#include "error_handler.h"
#include "token.h"
#include <string>

struct RuntimeError : std::runtime_error {
  Token token_;
  RuntimeError(Token token, const std::string &message)
      : token_(token), std::runtime_error(message) {}
};

struct Interpreter : ExprVisitor, StmtVisitor {
  Interpreter(ErrorHandler &error_handler) : error_handler_(error_handler) {}

  void interpret(Expr &expr);

  LiteralValue visit(Literal &expr) override;

  LiteralValue visit(Grouping &expr) override;

  LiteralValue visit(Unary &expr) override;

  LiteralValue visit(Binary &expr) override;

  LiteralValue visit(Ternary &expr) override;

  void visit(PrintStmt &stmt) override;
  void visit(ExprStmt &stmt) override;

private:
  ErrorHandler &error_handler_;

  LiteralValue evaluate(Expr &expr);
  void execute(Stmt &stmt);

  bool isTruthy(const LiteralValue &expr);
  std::string stringify(const LiteralValue &value);

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
