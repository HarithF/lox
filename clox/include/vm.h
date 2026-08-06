#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include <stdint.h>

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;

  Value stack[STACK_MAX];
  Value *stackTop;
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RINTIME_ERROR,
} InterpretResult;

void initVM(VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(const char *source, VM *vm);

void push(Value val, VM *vm);
Value pop(VM *vm);

#endif
