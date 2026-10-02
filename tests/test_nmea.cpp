// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "geo.h"
#include "nmea.h"
#include "testing.h"
#include "time_util.h"

using namespace observer;

namespace {

/** Append the correct "*hh" checksum to "$...". */
wxString WithChecksum(const wxString& body) {
  unsigned sum = 0;
  for (size_t i = 1; i < body.length(); ++i) sum ^= body[i].GetValue() & 0xFF;
  return body + wxString::Format("*%02X\r\n", sum);
}

}  // namespace

TEST(nmea_split_checks_checksum) {
  std::vector<wxString> f;
  CHECK(NmeaSplit(WithChecksum("$IIDPT,12.4,0.5,"), &f));
  CHECK_EQ(f.size(), 4u);
  CHECK_EQ(f[0], "IIDPT");
  CHECK_EQ(f[1], "12.4");
  CHECK(!NmeaSplit("$IIDPT,12.4,0.5,*00", &f));
  CHECK(NmeaSplit("$IIDPT,12.4,0.5,", &f));  // no checksum is allowed
  CHECK(!NmeaSplit("garbage", &f));
}

TEST(nmea_depth_priority_and_offset) {
  NmeaTracker t;
  t.Process(WithChecksum("$SDDBT,40.0,f,12.2,M,6.7,F"), 10, 0);
  Environment e = t.Snapshot(10, 30, NoValue(), NoValue());
  CHECK_NEAR(e.depth_m, 12.2, 1e-9);
  t.Process(WithChecksum("$SDDPT,12.2,0.6,100"), 11, 0);
  e = t.Snapshot(11, 30, NoValue(), NoValue());
  CHECK_NEAR(e.depth_m, 12.8, 1e-9);
}

TEST(nmea_depth_feet_only) {
  NmeaTracker t;
  t.Process(WithChecksum("$SDDBS,10.0,f,,M,,F"), 0, 0);
  CHECK_NEAR(t.Snapshot(0, 30, NoValue(), NoValue()).depth_m, 3.048, 1e-9);
}

TEST(nmea_values_go_stale) {
  NmeaTracker t;
  t.Process(WithChecksum("$YXMTW,14.5,C"), 100, 0);
  CHECK_NEAR(t.Snapshot(120, 30, NoValue(), NoValue()).water_temp_c, 14.5,
             1e-9);
  CHECK(!HasValue(t.Snapshot(131, 30, NoValue(), NoValue()).water_temp_c));
}

TEST(nmea_water_temp_fahrenheit) {
  NmeaTracker t;
  t.Process(WithChecksum("$YXMTW,59.0,F"), 0, 0);
  CHECK_NEAR(t.Snapshot(0, 30, NoValue(), NoValue()).water_temp_c, 15.0,
             1e-9);
}

TEST(nmea_wind_mwd_preferred) {
  NmeaTracker t;
  t.Process(WithChecksum("$WIMWV,45,R,12.0,N,A"), 0, 0);
  t.Process(WithChecksum("$WIMWD,270.0,T,255.0,M,14.0,N,7.2,M"), 0, 0);
  const Environment e = t.Snapshot(0, 30, 10, 5);
  CHECK_NEAR(e.wind_dir_deg, 270, 1e-9);
  CHECK_NEAR(e.wind_speed_kn, 14, 1e-9);
}

TEST(nmea_wind_true_relative_needs_heading) {
  NmeaTracker t;
  t.Process(WithChecksum("$WIMWV,300,T,10.0,M,A"), 0, 0);  // m/s
  Environment e = t.Snapshot(0, 30, 90, NoValue());
  CHECK_NEAR(e.wind_dir_deg, 30, 1e-9);
  CHECK_NEAR(e.wind_speed_kn, 10 * 3600.0 / 1852.0, 1e-9);
  e = t.Snapshot(0, 30, NoValue(), NoValue());
  CHECK(!HasValue(e.wind_dir_deg));
  CHECK(HasValue(e.wind_speed_kn));
}

TEST(nmea_wind_apparent_converted) {
  NmeaTracker t;
  t.Process(WithChecksum("$WIMWV,180,R,5.0,N,A"), 0, 0);
  const Environment e = t.Snapshot(0, 30, 0, 5);
  CHECK_NEAR(e.wind_speed_kn, 10, 1e-9);
  CHECK_NEAR(e.wind_dir_deg, 180, 1e-9);
}

TEST(nmea_wind_invalid_status_ignored) {
  NmeaTracker t;
  t.Process(WithChecksum("$WIMWV,45,R,12.0,N,V"), 0, 0);
  CHECK(!HasValue(t.Snapshot(0, 30, 0, 5).wind_speed_kn));
}

TEST(nmea_clock_offset_from_rmc_and_zda) {
  NmeaTracker t;
  const int64_t gnss = UtcToEpoch(2026, 10, 2, 14, 3, 27);
  int64_t off = 0;
  CHECK(!t.ClockOffset(0, 600, &off));
  t.Process(WithChecksum("$GPRMC,140327.00,A,4740.890,N,12225.164,W,5.2,"
                         "180.0,021026,15.0,E,A"),
            0, gnss - 90);
  CHECK(t.ClockOffset(1, 600, &off));
  CHECK_EQ(off, 90);
  t.Process(WithChecksum("$GPZDA,140327.00,02,10,2026,00,00"), 2, gnss);
  CHECK(t.ClockOffset(2, 600, &off));
  CHECK_EQ(off, 0);
  CHECK(!t.ClockOffset(1000, 600, &off));
}

TEST(nmea_rmc_void_fix_ignored) {
  NmeaTracker t;
  int64_t off;
  t.Process(WithChecksum("$GPRMC,140327.00,V,,,,,,,021026,,,N"), 0, 0);
  CHECK(!t.ClockOffset(0, 600, &off));
}
