#pragma once
#include "lox_callable.h"
#include "token.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

struct RuntimeError : std::runtime_error {
  Token token_;
  RuntimeError(Token token, const std::string &message)
      : token_(token), std::runtime_error(message) {}
};

struct BreakException {};
struct ReturnException {
  LiteralValue value_;

  ReturnException(LiteralValue value) : value_(value) {};
};

class ScopeGuard {
  std::function<void()> f;

public:
  ScopeGuard(std::function<void()> f) : f(std::move(f)) {}
  ~ScopeGuard() { f(); }
};

class Environment {
public:
  Environment() : enclosing_(nullptr) {}

  Environment(std::shared_ptr<Environment> enclosing)
      : enclosing_(std::move(enclosing)) {}

  void define(const std::string &name, std::optional<LiteralValue> value) {
    if (values.contains(name))
      throw std::runtime_error("Variable '" + name +
                               "' already declared in this scope.");
    values[name] = std::move(value);
  }

  void assign(const Token &name, LiteralValue value) {
    auto it = values.find(name.lexeme);
    if (it != values.end()) {
      it->second = std::move(value);
      return;
    }
    if (enclosing_) {
      enclosing_->assign(name, value);
      return;
    }
    throw RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  LiteralValue get(const Token &name) {
    auto it = values.find(name.lexeme);
    if (it != values.end()) {
      if (!it->second)
        throw RuntimeError(name,
                           "Variable '" + name.lexeme + "' is uninitialized.");
      return *it->second;
    }
    if (enclosing_)
      return enclosing_->get(name);
    throw RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  bool is_defined(const std::string &name) const {
    return values.contains(name);
  }

  LiteralValue get_at(int distance, std::string name) {
    auto env = ancestor(distance);
    auto it = env->values.find(name);
    if (it != env->values.end() && it->second)
      return *it->second;
    throw std::runtime_error("Undefined variable '" + name + "' at distance.");
  }

  void assign_at(int distance, Token name, LiteralValue value) {
    ancestor(distance)->values[name.lexeme] = std::move(value);
  }

  Environment *ancestor(int distance) {
    Environment *env = this;
    for (int i = 0; i < distance; i++)
      env = env->enclosing_.get();

    return env;
  }

private:
  std::shared_ptr<Environment> enclosing_;
  std::unordered_map<std::string, std::optional<LiteralValue>> values;
};
