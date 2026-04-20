#pragma once

#include "Expr.h"
#include <memory>

struct ExprStmt;
struct PrintStmt;

struct StmtVisitor {
  virtual void visit(ExprStmt &) = 0;
  virtual void visit(PrintStmt &) = 0;
  virtual ~StmtVisitor() = default;
};

struct Stmt {
  virtual ~Stmt() = default;
  virtual void accept(StmtVisitor &) = 0;
};

struct ExprStmt : public Stmt {
  std::unique_ptr<Expr> expression;

  ExprStmt(std::unique_ptr<Expr> expression)
      : expression(std::move(expression)) {}
  void accept(StmtVisitor &visitor) override { visitor.visit(*this); }
};
struct PrintStmt : public Stmt {
  std::unique_ptr<Expr> expression;

  PrintStmt(std::unique_ptr<Expr> expression)
      : expression(std::move(expression)) {}
  void accept(StmtVisitor &visitor) override { visitor.visit(*this); }
};
