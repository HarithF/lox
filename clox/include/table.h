#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// only needs VM as a pointer;
typedef struct VM VM;

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table, VM *vm);

bool tableGet(Table *table, ObjString *key, Value *value);
bool tableSet(Table *table, ObjString *key, Value value, VM *vm);
bool tableDelete(Table *table, ObjString *key);
void tableAddAll(Table *from, Table *to, VM *vm);

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);

#endif
