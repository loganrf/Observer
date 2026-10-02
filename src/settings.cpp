// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <wx/fileconf.h>
#include <wx/intl.h>

namespace observer {

namespace {

const char* kGroup = "/PlugIns/Observer";
const size_t kMaxRecentSpecies = 40;

}  // namespace

wxString JoinList(const wxArrayString& items) {
  wxString out;
  for (const wxString& item : items) {
    wxString clean = item;
    clean.Replace(";", ",");
    clean.Trim(true).Trim(false);
    if (clean.empty()) continue;
    if (!out.empty()) out += ";";
    out += clean;
  }
  return out;
}

wxArrayString SplitList(const wxString& text) {
  wxArrayString out;
  for (wxString item : wxSplit(text, ';', '\0')) {
    item.Trim(true).Trim(false);
    if (!item.empty() && out.Index(item) == wxNOT_FOUND) out.Add(item);
  }
  return out;
}

wxArrayString Settings::DefaultCategories() {
  wxArrayString a;
  a.Add(_("Whale"));
  a.Add(_("Dolphin"));
  a.Add(_("Porpoise"));
  a.Add(_("Seal or sea lion"));
  a.Add(_("Seabird"));
  a.Add(_("Shark or ray"));
  a.Add(_("Turtle"));
  a.Add(_("Fish"));
  a.Add(_("Other"));
  return a;
}

wxArrayString Settings::DefaultBehaviours() {
  wxArrayString a;
  a.Add(_("Travelling"));
  a.Add(_("Feeding"));
  a.Add(_("Resting"));
  a.Add(_("Socialising"));
  a.Add(_("Breaching"));
  a.Add(_("Bow-riding"));
  a.Add(_("Diving"));
  a.Add(_("Milling"));
  return a;
}

wxArrayString Settings::Confidences() {
  wxArrayString a;
  a.Add(_("Certain"));
  a.Add(_("Probable"));
  a.Add(_("Possible"));
  return a;
}

void Settings::Load(wxFileConfig* config, const wxString& default_log_dir) {
  categories = DefaultCategories();
  behaviours = DefaultBehaviours();
  log_dir = default_log_dir;
  if (!config) return;

  const wxString old_path = config->GetPath();
  config->SetPath(kGroup);
  config->Read("LogDir", &log_dir, default_log_dir);
  if (log_dir.empty()) log_dir = default_log_dir;
  config->Read("Observer", &observer, wxString());
  config->Read("Vessel", &vessel, wxString());
  config->Read("ShowMarks", &show_marks, true);
  config->Read("ShowMarkNames", &show_mark_names, true);
  config->Read("CopyMedia", &copy_media, true);
  config->Read("Hotkey", &hotkey, true);
  int unit = 0;
  config->Read("RangeUnit", &unit, 0);
  range_unit = unit == 1 ? RangeUnit::kNauticalMiles : RangeUnit::kMetres;

  wxString list;
  if (config->Read("Categories", &list) && !SplitList(list).empty())
    categories = SplitList(list);
  if (config->Read("Behaviours", &list) && !SplitList(list).empty())
    behaviours = SplitList(list);
  if (config->Read("RecentSpecies", &list)) recent_species = SplitList(list);
  config->Read("LastCategory", &last_category, wxString());
  config->SetPath(old_path);
}

void Settings::Save(wxFileConfig* config) const {
  if (!config) return;
  const wxString old_path = config->GetPath();
  config->SetPath(kGroup);
  config->Write("LogDir", log_dir);
  config->Write("Observer", observer);
  config->Write("Vessel", vessel);
  config->Write("ShowMarks", show_marks);
  config->Write("ShowMarkNames", show_mark_names);
  config->Write("CopyMedia", copy_media);
  config->Write("Hotkey", hotkey);
  config->Write("RangeUnit", static_cast<int>(range_unit));
  config->Write("Categories", JoinList(categories));
  config->Write("Behaviours", JoinList(behaviours));
  config->Write("RecentSpecies", JoinList(recent_species));
  config->Write("LastCategory", last_category);
  config->SetPath(old_path);
  config->Flush();
}

void Settings::RememberSpecies(const wxString& species) {
  wxString s = species;
  s.Trim(true).Trim(false);
  if (s.empty()) return;
  const int existing = recent_species.Index(s, false);
  if (existing != wxNOT_FOUND) recent_species.RemoveAt(existing);
  recent_species.Insert(s, 0);
  while (recent_species.size() > kMaxRecentSpecies) recent_species.RemoveAt(recent_species.size() - 1);
}

}  // namespace observer
