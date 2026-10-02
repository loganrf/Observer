// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_SETTINGS_H
#define OBSERVER_SETTINGS_H

#include <wx/arrstr.h>
#include <wx/string.h>

class wxFileConfig;

namespace observer {

/** Units for the range to an animal. */
enum class RangeUnit { kMetres = 0, kNauticalMiles = 1 };

/** Preferences, kept in OpenCPN's config under /PlugIns/Observer. */
struct Settings {
  wxString log_dir;
  wxString observer;
  wxString vessel;
  bool show_marks = true;
  bool show_mark_names = true;
  bool copy_media = true;
  bool hotkey = true;
  RangeUnit range_unit = RangeUnit::kMetres;
  wxArrayString categories;
  wxArrayString behaviours;
  wxArrayString recent_species;  ///< Most recent first
  wxString last_category;

  void Load(wxFileConfig* config, const wxString& default_log_dir);
  void Save(wxFileConfig* config) const;

  /** Move a species to the front of the recent list. */
  void RememberSpecies(const wxString& species);

  static wxArrayString DefaultCategories();
  static wxArrayString DefaultBehaviours();
  static wxArrayString Confidences();
};

/** Join with ';' and split back, for list-valued config entries. */
wxString JoinList(const wxArrayString& items);
wxArrayString SplitList(const wxString& text);

}  // namespace observer

#endif  // OBSERVER_SETTINGS_H
