// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_STORE_H
#define OBSERVER_STORE_H

#include <vector>

#include <wx/arrstr.h>
#include <wx/string.h>

#include "observation.h"

namespace observer {

/**
 * The sighting log: observations.csv plus a media/ folder, in one
 * directory a person can open, copy or back up.
 *
 * New sightings are appended to the CSV so a power cut can at worst lose
 * the record being written. Edits and deletions rewrite the file through
 * a temporary copy, keeping the previous version as observations.csv.bak.
 *
 * The store never rewrites a file it could not read in full: rows it had
 * to skip or columns it does not know would be lost. Such a log can still
 * take new sightings while appending is safe; edits and deletions are
 * refused with the reason until the file is fixed.
 */
class ObservationStore {
public:
  explicit ObservationStore(const wxString& dir);

  const wxString& Dir() const { return dir_; }
  wxString LogPath() const;
  wxString MediaDir() const;

  /** Read the log. A missing file is an empty log, not an error. */
  bool Load(wxString* error);

  /**
   * What the last Load() could not read, for showing to the person, or ""
   * if it read everything.
   */
  wxString LoadProblems() const;

  const std::vector<Observation>& All() const { return items_; }
  const Observation* Find(const wxString& id) const;

  /** Add a sighting and append it to the CSV. */
  bool Add(const Observation& obs, wxString* error);

  /** Replace the sighting with the same id and rewrite the CSV. */
  bool Update(const Observation& obs, wxString* error);

  /** Remove a sighting (not its media) and rewrite the CSV. */
  bool Remove(const wxString& id, wxString* error);

  /**
   * Copy a media file into media/ as "<id>-<name>" and return the path to
   * store in Observation::media, relative to the log folder.
   */
  bool ImportMedia(const wxString& id, const wxString& source,
                   wxString* stored, wxString* error);

  /** Absolute path for an Observation::media value ("" if none). */
  wxString MediaPath(const wxString& media) const;

  /** Delete a media file if it lives inside media/. */
  bool DeleteMedia(const wxString& media);

private:
  bool EnsureDir(wxString* error) const;
  bool WriteAll(wxString* error);
  /** "" if rewriting the file loses nothing, else why it would. */
  wxString RewriteBlocker() const;

  wxString dir_;
  std::vector<Observation> items_;
  bool header_current_ = true;  // CSV on disk uses today's columns
  bool utf8_ = true;            // CSV on disk is UTF-8
  int skipped_rows_ = 0;        // rows Load() could not read
  wxArrayString unknown_columns_;
};

/** Make a file name safe on every platform: no separators or odd chars. */
wxString SanitizeFileName(const wxString& name);

}  // namespace observer

#endif  // OBSERVER_STORE_H
