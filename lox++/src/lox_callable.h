#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>

struct LoxCallable;
struct Interpreter;
struct FuncStmt;
struct Environment;

using LiteralValue = std::variant<std::string, double, bool, std::monostate,
                                  std::shared_ptr<LoxCallable>>;

struct LoxCallable {
  virtual LiteralValue call(Interpreter &, std::vector<LiteralValue>) = 0;
  virtual int arity() = 0;
  virtual std::string to_string() const = 0;
  virtual ~LoxCallable() = default;
};

// native functions
struct ClockCallable : LoxCallable {
  int arity() override { return 0; }

  LiteralValue call(Interpreter &, std::vector<LiteralValue>) override {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
  }

  std::string to_string() const override { return "<native fn>"; }
};

// Lox Functions

struct LoxFunction : LoxCallable {
  FuncStmt &declaration_;
  Environment *closure_;

  LoxFunction(FuncStmt &declaration, Environment *closure);
  int arity() override;
  LiteralValue call(Interpreter &, std::vector<LiteralValue>) override;
  std::string to_string() const override;
};
