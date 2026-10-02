// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_TIME_UTIL_H
#define OBSERVER_TIME_UTIL_H

#include <cstdint>

#include <wx/string.h>

namespace observer {

/** Seconds since 1970-01-01 for a proleptic Gregorian UTC date and time. */
int64_t UtcToEpoch(int year, int month, int day, int hour, int minute,
                   int second);

/** Break epoch seconds into a UTC date and time. */
void EpochToUtc(int64_t t, int* year, int* month, int* day, int* hour,
                int* minute, int* second);

/** "2026-10-02T14:03:27Z" */
wxString FormatUtc(int64_t t);

/** "2026-10-02T07:03:27-07:00" for a local offset of -420 minutes. */
wxString FormatLocal(int64_t t, int offset_minutes);

/**
 * Parse an ISO 8601 date-time: "YYYY-MM-DDThh:mm:ss" followed by "Z" or
 * "+hh:mm"/"-hh:mm" (a space may replace the "T"; fractional seconds are
 * dropped). Returns false on anything else.
 */
bool ParseIso8601(const wxString& text, int64_t* t, int* offset_minutes);

/** The host's UTC offset, in minutes, at epoch time t. */
int LocalOffsetMinutes(int64_t t);

/** Current wall-clock time in epoch seconds. */
int64_t NowEpoch();

/** Monotonic seconds, for measuring data age. */
double MonotonicSeconds();

}  // namespace observer

#endif  // OBSERVER_TIME_UTIL_H
