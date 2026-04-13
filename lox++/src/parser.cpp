#include "parser.h"
#include "Expr.h"
#include "token.h"
#include <memory>
#include <utility>
#include <variant>
ExprPtr Parser::expression() { return equality(); }

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
    return std::make_unique<Literal>(false);
  case TokenType::TRUE:
    return std::make_unique<Literal>(true);
  case TokenType::NIL:
    return std::make_unique<Literal>(std::monostate);
  case TokenType::NUMBER:
  case TokenType::STRING:
    return std::make_unique<Literal>(previous().literal);

  case TokenType::LEFT_PAREN:
    auto expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    return std::make_unique<Grouping>(expr);
  default:
    break;
  }
}
