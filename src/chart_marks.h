// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_CHART_MARKS_H
#define OBSERVER_CHART_MARKS_H

#include <set>
#include <vector>

#include <wx/string.h>

#include "observation.h"

namespace observer {

class ObservationStore;

/**
 * Shows sightings on the chart as OpenCPN marks.
 *
 * The marks are temporary: OpenCPN does not save them, the log is the
 * only record, and they are rebuilt from it each session. Each mark links
 * to its photo or video so it opens from the mark's properties.
 */
class ChartMarks {
public:
  /** Register the sighting icon. Needs OpenCPN's LateInit() to have run. */
  bool RegisterIcon(const wxString& svg_path);

  /** Remove every mark and add one per sighting, if enabled. */
  void Sync(const std::vector<Observation>& items,
            const ObservationStore& store, bool enabled, bool show_names);

  void Add(const Observation& obs, const ObservationStore& store,
           bool show_names);
  void Remove(const wxString& id);
  void Clear();

private:
  static wxString Guid(const wxString& id);

  std::set<wxString> shown_;  // observation ids with a mark on the chart
  bool icon_ok_ = false;
};

/** The text OpenCPN shows in a sighting mark's description. */
wxString MarkDescription(const Observation& obs);

}  // namespace observer

#endif  // OBSERVER_CHART_MARKS_H
