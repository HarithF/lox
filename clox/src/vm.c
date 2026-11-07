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
#include <time.h>

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack(VM *vm) {
  vm->stackTop = vm->stack;
  vm->frameCount = 0;
  vm->openUpvalues = NULL;
}

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  for (int i = vm->frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm->frames[i];
    ObjFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction].line);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack(vm);
}

static void defineNative(const char *name, NatveFn function, int arity,
                         VM *vm) {
  push(OBJ_VAL(copyString(name, (int)strlen(name), vm)), vm);
  push(OBJ_VAL(newNative(function, arity, vm)), vm);
  tableSet(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1], vm);
  pop(vm);
  pop(vm);
}

void initVM(VM *vm) {
  resetStack(vm);
  vm->objects = NULL;
  vm->compiler = NULL;
  initTable(&vm->globals);
  initTable(&vm->strings);
  defineNative("clock", clockNative, 0, vm);
}

void freeVM(VM *vm) {
  freeTable(&vm->globals, vm);
  freeTable(&vm->strings, vm);
  freeObjects(vm->objects, vm);
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

static bool call(ObjClosure *closure, int argCount, VM *vm) {
  if (argCount != closure->function->arity) {
    runtimeError(vm, "Expected %d arguments but got %d",
                 closure->function->arity, argCount);
    return false;
  }
  if (vm->frameCount == FRAMES_MAX) {
    runtimeError(vm, "Stack overflow");
    return false;
  }
  CallFrame *frame = &vm->frames[vm->frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm->stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount, VM *vm) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLOSURE:
      return call(AS_CLOSURE(callee), argCount, vm);
    case OBJ_NATIVE: {
      ObjNative *native = AS_NATIVE_OBJ(callee);
      if (argCount != native->arity) {
        runtimeError(vm, "Expected %d arguments but got %d", native->arity,
                     argCount);
        return false;
      }

      Value result = native->function(argCount, vm->stackTop - argCount);
      vm->stackTop -= argCount + 1;
      push(result, vm);
      return true;
    }
    default:
      break; // non-callable object
    }
  }
  runtimeError(vm, "Can only call functions and classes");
  return false;
}

static ObjUpvalue *captureUpvalue(Value *local, VM *vm) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm->openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue *createdUpvalue = newUpvalue(local, vm);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }
  return createdUpvalue;
}

static void closeUpvalue(Value *last, VM *vm) {
  while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->openUpvalues = upvalue->next;
  }
}

static bool isFalsey(Value val) {
  return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

static void concatenate(VM *vm) {
  ObjString *b = AS_STRING(pop(vm));
  ObjString *a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1, vm);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length, vm);
  push(OBJ_VAL(result), vm);
}

static InterpretResult run(VM *vm) {

  CallFrame *frame = &vm->frames[vm->frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
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
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
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
      push(frame->slots[slot], vm);
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
      tableSet(&vm->globals, name, peek(0, vm), vm);
      pop(vm);
      break;
    }

    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0, vm);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm->globals, name, peek(0, vm), vm)) {
        tableDelete(&vm->globals, name);
        runtimeError(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RINTIME_ERROR;
      }
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*frame->closure->upvalues[slot]->location, vm);
      break;
    }

    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek(0, vm);
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
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0, vm)))
        frame->ip += offset;

      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount, vm), argCount, vm)) {
        return INTERPRET_RINTIME_ERROR;
      }
      frame = &vm->frames[vm->frameCount - 1];
      break;
    }

    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function, vm);
      push(OBJ_VAL(closure), vm);
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->slots + index, vm);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      break;
    }
    case OP_CLOSE_UPVALUE:
      closeUpvalue(vm->stackTop - 1, vm);
      pop(vm);
      break;

    case OP_RETURN: {
      Value result = pop(vm);
      closeUpvalue(frame->slots, vm);
      vm->frameCount--;
      if (vm->frameCount == 0) {
        return INTERPRET_OK;
      }

      vm->stackTop = frame->slots;
      push(result, vm);
      frame = &vm->frames[vm->frameCount - 1];
      break;
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
}

InterpretResult interpret(const char *source, VM *vm) {
  ObjFunction *function = compile(source, vm);
  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function), vm);
  ObjClosure *closure = newClosure(function, vm);
  pop(vm);
  push(OBJ_VAL(closure), vm);
  call(closure, 0, vm);
  return run(vm);
}
