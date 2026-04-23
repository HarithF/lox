#include "Expr.h"
#include "Stmt.h"
#include "environment.h"
#include "error_handler.h"
#include "token.h"
#include <string>
#include <vector>

using StmtPtr = std::unique_ptr<Stmt>;

struct Interpreter : ExprVisitor, StmtVisitor {
  Interpreter(ErrorHandler &error_handler) : error_handler_(error_handler) {}

  void interpret(const std::vector<StmtPtr> &);

  LiteralValue visit(Literal &expr) override;
  LiteralValue visit(Grouping &expr) override;
  LiteralValue visit(Unary &expr) override;
  LiteralValue visit(Binary &expr) override;
  LiteralValue visit(Ternary &expr) override;
  LiteralValue visit(Variable &expr) override;
  LiteralValue visit(Assign &expr) override;
  LiteralValue visit(Logical &expr) override;

  void visit(PrintStmt &stmt) override;
  void visit(ExprStmt &stmt) override;
  void visit(IfStmt &stmt) override;
  void visit(VarStmt &stmt) override;
  void visit(BlockStmt &stmt) override;
  void visit(WhileStmt &stmt) override;

  std::string stringify(const LiteralValue &value);
  LiteralValue evaluate(Expr &expr);

private:
  ErrorHandler &error_handler_;
  Environment env = Environment();

  void execute(Stmt &stmt);
  void execute_block(const std::vector<StmtPtr> &, Environment);

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
