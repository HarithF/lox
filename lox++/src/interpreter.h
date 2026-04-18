#include "Expr.h"
#include "parser.h"
#include "token.h"

struct Interpreter : ExprVisitor {
  LiteralValue visit(Literal &expr) override;

  LiteralValue visit(Grouping &expr) override;

  LiteralValue visit(Unary &expr) override;

  LiteralValue visit(Binary &expr) override;

private:
  LiteralValue evaluate(ExprPtr expr);
  bool isTruthy(const LiteralValue &expr);
};
