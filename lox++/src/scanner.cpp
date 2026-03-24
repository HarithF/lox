#include "scanner.h"
#include "token.h"
#include <vector>

std::vector<Token> Scanner::scan_tokens() {
  while (stream_.peek() != EOF) {
    start = current;
    scan_tokens();
  }
  tokens_.push_back(Token(TokenType::EOF_, "", std::monostate{}, line));
  return tokens_;
}
