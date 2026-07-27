#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

void initScanner(const char *source, Scanner *scanner) {
  scanner->start = source;
  scanner->current = source;
  scanner->line = 1;
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static char advance(Scanner *scanner) {
  scanner->current++;
  return scanner->current[-1];
}

static bool match(char expected, Scanner *scanner) {
  if (isAtEnd(scanner))
    return false;
  if (*scanner->current != expected)
    return false;
  scanner->current++;
  return true;
}

static Token makeToken(TokenType type, Scanner *scanner) {
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.line = scanner->line;
  return token;
}

static Token errorToken(const char *msg, Scanner *scanner) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = scanner->start;
  token.length = (int)strlen(msg);
  token.line = scanner->line;
  return token;
}

Token scanToken(Scanner *scanner) {
  scanner->start = scanner->current;

  if (isAtEnd(scanner))
    return makeToken(TOKEN_EOF, scanner);

  char c = advance(scanner);

  switch (c) {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN, scanner);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN, scanner);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE, scanner);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE, scanner);
  case ';':
    return makeToken(TOKEN_SEMICOLON, scanner);
  case ',':
    return makeToken(TOKEN_COMMA, scanner);
  case '.':
    return makeToken(TOKEN_DOT, scanner);
  case '-':
    return makeToken(TOKEN_MINUS, scanner);
  case '+':
    return makeToken(TOKEN_PLUS, scanner);
  case '/':
    return makeToken(TOKEN_SLASH, scanner);
  case '*':
    return makeToken(TOKEN_STAR, scanner);
  case '!':
    return makeToken(match('=', scanner) ? TOKEN_BANG_EQUAL : TOKEN_BANG,
                     scanner);
  case '=':
    return makeToken(match('=', scanner) ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL,
                     scanner);
  case '<':
    return makeToken(match('=', scanner) ? TOKEN_LESS_EQUAL : TOKEN_LESS,
                     scanner);
  case '>':
    return makeToken(match('=', scanner) ? TOKEN_GREATER_EQUAL : TOKEN_GREATER,
                     scanner);
  }

  return errorToken("Unexpected character.", scanner);
}
