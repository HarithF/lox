#include "lox_callable.h"
#include "Stmt.h"
#include "environment.h"
#include "interpreter.h"
#include <memory>

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

std::shared_ptr<LoxFunction> LoxClass::find_method(const std::string name) {
  auto it = methods_.find(name);
  if (it != methods_.end()) {
    return it->second;
  }
  return nullptr;
}

LiteralValue LoxInstance::get(Token name) {
  auto it = fields_.find(name.lexeme);
  if (it != fields_.end()) {
    return it->second;
  }
  if (auto method = klass->find_method(name.lexeme)) {
    return method->bind(shared_from_this());
    ;
  }
  throw RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
}

std::shared_ptr<LoxCallable>
LoxFunction::bind(std::shared_ptr<LoxInstance> instance) {
  auto env = std::make_shared<Environment>(closure_);
  env->define("this", std::move(instance));
  return std::make_shared<LoxFunction>(declaration_, std::move(env));
}

void LoxInstance::set(Token name, LiteralValue value) {
  fields_[name.lexeme] = std::move(value);
}
