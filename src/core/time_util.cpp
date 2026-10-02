// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "time_util.h"

#include <chrono>
#include <cstdlib>
#include <ctime>

namespace observer {

namespace {

// Howard Hinnant's days_from_civil / civil_from_days: exact for the whole
// proleptic Gregorian calendar and free of time zone state.
int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDays(int64_t z, int* y, int* m, int* d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned dd = doy - (153 * mp + 2) / 5 + 1;
  const unsigned mm = mp < 10 ? mp + 3 : mp - 9;
  *y = static_cast<int>(yy + (mm <= 2));
  *m = static_cast<int>(mm);
  *d = static_cast<int>(dd);
}

int64_t FloorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

bool ReadDigits(const wxString& s, size_t* pos, size_t n, int* out) {
  if (*pos + n > s.length()) return false;
  int v = 0;
  for (size_t i = 0; i < n; ++i) {
    wxUniChar c = s[*pos + i];
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<int>(c.GetValue() - '0');
  }
  *pos += n;
  *out = v;
  return true;
}

bool Expect(const wxString& s, size_t* pos, char c) {
  if (*pos >= s.length() || s[*pos] != c) return false;
  ++*pos;
  return true;
}

}  // namespace

int64_t UtcToEpoch(int year, int month, int day, int hour, int minute,
                   int second) {
  return DaysFromCivil(year, static_cast<unsigned>(month),
                       static_cast<unsigned>(day)) *
             86400 +
         hour * 3600 + minute * 60 + second;
}

void EpochToUtc(int64_t t, int* year, int* month, int* day, int* hour,
                int* minute, int* second) {
  const int64_t days = FloorDiv(t, 86400);
  int64_t rem = t - days * 86400;
  CivilFromDays(days, year, month, day);
  *hour = static_cast<int>(rem / 3600);
  rem %= 3600;
  *minute = static_cast<int>(rem / 60);
  *second = static_cast<int>(rem % 60);
}

wxString FormatUtc(int64_t t) {
  int y, mo, d, h, mi, s;
  EpochToUtc(t, &y, &mo, &d, &h, &mi, &s);
  return wxString::Format("%04d-%02d-%02dT%02d:%02d:%02dZ", y, mo, d, h, mi,
                          s);
}

wxString FormatLocal(int64_t t, int offset_minutes) {
  int y, mo, d, h, mi, s;
  EpochToUtc(t + static_cast<int64_t>(offset_minutes) * 60, &y, &mo, &d, &h,
             &mi, &s);
  const char sign = offset_minutes < 0 ? '-' : '+';
  const int off = std::abs(offset_minutes);
  return wxString::Format("%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", y, mo, d,
                          h, mi, s, sign, off / 60, off % 60);
}

bool ParseIso8601(const wxString& text, int64_t* t, int* offset_minutes) {
  wxString s = text;
  s.Trim(true).Trim(false);
  size_t p = 0;
  int y, mo, d, h, mi, sec;
  if (!ReadDigits(s, &p, 4, &y) || !Expect(s, &p, '-') ||
      !ReadDigits(s, &p, 2, &mo) || !Expect(s, &p, '-') ||
      !ReadDigits(s, &p, 2, &d))
    return false;
  if (p >= s.length() || (s[p] != 'T' && s[p] != 't' && s[p] != ' '))
    return false;
  ++p;
  if (!ReadDigits(s, &p, 2, &h) || !Expect(s, &p, ':') ||
      !ReadDigits(s, &p, 2, &mi) || !Expect(s, &p, ':') ||
      !ReadDigits(s, &p, 2, &sec))
    return false;
  if (p < s.length() && (s[p] == '.' || s[p] == ',')) {
    ++p;
    while (p < s.length() && s[p] >= '0' && s[p] <= '9') ++p;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60)
    return false;

  int offset = 0;
  if (p < s.length() && (s[p] == 'Z' || s[p] == 'z')) {
    ++p;
  } else if (p < s.length() && (s[p] == '+' || s[p] == '-')) {
    const int sign = s[p] == '-' ? -1 : 1;
    ++p;
    int oh, om = 0;
    if (!ReadDigits(s, &p, 2, &oh)) return false;
    if (p < s.length() && s[p] == ':') ++p;
    if (p < s.length() && !ReadDigits(s, &p, 2, &om)) return false;
    offset = sign * (oh * 60 + om);
  } else {
    return false;
  }
  if (p != s.length()) return false;

  *t = UtcToEpoch(y, mo, d, h, mi, sec) - static_cast<int64_t>(offset) * 60;
  if (offset_minutes) *offset_minutes = offset;
  return true;
}

int LocalOffsetMinutes(int64_t t) {
  std::time_t tt = static_cast<std::time_t>(t);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &tt) != 0) return 0;
#else
  if (!localtime_r(&tt, &local)) return 0;
#endif
  const int64_t as_utc = UtcToEpoch(local.tm_year + 1900, local.tm_mon + 1,
                                    local.tm_mday, local.tm_hour, local.tm_min,
                                    local.tm_sec);
  return static_cast<int>((as_utc - t) / 60);
}

int64_t NowEpoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch())
      .count();
}

double MonotonicSeconds() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace observer
