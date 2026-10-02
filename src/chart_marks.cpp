// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chart_marks.h"

#include <memory>

#include <wx/bitmap.h>
#include <wx/filename.h>
#include <wx/listimpl.cpp>

#include "ocpn_api.h"

#include "export.h"
#include "geo.h"
#include "store.h"
#include "time_util.h"

// OpenCPN declares this list type but does not export its implementation
// on every platform; plugins that fill one define it themselves.
WX_DEFINE_LIST(Plugin_HyperlinkList);

namespace observer {

namespace {

wxString FileUrl(const wxString& path) {
  wxString p = path;
  p.Replace("\\", "/");
  return (p.StartsWith("/") ? "file://" : "file:///") + p;
}

}  // namespace

wxString MarkDescription(const Observation& o) {
  wxString d;
  d << FormatUtc(o.utc);
  if (o.count > 0) d << "\n" << _("Count") << ": " << o.count;
  if (!o.category.empty()) d << "\n" << _("Category") << ": " << o.category;
  if (!o.behaviour.empty()) d << "\n" << _("Behaviour") << ": " << o.behaviour;
  if (!o.confidence.empty())
    d << "\n" << _("Confidence") << ": " << o.confidence;
  if (!o.observer.empty()) d << "\n" << _("Observer") << ": " << o.observer;
  if (!o.description.empty()) d << "\n\n" << o.description;
  return d;
}

wxString ChartMarks::Guid(const wxString& id) { return "observer-" + id; }

bool ChartMarks::RegisterIcon(const wxString& svg_path) {
  if (!wxFileName::FileExists(svg_path)) return false;
  const unsigned px = 24;
  wxBitmap bmp = GetBitmapFromSVGFile(svg_path, px, px);
  if (!bmp.IsOk()) return false;
  icon_ok_ = AddCustomWaypointIcon(&bmp, kSightingIconName,
                                   _("Wildlife sighting"));
  return icon_ok_;
}

void ChartMarks::Add(const Observation& obs, const ObservationStore& store,
                     bool show_names) {
  if (!obs.HasMarkPosition()) return;
  Remove(obs.id);

  wxString name = obs.Title();
  if (obs.count > 1) name << " (" << obs.count << ")";

  PlugIn_Waypoint_Ex wp(obs.MarkLat(), NormalizeLongitude(obs.MarkLon()),
                        icon_ok_ ? wxString(kSightingIconName) : "circle",
                        name, Guid(obs.id));
  wp.m_MarkDescription = MarkDescription(obs);
  wp.m_CreateTime = wxDateTime(static_cast<time_t>(obs.utc));
  wp.IsNameVisible = show_names;
  wp.IsVisible = true;

  // OpenCPN copies the links, so they only need to outlive this call.
  std::unique_ptr<Plugin_Hyperlink> link;
  std::unique_ptr<Plugin_HyperlinkList> links;
  const wxString media = store.MediaPath(obs.media);
  if (!media.empty()) {
    link = std::make_unique<Plugin_Hyperlink>();
    link->DescrText = _("Photo or video");
    link->Link = FileUrl(media);
    link->Type = "";
    links = std::make_unique<Plugin_HyperlinkList>();
    links->Append(link.get());
    wp.m_HyperlinkList = links.get();
  }

  if (AddSingleWaypointEx(&wp, false)) shown_.insert(obs.id);
  wp.m_HyperlinkList = nullptr;
}

void ChartMarks::Remove(const wxString& id) {
  if (shown_.erase(id) == 0) return;
  wxString guid = Guid(id);
  DeleteSingleWaypoint(guid);
}

void ChartMarks::Clear() {
  for (const wxString& id : shown_) {
    wxString guid = Guid(id);
    DeleteSingleWaypoint(guid);
  }
  shown_.clear();
}

void ChartMarks::Sync(const std::vector<Observation>& items,
                      const ObservationStore& store, bool enabled,
                      bool show_names) {
  Clear();
  if (!enabled) return;
  for (const Observation& o : items) Add(o, store, show_names);
}

}  // namespace observer
