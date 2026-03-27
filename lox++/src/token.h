#pragma once
#include "token_type.h"
#include <iostream>
#include <string>
#include <variant>
// equivalent of Java's Object literal — can hold string, double, or nothing
// (null)
using LiteralValue = std::variant<std::string, double, std::monostate>;

class Token {
public:
  TokenType type;
  std::string lexeme;
  LiteralValue literal;
  int line;

  Token(TokenType type, std::string lexeme, LiteralValue literal, int line)
      : type(type), lexeme(lexeme), literal(literal), line(line) {}

  std::string to_string() const {
    std::string lit = std::visit(
        [](auto val) -> std::string {
          if constexpr (std::is_same_v<decltype(val), std::string>)
            return val;
          else if constexpr (std::is_same_v<decltype(val), double>)
            return std::to_string(val);
          else
            return "";
        },
        literal);
    return type_to_string(type) + " " + lexeme + " " + lit;
  }
};
