#include "ast_printer.h"
#include "error_handler.h"
#include "interpreter.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <print>
#include <string>
#include <vector>

void run_file(const std::string &, ErrorHandler &);
void run_prompt(ErrorHandler &);
void run(std::istream &, ErrorHandler &, Interpreter &);

int main(int argc, char **argv) {
  ErrorHandler error_handler;
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0] << " <input_script>" << std::endl;
    return EXIT_FAILURE;
  } else if (argc == 2) {
    std::string filename(argv[1]);
    if (filename == "pretty-test") {
      AstPrinter::test_printer();
      return 0;
    }
    run_file(filename, error_handler);
  } else {
    run_prompt(error_handler);
  }
}

void run_file(const std::string &path, ErrorHandler &error_handler) {
  Interpreter interpreter(error_handler);
  std::ifstream stream(path);
  if (!stream.is_open()) {
    std::cerr << "Error: could not open file '" << path << "'" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  run(stream, error_handler, interpreter);
  if (error_handler.had_runtime_error())
    std::exit(70);
  if (error_handler.had_error())
    std::exit(65);
}

void run_prompt(ErrorHandler &error_handler) {
  std::string line;
  Interpreter interpreter(error_handler);

  while (true) {
    std::print("> ");
    if (!std::getline(std::cin, line))
      break;
    std::istringstream stream(line);
    run(stream, error_handler, interpreter);
    error_handler.reset();
  }
}

void run(std::istream &stream, ErrorHandler &error_handler,
         Interpreter &interpreter) {
  Scanner scanner(stream, error_handler);
  std::vector<Token> tokens = scanner.scan_tokens();
  Parser parser = Parser(tokens, error_handler);
  ExprPtr expr = parser.parse();

  if (!expr)
    return;
  interpreter.interpret(*expr);

  std::println("{}", AstPrinter().print(*expr));
}
