// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "geo.h"

#include <cmath>
#include <limits>
#include <vector>

namespace observer {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusM = 6371008.8;  // IUGG mean radius

double ToRad(double deg) { return deg * kPi / 180.0; }
double ToDeg(double rad) { return rad * 180.0 / kPi; }

const wchar_t* kDeg = L"\u00B0";
const wchar_t* kMin = L"\u2032";
const wchar_t* kSec = L"\u2033";

wxString FormatCoordinate(double v, LatLonFormat fmt, char pos, char neg,
                          int deg_width) {
  if (!HasValue(v)) return "--";
  const char hemi = v < 0 ? neg : pos;
  double a = std::fabs(v);
  switch (fmt) {
    case LatLonFormat::kDecimalDegrees:
      return wxString::Format("%.5f", a) + kDeg + wxString::Format(" %c", hemi);
    case LatLonFormat::kDegreesMinutesSeconds: {
      // Round once at the finest unit so 59.95" never prints as 60.0".
      long tenths = std::lround(a * 36000.0);
      const long deg = tenths / 36000;
      tenths -= deg * 36000;
      const long min = tenths / 600;
      tenths -= min * 600;
      return wxString::Format(deg_width == 2 ? "%02ld" : "%03ld", deg) + kDeg +
             wxString::Format(" %02ld", min) + kMin +
             wxString::Format(" %04.1f", tenths / 10.0) + kSec +
             wxString::Format(" %c", hemi);
    }
    case LatLonFormat::kDegreesDecimalMinutes:
    default: {
      long thousandths = std::lround(a * 60000.0);
      const long deg = thousandths / 60000;
      thousandths -= deg * 60000;
      return wxString::Format(deg_width == 2 ? "%02ld" : "%03ld", deg) + kDeg +
             wxString::Format(" %06.3f", thousandths / 1000.0) + kMin +
             wxString::Format(" %c", hemi);
    }
  }
}

}  // namespace

bool HasValue(double v) { return std::isfinite(v); }

double NoValue() { return std::numeric_limits<double>::quiet_NaN(); }

bool IsValidPosition(double lat, double lon) {
  return HasValue(lat) && HasValue(lon) && lat >= -90.0 && lat <= 90.0 &&
         lon >= -180.0 && lon <= 360.0;
}

double NormalizeDegrees(double deg) {
  double d = std::fmod(deg, 360.0);
  if (d < 0) d += 360.0;
  if (d >= 360.0) d -= 360.0;
  return d;
}

double NormalizeLongitude(double lon) {
  double l = std::fmod(lon + 180.0, 360.0);
  if (l < 0) l += 360.0;
  return l - 180.0;
}

void Destination(double lat, double lon, double bearing_deg, double dist_m,
                 double* lat2, double* lon2) {
  const double phi1 = ToRad(lat);
  const double lambda1 = ToRad(lon);
  const double theta = ToRad(bearing_deg);
  const double delta = dist_m / kEarthRadiusM;

  const double sin_phi2 = std::sin(phi1) * std::cos(delta) +
                          std::cos(phi1) * std::sin(delta) * std::cos(theta);
  const double phi2 = std::asin(sin_phi2);
  const double y = std::sin(theta) * std::sin(delta) * std::cos(phi1);
  const double x = std::cos(delta) - std::sin(phi1) * sin_phi2;
  const double lambda2 = lambda1 + std::atan2(y, x);

  *lat2 = ToDeg(phi2);
  *lon2 = NormalizeLongitude(ToDeg(lambda2));
}

double DistanceMetres(double lat1, double lon1, double lat2, double lon2) {
  const double phi1 = ToRad(lat1);
  const double phi2 = ToRad(lat2);
  const double dphi = ToRad(lat2 - lat1);
  const double dlambda = ToRad(lon2 - lon1);
  const double a = std::sin(dphi / 2) * std::sin(dphi / 2) +
                   std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2) *
                       std::sin(dlambda / 2);
  return 2 * kEarthRadiusM * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

wxString FormatLatitude(double lat, LatLonFormat fmt) {
  return FormatCoordinate(lat, fmt, 'N', 'S', 2);
}

wxString FormatLongitude(double lon, LatLonFormat fmt) {
  return FormatCoordinate(HasValue(lon) ? NormalizeLongitude(lon) : lon, fmt,
                          'E', 'W', 3);
}

bool ParseCoordinate(const wxString& text, bool is_latitude, double* value) {
  wxString s = text;
  s.Trim(true).Trim(false);
  if (s.empty()) return false;

  // A lone comma is a decimal separator ("47,6815").
  if (s.Find('.') == wxNOT_FOUND) s.Replace(",", ".");

  int hemi_sign = 0;
  bool negative = false;
  std::vector<double> numbers;
  wxString token;

  auto flush = [&]() -> bool {
    if (token.empty()) return true;
    double v;
    if (!token.ToCDouble(&v)) return false;
    numbers.push_back(v);
    token.clear();
    return true;
  };

  for (wxUniChar c : s) {
    const wxUniChar u = (c >= 'a' && c <= 'z') ? wxUniChar(c.GetValue() - 32) : c;
    if ((c >= '0' && c <= '9') || c == '.') {
      token += c;
      continue;
    }
    if (!flush()) return false;
    if (c == '-' && numbers.empty() && token.empty()) {
      negative = true;
    } else if (c == '+' && numbers.empty()) {
      // explicit positive sign
    } else if (u == 'N' || u == 'S' || u == 'E' || u == 'W') {
      if (hemi_sign != 0) return false;
      const bool lat_letter = u == 'N' || u == 'S';
      if (lat_letter != is_latitude) return false;
      hemi_sign = (u == 'S' || u == 'W') ? -1 : 1;
    } else if (c == ' ' || c == '\t' || c == ':' || c == '\'' || c == '"' ||
               c == ',' || c.GetValue() == 0x00B0 || c.GetValue() == 0x00BA ||
               c.GetValue() == 0x2032 || c.GetValue() == 0x2033 ||
               c.GetValue() == 0x2019 || c.GetValue() == 0x201D) {
      // separators: space, colon, degree, minute and second marks
    } else {
      return false;
    }
  }
  if (!flush()) return false;
  if (numbers.empty() || numbers.size() > 3) return false;

  double deg = numbers[0];
  double result = deg;
  if (numbers.size() >= 2) {
    if (deg != std::floor(deg)) return false;
    const double min = numbers[1];
    if (min < 0 || min >= 60) return false;
    result += min / 60.0;
    if (numbers.size() == 3) {
      if (min != std::floor(min)) return false;
      const double sec = numbers[2];
      if (sec < 0 || sec >= 60) return false;
      result += sec / 3600.0;
    }
  }
  if (negative && hemi_sign != 0) return false;  // "-47 N" is ambiguous
  if (negative || hemi_sign < 0) result = -result;

  const double limit = is_latitude ? 90.0 : 180.0;
  if (std::fabs(result) > limit) return false;
  *value = result;
  return true;
}

void TrueWindFromApparent(double awa_deg, double aws, double boat_speed,
                          double heading_deg, double* twd_deg, double* tws) {
  const double awa = ToRad(awa_deg);
  const double u = aws * std::cos(awa) - boat_speed;
  const double v = aws * std::sin(awa);
  *tws = std::sqrt(u * u + v * v);
  const double twa = (*tws < 1e-9) ? 0.0 : ToDeg(std::atan2(v, u));
  *twd_deg = NormalizeDegrees(heading_deg + twa);
}

}  // namespace observer
