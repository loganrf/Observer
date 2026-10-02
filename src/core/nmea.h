// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_NMEA_H
#define OBSERVER_NMEA_H

#include <cstdint>
#include <vector>

#include <wx/string.h>

namespace observer {

/**
 * Split an NMEA 0183 sentence into fields. The first field is the address
 * ("IIDPT"). Returns false if the sentence is malformed or its checksum,
 * when present, does not match.
 */
bool NmeaSplit(const wxString& sentence, std::vector<wxString>* fields);

/** The environment around the boat at one moment. NaN when unknown. */
struct Environment {
  double depth_m;          ///< Below the surface where the sensor says so
  double water_temp_c;
  double wind_speed_kn;    ///< True wind speed
  double wind_dir_deg;     ///< True wind direction, from, degrees true
};

/**
 * Keeps the latest instrument data from NMEA 0183 sentences forwarded by
 * OpenCPN, each value stamped with when it arrived, and the offset of the
 * host clock from GNSS time.
 *
 * Handles DPT, DBS, DBT (depth), MTW (water temperature), MWD (true wind),
 * MWV (true or apparent wind), RMC and ZDA (GNSS date and time).
 */
class NmeaTracker {
public:
  NmeaTracker();

  /**
   * Feed one sentence. `now` is a monotonic time in seconds;
   * `system_epoch` the host's wall clock when it arrived.
   */
  void Process(const wxString& sentence, double now, int64_t system_epoch);

  /**
   * Values no older than max_age seconds at monotonic time `now`.
   * heading_deg and sog_kn (NaN if unknown) let apparent and boat-relative
   * true wind be turned into a true wind direction.
   */
  Environment Snapshot(double now, double max_age, double heading_deg,
                       double sog_kn) const;

  /**
   * GNSS time minus host time, in seconds, if a GNSS time arrived within
   * max_age seconds; returns false otherwise.
   */
  bool ClockOffset(double now, double max_age, int64_t* offset) const;

private:
  struct Value {
    double v;
    double at;  // monotonic seconds, -1 if never set
  };
  static bool Fresh(const Value& value, double now, double max_age);

  void HandleDepth(const std::vector<wxString>& f, const wxString& type,
                   double now);
  void HandleWind(const std::vector<wxString>& f, const wxString& type,
                  double now);
  void HandleTime(const std::vector<wxString>& f, const wxString& type,
                  double now, int64_t system_epoch);

  Value depth_dpt_, depth_dbs_, depth_dbt_;
  Value water_temp_;
  Value mwd_dir_, mwd_speed_;
  Value true_rel_angle_, true_rel_speed_;   // MWV, T: relative to the bow
  Value app_angle_, app_speed_;             // MWV, R
  Value clock_offset_;
};

}  // namespace observer

#endif  // OBSERVER_NMEA_H
