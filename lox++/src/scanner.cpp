#include "scanner.h"
#include "token.h"
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
static const std::unordered_map<std::string, TokenType> keywords = {
    {"and", TokenType::AND},       {"class", TokenType::CLASS},
    {"else", TokenType::ELSE},     {"false", TokenType::FALSE},
    {"for", TokenType::FOR},       {"fun", TokenType::FUN},
    {"if", TokenType::IF},         {"nil", TokenType::NIL},
    {"or", TokenType::OR},         {"print", TokenType::PRINT},
    {"return", TokenType::RETURN}, {"super", TokenType::SUPER},
    {"this", TokenType::THIS},     {"true", TokenType::TRUE},
    {"var", TokenType::VAR},       {"while", TokenType::WHILE},
};
std::optional<TokenType> keyword_type(const std::string &word) {
  auto it = keywords.find(word);
  if (it != keywords.end())
    return it->second;
  return {};
}
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
    } else if (match('*')) {
      int depth = 1;
      while (!is_endfile() && depth > 0) {
        if (stream_.peek() == '\n')
          line++;

        if (advance() == '/' && stream_.peek() == '*') {
          advance(); // consume *
          depth++;
        } else if (buff.back() == '*' && stream_.peek() == '/') {
          advance(); // consume /
          depth--;
        }
      }
      if (is_endfile() && depth > 0)
        error_handler_.error(line, "Unterminated block comment!");
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
    if (std::isdigit(c)) {
      lex_number();
    } else if (is_alpha(c)) {
      lex_id();
    } else {
      error_handler_.error(line, "Unexpected character.");
      buff.clear();
    }
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
void Scanner::lex_number() {
  while (isdigit(stream_.peek())) {
    advance();
  }
  if (stream_.peek() == '.') {
    auto dot = stream_.get();
    if (isdigit(stream_.peek())) {
      buff += dot;
      while (isdigit(stream_.peek())) {
        advance();
      }
    } else {
      stream_.putback(dot);
    }
  }
  auto value = std::stod(buff);
  tok_add(TokenType::NUMBER, value);
}
void Scanner::lex_id() {
  while (is_alpha_num(stream_.peek())) {
    advance();
  }
  if (auto type = keyword_type(buff)) {
    tok_add(*type);
  } else {
    tok_add(TokenType::IDENTIFIER);
  }
}
