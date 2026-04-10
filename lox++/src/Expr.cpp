#pragma once
#include "token.h"
#include <memory>

struct Binary;
struct Grouping;
struct Literal;
struct Unary;

struct ExprVisitor {
	virtual LiteralValue visit(Binary&) = 0;
	virtual LiteralValue visit(Grouping&) = 0;
	virtual LiteralValue visit(Literal&) = 0;
	virtual LiteralValue visit(Unary&) = 0;
	virtual ~ExprVisitor() = default;
};

struct Expr {
	virtual ~Expr() = default;
	virtual LiteralValue accept(ExprVisitor&) = 0;
};

struct Binary : public  Expr {
	std::unique_ptr<Expr> left;

	Token operator_;

	std::unique_ptr<Expr> right;


	Binary(std::unique_ptr<Expr> left, Token operator_, std::unique_ptr<Expr> right)
		: left(std::move(left)), operator_(operator_), right(std::move(right)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};
struct Grouping : public  Expr {
	std::unique_ptr<Expr> expression;


	Grouping(std::unique_ptr<Expr> expression)
		: expression(std::move(expression)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};
struct Literal : public  Expr {
	LiteralValue value;


	Literal(LiteralValue value)
		: value(std::move(value)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};
struct Unary : public  Expr {
	Token operator_;

	std::unique_ptr<Expr> right;


	Unary(Token operator_, std::unique_ptr<Expr> right)
		: operator_(operator_), right(std::move(right)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};

