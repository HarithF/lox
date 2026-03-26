#pragma once
#include "error_handler.h"
#include "token.h"
#include <cctype>
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
  void tok_add(TokenType, LiteralValue);
  char advance();
  bool match(char);
  void lex_string();
  void lex_number();
  void lex_id();
  bool is_endfile() { return stream_.peek() == EOF; }
  void tok_add(TokenType tok) { tok_add(tok, std::monostate()); }
  bool is_alpha(char c) { return isalpha(c) || c == '_'; }
  bool is_alpha_num(char c) { return is_alpha(c) || std::isdigit(c); }
};
