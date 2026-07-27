#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char *source, VM *vm) {
  Scanner scanner;
  initScanner(source, &scanner);
  int line = -1;

  for (;;) {
    Token tok = scanToken(&scanner);
    if (tok.line != line) {
      printf("%4d ", tok.line);
      line = tok.line;
    } else {
      printf("   |");
    }
    printf("%2d '%.*s'\n", tok.type, tok.length, tok.start);

    if (tok.type == TOKEN_EOF)
      break;
  }
}
