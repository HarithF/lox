#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// only needs VM as a pointer;
typedef struct VM VM;

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_LOCAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
} OpCode;

typedef struct {
  int line;
  int count;
} LineRun;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  LineRun *lines;
  int line_count;
  int line_capacity;
  ValueArray constants;
} Chunk;

void freeChunk(Chunk *chunk, VM *vm);
void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line, VM *vm);

int getLine(Chunk *chunk, int instruction);

int addConstant(Chunk *chunk, Value val, VM *vm);

#endif
