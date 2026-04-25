#include "lox_callable.h"
#include "Stmt.h"
#include "environment.h"
#include "interpreter.h"

// LoxFunction

int LoxFunction::arity() { return declaration_.params.size(); }

LiteralValue LoxFunction::call(Interpreter &interpreter,
                               std::vector<LiteralValue> args) {

  auto env = std::make_shared<Environment>(closure_);
  for (int i = 0; i < (int)declaration_.params.size(); i++)
    env->define(declaration_.params[i].lexeme, args[i]);
  try {
    interpreter.execute_block(declaration_.body, env);
  } catch (ReturnException ret_value) {
    return ret_value.value_;
  }
  return std::monostate{};
}

std::string LoxFunction::to_string() const {
  return "<fn " + declaration_.name.lexeme + ">";
}
