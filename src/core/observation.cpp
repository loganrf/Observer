// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "observation.h"

#include <cmath>

#include "geo.h"
#include "time_util.h"

namespace observer {

namespace {

// Column names are the log's public format: add new ones at the end and
// never rename or reuse one.
const CsvRow kColumns = {
    "id",
    "utc_time",
    "local_time",
    "time_source",
    "latitude",
    "longitude",
    "position_source",
    "fix_age_s",
    "cog_deg_true",
    "sog_kn",
    "heading_deg_true",
    "category",
    "species",
    "count",
    "behaviour",
    "confidence",
    "bearing_deg_true",
    "range_m",
    "sighting_latitude",
    "sighting_longitude",
    "depth_m",
    "water_temp_c",
    "wind_speed_kn",
    "wind_dir_deg_true",
    "observer",
    "vessel",
    "description",
    "media",
};

double ParseOr(const wxString& s) {
  double v;
  return ParseNumber(s, &v) ? v : NoValue();
}

}  // namespace

Observation::Observation()
    : utc(0),
      utc_offset_min(0),
      lat(NoValue()),
      lon(NoValue()),
      position_source("none"),
      fix_age_s(NoValue()),
      cog_deg(NoValue()),
      sog_kn(NoValue()),
      heading_deg(NoValue()),
      count(1),
      bearing_deg(NoValue()),
      range_m(NoValue()),
      sighting_lat(NoValue()),
      sighting_lon(NoValue()),
      depth_m(NoValue()),
      water_temp_c(NoValue()),
      wind_speed_kn(NoValue()),
      wind_dir_deg(NoValue()) {}

double Observation::MarkLat() const {
  return IsValidPosition(sighting_lat, sighting_lon) ? sighting_lat : lat;
}

double Observation::MarkLon() const {
  return IsValidPosition(sighting_lat, sighting_lon) ? sighting_lon : lon;
}

bool Observation::HasMarkPosition() const {
  return IsValidPosition(MarkLat(), MarkLon());
}

void Observation::UpdateSightingPosition() {
  if (IsValidPosition(lat, lon) && HasValue(bearing_deg) &&
      HasValue(range_m) && range_m >= 0) {
    Destination(lat, lon, bearing_deg, range_m, &sighting_lat, &sighting_lon);
  } else {
    sighting_lat = NoValue();
    sighting_lon = NoValue();
  }
}

wxString Observation::Title() const {
  if (!species.empty()) return species;
  if (!category.empty()) return category;
  return "Sighting";
}

const CsvRow& ObservationColumns() { return kColumns; }

wxString FormatNumber(double v, int decimals) {
  if (!HasValue(v)) return wxString();
  wxString s = wxString::FromCDouble(v, decimals);
  if (s == "-0" || s.StartsWith("-0.")) {
    // Avoid "-0.0" for values that round to zero.
    double back;
    if (s.ToCDouble(&back) && back == 0.0) s = s.Mid(1);
  }
  return s;
}

bool ParseNumber(const wxString& s, double* v) {
  wxString t = s;
  t.Trim(true).Trim(false);
  if (t.empty()) return false;
  return t.ToCDouble(v) && std::isfinite(*v);
}

CsvRow ObservationToRow(const Observation& o) {
  CsvRow row;
  row.reserve(kColumns.size());
  row.push_back(o.id);
  row.push_back(FormatUtc(o.utc));
  row.push_back(FormatLocal(o.utc, o.utc_offset_min));
  row.push_back(o.time_source);
  row.push_back(FormatNumber(o.lat, 6));
  row.push_back(FormatNumber(o.lon, 6));
  row.push_back(o.position_source);
  row.push_back(FormatNumber(o.fix_age_s, 0));
  row.push_back(FormatNumber(o.cog_deg, 1));
  row.push_back(FormatNumber(o.sog_kn, 1));
  row.push_back(FormatNumber(o.heading_deg, 1));
  row.push_back(o.category);
  row.push_back(o.species);
  row.push_back(o.count > 0 ? wxString::Format("%d", o.count) : wxString());
  row.push_back(o.behaviour);
  row.push_back(o.confidence);
  row.push_back(FormatNumber(o.bearing_deg, 0));
  row.push_back(FormatNumber(o.range_m, 0));
  row.push_back(FormatNumber(o.sighting_lat, 6));
  row.push_back(FormatNumber(o.sighting_lon, 6));
  row.push_back(FormatNumber(o.depth_m, 1));
  row.push_back(FormatNumber(o.water_temp_c, 1));
  row.push_back(FormatNumber(o.wind_speed_kn, 1));
  row.push_back(FormatNumber(o.wind_dir_deg, 0));
  row.push_back(o.observer);
  row.push_back(o.vessel);
  row.push_back(o.description);
  row.push_back(o.media);
  return row;
}

bool ObservationFromRow(const CsvRow& header, const CsvRow& row,
                        Observation* obs) {
  std::map<wxString, wxString> v;
  for (size_t i = 0; i < header.size() && i < row.size(); ++i)
    v[header[i]] = row[i];
  auto get = [&v](const char* key) -> wxString {
    auto it = v.find(key);
    return it == v.end() ? wxString() : it->second;
  };

  Observation o;
  o.id = get("id");
  if (o.id.empty()) return false;

  int offset = 0;
  if (!ParseIso8601(get("utc_time"), &o.utc, nullptr)) return false;
  int64_t local_t;
  if (ParseIso8601(get("local_time"), &local_t, &offset))
    o.utc_offset_min = offset;
  o.time_source = get("time_source");

  o.lat = ParseOr(get("latitude"));
  o.lon = ParseOr(get("longitude"));
  o.position_source = get("position_source");
  if (o.position_source.empty()) o.position_source = "none";
  o.fix_age_s = ParseOr(get("fix_age_s"));
  o.cog_deg = ParseOr(get("cog_deg_true"));
  o.sog_kn = ParseOr(get("sog_kn"));
  o.heading_deg = ParseOr(get("heading_deg_true"));

  o.category = get("category");
  o.species = get("species");
  long count = 0;
  o.count = get("count").ToLong(&count) && count > 0 ? static_cast<int>(count)
                                                       : 0;
  o.behaviour = get("behaviour");
  o.confidence = get("confidence");

  o.bearing_deg = ParseOr(get("bearing_deg_true"));
  o.range_m = ParseOr(get("range_m"));
  o.sighting_lat = ParseOr(get("sighting_latitude"));
  o.sighting_lon = ParseOr(get("sighting_longitude"));

  o.depth_m = ParseOr(get("depth_m"));
  o.water_temp_c = ParseOr(get("water_temp_c"));
  o.wind_speed_kn = ParseOr(get("wind_speed_kn"));
  o.wind_dir_deg = ParseOr(get("wind_dir_deg_true"));

  o.observer = get("observer");
  o.vessel = get("vessel");
  o.description = get("description");
  o.media = get("media");

  *obs = o;
  return true;
}

wxString MakeObservationId(int64_t utc, uint32_t salt) {
  int y, mo, d, h, mi, s;
  EpochToUtc(utc, &y, &mo, &d, &h, &mi, &s);
  return wxString::Format("%04d%02d%02dT%02d%02d%02dZ-%04x", y, mo, d, h, mi,
                          s, static_cast<unsigned>(salt & 0xFFFF));
}

}  // namespace observer
