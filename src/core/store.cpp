// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "store.h"

#include <algorithm>
#include <string>

#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/strconv.h>

namespace observer {

namespace {

const char* kLogName = "observations.csv";
const char* kMediaDirName = "media";

/**
 * Read a text file. UTF-8 is expected; anything else is read as Windows
 * code page 1252 (what spreadsheet programs on Windows save as "CSV"),
 * which can decode any byte, and *utf8 is cleared.
 */
bool ReadText(const wxString& path, wxString* text, bool* utf8,
              wxString* error) {
  wxFFile f;
  if (!f.Open(path, "rb")) {
    *error = wxString::Format("Cannot open %s", path);
    return false;
  }
  const wxFileOffset len = f.Length();
  if (len < 0) {
    *error = wxString::Format("Cannot read %s", path);
    return false;
  }
  std::string bytes(static_cast<size_t>(len), '\0');
  if (len > 0 && f.Read(&bytes[0], bytes.size()) != bytes.size()) {
    *error = wxString::Format("Cannot read %s", path);
    return false;
  }
  *utf8 = true;
  *text = wxString::FromUTF8(bytes.data(), bytes.size());
  if (text->empty() && !bytes.empty()) {
    *utf8 = false;
    *text = wxString(bytes.data(), wxCSConv(wxFONTENCODING_CP1252),
                     bytes.size());
    if (text->empty())
      *text = wxString(bytes.data(), wxConvISO8859_1, bytes.size());
  }
  return true;
}

bool EndsWithNewline(const wxString& path) {
  wxFFile f;
  if (!f.Open(path, "rb")) return true;
  const wxFileOffset len = f.Length();
  if (len <= 0) return true;
  if (!f.Seek(len - 1)) return true;
  char c = 0;
  if (f.Read(&c, 1) != 1) return true;
  return c == '\n' || c == '\r';
}

wxString Header() {
  // UTF-8 byte order mark first, so spreadsheet programs pick the right
  // encoding for species names with accents.
  return wxString(wxUniChar(0xFEFF)) + CsvFormatRow(ObservationColumns());
}

}  // namespace

wxString SanitizeFileName(const wxString& name) {
  wxString out;
  for (wxUniChar c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                    c == '_' || c.GetValue() > 0x7F;
    out += ok ? c : wxUniChar('_');
  }
  while (out.StartsWith(".")) out = out.Mid(1);
  if (out.empty()) out = "media";
  if (out.length() > 120) {
    const wxString ext = wxFileName(out).GetExt();
    out = out.Left(100) + (ext.empty() ? wxString() : "." + ext.Left(10));
  }
  return out;
}

ObservationStore::ObservationStore(const wxString& dir) : dir_(dir) {}

wxString ObservationStore::LogPath() const {
  return wxFileName(dir_, kLogName).GetFullPath();
}

wxString ObservationStore::MediaDir() const {
  wxFileName fn = wxFileName::DirName(dir_);
  fn.AppendDir(kMediaDirName);
  return fn.GetPath();
}

bool ObservationStore::EnsureDir(wxString* error) const {
  if (wxFileName::DirExists(dir_)) return true;
  if (!wxFileName::Mkdir(dir_, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
    *error = wxString::Format("Cannot create the log folder %s", dir_);
    return false;
  }
  return true;
}

bool ObservationStore::Load(wxString* error) {
  items_.clear();
  header_current_ = true;
  utf8_ = true;
  skipped_rows_ = 0;
  unknown_columns_.clear();
  const wxString path = LogPath();
  if (!wxFileName::FileExists(path)) return true;

  wxString text;
  if (!ReadText(path, &text, &utf8_, error)) return false;
  const std::vector<CsvRow> rows = CsvParse(text);
  if (rows.empty()) {
    header_current_ = false;  // an empty file needs its header written
    return true;
  }

  const CsvRow& header = rows[0];
  if (std::find(header.begin(), header.end(), "id") == header.end()) {
    // Not a log this version can read at all; refuse to touch it.
    header_current_ = false;
    skipped_rows_ = static_cast<int>(rows.size());
    *error = wxString::Format("%s has no header row", path);
    return false;
  }
  header_current_ = header == ObservationColumns();
  const CsvRow& known = ObservationColumns();
  for (const wxString& column : header) {
    if (std::find(known.begin(), known.end(), column) == known.end() &&
        unknown_columns_.Index(column) == wxNOT_FOUND)
      unknown_columns_.Add(column);
  }
  for (size_t i = 1; i < rows.size(); ++i) {
    Observation obs;
    if (rows[i].size() > header.size() ||
        !ObservationFromRow(header, rows[i], &obs) || Find(obs.id)) {
      ++skipped_rows_;
      continue;
    }
    items_.push_back(obs);
  }
  return true;
}

wxString ObservationStore::LoadProblems() const {
  wxString out;
  if (skipped_rows_ > 0)
    out = wxString::Format("%d rows could not be read", skipped_rows_);
  if (!unknown_columns_.empty()) {
    if (!out.empty()) out += "; ";
    out += "unknown columns: " + wxJoin(unknown_columns_, ',', '\0');
  }
  if (!utf8_) {
    if (!out.empty()) out += "; ";
    out += "not UTF-8, read as Windows-1252";
  }
  return out;
}

wxString ObservationStore::RewriteBlocker() const {
  if (skipped_rows_ == 0 && unknown_columns_.empty()) return wxString();
  return wxString::Format(
      "%s has %s. Observer will not rewrite it and lose them; fix the file "
      "or move it out of the log folder.",
      LogPath(),
      skipped_rows_ > 0
          ? wxString::Format("%d rows it cannot read", skipped_rows_)
          : "columns it does not know (" +
                wxJoin(unknown_columns_, ',', '\0') + ")");
}

const Observation* ObservationStore::Find(const wxString& id) const {
  for (const Observation& o : items_)
    if (o.id == id) return &o;
  return nullptr;
}

bool ObservationStore::Add(const Observation& obs, wxString* error) {
  if (Find(obs.id)) {
    *error = wxString::Format("A sighting with id %s is already logged",
                              obs.id);
    return false;
  }
  if (!EnsureDir(error)) return false;
  items_.push_back(obs);

  const wxString path = LogPath();
  const bool exists = wxFileName::FileExists(path);
  if (!exists || !header_current_ || !utf8_) {
    // A new file, or one in an older format that has to be rewritten.
    const wxString blocker = exists ? RewriteBlocker() : wxString();
    if (!blocker.empty()) {
      *error = blocker;
      items_.pop_back();
      return false;
    }
    if (exists && (!header_current_ || !utf8_)) {
      // Keep the file as it was before converting it.
      const wxString stamp = wxDateTime::Now().Format("%Y%m%d-%H%M%S");
      wxCopyFile(path, path + ".before-" + stamp, false);
    }
    if (WriteAll(error)) return true;
    items_.pop_back();
    return false;
  }

  wxString text;
  if (!EndsWithNewline(path)) text += "\r\n";
  text += CsvFormatRow(ObservationToRow(obs));
  wxFFile f;
  if (!f.Open(path, "ab") || !f.Write(text, wxConvUTF8) || !f.Flush()) {
    *error = wxString::Format("Cannot write to %s", path);
    items_.pop_back();
    return false;
  }
  return true;
}

bool ObservationStore::Update(const Observation& obs, wxString* error) {
  const wxString blocker = RewriteBlocker();
  if (!blocker.empty()) {
    *error = blocker;
    return false;
  }
  auto it = std::find_if(items_.begin(), items_.end(),
                         [&](const Observation& o) { return o.id == obs.id; });
  if (it == items_.end()) {
    *error = wxString::Format("No sighting with id %s", obs.id);
    return false;
  }
  const Observation previous = *it;
  *it = obs;
  if (WriteAll(error)) return true;
  *it = previous;
  return false;
}

bool ObservationStore::Remove(const wxString& id, wxString* error) {
  const wxString blocker = RewriteBlocker();
  if (!blocker.empty()) {
    *error = blocker;
    return false;
  }
  auto it = std::find_if(items_.begin(), items_.end(),
                         [&](const Observation& o) { return o.id == id; });
  if (it == items_.end()) {
    *error = wxString::Format("No sighting with id %s", id);
    return false;
  }
  const Observation previous = *it;
  const auto index = it - items_.begin();
  items_.erase(it);
  if (WriteAll(error)) return true;
  items_.insert(items_.begin() + index, previous);
  return false;
}

bool ObservationStore::WriteAll(wxString* error) {
  if (!EnsureDir(error)) return false;
  const wxString path = LogPath();
  const wxString tmp = path + ".tmp";

  wxString text = Header();
  for (const Observation& o : items_) text += CsvFormatRow(ObservationToRow(o));

  {
    wxFFile f;
    if (!f.Open(tmp, "wb") || !f.Write(text, wxConvUTF8) || !f.Flush()) {
      *error = wxString::Format("Cannot write %s", tmp);
      return false;
    }
  }
  if (wxFileName::FileExists(path)) wxCopyFile(path, path + ".bak", true);
  if (!wxRenameFile(tmp, path, true)) {
    *error = wxString::Format("Cannot replace %s", path);
    return false;
  }
  header_current_ = true;
  utf8_ = true;
  return true;
}

bool ObservationStore::ImportMedia(const wxString& id, const wxString& source,
                                   wxString* stored, wxString* error) {
  if (!wxFileName::FileExists(source)) {
    *error = wxString::Format("%s does not exist", source);
    return false;
  }
  if (!EnsureDir(error)) return false;
  const wxString media_dir = MediaDir();
  if (!wxFileName::DirExists(media_dir) &&
      !wxFileName::Mkdir(media_dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
    *error = wxString::Format("Cannot create %s", media_dir);
    return false;
  }

  const wxFileName src(source);
  const wxString base = SanitizeFileName(id + "-" + src.GetName());
  const wxString ext = SanitizeFileName(src.GetExt());
  wxString name = src.GetExt().empty() ? base : base + "." + ext;
  for (int n = 2; wxFileName::FileExists(wxFileName(media_dir, name).GetFullPath());
       ++n) {
    name = wxString::Format("%s-%d", base, n) +
           (src.GetExt().empty() ? wxString() : "." + ext);
  }
  const wxString dest = wxFileName(media_dir, name).GetFullPath();
  if (!wxCopyFile(source, dest, false)) {
    *error = wxString::Format("Cannot copy %s to %s", source, dest);
    return false;
  }
  *stored = wxString(kMediaDirName) + "/" + name;
  return true;
}

wxString ObservationStore::MediaPath(const wxString& media) const {
  if (media.empty()) return wxString();
  if (wxFileName(media).IsAbsolute()) return media;
  wxString native = media;
  native.Replace("/", wxFileName::GetPathSeparator());
  wxString base = dir_;
  if (!base.EndsWith(wxFileName::GetPathSeparator()))
    base += wxFileName::GetPathSeparator();
  return wxFileName(base + native).GetFullPath();
}

bool ObservationStore::DeleteMedia(const wxString& media) {
  if (!media.StartsWith(wxString(kMediaDirName) + "/")) return false;
  if (media.Contains("..")) return false;
  const wxString path = MediaPath(media);
  return wxFileName::FileExists(path) && wxRemoveFile(path);
}

}  // namespace observer
