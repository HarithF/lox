#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void repl(VM *vm) {
  char line[1024];

  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line, vm);
  }
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), file_size, file);
  if (bytesRead < file_size) {
    fprintf(stderr, "Coud not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path, VM *vm) {
  char *source = readFile(path);
  InterpretResult result = interpret(source, vm);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RINTIME_ERROR)
    exit(70);
}

int main(int argc, const char *argv[]) {
  VM vm;
  initVM(&vm);

  if (argc == 1) {
    repl(&vm);
  } else if (argc == 2) {
    runFile(argv[1], &vm);
  } else {
    fprintf(stderr, "Usage: ./clox <path>");
    exit(64);
  }

  freeVM(&vm);

  return 0;
}
