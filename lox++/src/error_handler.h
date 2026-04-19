#pragma once
#include <iostream>
#include <print>
#include <string>

class ErrorHandler {
public:
  void error(int line, const std::string &message) {
    report(line, "", message);
  }

  void report(int line, const std::string &where, const std::string &message) {
    std::cerr << "[line " << line << "] Error" << where << ": " << message
              << std::endl;
    had_error_ = true;
  }

  void runtime_error(int line, const std::string &message) {
    std::println(stderr, "{}\n[line {}]", message, line);
    had_runtime_error_ = true;
  }

  bool had_error() const { return had_error_; }
  bool had_runtime_error() const { return had_runtime_error_; }
  void reset() { had_error_ = false; }

private:
  bool had_error_ = false;
  bool had_runtime_error_ = false;
};
