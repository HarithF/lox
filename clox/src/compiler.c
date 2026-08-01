#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Scanner *scanner;
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_CONDITIONAL, //? :
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser *);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

Chunk *compilingChung;

// forward decl:
static void expression(Parser *parser);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence, Parser *parser);

// helpers:

static Chunk *currentChunk() { return compilingChung; }

void initParser(Scanner *scanner, Parser *parser) {
  parser->scanner = scanner;
  parser->hadError = false;
  parser->panicMode = false;
  parser->current = (Token){0};
  parser->previous = (Token){0};
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

static void emitByte(uint8_t byte, Parser *parser) {
  writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2, Parser *parser) {
  emitByte(byte1, parser);
  emitByte(byte2, parser);
}

static void emitReturn(Parser *parser) { emitByte(OP_RETURN, parser); }

static uint8_t makeConstant(Value value, Parser *parser) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk", parser);
    return 0;
  }
  return (uint8_t)constant;
}

static void emitConstat(Value value, Parser *parser) {
  emitBytes(OP_CONSTANT, makeConstant(value, parser), parser);
}

static void endCompiler(Parser *parser) {
  emitReturn(parser);
#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static void binary(Parser *parser) {
  TokenType operatorType = parser->previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1), parser);

  switch (operatorType) {
  case TOKEN_PLUS:
    emitByte(OP_ADD, parser);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT, parser);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY, parser);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE, parser);
    break;
  default:
    return; // Unreachable.
  }
}

static void ternary(Parser *parser) {
  // left operand (condition) is already compiled

  expression(parser); // parse thenExpr

  consume(TOKEN_COLON, "Expect : after ternary operator ?", parser);

  parsePrecedence(PREC_CONDITIONAL, parser); // parse elseExpr
}

static void expression(Parser *parser) {
  parsePrecedence(PREC_ASSIGNMENT, parser);
}

static void grouping(Parser *parser) {
  expression(parser);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.", parser);
}

static void number(Parser *parser) {
  double value = strtod(parser->previous.start, NULL);
  emitConstat(value, parser);
}

static void unary(Parser *parser) {
  TokenType operatorType = parser->previous.type;

  parsePrecedence(PREC_UNARY, parser);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE, parser);
  default:
    return; // unreachable
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
    [TOKEN_QUESTION] = {NULL, ternary, PREC_CONDITIONAL},
};

static void parsePrecedence(Precedence precedence, Parser *parser) {
  advance(parser);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expected expression.", parser);
    return;
  }

  prefixRule(parser);

  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(parser);
  }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

bool compile(const char *source, Chunk *chunk) {
  Scanner scanner;
  Parser parser;
  compilingChung = chunk;
  initScanner(source, &scanner);
  initParser(&scanner, &parser);

  advance(&parser);
  expression(&parser);
  consume(TOKEN_EOF, "Expect end of expression.", &parser);
  endCompiler(&parser);

  return !parser.hadError;
}
