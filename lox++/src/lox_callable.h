#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct LoxCallable;
struct Interpreter;
struct FuncStmt;
struct Environment;
struct LoxInstance;

class Token;

using LiteralValue =
    std::variant<std::string, double, bool, std::monostate,
                 std::shared_ptr<LoxCallable>, std::shared_ptr<LoxInstance>>;

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
  std::shared_ptr<Environment> closure_;

  LoxFunction(FuncStmt &declaration, std::shared_ptr<Environment> closure)
      : declaration_(declaration), closure_(std::move(closure)) {}
  int arity() override;
  LiteralValue call(Interpreter &, std::vector<LiteralValue>) override;
  std::string to_string() const override;
};

struct LoxClass : LoxCallable {
  std::string name_;

  LoxClass(std::string name) : name_(name) {}

  int arity() override { return 0; }

  LiteralValue call(Interpreter &, std::vector<LiteralValue>) override {
    return std::make_shared<LoxInstance>(this);
  }

  std::string to_string() const override { return "<class " + name_ + ">"; }
};

class LoxInstance {
public:
  LoxInstance(LoxClass *klass) : klass(klass) {}

  std::string to_string() const { return klass->name_ + " instance"; }

  LiteralValue get(Token &name);

private:
  LoxClass *klass;
  std::unordered_map<std::string, LiteralValue> fields_{};
};
