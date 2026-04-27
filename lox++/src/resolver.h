
#include "Expr.h"
#include "Stmt.h"
#include "error_handler.h"
#include "interpreter.h"
#include "lox_callable.h"
#include "parser.h"
#include "token.h"
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

struct Resolver : ExprVisitor, StmtVisitor {

  Resolver(Interpreter &interpreter, ErrorHandler &error_handler)
      : interpreter_(interpreter), error_handler_(error_handler) {};

  LiteralValue visit(Variable &) override;
  LiteralValue visit(Assign &) override;
  LiteralValue visit(Binary &) override;
  LiteralValue visit(Call &) override;
  LiteralValue visit(Grouping &) override;
  LiteralValue visit(Literal &) override;
  LiteralValue visit(Logical &) override;
  LiteralValue visit(Ternary &) override;
  LiteralValue visit(Unary &) override;

  void visit(ExprStmt &) override;
  void visit(BlockStmt &) override;
  void visit(VarStmt &) override;
  void visit(FuncStmt &) override;
  void visit(IfStmt &) override;
  void visit(PrintStmt &) override;
  void visit(ReturnStmt &) override;
  void visit(WhileStmt &) override;
  void visit(BreakStmt &) override;

  void resolve(const std::vector<StmtPtr> &);

  enum class FunctionType { NONE, FUNCTION };

private:
  Interpreter &interpreter_;
  ErrorHandler error_handler_;
  std::stack<std::unordered_map<std::string, bool>> scopes{};

  FunctionType current_function_ = FunctionType::NONE;

  void resolve(Expr &expr) { expr.accept(*this); };
  void resolve(Stmt &stmt) { stmt.accept(*this); };
  void resolve_local(Expr &, Token);
  void resolve_function(FuncStmt &, FunctionType);

  void beginScope();
  void endScope();

  void declare(Token);
  void define(Token);
};
