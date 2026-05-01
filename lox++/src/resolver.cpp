#include "resolver.h"
#include "Expr.h"
#include "Stmt.h"
#include "lox_callable.h"
#include "token.h"
#include <unordered_map>
#include <variant>

//   ....  Expression Visitors   ....
LiteralValue Resolver::visit(Variable &expr) {
  if (!scopes.empty()) {
    auto &scope = scopes.top();
    auto it = scope.find(expr.name.lexeme);
    if (it != scope.end() && it->second == VarState::DECLARED) {
      error_handler_.error(
          expr.name.line, "Cannot read local variable in its own initializer.");
    }
  }
  if (!scopes.empty())
    scopes.top()[expr.name.lexeme] = VarState::USED;
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

LiteralValue Resolver::visit(Get &expr) {
  resolve(*expr.object);
  return std::monostate{};
}

LiteralValue Resolver::visit(Set &expr) {
  resolve(*expr.value);
  resolve(*expr.object);
  return std::monostate{};
}

LiteralValue Resolver::visit(Super &expr) {
  if (current_class == ClassType::NONE) {
    error_handler_.error(expr.keyword.line,
                         "cannot use 'super' outside of a class");
  } else if (current_class != ClassType::SUBCLASS) {
    error_handler_.error(expr.keyword.line,
                         "cannot use 'super' in a class with no superclass");
  }
  resolve_local(expr, expr.keyword);
  return std::monostate{};
}

LiteralValue Resolver::visit(This &expr) {
  if (current_class == ClassType::NONE) {
    error_handler_.error(expr.keyword.line,
                         "Cannot use 'this' outside of class");
    return std::monostate{};
  }
  resolve_local(expr, expr.keyword);
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
  if (!scopes.empty())
    scopes.top()[stmt.name.lexeme] = VarState::USED;
  resolve_function(stmt, FunctionType::FUNCTION);
}

void Resolver::visit(ClassStmt &stmt) {
  auto enclosing_class = current_class;
  current_class = ClassType::CLASS;

  declare(stmt.name);
  define(stmt.name);

  if (stmt.superclass && (stmt.name.lexeme == stmt.superclass->name.lexeme))
    error_handler_.error(stmt.superclass->name.line,
                         "A class cannot inherit from itself.");

  if (stmt.superclass) {
    current_class = ClassType::SUBCLASS;
    resolve(*stmt.superclass);
    beginScope();
    scopes.top()["super"] = VarState::USED;
  }

  beginScope();
  scopes.top()["this"] = VarState::USED;

  for (auto &method : stmt.methods) {
    auto decl = FunctionType::METHOD;
    if (method->name.lexeme == "init")
      decl = FunctionType::INITIALIZER;
    resolve_function(*method, decl);
  }
  endScope();
  if (stmt.superclass)
    endScope();
  current_class = enclosing_class;
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
  if (stmt.value) {
    if (current_function_ == FunctionType::INITIALIZER)
      error_handler_.error(stmt.keyword.line,
                           "Cannot return a vlaue from an initializer");
    resolve(*stmt.value);
  }
}

void Resolver::visit(BreakStmt &stmt) {}

// ....... Helper functions .........

void Resolver::resolve(const std::vector<StmtPtr> &statements) {
  for (auto &stmt : statements) {
    resolve(*stmt);
  }
}

void Resolver::beginScope() { scopes.push({}); }

void Resolver::endScope() {
  for (const auto &[name, state] : scopes.top()) {
    if (state != VarState::USED)
      error_handler_.error(0, "Local variable '" + name + "' is never used.");
  }
  scopes.pop();
}

void Resolver::declare(Token name) {
  if (scopes.empty())
    return;

  scopes.top()[name.lexeme] = VarState::DECLARED;
}

void Resolver::define(Token name) {
  if (scopes.empty())
    return;

  scopes.top()[name.lexeme] = VarState::DEFINED;
}

void Resolver::resolve_local(Expr &expr, Token name) {
  int depth = 0;
  auto temp = scopes;
  while (!temp.empty()) {
    if (temp.top().contains(name.lexeme)) {
      temp.top()[name.lexeme] = VarState::USED;
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
