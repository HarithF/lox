#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
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

  std::shared_ptr<LoxCallable> bind(std::shared_ptr<LoxInstance>);
};

struct LoxClass : LoxCallable {
  std::string name_;
  std::unordered_map<std::string, std::shared_ptr<LoxFunction>> methods_;

  LoxClass(
      std::string name,
      std::unordered_map<std::string, std::shared_ptr<LoxFunction>> methods)
      : name_(name), methods_(std::move(methods)) {}

  int arity() override { return 0; }

  LiteralValue call(Interpreter &, std::vector<LiteralValue>) override {
    return std::make_shared<LoxInstance>(this);
  }

  std::shared_ptr<LoxFunction> find_method(std::string name);

  std::string to_string() const override { return "<class " + name_ + ">"; }
};

class LoxInstance : public std::enable_shared_from_this<LoxInstance> {
public:
  LoxInstance(LoxClass *klass) : klass(klass) {}

  std::string to_string() const { return klass->name_ + " instance"; }

  LiteralValue get(Token name);
  void set(Token name, LiteralValue value);

private:
  LoxClass *klass;
  std::unordered_map<std::string, LiteralValue> fields_{};
};
