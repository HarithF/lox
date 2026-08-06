#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"

#define ALLOCATE_OBJ(type, objectType, vm)                                     \
  (type *)allocateObject(sizeof(type), objectType, vm)

static Obj *allocateObject(size_t size, ObjType type, VM *vm) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->next = vm->objects;
  vm->objects = object;
  return object;
}

static ObjString *allocateString(char *chars, int length, VM *vm) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING, vm);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjString *takeString(char *chars, int length, VM *vm) {
  return allocateString(chars, length, vm);
}

ObjString *copyString(const char *chars, int length, VM *vm) {
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, vm);
}
