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
  Parser parser;
  Chunk *chunk;
  VM *vm;
} Compiler;

bool compile(const char *source, Chunk *chunk, VM *vm);
void initCompiler(Parser *parser, Compiler *compiler);

#endif
