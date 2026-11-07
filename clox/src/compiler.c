#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include "vm.h"

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
static void and_(Compiler *);
static uint8_t parseVariable(const char *, Compiler *);
static uint8_t identifierConstant(Compiler *);
static int resolveLocal(Compiler *, Scoper *, Token *);
static void addLocal(Token, Compiler *);
static void markInitialized(Compiler *);

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
    fprintf(stderr, " at '%.*s'", tok->length, tok->start);
  }
  fprintf(stderr, ": %s\n", msg);
  compiler->parser.hadError = true;
}

static void error(const char *msg, Compiler *compiler) {
  errorAt(&compiler->parser.previous, msg, compiler);
}

static void errorAtCurrent(const char *msg, Compiler *compiler) {
  errorAt(&compiler->parser.current, msg, compiler);
}
static void advance(Compiler *compiler) {
  compiler->parser.previous = compiler->parser.current;

  for (;;) {
    compiler->parser.current = scanToken(compiler->parser.scanner);
    if (compiler->parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(compiler->parser.current.start, compiler);
  }
}

static void consume(TokenType type, const char *msg, Compiler *compiler) {
  if (compiler->parser.current.type == type) {
    advance(compiler);
    return;
  }
  errorAtCurrent(msg, compiler);
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

static Chunk *currentChunk(Compiler *compiler) {
  return &compiler->scoper->function->chunk;
}

static void emitByte(uint8_t byte, Compiler *compiler) {
  writeChunk(currentChunk(compiler), byte, compiler->parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2, Compiler *compiler) {
  emitByte(byte1, compiler);
  emitByte(byte2, compiler);
}

static void emitLoop(int loopStart, Compiler *compiler) {
  emitByte(OP_LOOP, compiler);

  int offset = currentChunk(compiler)->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.", compiler);

  emitByte((offset >> 8) & 0xff, compiler);
  emitByte(offset & 0xff, compiler);
}

static int emitJump(uint8_t instruction, Compiler *compiler) {
  emitByte(instruction, compiler);
  emitByte(0xff, compiler);
  emitByte(0xff, compiler);
  return currentChunk(compiler)->count - 2;
}

static void patchJump(int offset, Compiler *compiler) {
  int jump = currentChunk(compiler)->count - offset -
             2; // -2 to adjust for the bytecode for the jump offset

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.", compiler);
  }

  currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
  currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}
static void emitReturn(Compiler *compiler) {
  emitByte(OP_NIL, compiler);
  emitByte(OP_RETURN, compiler);
}

static uint8_t makeConstant(Value value, Compiler *compiler) {
  int constant = addConstant(currentChunk(compiler), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk", compiler);
    return 0;
  }
  return (uint8_t)constant;
}

static void emitConstat(Value value, Compiler *compiler) {
  emitBytes(OP_CONSTANT, makeConstant(value, compiler), compiler);
}

static void initScoper(Scoper *scoper, FunctionType type, Compiler *compiler) {
  scoper->enclosing = compiler->scoper;
  scoper->type = type;
  scoper->localCount = 0;
  scoper->scopeDepth = 0;
  scoper->loop = NULL;
  scoper->function = newFunction(compiler->vm);
  compiler->scoper = scoper;
  if (type != TYPE_SCRIPT) {
    scoper->function->name =
        copyString(compiler->parser.previous.start,
                   compiler->parser.previous.length, compiler->vm);
  }

  Local *local = &scoper->locals[scoper->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->name.start = "";
  local->name.length = 0;
}

static ObjFunction *endCompiler(Compiler *compiler) {
  emitReturn(compiler);
  ObjFunction *function = compiler->scoper->function;
#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser.hadError) {
    disassembleChunk(currentChunk(compiler), function->name != NULL
                                                 ? function->name->chars
                                                 : "<script>");
  }
#endif

  compiler->scoper = compiler->scoper->enclosing;
  return function;
}

static void beginScope(Compiler *compiler) { compiler->scoper->scopeDepth++; }

static void endScope(Compiler *compiler) {
  compiler->scoper->scopeDepth--;
  while (compiler->scoper->localCount > 0 &&
         compiler->scoper->locals[compiler->scoper->localCount - 1].depth >
             compiler->scoper->scopeDepth) {
    if (compiler->scoper->locals[compiler->scoper->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE, compiler);
    } else {
      emitByte(OP_POP, compiler);
    }
    compiler->scoper->localCount--;
  }
}

static uint8_t argumentList(Compiler *compiler) {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN, &compiler->parser)) {
    do {
      expression(compiler);
      if (argCount == 255) {
        error("Cannot have more than 255 arguments", compiler);
      }
      argCount++;
    } while (match(TOKEN_COMMA, compiler));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments", compiler);
  return argCount;
}

static void beginLoop(Loop *loop, int start, Compiler *compiler) {
  loop->enclosing = compiler->scoper->loop;
  loop->start = start;
  loop->scopeDepth = compiler->scoper->scopeDepth;
  loop->breakCount = 0;
  compiler->scoper->loop = loop;
}

// Call after the loop's exit path, so every 'break' lands past it.
static void endLoop(Compiler *compiler) {
  Loop *loop = compiler->scoper->loop;
  for (int i = 0; i < loop->breakCount; i++)
    patchJump(loop->breakJumps[i], compiler);
  compiler->scoper->loop = loop->enclosing;
}

static void popLoopLocals(Compiler *compiler) {
  Scoper *scoper = compiler->scoper;
  for (int i = scoper->localCount - 1;
       i >= 0 && scoper->locals[i].depth > scoper->loop->scopeDepth; i--) {
    emitByte(OP_POP, compiler);
  }
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

static int addUpvalue(Scoper *scoper, uint8_t index, bool isLocal,
                      Compiler *compiler) {
  int upvalueCount = scoper->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &scoper->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal)
      return i;
  }
  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in functions.", compiler);
    return 0;
  }
  scoper->upvalues[upvalueCount].isLocal = isLocal;
  scoper->upvalues[upvalueCount].index = index;
  return scoper->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Scoper *scoper, Token *name) {
  if (scoper->enclosing == NULL)
    return -1;

  int local = resolveLocal(compiler, scoper->enclosing, name);
  if (local != -1) {
    compiler->scoper->enclosing->locals[local].isCaptured = true;
    return addUpvalue(scoper, (uint8_t)local, true, compiler);
  }
  int upvalue = resolveUpvalue(compiler, scoper->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(scoper, (uint8_t)upvalue, false, compiler);
  }
  return -1;
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
  // condition is already on the stack
  int elseJump = emitJump(OP_JUMP_IF_FALSE, compiler);
  emitByte(OP_POP, compiler); // pop condition (then path)
  expression(compiler);       // parse thenExpr

  int endJump = emitJump(OP_JUMP, compiler);

  consume(TOKEN_COLON, "Expect ':' after then branch of ternary.", compiler);

  patchJump(elseJump, compiler);
  emitByte(OP_POP, compiler);                  // pop condition (else path)
  parsePrecedence(PREC_CONDITIONAL, compiler); // parse elseExpr
  patchJump(endJump, compiler);
}

static void call(Compiler *compiler) {
  uint8_t argCount = argumentList(compiler);
  emitBytes(OP_CALL, argCount, compiler);
}

static void number(Compiler *compiler) {
  double value = strtod(compiler->parser.previous.start, NULL);
  emitConstat(NUMBER_VAL(value), compiler);
}

static void or_(Compiler *compiler) {

  int elseJump = emitJump(OP_JUMP_IF_FALSE, compiler);
  int endJump = emitJump(OP_JUMP, compiler);

  patchJump(elseJump, compiler);
  emitByte(OP_POP, compiler);

  parsePrecedence(PREC_OR, compiler);
  patchJump(endJump, compiler);
}

static void and_(Compiler *compiler) {
  int endJump = emitJump(OP_JUMP_IF_FALSE, compiler);

  emitByte(OP_POP, compiler);
  parsePrecedence(PREC_AND, compiler);

  patchJump(endJump, compiler);
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
  uint8_t getOp, setOp;
  Token *name = &compiler->parser.previous;
  int arg = resolveLocal(compiler, compiler->scoper, name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(compiler, compiler->scoper, name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(compiler);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  if (compiler->canAssign && match(TOKEN_EQUAL, compiler)) {
    expression(compiler);
    emitBytes(setOp, arg, compiler);
  } else {
    emitBytes(getOp, arg, compiler);
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

static void block(Compiler *compiler) {
  while (!check(TOKEN_RIGHT_BRACE, &compiler->parser) &&
         !check(TOKEN_EOF, &compiler->parser)) {
    declaration(compiler);
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.", compiler);
}

static void function(FunctionType type, Compiler *compiler) {
  Scoper scope;
  initScoper(&scope, type, compiler);
  beginScope(compiler);

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name", compiler);
  if (!check(TOKEN_RIGHT_PAREN, &compiler->parser)) {
    ObjFunction *fun_curr = compiler->scoper->function;
    do {
      fun_curr->arity++;
      if (fun_curr->arity > 255) {
        errorAtCurrent("Cannot have more than 255 parameters", compiler);
      }
      uint8_t constant = parseVariable("Expect parameter name", compiler);
      defineVariable(constant, compiler);
    } while (match(TOKEN_COMMA, compiler));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters", compiler);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body", compiler);

  block(compiler);

  ObjFunction *function = endCompiler(compiler);
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function), compiler), compiler);

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(scope.upvalues[i].isLocal ? 1 : 0, compiler);
    emitByte(scope.upvalues[i].index, compiler);
  }
}

static void funDeclaration(Compiler *compiler) {
  uint8_t global = parseVariable("Expect function name.", compiler);
  markInitialized(compiler);
  function(TYPE_FUNCTION, compiler);
  defineVariable(global, compiler);
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

static void forStatement(Compiler *compiler) {
  beginScope(compiler);
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'", compiler);
  if (match(TOKEN_SEMICOLON, compiler)) {
    // no initializer
  } else if (match(TOKEN_VAR, compiler)) {
    varDeclaration(compiler);
  } else {
    expressionStatement(compiler);
  }
  int loopStart = currentChunk(compiler)->count;
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON, compiler)) {
    expression(compiler);
    consume(TOKEN_SEMICOLON, "Expect ';'", compiler);

    // jump out of the loop
    exitJump = emitJump(OP_JUMP_IF_FALSE, compiler);
    emitByte(OP_POP, compiler);
  }

  if (!match(TOKEN_RIGHT_PAREN, compiler)) {
    int bodyJump = emitJump(OP_JUMP, compiler);
    int incrementStart = currentChunk(compiler)->count;

    expression(compiler);
    emitByte(OP_POP, compiler);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.", compiler);

    emitLoop(loopStart, compiler);
    loopStart = incrementStart;
    patchJump(bodyJump, compiler);
  }

  Loop loop;
  beginLoop(&loop, loopStart, compiler);
  statement(compiler);
  emitLoop(loopStart, compiler);
  if (exitJump != -1) {
    patchJump(exitJump, compiler);
    emitByte(OP_POP, compiler); // condition
  }
  endLoop(compiler);
  endScope(compiler);
}

static void printStatement(Compiler *compiler) {
  expression(compiler);
  consume(TOKEN_SEMICOLON, "Expect ';' after value.", compiler);
  emitByte(OP_PRINT, compiler);
}

static void returnStatment(Compiler *compiler) {
  if (compiler->scoper->type == TYPE_SCRIPT) {
    error("Cannot return from top-level code.", compiler);
  }
  if (match(TOKEN_SEMICOLON, compiler)) {
    emitReturn(compiler);
  } else {
    expression(compiler);
    consume(TOKEN_SEMICOLON, "Expect ';' after return value", compiler);
    emitByte(OP_RETURN, compiler);
  }
}

static void ifStatement(Compiler *compiler) {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if;.", compiler);
  expression(compiler);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition", compiler);

  int thenJump = emitJump(OP_JUMP_IF_FALSE, compiler);
  emitByte(OP_POP, compiler);
  statement(compiler);

  int elseJump = emitJump(OP_JUMP, compiler);
  patchJump(thenJump, compiler);
  emitByte(OP_POP, compiler);

  if (match(TOKEN_ELSE, compiler))
    statement(compiler);
  patchJump(elseJump, compiler);
}

static void caseBody(Compiler *compiler) {
  beginScope(compiler);
  while (!check(TOKEN_CASE, &compiler->parser) &&
         !check(TOKEN_DEFAULT, &compiler->parser) &&
         !check(TOKEN_RIGHT_BRACE, &compiler->parser) &&
         !check(TOKEN_EOF, &compiler->parser)) {
    declaration(compiler);
  }
  endScope(compiler);
}

static void switchStatement(Compiler *compiler) {
  beginScope(compiler);
  consume(TOKEN_LEFT_PAREN, "Expect '(' after Switch.", compiler);
  expression(compiler);
  addLocal((Token){.type = TOKEN_IDENTIFIER,
                   .start = "",
                   .length = 0,
                   .line = compiler->parser.current.line},
           compiler);
  markInitialized(compiler);
  uint8_t slot = compiler->scoper->localCount - 1;
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression", compiler);
  consume(TOKEN_LEFT_BRACE, "Expect '{' after Switch expression", compiler);
  int endJumps[256];
  int caseCount = 0;
  while (match(TOKEN_CASE, compiler)) {
    emitBytes(OP_GET_LOCAL, slot, compiler);
    expression(compiler);
    consume(TOKEN_COLON, "Expect ':' after case value.", compiler);
    emitByte(OP_EQUAL, compiler);

    int nextCase = emitJump(OP_JUMP_IF_FALSE, compiler);
    emitByte(OP_POP, compiler); // pop true

    caseBody(compiler);

    if (caseCount == 256)
      error("Too many cases in switch.", compiler);
    else
      endJumps[caseCount++] = emitJump(OP_JUMP, compiler);

    patchJump(nextCase, compiler); // false lands here...
    emitByte(OP_POP, compiler);    // pops false
  }

  if (match(TOKEN_DEFAULT, compiler)) {
    consume(TOKEN_COLON, "Expect ':' after 'default'.", compiler);
    caseBody(compiler);
    if (!check(TOKEN_RIGHT_BRACE, &compiler->parser))
      errorAtCurrent("'default' must be the last case in a switch.", compiler);
  }

  for (int i = 0; i < caseCount; i++)
    patchJump(endJumps[i], compiler);

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch cases.", compiler);
  endScope(compiler);
}

static void whileStatement(Compiler *compiler) {
  int loopStart = currentChunk(compiler)->count;

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while.", compiler);
  expression(compiler);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition", compiler);

  int exitJump = emitJump(OP_JUMP_IF_FALSE, compiler);
  emitByte(OP_POP, compiler);

  Loop loop;
  beginLoop(&loop, loopStart, compiler);
  statement(compiler);
  emitLoop(loopStart, compiler);

  patchJump(exitJump, compiler);
  emitByte(OP_POP, compiler);
  endLoop(compiler);
}

static void breakStatement(Compiler *compiler) {
  Loop *loop = compiler->scoper->loop;
  if (loop == NULL) {
    error("Can't use 'break' outside of a loop.", compiler);
  } else {
    popLoopLocals(compiler);
    if (loop->breakCount == UINT8_COUNT)
      error("Too many 'break' statements in one loop.", compiler);
    else
      loop->breakJumps[loop->breakCount++] = emitJump(OP_JUMP, compiler);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.", compiler);
}

static void continueStatement(Compiler *compiler) {
  Loop *loop = compiler->scoper->loop;
  if (loop == NULL) {
    error("Can't use 'continue' outside of a loop.", compiler);
  } else {
    popLoopLocals(compiler);
    emitLoop(loop->start, compiler);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.", compiler);
}

static void synchronize(Compiler *compiler) {
  compiler->parser.panicMode = false;

  // A stray 'case'/'default'
  if (check(TOKEN_CASE, &compiler->parser) ||
      check(TOKEN_DEFAULT, &compiler->parser)) {
    int depth = 0;
    while (!check(TOKEN_EOF, &compiler->parser)) {
      if (check(TOKEN_LEFT_BRACE, &compiler->parser)) {
        depth++;
      } else if (check(TOKEN_RIGHT_BRACE, &compiler->parser) && depth-- == 0) {
        advance(compiler);
        return;
      }
      advance(compiler);
    }
    return;
  }

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
    case TOKEN_SWITCH:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
      return;
    default:; // nothing
    }
    advance(compiler);
  }
}

static void declaration(Compiler *compiler) {
  if (match(TOKEN_FUN, compiler)) {
    funDeclaration(compiler);
  } else if (match(TOKEN_VAR, compiler)) {
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
  } else if (match(TOKEN_FOR, compiler)) {
    forStatement(compiler);
  } else if (match(TOKEN_IF, compiler)) {
    ifStatement(compiler);
  } else if (match(TOKEN_RETURN, compiler)) {
    returnStatment(compiler);
  } else if (match(TOKEN_WHILE, compiler)) {
    whileStatement(compiler);
  } else if (match(TOKEN_SWITCH, compiler)) {
    switchStatement(compiler);
  } else if (match(TOKEN_LEFT_BRACE, compiler)) {
    beginScope(compiler);
    block(compiler);
    endScope(compiler);
  } else if (match(TOKEN_BREAK, compiler)) {
    breakStatement(compiler);
  } else if (match(TOKEN_CONTINUE, compiler)) {
    continueStatement(compiler);
  } else {
    expressionStatement(compiler);
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
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
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
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

static bool identifierEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Scoper *scoper, Token *name) {
  for (int i = scoper->localCount - 1; i >= 0; i--) {
    Local *local = &scoper->locals[i];
    if (identifierEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Cannot read local variable in its own initializer", compiler);
      }
      return i;
    }
  }
  return -1;
}

static void addLocal(Token name, Compiler *compiler) {
  if (compiler->scoper->localCount == UINT8_COUNT) {
    error("Too many local variables in Function", compiler);
    return;
  }
  Local *local = &compiler->scoper->locals[compiler->scoper->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable(Compiler *compiler) {
  Token *name = &compiler->parser.previous;

  for (int i = compiler->scoper->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->scoper->locals[i];
    if (local->depth != -1 && local->depth < compiler->scoper->scopeDepth) {
      break;
    }
    if (identifierEqual(name, &local->name)) {
      error("Already a variable with this name in this scope", compiler);
    }
  }

  if (compiler->scoper->scopeDepth == 0)
    return;

  addLocal(*name, compiler);
}

static uint8_t parseVariable(const char *errorMsg, Compiler *compiler) {
  consume(TOKEN_IDENTIFIER, errorMsg, compiler);
  declareVariable(compiler);
  if (compiler->scoper->scopeDepth > 0)
    return 0;

  return identifierConstant(compiler);
}

static void markInitialized(Compiler *compiler) {
  if (compiler->scoper->scopeDepth == 0)
    return;
  compiler->scoper->locals[compiler->scoper->localCount - 1].depth =
      compiler->scoper->scopeDepth;
}

static void defineVariable(uint8_t global, Compiler *compiler) {
  if (compiler->scoper->scopeDepth > 0) {
    markInitialized(compiler);
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global, compiler);
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

ObjFunction *compile(const char *source, VM *vm) {
  Scanner scanner;
  Compiler compiler;
  Scoper scoper;

  compiler.vm = vm;
  compiler.scoper = NULL;
  initScanner(source, &scanner);
  initParser(&scanner, &compiler);
  initScoper(&scoper, TYPE_SCRIPT, &compiler);

  advance(&compiler);
  while (!match(TOKEN_EOF, &compiler)) {
    declaration(&compiler);
  }
  ObjFunction *function = endCompiler(&compiler);

  return compiler.parser.hadError ? NULL : function;
}
