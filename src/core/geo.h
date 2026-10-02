// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_GEO_H
#define OBSERVER_GEO_H

#include <wx/string.h>

namespace observer {

constexpr double kMetresPerNm = 1852.0;

/** True for a finite number; NaN marks a missing value throughout. */
bool HasValue(double v);

/** Missing-value marker. */
double NoValue();

/** True when lat/lon are both present and in range. */
bool IsValidPosition(double lat, double lon);

/** Wrap an angle into [0, 360). */
double NormalizeDegrees(double deg);

/** Wrap a longitude into [-180, 180). */
double NormalizeLongitude(double lon);

/**
 * Great-circle destination from (lat, lon) along an initial true bearing
 * for a distance in metres, on a sphere of mean Earth radius.
 */
void Destination(double lat, double lon, double bearing_deg, double dist_m,
                 double* lat2, double* lon2);

/** Great-circle distance in metres between two positions. */
double DistanceMetres(double lat1, double lon1, double lat2, double lon2);

/** Position display formats, numbered as OpenCPN's GetLatLonFormat(). */
enum class LatLonFormat {
  kDegreesDecimalMinutes = 0,
  kDecimalDegrees = 1,
  kDegreesMinutesSeconds = 2
};

/** "47° 40.890′ N" and friends. */
wxString FormatLatitude(double lat, LatLonFormat fmt);
wxString FormatLongitude(double lon, LatLonFormat fmt);

/**
 * Parse a latitude or longitude typed by a person. Accepts signed decimal
 * degrees ("-122.4194"), degrees and decimal minutes ("47 40.89 N",
 * "47°40.890'N", "N 47 40.89") and degrees, minutes, seconds
 * ("47 40 53.4 N"). A hemisphere letter overrides the sign. Returns false
 * for anything else or a value out of range.
 */
bool ParseCoordinate(const wxString& text, bool is_latitude, double* value);

/**
 * True wind from apparent wind, ignoring current and leeway.
 * awa_deg: apparent wind angle off the bow, 0..360 (or -180..180,
 * negative to port); aws and boat_speed in the same unit; heading_deg the
 * boat's true heading. Writes true wind direction (from, degrees true)
 * and true wind speed.
 */
void TrueWindFromApparent(double awa_deg, double aws, double boat_speed,
                          double heading_deg, double* twd_deg, double* tws);

}  // namespace observer

#endif  // OBSERVER_GEO_H
