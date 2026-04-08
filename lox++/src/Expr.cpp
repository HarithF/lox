#pragma once
#include "token.h"
#include <memory>

struct Expr {
  virtual ~Expr() = default;
};

struct Binary : public Expr {
  std::unique_ptr<Expr> left;

  Token operator_;

  std::unique_ptr<Expr> right;

  Binary(std::unique_ptr<Expr> left, Token operator_,
         std::unique_ptr<Expr> right)
      : left(std::move(left)), operator_(operator_), right(std::move(right)) {}
};
struct Grouping : public Expr {
  std::unique_ptr<Expr> expression;

  Grouping(std::unique_ptr<Expr> expression)
      : expression(std::move(expression)) {}
};
struct Literal : public Expr {
  LiteralValue value;

  Literal(LiteralValue value) : value(value) {}
};
struct Unary : public Expr {
  Token operator_;

  std::unique_ptr<Expr> right;

  Unary(Token operator_, std::unique_ptr<Expr> right)
      : operator_(operator_), right(std::move(right)) {}
};
