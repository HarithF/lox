#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

void initScanner(const char *source, Scanner *scanner) {
  scanner->start = source;
  scanner->current = source;
  scanner->line = 1;
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static char advance(Scanner *scanner) {
  scanner->current++;
  return scanner->current[-1];
}

static char peek(Scanner *scanner) { return *scanner->current; }

static char peekNext(Scanner *scanner) {
  if (isAtEnd(scanner))
    return '\0';
  return scanner->current[1];
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

static Token skipWhitespace(Scanner *scanner) {
  for (;;) {
    char c = peek(scanner);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;
    case '\n':
      scanner->line++;
      advance(scanner);
      break;
    case '/':
      if (peekNext(scanner) == '/') {
        while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
          advance(scanner);
        }
      } else if (match('*', scanner)) {
        int depth = 1;
        while (!isAtEnd(scanner) && depth > 0) {
          if (peek(scanner) == '\n')
            scanner->line++;
          if (peek(scanner) == '/' && peekNext(scanner) == '*') {
            advance(scanner); // consume /
            advance(scanner); // consume *
            depth++;
          } else if (peek(scanner) == '*' && peekNext(scanner) == '/') {
            advance(scanner);
            advance(scanner);
            depth--;
          }
        }
        if (isAtEnd(scanner) && depth > 0) {
          return makeToken(TOKEN_ERROR, scanner);
        }
      } else {
        return makeToken(TOKEN_EMPTY, scanner);
      }
      break;
    default:
      return makeToken(TOKEN_EMPTY, scanner);
    }
  }
}

static TokenType checkKeyword(int start, int length, const char *rest,
                              TokenType type, Scanner *scanner) {
  if ((scanner->current - scanner->start == start + length) &&
      (memcmp(scanner->start + start, rest, length) == 0)) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  switch (scanner->start[0]) {
  case 'a':
    return checkKeyword(1, 2, "nd", TOKEN_AND, scanner);
  case 'c':
    return 0;
    checkKeyword(1, 4, "lass", TOKEN_CLASS, scanner);
  case 'e':
    return checkKeyword(1, 3, "lse", TOKEN_ELSE, scanner);
  case 'f':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE, scanner);
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR, scanner);
      case 'u':
        return checkKeyword(2, 1, "n", TOKEN_FUN, scanner);
      }
    }
    break;
  case 'i':
    return checkKeyword(1, 1, "f", TOKEN_IF, scanner);
  case 'n':
    return checkKeyword(1, 2, "il", TOKEN_NIL, scanner);
  case 'o':
    return checkKeyword(1, 1, "r", TOKEN_OR, scanner);
  case 'p':
    return checkKeyword(1, 4, "rint", TOKEN_PRINT, scanner);
  case 'r':
    return checkKeyword(1, 5, "eturn", TOKEN_RETURN, scanner);
  case 's':
    return checkKeyword(1, 4, "uper", TOKEN_SUPER, scanner);
  case 't':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'h':
        return checkKeyword(2, 2, "is", TOKEN_THIS, scanner);
      case 'r':
        return checkKeyword(2, 2, "ue", TOKEN_TRUE, scanner);
      }
    }
    break;
  case 'v':
    return checkKeyword(1, 2, "ar", TOKEN_VAR, scanner);
  case 'w':
    return checkKeyword(1, 4, "hile", TOKEN_WHILE, scanner);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) {
    advance(scanner);
  }
  return makeToken(identifierType(scanner), scanner);
}

static Token number(Scanner *scanner) {
  while (isDigit(peek(scanner))) {
    advance(scanner);
  }
  if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
    advance(scanner);
    while (isDigit(peek(scanner))) {
      advance(scanner);
    }
  }
  return makeToken(TOKEN_NUMBER, scanner);
}

static Token string(Scanner *scanner) {
  while (peek(scanner) != '"' && !isAtEnd(scanner)) {
    if (match('\n', scanner)) {
      scanner->line++;
    }
  }
  if (isAtEnd(scanner)) {
    return errorToken("Unterminated string.", scanner);
  }
  advance(scanner); // closing qoute
  return makeToken(TOKEN_STRING, scanner);
}

Token scanToken(Scanner *scanner) {
  if (skipWhitespace(scanner).type == TOKEN_ERROR) {
    // todo
  }
  scanner->start = scanner->current;

  if (isAtEnd(scanner))
    return makeToken(TOKEN_EOF, scanner);

  char c = advance(scanner);
  if (isAlpha(c))
    return identifier(scanner);
  if (isDigit(c)) {
    return number(scanner);
  }

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
  case '?':
    return makeToken(TOKEN_QUESTION, scanner);
  case ':':
    return makeToken(TOKEN_COLON, scanner);
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
  case '"':
    return string(scanner);
  }

  return errorToken("Unexpected character.", scanner);
}
