#pragma once
#include "error_handler.h"
#include "token.h"
#include <istream>
#include <vector>

class Scanner {
public:
  Scanner(std::iostream &stream, ErrorHandler &error_handler)
      : stream_(stream), error_handler_(error_handler) {};

  std::vector<Token> scan_tokens();

private:
  std::istream &stream_;
  ErrorHandler &error_handler_;
  std::vector<Token> tokens_;
  int start = 0;
  int current = 0;
  int line = 1;
};
