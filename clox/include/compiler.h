#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
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

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Scoper;

typedef struct {
  Parser parser;
  Scoper scoper;
  Chunk *chunk;
  VM *vm;
  bool canAssign;
} Compiler;

bool compile(const char *source, Chunk *chunk, VM *vm);
void initCompiler(Compiler *compiler);

#endif
