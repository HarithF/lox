#include "parser.h"
#include "Expr.h"
#include "Stmt.h"
#include "token.h"
#include <memory>
#include <utility>
#include <variant>
#include <vector>
ExprPtr Parser::expression() { return comma(); }

ExprPtr Parser::comma() {
  auto expr = assignment();
  while (match({TokenType::COMMA})) {
    Token op = previous();
    auto right = assignment();
    expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::assignment() {
  auto expr = ternary();
  if (match({TokenType::EQUAL})) {
    auto equals = previous();
    auto value = assignment();

    if (auto *var = dynamic_cast<Variable *>(expr.get())) {
      return std::make_unique<Assign>(var->name, std::move(value));
    }
    error(equals, "Invalid assignment target.");
  }
  return expr;
}

ExprPtr Parser::ternary() {
  auto expr = or_op();

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
ExprPtr Parser::or_op() {
  auto expr = and_op();

  while (match({TokenType::OR})) {
    Token operator_ = previous();
    auto right = and_op();
    expr = std::make_unique<Logical>(expr, operator_, right)
  }
  return expr;
}
ExprPtr Parser::and_op() {
  auto expr = equality();

  while (match({TokenType::OR})) {
    Token operator_ = previous();
    auto right = equality();
    expr = std::make_unique<Logical>(expr, operator_, right)
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
  case TokenType::IDENTIFIER:
    advance();
    return std::make_unique<Variable>(previous());
  default:
    throw error(peek(), "Expect expression.");
  }
}

StmtPtr Parser::declaration() {
  try {
    if (match({TokenType::VAR}))
      return var_declaration();
    return statement();
  } catch (ParseError &error) {
    synchronize();
    return nullptr;
  }
}

StmtPtr Parser::var_declaration() {
  auto name = consume(TokenType::IDENTIFIER, "Expect variable name.");
  ExprPtr init;
  if (match({TokenType::EQUAL}))
    init = expression();

  consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
  return std::make_unique<VarStmt>(name, std::move(init));
}

StmtPtr Parser::statement() {
  switch (peek().type) {
  case TokenType::IF:
    advance();
    return if_stmt();
  case TokenType::PRINT:
    advance();
    return print_stmt();
  case TokenType::LEFT_BRACE:
    advance();
    return std::make_unique<BlockStmt>(block());

  default:
    return expr_stmt();
  }
}

std::vector<StmtPtr> Parser::block() {
  std::vector<StmtPtr> statements{};

  while (!check(TokenType::RIGHT_BRACE) && !is_at_end()) {
    statements.push_back(declaration());
  }
  consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
  return statements;
}

StmtPtr Parser::print_stmt() {
  auto val = expression();
  consume(TokenType::SEMICOLON, "Expect ';' after expression");
  return std::make_unique<PrintStmt>(std::move(val));
}

StmtPtr Parser::expr_stmt() {
  auto expr = expression();
  consume(TokenType::SEMICOLON, "Expect ';' after expression");
  return std::make_unique<ExprStmt>(std::move(expr));
}

StmtPtr Parser::if_stmt() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
  auto cond = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition");

  auto then_b = statement();
  StmtPtr else_b{};

  if (match({TokenType::ELSE})) {
    else_b = statement();
  }

  return std::make_unique<IfStmt>(std::move(cond), std::move(then_b),
                                  std::move(else_b));
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
