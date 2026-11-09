#include "memory.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include "stdio.h"
#endif

void freeObject(Obj *object, VM *vm) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void *)object, object->type);
#endif

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
    FREE(ObjNative, object, vm);
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
  free(vm->greyStack);
}

static void markRoots(VM *vm) {
  for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
    markValue(*slot, vm);
  }
  for (int i = 0; i < vm->frameCount; i++) {
    markObject((Obj *)vm->frames[i].closure, vm);
  }
  for (ObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj *)upvalue, vm);
  }
  markTable(&vm->globals);
  markCompilerRoots(vm);
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

void markObject(Obj *object, VM *vm) {
  if (object == NULL)
    return;
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->isMarked = true;
  if (vm->greyCapacity < vm->greyCount + 1) {
    vm->greyCapacity = GROW_CAPACITY(vm->greyCapacity);
    vm->greyStack =
        (Obj **)realloc(vm->greyStack, sizeof(Obj *) * vm->greyCapacity);
    if (vm->greyStack == NULL)
      exit(1);
  }
  vm->greyStack[vm->greyCount++] = object;
}

void markValue(Value value, VM *vm) {
  if (IS_OBJ(value))
    markObject(AS_OBJ(value), vm);
}

void collectGarbage(VM *vm) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
#endif

  markRoots(vm);

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
#endif
}
