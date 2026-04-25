#pragma once
#include "Expr.h"
#include <vector>

struct BlockStmt;
struct ExprStmt;
struct FuncStmt;
struct IfStmt;
struct PrintStmt;
struct ReturnStmt;
struct VarStmt;
struct WhileStmt;
struct BreakStmt;

struct StmtVisitor {
	virtual void visit(BlockStmt&) = 0;
	virtual void visit(ExprStmt&) = 0;
	virtual void visit(FuncStmt&) = 0;
	virtual void visit(IfStmt&) = 0;
	virtual void visit(PrintStmt&) = 0;
	virtual void visit(ReturnStmt&) = 0;
	virtual void visit(VarStmt&) = 0;
	virtual void visit(WhileStmt&) = 0;
	virtual void visit(BreakStmt&) = 0;
	virtual ~StmtVisitor() = default;
};

struct Stmt {
	virtual ~Stmt() = default;
	virtual void accept(StmtVisitor&) = 0;
};

struct BlockStmt : public  Stmt {
	std::vector<std::unique_ptr<Stmt>> statements;


	BlockStmt(std::vector<std::unique_ptr<Stmt>> statements)
		: statements(std::move(statements)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct ExprStmt : public  Stmt {
	std::unique_ptr<Expr> expression;


	ExprStmt(std::unique_ptr<Expr> expression)
		: expression(std::move(expression)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct FuncStmt : public  Stmt {
	Token name;

	std::vector<Token> params;

	std::vector<std::unique_ptr<Stmt>> body;


	FuncStmt(Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body)
		: name(name), params(params), body(std::move(body)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct IfStmt : public  Stmt {
	std::unique_ptr<Expr> cond;

	std::unique_ptr<Stmt> then_b;

	std::unique_ptr<Stmt> else_b;


	IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then_b, std::unique_ptr<Stmt> else_b)
		: cond(std::move(cond)), then_b(std::move(then_b)), else_b(std::move(else_b)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct PrintStmt : public  Stmt {
	std::unique_ptr<Expr> expression;


	PrintStmt(std::unique_ptr<Expr> expression)
		: expression(std::move(expression)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct ReturnStmt : public  Stmt {
	Token keyword;

	std::unique_ptr<Expr> value;


	ReturnStmt(Token keyword, std::unique_ptr<Expr> value)
		: keyword(keyword), value(std::move(value)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct VarStmt : public  Stmt {
	Token name;

	std::unique_ptr<Expr> initializer;


	VarStmt(Token name, std::unique_ptr<Expr> initializer)
		: name(name), initializer(std::move(initializer)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct WhileStmt : public  Stmt {
	std::unique_ptr<Expr> cond;

	std::unique_ptr<Stmt> body;


	WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
		: cond(std::move(cond)), body(std::move(body)) {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};
struct BreakStmt : public  Stmt {
	BreakStmt() {}
	void accept(StmtVisitor& visitor) override {
		visitor.visit(*this);
	}
};

