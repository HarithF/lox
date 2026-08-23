#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void resetStack(VM *vm) { vm->stackTop = vm->stack; }

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  size_t instruction = vm->ip - vm->chunk->code - 1;
  int line = getLine(vm->chunk, instruction);
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack(vm);
}

void initVM(VM *vm) {
  resetStack(vm);
  vm->objects = NULL;
  initTable(&vm->globals);
  initTable(&vm->strings);
}

void freeVM(VM *vm) {
  freeTable(&vm->globals);
  freeTable(&vm->strings);
  freeObjects(vm->objects);
}

void push(Value val, VM *vm) {
  *vm->stackTop = val;
  vm->stackTop++;
}

Value pop(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

static Value peek(int distance, VM *vm) { return vm->stackTop[-1 - distance]; }

static bool isFalsey(Value val) {
  return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

static void concatenate(VM *vm) {
  ObjString *b = AS_STRING(pop(vm));
  ObjString *a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length, vm);
  push(OBJ_VAL(result), vm);
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0, vm)) || !IS_NUMBER(peek(1, vm))) {                  \
      runtimeError(vm, "Operands must be numbers.");                           \
      return INTERPRET_RINTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop(vm));                                             \
    double a = AS_NUMBER(pop(vm));                                             \
    push(valueType(a op b), vm);                                               \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant, vm);
      break;
    }
    case OP_NIL:
      push(NIL_VAL, vm);
      break;
    case OP_FALSE:
      push(BOOL_VAL(false), vm);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true), vm);
      break;

    case OP_POP:
      pop(vm);
      break;

    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(vm->stack[slot], vm);
      break;
    }

    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm->globals, name, &value)) {
        runtimeError(vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RINTIME_ERROR;
      }
      push(value, vm);
      break;
    }

    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm->globals, name, peek(0, vm));
      pop(vm);
      break;
    }

    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      vm->stack[slot] = peek(0, vm);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm->globals, name, peek(0, vm))) {
        tableDelete(&vm->globals, name);
        runtimeError(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RINTIME_ERROR;
      }
      break;
    }

    case OP_EQUAL: {
      Value b = pop(vm);
      Value a = pop(vm);
      push(BOOL_VAL(valuesEqual(a, b)), vm);
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_ADD:
      if (IS_STRING(peek(0, vm)) && IS_STRING(peek(1, vm))) {
        concatenate(vm);
      } else if (IS_NUMBER(peek(0, vm)) && IS_NUMBER(peek(1, vm))) {
        double b = AS_NUMBER(pop(vm));
        double a = AS_NUMBER(pop(vm));
        push(NUMBER_VAL(a + b), vm);
      } else {
        runtimeError(vm, "Operands must be two numbers or two strings.");
        return INTERPRET_RINTIME_ERROR;
      }
      break;
      break;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop(vm))), vm);
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0, vm))) {
        runtimeError(vm, "Operand must be a number.");
        return INTERPRET_RINTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop(vm))), vm);
      break;

    case OP_PRINT: {
      printValue(pop(vm));
      printf("\n");
      break;
    }
    case OP_RETURN: {

      return INTERPRET_OK;
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source, VM *vm) {

  Chunk chunk;
  initChunk(&chunk);
  if (!compile(source, &chunk, vm)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult result = run(vm);
  freeChunk(&chunk);

  return result;
}
