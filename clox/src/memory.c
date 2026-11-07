#include "memory.h"
#include "chunk.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include "stdio.h"
#endif

void collectGarbage(VM *vm) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
#endif

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
#endif
}
void *reallocate(void *pointer, size_t old_size, size_t new_size, VM *vm) {
  if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
    collectGarbage(vm);
#endif
  }
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, new_size);
  if (result == NULL)
    exit(1);

  return result;
}

void freeObject(Obj *object, VM *vm) {
  switch (object->type) {
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount, vm);
    FREE(ObjClosure, object, vm);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk, vm);
    FREE(ObjFunction, object, vm);
    break;
  }
  case OBJ_NATIVE: {
    FREE(OBJ_NATIVE, object, vm);
    break;
  }
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1, vm);
    FREE(ObjString, object, vm);
    break;
  }
  case OBJ_UPVALUE:
    FREE(ObjUpvalue, object, vm);
    break;
  }
}

void freeObjects(void *list, VM *vm) {
  Obj *object = (Obj *)list;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object, vm);
    object = next;
  }
}
