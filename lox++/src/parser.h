#pragma once
#include "Expr.h"
#include "Stmt.h"
#include "error_handler.h"
#include "token.h"
#include <initializer_list>
#include <string>
#include <vector>

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct ParseError : std::runtime_error {
  ParseError() : std::runtime_error("parse error") {}
};

class Parser {
public:
  Parser(const std::vector<Token> &tokens, ErrorHandler &error_handler)
      : tokens_(tokens), error_handler_(error_handler) {}

  std::vector<StmtPtr> parse() {
    std::vector<StmtPtr> statements{};
    while (!is_at_end()) {
      statements.push_back(declaration());
    }
    return statements;
  }

  ExprPtr parse_expr() {
    try {
      auto expr = expression();
      if (!is_at_end())
        return nullptr;
      return expr;
    } catch (const ParseError &) {
      return nullptr;
    }
  }

private:
  std::vector<Token> tokens_;
  ErrorHandler error_handler_;
  int curr = 0;
  int loop_depth_ = 0;

  ExprPtr expression();
  ExprPtr comma();
  ExprPtr assignment();
  ExprPtr ternary();
  ExprPtr or_op();
  ExprPtr and_op();
  ExprPtr equality();
  ExprPtr comparasion();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr call();
  ExprPtr primary();

  StmtPtr declaration();
  StmtPtr var_declaration();
  StmtPtr statement();
  StmtPtr print_stmt();
  StmtPtr expr_stmt();
  StmtPtr if_stmt();
  StmtPtr while_stmt();
  StmtPtr for_stmt();
  StmtPtr break_stmt();
  StmtPtr function_stmt(std::string);
  std::vector<StmtPtr> block();

  void synchronize();
  ExprPtr finish_call(ExprPtr);

  Token consume(TokenType type, std::string message) {
    if (check(type))
      return advance();

    throw error(peek(), message);
  };

  ParseError error(const Token &token, const std::string &message) {
    if (token.type == TokenType::EOF_) {
      error_handler_.report(token.line, " at end", message);
    } else {
      error_handler_.report(token.line, " at '" + token.lexeme + "'", message);
    }
    return ParseError();
  }

  bool match(std::initializer_list<TokenType> types) {
    for (auto type : types) {
      if (check(type)) {
        advance();
        return true;
      }
    }
    return false;
  };
  bool check(TokenType type) {
    if (is_at_end())
      return false;
    return peek().type == type;
  };

  Token peek() { return tokens_.at(curr); }
  Token previous() { return tokens_.at(curr - 1); }
  Token advance() {
    if (!is_at_end())
      curr++;
    return previous();
  };

  bool is_at_end() { return peek().type == TokenType::EOF_; };
};
