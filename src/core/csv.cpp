// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "csv.h"

namespace observer {

wxString CsvEscape(const wxString& field) {
  bool quote = field.find_first_of(",\"\r\n") != wxString::npos;
  if (!field.empty() && (field[0] == ' ' || field.Last() == ' ')) quote = true;
  if (!quote) return field;
  wxString out = "\"";
  for (wxUniChar c : field) {
    if (c == '"') out += '"';
    out += c;
  }
  out += '"';
  return out;
}

wxString CsvFormatRow(const CsvRow& row) {
  wxString line;
  for (size_t i = 0; i < row.size(); ++i) {
    if (i > 0) line += ',';
    line += CsvEscape(row[i]);
  }
  line += "\r\n";
  return line;
}

std::vector<CsvRow> CsvParse(const wxString& text) {
  std::vector<CsvRow> rows;
  CsvRow row;
  wxString field;
  bool in_quotes = false;
  bool field_started = false;  // distinguishes "" from no field at all

  auto end_field = [&]() {
    row.push_back(field);
    field.clear();
    field_started = false;
  };
  auto end_row = [&]() {
    end_field();
    const bool blank = row.size() == 1 && row[0].empty();
    if (!blank) rows.push_back(row);
    row.clear();
  };

  auto it = text.begin();
  if (it != text.end() && *it == wxUniChar(0xFEFF)) ++it;

  for (; it != text.end(); ++it) {
    const wxUniChar c = *it;
    if (in_quotes) {
      if (c == '"') {
        auto next = it;
        ++next;
        if (next != text.end() && *next == '"') {
          field += '"';
          it = next;
        } else {
          in_quotes = false;
        }
      } else {
        field += c;
      }
      continue;
    }
    if (c == '"' && !field_started && field.empty()) {
      in_quotes = true;
      field_started = true;
    } else if (c == ',') {
      end_field();
    } else if (c == '\r') {
      auto next = it;
      ++next;
      if (next != text.end() && *next == '\n') it = next;
      end_row();
    } else if (c == '\n') {
      end_row();
    } else {
      field += c;
      field_started = true;
    }
  }
  if (field_started || !field.empty() || !row.empty()) end_row();
  return rows;
}

}  // namespace observer
