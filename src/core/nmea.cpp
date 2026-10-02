// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nmea.h"

#include <cmath>

#include "geo.h"
#include "time_util.h"

namespace observer {

namespace {

constexpr double kFeetToMetres = 0.3048;
constexpr double kFathomsToMetres = 1.8288;

int HexValue(wxUniChar c) {
  if (c >= '0' && c <= '9') return static_cast<int>(c.GetValue() - '0');
  if (c >= 'A' && c <= 'F') return static_cast<int>(c.GetValue() - 'A' + 10);
  if (c >= 'a' && c <= 'f') return static_cast<int>(c.GetValue() - 'a' + 10);
  return -1;
}

bool ToNumber(const wxString& s, double* v) {
  if (s.empty()) return false;
  return s.ToCDouble(v) && std::isfinite(*v);
}

const wxString& Field(const std::vector<wxString>& f, size_t i) {
  static const wxString empty;
  return i < f.size() ? f[i] : empty;
}

/** Depth from a DBS/DBT layout: feet,f,metres,M,fathoms,F. */
bool DepthFromUnits(const std::vector<wxString>& f, double* metres) {
  double v;
  if (ToNumber(Field(f, 3), &v)) {
    *metres = v;
    return true;
  }
  if (ToNumber(Field(f, 1), &v)) {
    *metres = v * kFeetToMetres;
    return true;
  }
  if (ToNumber(Field(f, 5), &v)) {
    *metres = v * kFathomsToMetres;
    return true;
  }
  return false;
}

double SpeedToKnots(double v, const wxString& unit) {
  if (unit == "N") return v;
  if (unit == "K") return v / 1.852;
  if (unit == "M") return v * 3600.0 / 1852.0;
  if (unit == "S") return v * 1609.344 / 1852.0;
  return NoValue();
}

/** hhmmss(.ss) -> seconds of day, or -1. */
int SecondsOfDay(const wxString& s) {
  if (s.length() < 6) return -1;
  long h, m, sec;
  if (!s.Mid(0, 2).ToLong(&h) || !s.Mid(2, 2).ToLong(&m) ||
      !s.Mid(4, 2).ToLong(&sec))
    return -1;
  if (h > 23 || m > 59 || sec > 60) return -1;
  return static_cast<int>(h * 3600 + m * 60 + sec);
}

}  // namespace

bool NmeaSplit(const wxString& sentence, std::vector<wxString>* fields) {
  wxString s = sentence;
  s.Trim(true).Trim(false);
  if (s.length() < 7 || (s[0] != '$' && s[0] != '!')) return false;

  wxString body;
  const int star = s.Find('*', true);
  if (star != wxNOT_FOUND) {
    body = s.Mid(1, static_cast<size_t>(star) - 1);
    const wxString sum = s.Mid(static_cast<size_t>(star) + 1);
    if (sum.length() < 2) return false;
    const int hi = HexValue(sum[0]);
    const int lo = HexValue(sum[1]);
    if (hi < 0 || lo < 0) return false;
    unsigned expected = static_cast<unsigned>(hi * 16 + lo);
    unsigned actual = 0;
    for (wxUniChar c : body) actual ^= (c.GetValue() & 0xFF);
    if (actual != expected) return false;
  } else {
    body = s.Mid(1);
  }

  fields->clear();
  wxString field;
  for (wxUniChar c : body) {
    if (c == ',') {
      fields->push_back(field);
      field.clear();
    } else {
      field += c;
    }
  }
  fields->push_back(field);
  return !fields->empty() && (*fields)[0].length() >= 3;
}

NmeaTracker::NmeaTracker() {
  for (Value* v : {&depth_dpt_, &depth_dbs_, &depth_dbt_, &water_temp_,
                   &mwd_dir_, &mwd_speed_, &true_rel_angle_, &true_rel_speed_,
                   &app_angle_, &app_speed_, &clock_offset_}) {
    v->v = NoValue();
    v->at = -1;
  }
}

bool NmeaTracker::Fresh(const Value& value, double now, double max_age) {
  return value.at >= 0 && HasValue(value.v) && now - value.at <= max_age &&
         now >= value.at - 1.0;
}

void NmeaTracker::Process(const wxString& sentence, double now,
                          int64_t system_epoch) {
  std::vector<wxString> f;
  if (!NmeaSplit(sentence, &f)) return;
  const wxString& address = f[0];
  if (address.StartsWith("P")) return;  // proprietary
  const wxString type = address.Right(3).Upper();

  if (type == "DPT" || type == "DBS" || type == "DBT") {
    HandleDepth(f, type, now);
  } else if (type == "MTW") {
    double t;
    if (ToNumber(Field(f, 1), &t)) {
      if (Field(f, 2) == "F") t = (t - 32.0) * 5.0 / 9.0;
      water_temp_ = {t, now};
    }
  } else if (type == "MWD" || type == "MWV") {
    HandleWind(f, type, now);
  } else if (type == "RMC" || type == "ZDA") {
    HandleTime(f, type, now, system_epoch);
  }
}

void NmeaTracker::HandleDepth(const std::vector<wxString>& f,
                              const wxString& type, double now) {
  double d;
  if (type == "DPT") {
    if (!ToNumber(Field(f, 1), &d)) return;
    double offset;
    if (ToNumber(Field(f, 2), &offset)) d += offset;
    depth_dpt_ = {d, now};
  } else if (DepthFromUnits(f, &d)) {
    (type == "DBS" ? depth_dbs_ : depth_dbt_) = {d, now};
  }
}

void NmeaTracker::HandleWind(const std::vector<wxString>& f,
                             const wxString& type, double now) {
  double angle, speed;
  if (type == "MWD") {
    if (ToNumber(Field(f, 1), &angle)) mwd_dir_ = {NormalizeDegrees(angle), now};
    if (ToNumber(Field(f, 5), &speed)) {
      mwd_speed_ = {speed, now};
    } else if (ToNumber(Field(f, 7), &speed)) {
      mwd_speed_ = {speed * 3600.0 / 1852.0, now};
    }
    return;
  }
  // MWV: angle, reference (R/T), speed, unit, status
  if (Field(f, 5) != "A") return;
  if (!ToNumber(Field(f, 1), &angle) || !ToNumber(Field(f, 3), &speed)) return;
  const double kn = SpeedToKnots(speed, Field(f, 4));
  if (!HasValue(kn)) return;
  if (Field(f, 2) == "T") {
    true_rel_angle_ = {angle, now};
    true_rel_speed_ = {kn, now};
  } else if (Field(f, 2) == "R") {
    app_angle_ = {angle, now};
    app_speed_ = {kn, now};
  }
}

void NmeaTracker::HandleTime(const std::vector<wxString>& f,
                             const wxString& type, double now,
                             int64_t system_epoch) {
  const int sod = SecondsOfDay(Field(f, 1));
  if (sod < 0) return;
  long day, month, year;
  if (type == "RMC") {
    if (Field(f, 2) != "A") return;
    const wxString& date = Field(f, 9);
    if (date.length() != 6 || !date.Mid(0, 2).ToLong(&day) ||
        !date.Mid(2, 2).ToLong(&month) || !date.Mid(4, 2).ToLong(&year))
      return;
    year += year < 80 ? 2000 : 1900;
  } else {
    if (!Field(f, 2).ToLong(&day) || !Field(f, 3).ToLong(&month) ||
        !Field(f, 4).ToLong(&year))
      return;
  }
  if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1980) return;
  const int64_t gnss = UtcToEpoch(static_cast<int>(year),
                                  static_cast<int>(month),
                                  static_cast<int>(day), 0, 0, 0) +
                       sod;
  clock_offset_ = {static_cast<double>(gnss - system_epoch), now};
}

Environment NmeaTracker::Snapshot(double now, double max_age,
                                  double heading_deg, double sog_kn) const {
  Environment e{NoValue(), NoValue(), NoValue(), NoValue()};

  if (Fresh(depth_dpt_, now, max_age)) {
    e.depth_m = depth_dpt_.v;
  } else if (Fresh(depth_dbs_, now, max_age)) {
    e.depth_m = depth_dbs_.v;
  } else if (Fresh(depth_dbt_, now, max_age)) {
    e.depth_m = depth_dbt_.v;
  }

  if (Fresh(water_temp_, now, max_age)) e.water_temp_c = water_temp_.v;

  if (Fresh(mwd_dir_, now, max_age) && Fresh(mwd_speed_, now, max_age)) {
    e.wind_dir_deg = mwd_dir_.v;
    e.wind_speed_kn = mwd_speed_.v;
  } else if (Fresh(true_rel_angle_, now, max_age) &&
             Fresh(true_rel_speed_, now, max_age)) {
    e.wind_speed_kn = true_rel_speed_.v;
    if (HasValue(heading_deg))
      e.wind_dir_deg = NormalizeDegrees(heading_deg + true_rel_angle_.v);
  } else if (Fresh(app_angle_, now, max_age) &&
             Fresh(app_speed_, now, max_age) && HasValue(sog_kn)) {
    double twd, tws;
    TrueWindFromApparent(app_angle_.v, app_speed_.v, sog_kn,
                         HasValue(heading_deg) ? heading_deg : 0.0, &twd,
                         &tws);
    e.wind_speed_kn = tws;
    if (HasValue(heading_deg)) e.wind_dir_deg = twd;
  }
  return e;
}

bool NmeaTracker::ClockOffset(double now, double max_age,
                              int64_t* offset) const {
  if (!Fresh(clock_offset_, now, max_age)) return false;
  *offset = static_cast<int64_t>(std::llround(clock_offset_.v));
  return true;
}

}  // namespace observer
