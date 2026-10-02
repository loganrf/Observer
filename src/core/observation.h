// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_OBSERVATION_H
#define OBSERVER_OBSERVATION_H

#include <cstdint>
#include <map>
#include <vector>

#include <wx/string.h>

#include "csv.h"

namespace observer {

/** One wildlife sighting. Numbers are NaN when unknown. */
struct Observation {
  Observation();

  wxString id;             ///< "20261002T140327Z-3f9a", unique and sortable
  int64_t utc;             ///< Seconds since 1970-01-01 UTC
  int utc_offset_min;      ///< Host's offset from UTC when logged
  wxString time_source;    ///< "gnss" or "system"

  double lat, lon;         ///< Vessel position
  wxString position_source;  ///< "gnss", "cursor", "manual" or "none"
  double fix_age_s;        ///< Age of the GNSS fix when logged
  double cog_deg;          ///< Course over ground, true
  double sog_kn;           ///< Speed over ground
  double heading_deg;      ///< Heading, true

  wxString category;       ///< "Whale", "Seabird", ...
  wxString species;        ///< Free text, "Humpback whale"
  int count;               ///< Individuals seen, 0 if unknown
  wxString behaviour;
  wxString confidence;     ///< "Certain", "Probable" or "Possible"

  double bearing_deg;      ///< From the vessel to the animal, true
  double range_m;          ///< From the vessel to the animal
  double sighting_lat, sighting_lon;  ///< Estimated animal position

  double depth_m;
  double water_temp_c;
  double wind_speed_kn;    ///< True
  double wind_dir_deg;     ///< True, direction the wind blows from

  wxString observer;
  wxString vessel;
  wxString description;
  wxString media;          ///< Path relative to the log folder, or absolute

  /** Where to put the sighting: the animal if known, else the vessel. */
  double MarkLat() const;
  double MarkLon() const;
  bool HasMarkPosition() const;

  /** Recompute sighting_lat/lon from position, bearing and range. */
  void UpdateSightingPosition();

  /** "Humpback whale" or the category if no species was given. */
  wxString Title() const;
};

/** CSV header, in column order. */
const CsvRow& ObservationColumns();

/** One CSV record for an observation, in ObservationColumns() order. */
CsvRow ObservationToRow(const Observation& obs);

/**
 * Read an observation from a record whose column names are in `header`.
 * Unknown columns are ignored and missing ones keep their defaults, so logs
 * written by older and newer versions still load. Returns false if the
 * record has no id or time.
 */
bool ObservationFromRow(const CsvRow& header, const CsvRow& row,
                        Observation* obs);

/** A new id for a sighting logged at `utc`; `salt` makes it unique. */
wxString MakeObservationId(int64_t utc, uint32_t salt);

/** Number formatting that never depends on the locale. */
wxString FormatNumber(double v, int decimals);
bool ParseNumber(const wxString& s, double* v);

}  // namespace observer

#endif  // OBSERVER_OBSERVATION_H
