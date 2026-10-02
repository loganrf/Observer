// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "testing.h"
#include "time_util.h"

using namespace observer;

TEST(time_epoch_known_values) {
  CHECK_EQ(UtcToEpoch(1970, 1, 1, 0, 0, 0), 0);
  CHECK_EQ(UtcToEpoch(2000, 3, 1, 0, 0, 0), 951868800);
  CHECK_EQ(UtcToEpoch(2026, 10, 2, 14, 3, 27), 1790949807);
}

TEST(time_epoch_round_trip) {
  for (int64_t t : {int64_t(0), int64_t(951782400), int64_t(1790949807),
                    int64_t(-86401), int64_t(4102444799)}) {
    int y, mo, d, h, mi, s;
    EpochToUtc(t, &y, &mo, &d, &h, &mi, &s);
    CHECK_EQ(UtcToEpoch(y, mo, d, h, mi, s), t);
  }
}

TEST(time_format_utc_and_local) {
  CHECK_EQ(FormatUtc(1790949807), "2026-10-02T14:03:27Z");
  CHECK_EQ(FormatLocal(1790949807, -420), "2026-10-02T07:03:27-07:00");
  CHECK_EQ(FormatLocal(1790949807, 330), "2026-10-02T19:33:27+05:30");
}

TEST(time_parse_iso8601) {
  int64_t t = 0;
  int off = 99;
  CHECK(ParseIso8601("2026-10-02T14:03:27Z", &t, &off));
  CHECK_EQ(t, 1790949807);
  CHECK_EQ(off, 0);
  CHECK(ParseIso8601("2026-10-02T07:03:27-07:00", &t, &off));
  CHECK_EQ(t, 1790949807);
  CHECK_EQ(off, -420);
  CHECK(ParseIso8601("2026-10-02 19:33:27.250+0530", &t, &off));
  CHECK_EQ(t, 1790949807);
  CHECK_EQ(off, 330);
}

TEST(time_parse_rejects_garbage) {
  int64_t t;
  CHECK(!ParseIso8601("", &t, nullptr));
  CHECK(!ParseIso8601("2026-10-02", &t, nullptr));
  CHECK(!ParseIso8601("2026-10-02T14:03:27", &t, nullptr));  // no zone
  CHECK(!ParseIso8601("2026-13-02T14:03:27Z", &t, nullptr));
  CHECK(!ParseIso8601("2026-10-02T14:03:27Zjunk", &t, nullptr));
}
