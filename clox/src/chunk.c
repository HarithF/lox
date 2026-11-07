#include "chunk.h"
#include "memory.h"
#include "value.h"
#include <stdint.h>
#include <stdlib.h>

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->line_count = 0;
  chunk->line_capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void resizeLine(Chunk *chunk, VM *vm) {
  int old_capacity = chunk->line_capacity;
  chunk->line_capacity = GROW_CAPACITY(old_capacity);
  chunk->lines =
      GROW_ARRAY(LineRun, chunk->lines, old_capacity, chunk->line_capacity, vm);
}

void recordLine(Chunk *chunk, int line, VM *vm) {
  if (chunk->line_count == 0) {
    resizeLine(chunk, vm);
    chunk->lines[0] = (LineRun){line, 1};
    chunk->line_count++;
    return;
  }

  LineRun *last = &chunk->lines[chunk->line_count - 1];

  if (last->line == line) {
    last->count++;
  } else {
    if (chunk->line_capacity < chunk->line_count + 1) {
      resizeLine(chunk, vm);
    }
    chunk->lines[chunk->line_count++] = (LineRun){line, 1};
  }
}

int getLine(Chunk *chunk, int instruction) {
  int offset = 0;
  int i;
  for (i = 0; i < chunk->line_count; i++) {
    offset += chunk->lines[i].count;
    if (instruction < offset)
      return chunk->lines[i].line;
  }
  return -1;
}

void writeChunk(Chunk *chunk, uint8_t byte, int line, VM *vm) {

  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity, vm);
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

  recordLine(chunk, line, vm);
}

void freeChunk(Chunk *chunk, VM *vm) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity, vm);
  FREE_ARRAY(LineRun, chunk->lines, chunk->line_capacity, vm);
  freeValueArray(&chunk->constants, vm);
  initChunk(chunk);
}

int addConstant(Chunk *chunk, Value val, VM *vm) {
  writeValueArray(&chunk->constants, val, vm);
  return chunk->constants.count - 1;
}
