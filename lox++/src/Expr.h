#pragma once
#include "token.h"
#include <memory>

struct Assign;
struct Binary;
struct Grouping;
struct Literal;
struct Logical;
struct Unary;
struct Ternary;
struct Variable;

struct ExprVisitor {
	virtual LiteralValue visit(Assign&) = 0;
	virtual LiteralValue visit(Binary&) = 0;
	virtual LiteralValue visit(Grouping&) = 0;
	virtual LiteralValue visit(Literal&) = 0;
	virtual LiteralValue visit(Logical&) = 0;
	virtual LiteralValue visit(Unary&) = 0;
	virtual LiteralValue visit(Ternary&) = 0;
	virtual LiteralValue visit(Variable&) = 0;
	virtual ~ExprVisitor() = default;
};

struct Expr {
	virtual ~Expr() = default;
	virtual LiteralValue accept(ExprVisitor&) = 0;
};

struct Assign : public  Expr {
	Token name;

	std::unique_ptr<Expr> expression;


	Assign(Token name, std::unique_ptr<Expr> expression)
		: name(name), expression(std::move(expression)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
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
struct Logical : public  Expr {
	std::unique_ptr<Expr> left;

	Token operator_;

	std::unique_ptr<Expr> right;


	Logical(std::unique_ptr<Expr> left, Token operator_, std::unique_ptr<Expr> right)
		: left(std::move(left)), operator_(operator_), right(std::move(right)) {}
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
struct Ternary : public  Expr {
	std::unique_ptr<Expr> cond_;

	std::unique_ptr<Expr> then_b;

	std::unique_ptr<Expr> else_b;


	Ternary(std::unique_ptr<Expr> cond_, std::unique_ptr<Expr> then_b, std::unique_ptr<Expr> else_b)
		: cond_(std::move(cond_)), then_b(std::move(then_b)), else_b(std::move(else_b)) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};
struct Variable : public  Expr {
	Token name;


	Variable(Token name)
		: name(name) {}
	LiteralValue accept(ExprVisitor& visitor) override {
		return visitor.visit(*this);
	}
};

