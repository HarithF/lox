#include "value.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include <stdio.h>
#include <string.h>

void initValueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray *array, Value val) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values =
        GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }

  array->values[array->count] = val;
  array->count++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void printObject(Value val) {
  switch (OBJ_TYPE(val)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(val));
    break;
  }
}

void printValue(Value val) {
  switch (val.type) {
  case VAL_BOOL:
    printf(AS_BOOL(val) ? "true" : "false");
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_NUMBER:
    printf("%g", AS_NUMBER(val));
    break;
  case VAL_OBJ:
    printObject(val);
    break;
  }
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_OBJ: {
    return AS_OBJ(a) == AS_OBJ(b);
  }
  default:
    return false; // Unreachable.
  }
}
