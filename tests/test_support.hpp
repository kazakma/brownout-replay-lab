#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace brlab::test {

struct Case {
  std::string_view name;
  void (*function)();
};

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(std::string_view name, void (*function)()) { registry().push_back({name, function}); }
};

inline void require(bool condition, std::string_view expression, std::string_view file, int line) {
  if (!condition) {
    std::ostringstream message;
    message << file << ':' << line << ": requirement failed: " << expression;
    throw std::runtime_error(message.str());
  }
}

template <typename Function>
void require_throws(Function&& function, std::string_view expression, std::string_view file, int line) {
  try {
    std::forward<Function>(function)();
  } catch (const std::exception&) {
    return;
  }
  std::ostringstream message;
  message << file << ':' << line << ": expected exception: " << expression;
  throw std::runtime_error(message.str());
}

} // namespace brlab::test

#define BRLAB_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define BRLAB_CONCAT(lhs, rhs) BRLAB_CONCAT_IMPL(lhs, rhs)
#define BRLAB_TEST(name)                                                                            \
  static void BRLAB_CONCAT(brlab_test_, __LINE__)();                                                \
  static ::brlab::test::Registrar BRLAB_CONCAT(brlab_registrar_, __LINE__){                         \
      name, &BRLAB_CONCAT(brlab_test_, __LINE__)};                                                  \
  static void BRLAB_CONCAT(brlab_test_, __LINE__)()
#define BRLAB_REQUIRE(expression)                                                                   \
  ::brlab::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define BRLAB_REQUIRE_THROWS(expression)                                                            \
  ::brlab::test::require_throws([&] { static_cast<void>(expression); }, #expression, __FILE__,      \
                                __LINE__)
