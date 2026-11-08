#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "value.h"

typedef struct VM VM;

#define ALLOCATE(type, count, vm)                                              \
  (type *)reallocate(NULL, 0, sizeof(type) * (count), vm)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count, vm)                    \
  (type *)reallocate(pointer, sizeof(type) * (old_count),                      \
                     sizeof(type) * (new_count), vm)

#define FREE_ARRAY(type, pointer, old_count, vm)                               \
  reallocate(pointer, sizeof(type) * (old_count), 0, vm)

#define FREE(type, pointer, vm) reallocate(pointer, sizeof(type), 0, vm)

void *reallocate(void *pointer, size_t old_size, size_t new_size, VM *vm);
void markObject(Obj *object);
void markValue(Value value);
void collectGarbage(VM *vm);
void freeObjects(void *list, VM *vm);

#endif
