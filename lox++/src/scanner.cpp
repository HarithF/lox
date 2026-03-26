#include "scanner.h"
#include "token.h"
#include <variant>
#include <vector>

std::vector<Token> Scanner::scan_tokens() {
  while (stream_.peek() != EOF) {
    tok_scan();
  }
  tokens_.push_back(Token(TokenType::EOF_, "", std::monostate{}, line));
  return tokens_;
}

void Scanner::tok_scan() {
  char c = advance();

  switch (c) {
  case '(':
    tok_add(TokenType::LEFT_PAREN);
    break;
  case ')':
    tok_add(TokenType::RIGHT_PAREN);
    break;
  case '{':
    tok_add(TokenType::LEFT_BRACE);
    break;
  case '}':
    tok_add(TokenType::RIGHT_BRACE);
    break;
  case ',':
    tok_add(TokenType::COMMA);
    break;
  case '.':
    tok_add(TokenType::DOT);
    break;
  case '-':
    tok_add(TokenType::MINUS);
    break;
  case '+':
    tok_add(TokenType::PLUS);
    break;
  case ';':
    tok_add(TokenType::SEMICOLON);
    break;
  case '*':
    tok_add(TokenType::STAR);
    break;
  case '!':
    tok_add(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
    break;
  case '=':
    tok_add(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
    break;
  case '<':
    tok_add(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
    break;
  case '>':
    tok_add(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
    break;
  case '/':
    if (match('/')) {
      // comments
      while (stream_.peek() != '\n' && !is_endfile()) {
        advance();
      }
      buff.clear();
    } else {
      tok_add(TokenType::SLASH);
    }
    break;
  case ' ':
  case '\r':
  case '\t': // ignore white space
    buff.clear();
    break;
  case '\n':
    line++;
    buff.clear();
    break;

  case '"':
    lex_string();
    break;
  default:
    error_handler_.error(line, "Unexpected character.");
    buff.clear();
    break;
  }
}

char Scanner::advance() {
  char cur = stream_.get();
  buff += cur;
  return cur;
}

void Scanner::tok_add(TokenType tok, LiteralValue lit) {
  tokens_.push_back(Token(tok, buff, lit, line));
  buff.clear();
}
bool Scanner::match(char expect) {
  if (is_endfile())
    return false;
  if (stream_.peek() != expect)
    return false;
  buff += stream_.get();
  return true;
}
void Scanner::lex_string() {
  while (stream_.peek() != '"' && !is_endfile()) {
    if (stream_.peek() == '\n')
      line++;
    advance();
  }
  if (is_endfile()) {
    error_handler_.error(line, "Unterminated string.");
    return;
  }
  advance();
  auto value = buff.substr(1, buff.size() - 2); // strip opening and closing "
  tok_add(TokenType::STRING, value);
}
