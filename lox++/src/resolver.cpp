#include "resolver.h"
#include "Expr.h"
#include "Stmt.h"
#include "lox_callable.h"
#include "token.h"
#include <fstream>
#include <unordered_map>
#include <variant>

//   ....  Expression Visitors   ....
LiteralValue Resolver::visit(Variable &expr) {
  if (!scopes.empty()) {
    auto &scope = scopes.top();
    auto it = scope.find(expr.name.lexeme);
    if (it != scope.end() && it->second == false) {
      error_handler_.error(
          expr.name.line, "Cannot read local variable in its own initializer.");
    }
  }
  resolve_local(expr, expr.name);
  return std::monostate{};
}

LiteralValue Resolver::visit(Assign &expr) {
  resolve(*expr.expression);
  resolve_local(expr, expr.name);

  return std::monostate{};
}

LiteralValue Resolver::visit(Binary &expr) {
  resolve(*expr.left);
  resolve(*expr.right);

  return std::monostate{};
}

LiteralValue Resolver::visit(Call &expr) {
  resolve(*expr.callee);

  for (auto &arg : expr.args) {
    resolve(*arg);
  }

  return std::monostate{};
}

LiteralValue Resolver::visit(Grouping &expr) {
  resolve(*expr.expression);

  return std::monostate{};
}

LiteralValue Resolver::visit(Logical &expr) {
  resolve(*expr.left);
  resolve(*expr.right);

  return std::monostate{};
}

LiteralValue Resolver::visit(Ternary &expr) {
  resolve(*expr.cond_);
  resolve(*expr.then_b);
  resolve(*expr.else_b);

  return std::monostate{};
}

LiteralValue Resolver::visit(Unary &expr) {
  resolve(*expr.right);

  return std::monostate{};
}

LiteralValue Resolver::visit(Literal &expr) { return std::monostate{}; }
// ..... Statement Visitor  .......
void Resolver::visit(BlockStmt &stmt) {
  beginScope();
  resolve(stmt.statements);
  endScope();
}

void Resolver::visit(VarStmt &stmt) {
  declare(stmt.name);
  if (stmt.initializer)
    resolve(*stmt.initializer);
  define(stmt.name);
}

void Resolver::visit(WhileStmt &stmt) {
  resolve(*stmt.cond);
  resolve(*stmt.body);
}

void Resolver::visit(FuncStmt &stmt) {
  declare(stmt.name);
  define(stmt.name);
  resolve_function(stmt, FunctionType::FUNCTION);
}

void Resolver::visit(ExprStmt &stmt) { resolve(*stmt.expression); }

void Resolver::visit(IfStmt &stmt) {
  resolve(*stmt.cond);
  resolve(*stmt.then_b);
  if (stmt.else_b)
    resolve(*stmt.else_b);
}

void Resolver::visit(PrintStmt &stmt) { resolve(*stmt.expression); }

void Resolver::visit(ReturnStmt &stmt) {
  if (current_function_ == FunctionType::NONE)
    error_handler_.error(stmt.keyword.line,
                         "Can't return from top-level code.");
  if (stmt.value)
    resolve(*stmt.value);
}

void Resolver::visit(BreakStmt &stmt) {}

// ....... Helper functions .........

void Resolver::resolve(const std::vector<StmtPtr> &statements) {
  for (auto &stmt : statements) {
    resolve(*stmt);
  }
}

void Resolver::beginScope() { scopes.push({}); }

void Resolver::endScope() { scopes.pop(); }

void Resolver::declare(Token name) {
  if (scopes.empty())
    return;

  scopes.top()[name.lexeme] = false;
}

void Resolver::define(Token name) {
  if (scopes.empty())
    return;

  scopes.top()[name.lexeme] = true;
}

void Resolver::resolve_local(Expr &expr, Token name) {
  int depth = 0;
  auto temp = scopes;
  while (!temp.empty()) {
    if (temp.top().contains(name.lexeme)) {
      interpreter_.resolve(expr, depth);
      return;
    }
    temp.pop();
    depth++;
  }
}

void Resolver::resolve_function(FuncStmt &function, FunctionType type) {
  FunctionType enclosing = current_function_;
  current_function_ = type;

  beginScope();
  for (const auto &param : function.params) {
    declare(param);
    define(param);
  }
  resolve(function.body);
  endScope();
  current_function_ = enclosing;
}
