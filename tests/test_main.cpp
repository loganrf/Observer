// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>

#include <wx/init.h>

#include "testing.h"

namespace testing {

std::vector<TestCase>& Registry() {
  static std::vector<TestCase> registry;
  return registry;
}

int& Failures() {
  static int failures = 0;
  return failures;
}

}  // namespace testing

int main(int argc, char** argv) {
  wxInitializer init;
  if (!init.IsOk()) {
    std::cerr << "Cannot initialise wxWidgets\n";
    return 2;
  }
  int run = 0;
  for (const auto& t : testing::Registry()) {
    if (argc > 1 && std::strstr(t.name, argv[1]) == nullptr) continue;
    const int before = testing::Failures();
    t.fn();
    ++run;
    std::cout << (testing::Failures() == before ? "ok   " : "FAIL ") << t.name
              << "\n";
  }
  std::cout << run << " tests, " << testing::Failures() << " failed checks\n";
  return testing::Failures() == 0 ? 0 : 1;
}
