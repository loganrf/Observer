// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "csv.h"
#include "testing.h"

using namespace observer;

TEST(csv_escape_plain_field_unchanged) {
  CHECK_EQ(CsvEscape("Humpback whale"), "Humpback whale");
  CHECK_EQ(CsvEscape(""), "");
}

TEST(csv_escape_quotes_when_needed) {
  CHECK_EQ(CsvEscape("a,b"), "\"a,b\"");
  CHECK_EQ(CsvEscape("say \"hi\""), "\"say \"\"hi\"\"\"");
  CHECK_EQ(CsvEscape("two\nlines"), "\"two\nlines\"");
  CHECK_EQ(CsvEscape(" padded"), "\" padded\"");
}

TEST(csv_parse_round_trip) {
  const CsvRow row = {"id", "a,b", "say \"hi\"", "line 1\nline 2", "", "x"};
  const wxString text = CsvFormatRow(row) + CsvFormatRow({"1", "2"});
  const auto rows = CsvParse(text);
  CHECK_EQ(rows.size(), 2u);
  CHECK(rows[0] == row);
  CHECK_EQ(rows[1].size(), 2u);
}

TEST(csv_parse_lf_bom_and_blank_lines) {
  const wxString text =
      wxString(wxUniChar(0xFEFF)) + "a,b\n\n1,2\n3,\"\"\n";
  const auto rows = CsvParse(text);
  CHECK_EQ(rows.size(), 3u);
  CHECK_EQ(rows[0][0], "a");
  CHECK_EQ(rows[2][0], "3");
  CHECK_EQ(rows[2][1], "");
}

TEST(csv_parse_no_trailing_newline) {
  const auto rows = CsvParse("a,b\r\n1,2");
  CHECK_EQ(rows.size(), 2u);
  CHECK_EQ(rows[1][1], "2");
}

TEST(csv_parse_unicode) {
  const wxString name = wxString::FromUTF8("Dauphin bleu et blanc \xC3\xA9");
  const auto rows = CsvParse(CsvFormatRow({name}));
  CHECK_EQ(rows.size(), 1u);
  CHECK_EQ(rows[0][0], name);
}
