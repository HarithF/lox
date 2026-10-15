#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"
#include "vm.h"

typedef struct {
  Scanner *scanner;
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef struct {
  Token name;
  int depth;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

// A while/for loop being compiled; innermost first, linked via `enclosing`.
typedef struct Loop {
  struct Loop *enclosing;
  int start; // where 'continue' jumps back to
  int scopeDepth;
  int breakJumps[UINT8_COUNT];
  int breakCount;
} Loop;

// Per-function compilation state.
typedef struct Scoper {
  struct Scoper *enclosing;
  ObjFunction *function;
  FunctionType type;
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
  Loop *loop;
} Scoper;

// State shared across the whole compilation.
typedef struct {
  Parser parser;
  Scoper *scoper;
  VM *vm;
  bool canAssign;
} Compiler;

ObjFunction *compile(const char *source, VM *vm);

#endif
