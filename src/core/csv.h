// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_CSV_H
#define OBSERVER_CSV_H

#include <vector>

#include <wx/string.h>

namespace observer {

using CsvRow = std::vector<wxString>;

/** Quote a field when RFC 4180 requires it. */
wxString CsvEscape(const wxString& field);

/** One record, fields escaped, terminated by CRLF. */
wxString CsvFormatRow(const CsvRow& row);

/**
 * Parse RFC 4180 text: quoted fields may hold commas, doubled quotes and
 * line breaks; CRLF and LF both end a record; a leading UTF-8 BOM is
 * ignored; blank lines are skipped.
 */
std::vector<CsvRow> CsvParse(const wxString& text);

}  // namespace observer

#endif  // OBSERVER_CSV_H
