#pragma once
#include "Expr.h"
#include "token.h"
#include <print>
#include <string>
#include <variant>

struct AstPrinter : ExprVisitor {
  std::string print(Expr &expr) {
    return std::get<std::string>(expr.accept(*this));
  }

  LiteralValue visit(Binary &expr) override {
    return parenthesize(expr.operator_.lexeme, *expr.left, *expr.right);
  }

  LiteralValue visit(Grouping &expr) override {
    return parenthesize("group", *expr.expression);
  }

  LiteralValue visit(Literal &expr) override {
    return std::visit(
        [](auto val) -> LiteralValue {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            return std::string("nil");
          else if constexpr (std::is_same_v<T, double>)
            return std::to_string(val);
          else
            return val;
        },
        expr.value);
  }

  LiteralValue visit(Unary &expr) override {
    return parenthesize(expr.operator_.lexeme, *expr.right);
  }

  static void test_printer() {
    AstPrinter printer;
    auto expression = std::make_unique<Binary>(
        std::make_unique<Unary>(
            Token(TokenType::MINUS, "-", std::monostate{}, 1),
            std::make_unique<Literal>(LiteralValue{123.0})),
        Token(TokenType::STAR, "*", std::monostate{}, 1),
        std::make_unique<Grouping>(
            std::make_unique<Literal>(LiteralValue{45.67})));

    std::println("{}", printer.print(*expression));
  }

private:
  template <typename... Args>
  std::string parenthesize(const std::string &name, Args &...exprs) {
    std::string result = "(" + name;
    ((result += " " + std::get<std::string>(exprs.accept(*this))), ...);
    result += ")";
    return result;
  }
};
