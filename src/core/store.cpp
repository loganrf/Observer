// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "store.h"

#include <algorithm>

#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>

namespace observer {

namespace {

const char* kLogName = "observations.csv";
const char* kMediaDirName = "media";

bool ReadUtf8(const wxString& path, wxString* text, wxString* error) {
  wxFFile f;
  if (!f.Open(path, "rb")) {
    *error = wxString::Format("Cannot open %s", path);
    return false;
  }
  if (!f.ReadAll(text, wxConvUTF8)) {
    *error = wxString::Format("Cannot read %s as UTF-8", path);
    return false;
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
  const wxString path = LogPath();
  if (!wxFileName::FileExists(path)) return true;

  wxString text;
  if (!ReadUtf8(path, &text, error)) return false;
  const std::vector<CsvRow> rows = CsvParse(text);
  if (rows.empty()) return true;

  const CsvRow& header = rows[0];
  if (std::find(header.begin(), header.end(), "id") == header.end()) {
    *error = wxString::Format("%s has no header row", path);
    return false;
  }
  header_current_ = header == ObservationColumns();
  for (size_t i = 1; i < rows.size(); ++i) {
    Observation obs;
    if (ObservationFromRow(header, rows[i], &obs) && !Find(obs.id))
      items_.push_back(obs);
  }
  return true;
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
  if (!header_current_ || !wxFileName::FileExists(path)) {
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
