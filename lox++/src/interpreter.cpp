#include "interpreter.h"
#include "Expr.h"
#include "Stmt.h"
#include "environment.h"
#include "lox_callable.h"
#include "token.h"
#include <memory>
#include <optional>
#include <print>
#include <vector>

void Interpreter::interpret(const std::vector<StmtPtr> &program) {
  try {
    for (const auto &stmt : program) {
      if (!stmt)
        continue;
      execute(*stmt);
    }
  } catch (const RuntimeError &e) {
    error_handler_.runtime_error(e.token_.line, e.what());
  }
}

LiteralValue Interpreter::visit(Literal &expr) { return expr.value; }

LiteralValue Interpreter::visit(Grouping &expr) {
  return evaluate(*expr.expression);
}

LiteralValue Interpreter::visit(Call &expr) {
  auto callee = evaluate(*expr.callee);

  std::vector<LiteralValue> args;
  for (const auto &arg : expr.args)
    args.push_back(evaluate(*arg));

  auto *callable = std::get_if<std::shared_ptr<LoxCallable>>(&callee);
  if (!callable)
    throw RuntimeError(expr.paren, "Can only call functions and classes.");

  if ((int)args.size() != (*callable)->arity())
    throw RuntimeError(expr.paren, "Expected " +
                                       std::to_string((*callable)->arity()) +
                                       " arguments but got " +
                                       std::to_string(args.size()) + ".");

  return (*callable)->call(*this, args);
}

LiteralValue Interpreter::visit(Unary &expr) {
  auto right = evaluate(*expr.right);

  switch (expr.operator_.type) {
  case TokenType::MINUS:
    return -get<double>(right);
  case TokenType::BANG:
    return !isTruthy(right);
  default:
    return std::monostate();
  }
}

LiteralValue Interpreter::visit(Ternary &expr) {
  auto cond = evaluate(*expr.cond_);

  if (isTruthy(cond)) {
    return evaluate(*expr.then_b);
  } else {
    return evaluate(*expr.else_b);
  }
}

LiteralValue Interpreter::visit(Binary &expr) {
  auto right = evaluate(*expr.right);
  auto left = evaluate(*expr.left);

  switch (expr.operator_.type) {
  case TokenType::MINUS: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l - r;
  }
  case TokenType::SLASH: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    if (r == 0.0)
      throw RuntimeError(expr.operator_, "Division by zero.");
    return l / r;
  }
  case TokenType::STAR: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l * r;
  }
  case TokenType::GREATER: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l > r;
  }
  case TokenType::GREATER_EQUAL: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l >= r;
  }
  case TokenType::LESS: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l < r;
  }
  case TokenType::LESS_EQUAL: {
    auto [l, r] = check_number_operands(expr.operator_, left, right);
    return l <= r;
  }
  case TokenType::BANG_EQUAL:
    return left != right;
  case TokenType::EQUAL_EQUAL:
    return left == right;
  case TokenType::PLUS:
    return std::visit(
        [&expr, this](auto left, auto right) -> LiteralValue {
          using Tl = std::decay_t<decltype(left)>;
          using Tr = std::decay_t<decltype(right)>;
          if constexpr (std::is_same_v<Tl, double> &&
                        std::is_same_v<Tr, double>) {
            return left + right;
          } else if constexpr (std::is_same_v<Tl, std::string>) {
            return left + stringify(right);
          } else
            throw RuntimeError(
                expr.operator_,
                "Operands must agree, two doubles or  two string");
        },
        left, right);
  default:
    std::cerr << "Invalid Binary op";
    return std::monostate();
  }
}

LiteralValue Interpreter::visit(Variable &expr) {
  return look_up_variable(expr.name, expr);
}

LiteralValue Interpreter::visit(Assign &expr) {
  auto value = evaluate(*expr.expression);
  auto it = locals.find(&expr);
  if (it != locals.end())
    env_->assign_at(it->second, expr.name, value);
  else
    global_->assign(expr.name, value);
  return value;
}

LiteralValue Interpreter::visit(Logical &expr) {
  auto left = evaluate(*expr.left);

  if (expr.operator_.type == TokenType::OR) {
    if (isTruthy(left))
      return left;
  } else if (!isTruthy(left)) {
    return left;
  }
  return evaluate(*expr.right);
}

//        ..... Statements .....

void Interpreter::visit(ExprStmt &stmt) { evaluate(*stmt.expression); }

void Interpreter::visit(PrintStmt &stmt) {
  auto expr = evaluate(*stmt.expression);
  std::println("{}", stringify(expr));
}

void Interpreter::visit(VarStmt &stmt) {
  if (env_->is_defined(stmt.name.lexeme))
    throw RuntimeError(stmt.name, "Variable '" + stmt.name.lexeme +
                                      "' already declared in this scope.");
  std::optional<LiteralValue> value = std::nullopt;
  if (stmt.initializer)
    value = evaluate(*stmt.initializer);
  env_->define(stmt.name.lexeme, std::move(value));
}

void Interpreter::visit(IfStmt &stmt) {
  if (isTruthy(evaluate(*stmt.cond)))
    execute(*stmt.then_b);

  else if (stmt.else_b)
    execute(*stmt.else_b);
}

void Interpreter::visit(WhileStmt &stmt) {
  try {
    while (isTruthy(evaluate(*stmt.cond))) {
      execute(*stmt.body);
    }
  } catch (const BreakException &) {
  }
}

void Interpreter::visit(BlockStmt &stmt) {
  execute_block(stmt.statements, std::make_shared<Environment>(env_));
}

void Interpreter::visit(BreakStmt &) { throw BreakException{}; }

void Interpreter::visit(FuncStmt &stmt) {
  auto function = std::make_shared<LoxFunction>(stmt, env_);
  env_->define(stmt.name.lexeme, function);
}

void Interpreter::visit(ReturnStmt &stmt) {
  LiteralValue value{};
  if (stmt.value)
    value = evaluate(*stmt.value);

  throw ReturnException(value);
}

// ...... Resolver ........

void Interpreter::resolve(Expr &expr, int depth) { locals[&expr] = depth; }

LiteralValue Interpreter::look_up_variable(const Token &name, Expr &expr) {
  auto it = locals.find(&expr);
  if (it != locals.end())
    return env_->get_at(it->second, name.lexeme);
  else
    return global_->get(name);
}

//...................  Helper Functions   .......................
//

LiteralValue Interpreter::evaluate(Expr &expr) { return expr.accept(*this); }
void Interpreter::execute(Stmt &stmt) { stmt.accept(*this); }

void Interpreter::execute_block(const std::vector<StmtPtr> &statements,
                                std::shared_ptr<Environment> new_env) {
  auto previous = env_;
  ScopeGuard guard([previous, this]() { env_ = previous; });

  env_ = new_env;

  for (const auto &stmt : statements)
    execute(*stmt);
}

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

std::string Interpreter::stringify(const LiteralValue &value) {
  return std::visit(
      [](auto val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return "nil";
        else if constexpr (std::is_same_v<T, bool>)
          return val ? "true" : "false";
        else if constexpr (std::is_same_v<T, double>) {
          std::string text = std::to_string(val);
          if (text.ends_with(".000000"))
            text = text.substr(0, text.size() - 7);
          return text;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LoxCallable>>)
          return val->to_string();
        else
          return val;
      },
      value);
}
