#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include <stdint.h>

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RINTIME_ERROR,
} InterpretResult;

void initVM(VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(Chunk *chunk, VM *vm);

#endif
