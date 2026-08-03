#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUMBER(val) ((val).as.number)

#define BOOL_VAL(val) ((Value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(val) ((Value){VAL_NUMBER, {.number = val}})

// typedef double Value;

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;
bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value val);
void freeValueArray(ValueArray *array);

void printValue(Value val);

#endif
