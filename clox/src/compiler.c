#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef void (*ParseFn)(Compiler *);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

// forward decl:
static void expression(Compiler *);
static void declaration(Compiler *);
static void statement(Compiler *);
static ParseRule *getRule(TokenType);
static void parsePrecedence(Precedence, Compiler *);
static void defineVariable(uint8_t, Compiler *);
static uint8_t parseVariable(const char *, Compiler *);
static uint8_t identifierConstant(Compiler *);

// helpers:

void initParser(Scanner *scanner, Compiler *compiler) {
  compiler->parser.scanner = scanner;
  compiler->parser.hadError = false;
  compiler->parser.panicMode = false;
  compiler->parser.current = (Token){0};
  compiler->parser.previous = (Token){0};
}

static void errorAt(Token *tok, const char *msg, Compiler *compiler) {
  if (compiler->parser.panicMode)
    return;
  compiler->parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", tok->line);
  if (tok->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (tok->type == TOKEN_ERROR) {
    // nothing
  } else {
    fprintf(stderr, " at '%*s'", tok->length, tok->start);
  }
  fprintf(stderr, ": %s\n", msg);
  compiler->parser.hadError = true;
}

static void error(const char *msg, Compiler *compiler) {
  errorAt(&compiler->parser.current, msg, compiler);
}

static void errorAtCurrent(Compiler *compiler) {
  const char *msg = compiler->parser.current.start;

  errorAt(&compiler->parser.current, msg, compiler);
}
static void advance(Compiler *compiler) {
  compiler->parser.previous = compiler->parser.current;

  for (;;) {
    compiler->parser.current = scanToken(compiler->parser.scanner);
    if (compiler->parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(compiler);
  }
}

static void consume(TokenType type, const char *msg, Compiler *compiler) {
  if (compiler->parser.current.type == type) {
    advance(compiler);
    return;
  }
  errorAtCurrent(compiler);
}

static bool check(TokenType type, Parser *parser) {
  return parser->current.type == type;
}

static bool match(TokenType type, Compiler *compiler) {
  if (!check(type, &compiler->parser))
    return false;
  advance(compiler);
  return true;
}

static void emitByte(uint8_t byte, Compiler *compiler) {
  writeChunk(compiler->chunk, byte, compiler->parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2, Compiler *compiler) {
  emitByte(byte1, compiler);
  emitByte(byte2, compiler);
}

static void emitReturn(Compiler *compiler) { emitByte(OP_RETURN, compiler); }

static uint8_t makeConstant(Value value, Compiler *compiler) {
  int constant = addConstant(compiler->chunk, value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk", compiler);
    return 0;
  }
  return (uint8_t)constant;
}

static void emitConstat(Value value, Compiler *compiler) {
  emitBytes(OP_CONSTANT, makeConstant(value, compiler), compiler);
}

static void endCompiler(Compiler *compiler) {
  emitReturn(compiler);
#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser.hadError) {
    disassembleChunk(compiler->chunk, "code");
  }
#endif
}

static void unary(Compiler *compiler) {
  TokenType operatorType = compiler->parser.previous.type;

  parsePrecedence(PREC_UNARY, compiler);

  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT, compiler);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE, compiler);
    break;
  default:
    return; // unreachable
  }
}

static void binary(Compiler *compiler) {
  TokenType operatorType = compiler->parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1), compiler);

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT, compiler);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL, compiler);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER, compiler);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT, compiler);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS, compiler);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT, compiler);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD, compiler);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT, compiler);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY, compiler);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE, compiler);
    break;
  default:
    return; // Unreachable.
  }
}

static void ternary(Compiler *compiler) {
  // left operand (condition) is already compiled

  expression(compiler); // parse thenExpr

  consume(TOKEN_COLON, "Expect : after ternary operator ?", compiler);

  parsePrecedence(PREC_CONDITIONAL, compiler); // parse elseExpr
}

static void number(Compiler *compiler) {
  double value = strtod(compiler->parser.previous.start, NULL);
  emitConstat(NUMBER_VAL(value), compiler);
}

static void literal(Compiler *compiler) {
  switch (compiler->parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE, compiler);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE, compiler);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL, compiler);
    break;
  default:
    return; // unreachable
  }
}

static void string(Compiler *compiler) {
  emitConstat(
      OBJ_VAL(copyString(compiler->parser.previous.start + 1,
                         compiler->parser.previous.length - 2, compiler->vm)),
      compiler);
}

static void namedVariable(Compiler *compiler) {
  uint8_t arg = identifierConstant(compiler);
  if (compiler->canAssign && match(TOKEN_EQUAL, compiler)) {
    expression(compiler);
    emitBytes(OP_SET_GLOBAL, arg, compiler);
  } else {
    emitBytes(OP_GET_GLOBAL, arg, compiler);
  }
}

static void variable(Compiler *compiler) { namedVariable(compiler); }

static void grouping(Compiler *compiler) {
  expression(compiler);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.", compiler);
}

static void expression(Compiler *compiler) {
  parsePrecedence(PREC_ASSIGNMENT, compiler);
}

static void varDeclaration(Compiler *compiler) {
  uint8_t global = parseVariable("Expect variable name.", compiler);

  if (match(TOKEN_EQUAL, compiler)) {
    expression(compiler);
  } else {
    emitByte(OP_NIL, compiler);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.", compiler);

  defineVariable(global, compiler);
}

static void expressionStatement(Compiler *compiler) {
  expression(compiler);
  consume(TOKEN_SEMICOLON, "Expect ';' after expression", compiler);
  emitByte(OP_POP, compiler);
}

static void printStatement(Compiler *compiler) {
  expression(compiler);
  consume(TOKEN_SEMICOLON, "Expect ';' after value.", compiler);
  emitByte(OP_PRINT, compiler);
}

static void synchronize(Compiler *compiler) {
  compiler->parser.panicMode = false;
  while (compiler->parser.current.type != TOKEN_EOF) {
    if (compiler->parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (compiler->parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:; // nothing
    }
    advance(compiler);
  }
}

static void declaration(Compiler *compiler) {
  if (match(TOKEN_VAR, compiler)) {
    varDeclaration(compiler);
  } else {
    statement(compiler);
  }
  if (compiler->parser.panicMode)
    synchronize(compiler);
  ;
}

static void statement(Compiler *compiler) {
  if (match(TOKEN_PRINT, compiler)) {
    printStatement(compiler);
  } else {
    expressionStatement(compiler);
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
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
    [TOKEN_QUESTION] = {NULL, ternary, PREC_CONDITIONAL},
};

static void parsePrecedence(Precedence precedence, Compiler *compiler) {
  advance(compiler);
  ParseFn prefixRule = getRule(compiler->parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expected expression.", compiler);
    return;
  }

  compiler->canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(compiler);

  while (precedence <= getRule(compiler->parser.current.type)->precedence) {
    advance(compiler);
    ParseFn infixRule = getRule(compiler->parser.previous.type)->infix;
    infixRule(compiler);
  }
  if (compiler->canAssign && match(TOKEN_EQUAL, compiler)) {
    error("Invalid assignment target", compiler);
  }
}

static uint8_t identifierConstant(Compiler *compiler) {
  Token name = compiler->parser.previous;
  return makeConstant(
      OBJ_VAL(copyString(name.start, name.length, compiler->vm)), compiler);
}

static uint8_t parseVariable(const char *errorMsg, Compiler *compiler) {
  consume(TOKEN_IDENTIFIER, errorMsg, compiler);
  return identifierConstant(compiler);
}

static void defineVariable(uint8_t global, Compiler *compiler) {
  emitBytes(OP_DEFINE_GLOBAL, global, compiler);
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

bool compile(const char *source, Chunk *chunk, VM *vm) {
  Scanner scanner;
  // Parser parser;
  Compiler compiler;

  initScanner(source, &scanner);
  initParser(&scanner, &compiler);
  // initCompiler(&compiler);

  compiler.chunk = chunk;
  compiler.vm = vm;

  advance(&compiler);
  while (!match(TOKEN_EOF, &compiler)) {
    declaration(&compiler);
  }
  endCompiler(&compiler);

  return !compiler.parser.hadError;
}
