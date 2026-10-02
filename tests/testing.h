// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

// A minimal test harness: TEST() registers a function, CHECK*() record
// failures without stopping the test.

#ifndef OBSERVER_TESTING_H
#define OBSERVER_TESTING_H

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <wx/string.h>

namespace testing {

struct TestCase {
  const char* name;
  std::function<void()> fn;
};

std::vector<TestCase>& Registry();
int& Failures();

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) {
    Registry().push_back({name, std::move(fn)});
  }
};

inline std::string Str(const wxString& s) { return s.ToStdString(wxConvUTF8); }
inline std::string Str(const char* s) { return s; }
inline std::string Str(const std::string& s) { return s; }
template <typename T>
std::string Str(const T& v) {
  return std::to_string(v);
}

inline void Fail(const char* file, int line, const std::string& msg) {
  ++Failures();
  std::cerr << file << ":" << line << ": " << msg << "\n";
}

}  // namespace testing

#define TEST(name)                                                  \
  static void test_##name();                                        \
  static testing::Registrar registrar_##name(#name, test_##name);   \
  static void test_##name()

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) testing::Fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
  } while (0)

#define CHECK_EQ(a, b)                                                     \
  do {                                                                     \
    const auto& va_ = (a);                                                 \
    const auto& vb_ = (b);                                                 \
    if (!(va_ == vb_))                                                     \
      testing::Fail(__FILE__, __LINE__,                                    \
                    std::string("CHECK_EQ(" #a ", " #b "): ") +           \
                        testing::Str(va_) + " != " + testing::Str(vb_));   \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
  do {                                                                     \
    const double va_ = (a);                                                \
    const double vb_ = (b);                                                \
    if (!(std::fabs(va_ - vb_) <= (tol)))                                  \
      testing::Fail(__FILE__, __LINE__,                                    \
                    std::string("CHECK_NEAR(" #a ", " #b "): ") +          \
                        std::to_string(va_) + " vs " + std::to_string(vb_)); \
  } while (0)

#endif  // OBSERVER_TESTING_H
