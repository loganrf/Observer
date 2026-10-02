// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "geo.h"
#include "testing.h"

using namespace observer;

TEST(geo_destination_north_one_nm) {
  double lat, lon;
  Destination(47.0, -122.0, 0.0, kMetresPerNm, &lat, &lon);
  CHECK_NEAR(lat, 47.0 + 1.0 / 60.0, 1e-4);
  CHECK_NEAR(lon, -122.0, 1e-9);
}

TEST(geo_destination_round_trip_distance) {
  double lat, lon;
  Destination(47.6815, -122.4194, 135.0, 800.0, &lat, &lon);
  CHECK_NEAR(DistanceMetres(47.6815, -122.4194, lat, lon), 800.0, 0.01);
  CHECK(lat < 47.6815);
  CHECK(lon > -122.4194);
}

TEST(geo_destination_crosses_antimeridian) {
  double lat, lon;
  Destination(0.0, 179.999, 90.0, 1000.0, &lat, &lon);
  CHECK(lon < -179.9);
}

TEST(geo_normalize) {
  CHECK_NEAR(NormalizeDegrees(-10), 350, 1e-9);
  CHECK_NEAR(NormalizeDegrees(720), 0, 1e-9);
  CHECK_NEAR(NormalizeLongitude(190), -170, 1e-9);
  CHECK_NEAR(NormalizeLongitude(-180), -180, 1e-9);
}

TEST(geo_format_ddm) {
  CHECK_EQ(FormatLatitude(47.6815, LatLonFormat::kDegreesDecimalMinutes),
           wxString(L"47\u00B0 40.890\u2032 N"));
  CHECK_EQ(FormatLongitude(-122.4194, LatLonFormat::kDegreesDecimalMinutes),
           wxString(L"122\u00B0 25.164\u2032 W"));
  CHECK_EQ(FormatLatitude(NoValue(), LatLonFormat::kDecimalDegrees), "--");
}

TEST(geo_format_dms_rounds_up_cleanly) {
  // 59.99" must carry into the minutes, not print as 60.0".
  CHECK_EQ(FormatLatitude(10.0 + 59.0 / 60 + 59.99 / 3600,
                          LatLonFormat::kDegreesMinutesSeconds),
           wxString(L"11\u00B0 00\u2032 00.0\u2033 N"));
}

TEST(geo_parse_coordinate_formats) {
  double v = 0;
  CHECK(ParseCoordinate("47.6815", true, &v));
  CHECK_NEAR(v, 47.6815, 1e-9);
  CHECK(ParseCoordinate("-122.4194", false, &v));
  CHECK_NEAR(v, -122.4194, 1e-9);
  CHECK(ParseCoordinate("47 40.89 N", true, &v));
  CHECK_NEAR(v, 47.6815, 1e-9);
  CHECK(ParseCoordinate(wxString(L"122\u00B0 25.164\u2032 W"), false, &v));
  CHECK_NEAR(v, -122.4194, 1e-9);
  CHECK(ParseCoordinate("S 33 51 35.9", true, &v));
  CHECK_NEAR(v, -(33 + 51 / 60.0 + 35.9 / 3600.0), 1e-9);
  CHECK(ParseCoordinate("47,6815", true, &v));
  CHECK_NEAR(v, 47.6815, 1e-9);
}

TEST(geo_parse_coordinate_rejects) {
  double v;
  CHECK(!ParseCoordinate("", true, &v));
  CHECK(!ParseCoordinate("91", true, &v));
  CHECK(!ParseCoordinate("47 61 N", true, &v));
  CHECK(!ParseCoordinate("47 40 E", true, &v));  // E is not a latitude
  CHECK(!ParseCoordinate("-47 40 N", true, &v));
  CHECK(!ParseCoordinate("abc", true, &v));
  CHECK(!ParseCoordinate("1 2 3 4", false, &v));
}

TEST(geo_true_wind_from_apparent) {
  double twd, tws;
  // Motoring at 5 kn into still air: 5 kn apparent on the bow, no wind.
  TrueWindFromApparent(0, 5, 5, 90, &twd, &tws);
  CHECK_NEAR(tws, 0, 1e-9);
  // 10 kn true from astern while making 5 kn: 5 kn apparent from astern.
  TrueWindFromApparent(180, 5, 5, 0, &twd, &tws);
  CHECK_NEAR(tws, 10, 1e-9);
  CHECK_NEAR(twd, 180, 1e-9);
  // Beam reach: 10 kn true from starboard beam at 6 kn boat speed.
  TrueWindFromApparent(59.036, std::sqrt(136.0), 6, 0, &twd, &tws);
  CHECK_NEAR(tws, 10, 1e-3);
  CHECK_NEAR(twd, 90, 1e-2);
}
