// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "csv.h"
#include "export.h"
#include "testing.h"

using namespace observer;

namespace {

std::vector<Observation> Items() {
  Observation a;
  a.id = "a";
  a.utc = 1790949807;
  a.lat = 47.6815;
  a.lon = -122.4194;
  a.species = "Orca & calf <pod>";
  a.count = 3;
  a.description = "Line 1\nLine \"2\"";
  a.media = "media/a-IMG 1.jpg";

  Observation b;  // no position: skipped by GeoJSON and GPX
  b.id = "b";
  b.utc = 1790949907;
  b.species = "Seal";
  return {a, b};
}

wxString Resolve(const wxString& m) { return "/logs/" + m; }

}  // namespace

TEST(export_json_string_escapes) {
  CHECK_EQ(JsonString("a\"b\\c\n\x01"), "\"a\\\"b\\\\c\\n\\u0001\"");
}

TEST(export_xml_escape) {
  CHECK_EQ(XmlEscape("<a & 'b'>\x02"), "&lt;a &amp; &apos;b&apos;&gt;");
}

TEST(export_csv_resolves_media) {
  const auto rows = CsvParse(ExportCsv(Items(), Resolve));
  CHECK_EQ(rows.size(), 3u);
  CHECK_EQ(rows[1].back(), "/logs/media/a-IMG 1.jpg");
  CHECK_EQ(rows[2].back(), "");
}

TEST(export_geojson_shape) {
  const wxString json = ExportGeoJson(Items(), Resolve);
  CHECK(json.Contains("\"type\": \"FeatureCollection\""));
  CHECK(json.Contains("\"coordinates\": [-122.419400, 47.681500]"));
  CHECK(json.Contains("\"count\": 3"));
  CHECK(json.Contains("\"depth_m\": null"));
  CHECK(json.Contains("\"species\": \"Orca & calf <pod>\""));
  CHECK(!json.Contains("\"Seal\""));
}

TEST(export_geojson_empty) {
  CHECK_EQ(ExportGeoJson({}, Resolve),
           "{\n  \"type\": \"FeatureCollection\",\n  \"features\": []\n}\n");
}

TEST(export_gpx_waypoints) {
  const wxString gpx = ExportGpx(Items(), Resolve, "Observer test");
  CHECK(gpx.Contains("<wpt lat=\"47.681500\" lon=\"-122.419400\">"));
  CHECK(gpx.Contains("<time>2026-10-02T14:03:27Z</time>"));
  CHECK(gpx.Contains("<name>Orca &amp; calf &lt;pod&gt; (3)</name>"));
  CHECK(gpx.Contains("<link href=\"file:///logs/media/a-IMG%201.jpg\">"));
  CHECK(gpx.Contains("<sym>observer-sighting</sym>"));
  CHECK(!gpx.Contains("Seal"));
}
