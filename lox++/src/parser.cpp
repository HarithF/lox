#include "parser.h"
#include "Expr.h"
#include "token.h"
#include <memory>
#include <utility>
#include <variant>
ExprPtr Parser::expression() { return comma(); }

ExprPtr Parser::comma() {
  auto expr = ternary();
  while (match({TokenType::COMMA})) {
    Token op = previous();
    auto right = ternary();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::ternary() {
  auto expr = equality();

  if (match({TokenType::QUESTION})) {
    auto thenBranch = expression();
    consume(TokenType::COLON,
            "Expect ':' after then branch of ternary operator.");
    auto elseBranch = ternary();
    expr = std::make_unique<Ternary>(std::move(expr), std::move(thenBranch),
                                     std::move(elseBranch));
  }

  return expr;
}
ExprPtr Parser::equality() {
  auto expr = comparasion();
  while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
    Token op = previous();
    auto right = comparasion();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }

  return expr;
}

ExprPtr Parser::comparasion() {
  auto expr = term();
  while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS,
                TokenType::LESS_EQUAL})) {
    Token op = previous();
    auto right = term();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::term() {
  auto expr = factor();

  while (match({TokenType::PLUS, TokenType::MINUS})) {
    Token op = previous();
    auto right = factor();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::factor() {
  auto expr = unary();

  while (match({TokenType::STAR, TokenType::SLASH})) {
    Token op = previous();
    auto right = unary();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::unary() {
  if (match({TokenType::BANG, TokenType::MINUS})) {
    Token op = previous();
    auto right = unary();
    return std::make_unique<Unary>(op, std::move(right));
  }
  return primary();
}

ExprPtr Parser::primary() {
  switch (peek().type) {
  case TokenType::FALSE:
    advance();
    return std::make_unique<Literal>(false);
  case TokenType::TRUE:
    advance();
    return std::make_unique<Literal>(true);
  case TokenType::NIL:
    advance();
    return std::make_unique<Literal>(std::monostate());
  case TokenType::NUMBER:
  case TokenType::STRING: {
    auto val = peek().literal;
    advance();
    return std::make_unique<Literal>(val);
  }
  case TokenType::LEFT_PAREN: {
    advance();
    auto expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    return std::make_unique<Grouping>(std::move(expr));
  }
  default:
    throw error(peek(), "Expect expression.");
  }
}

void Parser::synchronize() {
  advance();
  while (!is_at_end()) {
    if (previous().type == TokenType::SEMICOLON)
      return;
    switch (peek().type) {
    case TokenType::CLASS:
    case TokenType::FUN:
    case TokenType::VAR:
    case TokenType::FOR:
    case TokenType::IF:
    case TokenType::WHILE:
    case TokenType::PRINT:
    case TokenType::RETURN:
      return;
    default:
      break;
    }
    advance();
  }
}
