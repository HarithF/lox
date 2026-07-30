#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
  Scanner *scanner;
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

void initParser(Scanner *scanner, Parser *parser) {
  parser->scanner = scanner;
  parser->hadError = false;
  parser->panicMode = false;
}

static void errorAt(Token *tok, const char *msg, Parser *parser) {
  if (parser->panicMode)
    return;
  parser->panicMode = true;
  fprintf(stderr, "[line %d] Error", tok->line);
  if (tok->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (tok->type == TOKEN_ERROR) {
    // nothing
  } else {
    fprintf(stderr, " at '%*s'", tok->length, tok->start);
  }
  fprintf(stderr, ": %s\n", msg);
  parser->hadError = true;
}

static void error(const char *msg, Parser *parser) {
  errorAt(&parser->current, msg, parser);
}

static void errorAtCurrent(Parser *parser) {
  const char *msg = parser->current.start;

  errorAt(&parser->current, msg, parser);
}
static void advance(Parser *parser) {
  parser->previous = parser->current;

  for (;;) {
    parser->current = scanToken(parser->scanner);
    if (parser->current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser);
  }
}

static void consume(TokenType type, const char *msg, Parser *parser) {
  if (parser->current.type == type) {
    advance(parser);
    return;
  }
  errorAtCurrent(parser);
}

bool compile(const char *source, Chunk *chunk) {
  Scanner scanner;
  Parser parser;
  initScanner(source, &scanner);
  initParser(&scanner, &parser);

  advance(&parser);
  expression(&scanner);
  consume(TOKEN_EOF, "Expect end of expression.", &parser);

  return !parser.hadError;
}
