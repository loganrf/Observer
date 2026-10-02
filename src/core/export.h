// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_EXPORT_H
#define OBSERVER_EXPORT_H

#include <functional>
#include <vector>

#include <wx/string.h>

#include "observation.h"

namespace observer {

/** Turns an Observation::media value into the path an export should use. */
using MediaResolver = std::function<wxString(const wxString&)>;

/** The same columns as the log, media as resolved paths. */
wxString ExportCsv(const std::vector<Observation>& items,
                   const MediaResolver& media);

/** RFC 7946 FeatureCollection of points, all columns as properties. */
wxString ExportGeoJson(const std::vector<Observation>& items,
                       const MediaResolver& media);

/** GPX 1.1 waypoints that OpenCPN and most chart plotters import. */
wxString ExportGpx(const std::vector<Observation>& items,
                   const MediaResolver& media, const wxString& creator);

/** JSON string literal, quotes included. */
wxString JsonString(const wxString& s);

/** Escape text for an XML element or attribute. */
wxString XmlEscape(const wxString& s);

/** Waypoint icon name OpenCPN uses for sightings. */
extern const char* const kSightingIconName;

}  // namespace observer

#endif  // OBSERVER_EXPORT_H
