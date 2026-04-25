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

private:
  std::shared_ptr<Environment> enclosing_;
  std::unordered_map<std::string, std::optional<LiteralValue>> values;
};
