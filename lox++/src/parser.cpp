#include "parser.h"
#include "Expr.h"
#include "Stmt.h"
#include "token.h"
#include <cmath>
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
    expr = std::make_unique<Logical>(std::move(expr), std::move(operator_),
                                     std::move(right));
  }
  return expr;
}
ExprPtr Parser::and_op() {
  auto expr = equality();

  while (match({TokenType::AND})) {
    Token operator_ = previous();
    auto right = equality();
    expr = std::make_unique<Logical>(std::move(expr), std::move(operator_),
                                     std::move(right));
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
  return call();
}

ExprPtr Parser::call() {
  auto expr = primary();

  while (true) {
    if (match({TokenType::LEFT_PAREN})) {
      expr = finish_call(std::move(expr));
    } else {
      break;
    }
  }
  return expr;
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

ExprPtr Parser::finish_call(ExprPtr callee) {
  std::vector<ExprPtr> args{};

  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      if (args.size() >= 255)
        error(peek(), "Can't have more than 255 arguments.");
      args.push_back(assignment());
    } while (match({TokenType::COMMA}));
  }
  Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
  return std::make_unique<Call>(std::move(callee), paren, std::move(args));
}

//       .....  Statements .......
StmtPtr Parser::declaration() {
  try {
    if (match({TokenType::VAR}))
      return var_declaration();
    if (match({TokenType::FUN}))
      return function_stmt("function");
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
  case TokenType::RETURN:
    return return_stmt();

  case TokenType::WHILE:
    advance();
    return while_stmt();

  case TokenType::FOR:
    advance();
    return for_stmt();
  case TokenType::BREAK:
    return break_stmt();

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

StmtPtr Parser::while_stmt() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
  auto cond = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");
  loop_depth_++;
  auto body = statement();
  loop_depth_--;
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

StmtPtr Parser::for_stmt() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
  StmtPtr init{};
  if (match({TokenType::SEMICOLON}))
    ;
  // continue;
  else if (match({TokenType::VAR}))
    init = var_declaration();
  else
    init = expr_stmt();

  ExprPtr cond{};
  if (!match({TokenType::SEMICOLON}))
    cond = expression();
  consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

  ExprPtr increment{};
  if (!match({TokenType::SEMICOLON}))
    increment = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ';' after for clauses");
  loop_depth_++;
  auto body = statement();
  if (increment) {
    std::vector<StmtPtr> stmts;
    stmts.push_back(std::move(body));
    stmts.push_back(std::make_unique<ExprStmt>(std::move(increment)));
    body = std::make_unique<BlockStmt>(std::move(stmts));
  }
  if (!cond)
    cond = std::make_unique<Literal>(true);
  body = std::make_unique<WhileStmt>(std::move(cond), std::move(body));

  if (init) {
    std::vector<StmtPtr> stmts;
    stmts.push_back(std::move(init));
    stmts.push_back(std::move(body));
    body = std::make_unique<BlockStmt>(std::move(stmts));
  }
  loop_depth_--;
  return body;
}

StmtPtr Parser::break_stmt() {
  auto keyword = advance();
  consume(TokenType::SEMICOLON, "Expect ';' after 'break'.");
  if (loop_depth_ == 0)
    throw error(keyword, "Cannot use 'break' outside of a loop.");
  return std::make_unique<BreakStmt>();
}

StmtPtr Parser::function_stmt(std::string kind) {
  Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
  consume(TokenType::LEFT_PAREN, "Expect '(' after " + kind + " name.");
  std::vector<Token> params{};
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      if (params.size() >= 255)
        error(peek(), "Cannot have mroe than 255 parameters");
      params.push_back(
          consume(TokenType::IDENTIFIER, "Expect parameter name."));
    } while (match({TokenType::COMMA}));
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters");
  consume(TokenType::LEFT_BRACE, "Expect '{' before " + kind + " body.");
  auto body = block();
  return std::make_unique<FuncStmt>(name, std::move(params), std::move(body));
}

StmtPtr Parser::return_stmt() {
  Token keyword = advance();
  ExprPtr value{};

  if (!check(TokenType::SEMICOLON))
    value = expression();

  consume(TokenType::SEMICOLON, "Expect ';' after return value");
  return std::make_unique<ReturnStmt>(keyword, std::move(value));
}

// ..............   Helper Functions   .................

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
