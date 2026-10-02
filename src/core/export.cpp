// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "export.h"

#include "csv.h"
#include "geo.h"
#include "time_util.h"

namespace observer {

const char* const kSightingIconName = "observer-sighting";

namespace {

// Columns written as JSON numbers rather than strings.
bool IsNumericColumn(const wxString& name) {
  static const char* const kNumeric[] = {
      "latitude",          "longitude",          "fix_age_s",
      "cog_deg_true",      "sog_kn",             "heading_deg_true",
      "count",             "bearing_deg_true",   "range_m",
      "sighting_latitude", "sighting_longitude", "depth_m",
      "water_temp_c",      "wind_speed_kn",      "wind_dir_deg_true"};
  for (const char* n : kNumeric)
    if (name == n) return true;
  return false;
}

CsvRow ExportRow(const Observation& o, const MediaResolver& media) {
  Observation copy = o;
  if (!copy.media.empty() && media) copy.media = media(copy.media);
  return ObservationToRow(copy);
}

wxString GpxDescription(const Observation& o) {
  wxString d;
  if (o.count > 0) d << "Count: " << o.count << "\n";
  if (!o.category.empty()) d << "Category: " << o.category << "\n";
  if (!o.behaviour.empty()) d << "Behaviour: " << o.behaviour << "\n";
  if (!o.confidence.empty()) d << "Confidence: " << o.confidence << "\n";
  if (!o.observer.empty()) d << "Observer: " << o.observer << "\n";
  if (!o.description.empty()) d << "\n" << o.description;
  d.Trim(true);
  return d;
}

wxString FileUrl(const wxString& path) {
  wxString p = path;
  p.Replace("\\", "/");
  wxString out = p.StartsWith("/") ? "file://" : "file:///";
  for (wxUniChar c : p) {
    if (c == ' ')
      out += "%20";
    else if (c == '#')
      out += "%23";
    else if (c == '%')
      out += "%25";
    else
      out += c;
  }
  return out;
}

}  // namespace

wxString JsonString(const wxString& s) {
  wxString out = "\"";
  for (wxUniChar c : s) {
    const auto v = c.GetValue();
    switch (v) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (v < 0x20)
          out += wxString::Format("\\u%04x", static_cast<unsigned>(v));
        else
          out += c;
    }
  }
  out += "\"";
  return out;
}

wxString XmlEscape(const wxString& s) {
  wxString out;
  for (wxUniChar c : s) {
    switch (c.GetValue()) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:
        if (c.GetValue() < 0x20 && c != '\n' && c != '\r' && c != '\t')
          break;  // not allowed in XML 1.0
        out += c;
    }
  }
  return out;
}

wxString ExportCsv(const std::vector<Observation>& items,
                   const MediaResolver& media) {
  wxString out = wxString(wxUniChar(0xFEFF)) + CsvFormatRow(ObservationColumns());
  for (const Observation& o : items) out += CsvFormatRow(ExportRow(o, media));
  return out;
}

wxString ExportGeoJson(const std::vector<Observation>& items,
                       const MediaResolver& media) {
  const CsvRow& columns = ObservationColumns();
  wxString out = "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [";
  bool first = true;
  for (const Observation& o : items) {
    if (!o.HasMarkPosition()) continue;
    const CsvRow row = ExportRow(o, media);
    out += first ? "\n" : ",\n";
    first = false;
    out += "    {\"type\": \"Feature\", \"geometry\": {\"type\": \"Point\", "
           "\"coordinates\": [";
    out += FormatNumber(NormalizeLongitude(o.MarkLon()), 6) + ", " +
           FormatNumber(o.MarkLat(), 6) + "]}, \"properties\": {";
    for (size_t i = 0; i < columns.size(); ++i) {
      if (i > 0) out += ", ";
      out += JsonString(columns[i]) + ": ";
      if (IsNumericColumn(columns[i]))
        out += row[i].empty() ? wxString("null") : row[i];
      else
        out += JsonString(row[i]);
    }
    out += "}}";
  }
  out += first ? "]\n}\n" : "\n  ]\n}\n";
  return out;
}

wxString ExportGpx(const std::vector<Observation>& items,
                   const MediaResolver& media, const wxString& creator) {
  wxString out =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"" +
      XmlEscape(creator) +
      "\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n";
  for (const Observation& o : items) {
    if (!o.HasMarkPosition()) continue;
    out += "  <wpt lat=\"" + FormatNumber(o.MarkLat(), 6) + "\" lon=\"" +
           FormatNumber(NormalizeLongitude(o.MarkLon()), 6) + "\">\n";
    out += "    <time>" + FormatUtc(o.utc) + "</time>\n";
    wxString name = o.Title();
    if (o.count > 1) name << " (" << o.count << ")";
    out += "    <name>" + XmlEscape(name) + "</name>\n";
    const wxString desc = GpxDescription(o);
    if (!desc.empty()) out += "    <desc>" + XmlEscape(desc) + "</desc>\n";
    if (!o.media.empty()) {
      const wxString path = media ? media(o.media) : o.media;
      out += "    <link href=\"" + XmlEscape(FileUrl(path)) +
             "\"><text>Media</text></link>\n";
    }
    out += "    <sym>" + wxString(kSightingIconName) + "</sym>\n";
    if (!o.category.empty())
      out += "    <type>" + XmlEscape(o.category) + "</type>\n";
    out += "  </wpt>\n";
  }
  out += "</gpx>\n";
  return out;
}

}  // namespace observer
