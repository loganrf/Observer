// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "geo.h"
#include "observation.h"
#include "testing.h"
#include "time_util.h"

using namespace observer;

namespace {

Observation Sample() {
  Observation o;
  o.utc = UtcToEpoch(2026, 10, 2, 14, 3, 27);
  o.utc_offset_min = -420;
  o.id = MakeObservationId(o.utc, 0x3f9a);
  o.time_source = "gnss";
  o.lat = 47.6815;
  o.lon = -122.4194;
  o.position_source = "gnss";
  o.fix_age_s = 1;
  o.cog_deg = 182.5;
  o.sog_kn = 5.2;
  o.heading_deg = 180;
  o.category = "Whale";
  o.species = wxString::FromUTF8("Humpback whale \xE2\x80\x94 mother, calf");
  o.count = 2;
  o.behaviour = "Breaching";
  o.confidence = "Certain";
  o.bearing_deg = 90;
  o.range_m = 500;
  o.UpdateSightingPosition();
  o.depth_m = 42.5;
  o.water_temp_c = 12.3;
  o.wind_speed_kn = 14;
  o.wind_dir_deg = 270;
  o.observer = "Logan";
  o.vessel = "Kestrel";
  o.description = "Two breaches, then \"tail slapping\".\nHeading north.";
  o.media = "media/20261002T140327Z-3f9a-IMG_0042.jpg";
  return o;
}

}  // namespace

TEST(observation_id_format) {
  CHECK_EQ(MakeObservationId(UtcToEpoch(2026, 10, 2, 14, 3, 27), 0x13f9a),
           "20261002T140327Z-3f9a");
}

TEST(observation_columns_match_row) {
  CHECK_EQ(ObservationColumns().size(), ObservationToRow(Sample()).size());
}

TEST(observation_round_trip) {
  const Observation a = Sample();
  Observation b;
  CHECK(ObservationFromRow(ObservationColumns(), ObservationToRow(a), &b));
  CHECK_EQ(b.id, a.id);
  CHECK_EQ(b.utc, a.utc);
  CHECK_EQ(b.utc_offset_min, -420);
  CHECK_EQ(b.species, a.species);
  CHECK_EQ(b.count, 2);
  CHECK_EQ(b.description, a.description);
  CHECK_EQ(b.media, a.media);
  CHECK_NEAR(b.lat, a.lat, 1e-6);
  CHECK_NEAR(b.lon, a.lon, 1e-6);
  CHECK_NEAR(b.sighting_lat, a.sighting_lat, 1e-6);
  CHECK_NEAR(b.wind_dir_deg, 270, 1e-9);
  CHECK_NEAR(b.depth_m, 42.5, 1e-9);
}

TEST(observation_missing_values_round_trip) {
  Observation a;
  a.utc = 1790949807;
  a.id = "x";
  a.count = 0;
  const CsvRow row = ObservationToRow(a);
  Observation b;
  CHECK(ObservationFromRow(ObservationColumns(), row, &b));
  CHECK(!HasValue(b.lat));
  CHECK(!HasValue(b.depth_m));
  CHECK_EQ(b.count, 0);
  CHECK_EQ(b.position_source, "none");
}

TEST(observation_reads_reordered_and_unknown_columns) {
  const CsvRow header = {"species", "future_column", "utc_time", "id"};
  const CsvRow row = {"Orca", "whatever", "2026-10-02T14:03:27Z", "abc"};
  Observation o;
  CHECK(ObservationFromRow(header, row, &o));
  CHECK_EQ(o.species, "Orca");
  CHECK_EQ(o.id, "abc");
  CHECK_EQ(o.utc, 1790949807);
}

TEST(observation_rejects_rows_without_id_or_time) {
  Observation o;
  CHECK(!ObservationFromRow({"id", "utc_time"}, {"", "2026-10-02T14:03:27Z"},
                            &o));
  CHECK(!ObservationFromRow({"id", "utc_time"}, {"a", "yesterday"}, &o));
}

TEST(observation_sighting_position) {
  Observation o = Sample();
  CHECK(o.HasMarkPosition());
  CHECK_NEAR(DistanceMetres(o.lat, o.lon, o.MarkLat(), o.MarkLon()), 500,
             0.01);
  o.range_m = NoValue();
  o.UpdateSightingPosition();
  CHECK_NEAR(o.MarkLat(), o.lat, 1e-12);
}

TEST(observation_number_format_is_locale_free) {
  CHECK_EQ(FormatNumber(12.26, 1), "12.3");
  CHECK_EQ(FormatNumber(-0.01, 1), "0.0");
  CHECK_EQ(FormatNumber(NoValue(), 1), "");
  double v;
  CHECK(ParseNumber(" 3.5 ", &v));
  CHECK_NEAR(v, 3.5, 1e-12);
  CHECK(!ParseNumber("", &v));
  CHECK(!ParseNumber("nan", &v));
}

TEST(observation_title) {
  Observation o;
  CHECK_EQ(o.Title(), "Sighting");
  o.category = "Seabird";
  CHECK_EQ(o.Title(), "Seabird");
  o.species = "Albatross";
  CHECK_EQ(o.Title(), "Albatross");
}
