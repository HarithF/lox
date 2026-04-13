#pragma once
#include "Expr.h"
#include "token.h"
#include <initializer_list>
#include <vector>

using ExprPtr = std::unique_ptr<Expr>;

class Parser {
public:
  Parser(const std::vector<Token> &tokens) : tokens_(tokens) {}

private:
  std::vector<Token> tokens_;
  int curr = 0;

  ExprPtr expression();
  ExprPtr equality();
  ExprPtr comparasion();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr primary();

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
