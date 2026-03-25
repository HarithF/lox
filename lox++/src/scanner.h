#pragma once
#include "error_handler.h"
#include "token.h"
#include <istream>
#include <vector>

class Scanner {
public:
  Scanner(std::istream &stream, ErrorHandler &error_handler)
      : stream_(stream), error_handler_(error_handler) {}

  std::vector<Token> scan_tokens();

private:
  std::istream &stream_;
  ErrorHandler &error_handler_;
  std::vector<Token> tokens_;
  int line = 1;
  std::string buff{};

  void tok_scan();
  void tok_add(TokenType);
  void tok_add(TokenType, LiteralValue);
  char advance();
  bool match(char);
};
