#include "test_support.hpp"

int main() {
  std::size_t failed = 0;
  for (const auto& test : brlab::test::registry()) {
    try {
      test.function();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    }
  }
  std::cout << brlab::test::registry().size() - failed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
