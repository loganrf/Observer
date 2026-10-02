// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include "store.h"
#include "testing.h"
#include "time_util.h"

using namespace observer;

namespace {

/** A fresh, empty temporary directory removed when the test ends. */
class TempDir {
public:
  TempDir() {
    path_ = wxFileName::CreateTempFileName("observer-test");
    wxRemoveFile(path_);
    wxFileName::Mkdir(path_, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  }
  ~TempDir() { wxFileName::Rmdir(path_, wxPATH_RMDIR_RECURSIVE); }
  const wxString& Path() const { return path_; }
  wxString File(const wxString& name) const {
    return wxFileName(path_, name).GetFullPath();
  }

private:
  wxString path_;
};

Observation Make(int64_t utc, const wxString& species) {
  Observation o;
  o.utc = utc;
  o.id = MakeObservationId(utc, static_cast<uint32_t>(utc));
  o.species = species;
  o.lat = 47.5;
  o.lon = -122.5;
  return o;
}

wxString ReadText(const wxString& path) {
  wxFFile f(path, "rb");
  wxString text;
  f.ReadAll(&text, wxConvUTF8);
  return text;
}

void WriteText(const wxString& path, const wxString& text) {
  wxFFile f(path, "wb");
  f.Write(text, wxConvUTF8);
}

}  // namespace

TEST(store_missing_log_is_empty) {
  TempDir dir;
  ObservationStore store(dir.File("log"));
  wxString error;
  CHECK(store.Load(&error));
  CHECK(store.All().empty());
}

TEST(store_add_appends_and_reloads) {
  TempDir dir;
  const wxString log_dir = dir.File("nested");  // created on first write
  wxString error;
  {
    ObservationStore store(log_dir);
    CHECK(store.Load(&error));
    CHECK(store.Add(Make(1790949807, "Orca"), &error));
    CHECK(store.Add(Make(1790949907, wxString::FromUTF8("Fou de Bassan \xC3\xA9")),
                    &error));
    CHECK(!store.Add(Make(1790949807, "Duplicate id"), &error));
  }
  ObservationStore reread(log_dir);
  CHECK(reread.Load(&error));
  CHECK_EQ(reread.All().size(), 2u);
  CHECK_EQ(reread.All()[1].species,
           wxString::FromUTF8("Fou de Bassan \xC3\xA9"));
  // One header, BOM first, CRLF records.
  const wxString text = ReadText(reread.LogPath());
  CHECK(text.StartsWith(wxString(wxUniChar(0xFEFF)) + "id,utc_time,"));
  CHECK_EQ(text.Freq('\n'), 3);
}

TEST(store_update_and_remove_keep_backup) {
  TempDir dir;
  wxString error;
  ObservationStore store(dir.Path());
  CHECK(store.Add(Make(1790949807, "Orca"), &error));
  Observation o = Make(1790949907, "Minke");
  CHECK(store.Add(o, &error));

  o.species = "Minke whale";
  o.count = 3;
  CHECK(store.Update(o, &error));
  CHECK(wxFileName::FileExists(store.LogPath() + ".bak"));
  CHECK(store.Remove(store.All()[0].id, &error));
  CHECK(!store.Remove("no-such-id", &error));

  ObservationStore reread(dir.Path());
  CHECK(reread.Load(&error));
  CHECK_EQ(reread.All().size(), 1u);
  CHECK_EQ(reread.All()[0].species, "Minke whale");
  CHECK_EQ(reread.All()[0].count, 3);
}

TEST(store_rewrites_old_header_before_appending) {
  TempDir dir;
  ObservationStore store(dir.Path());
  // A log from a hypothetical older version with fewer columns.
  WriteText(store.LogPath(),
            "id,utc_time,species\r\nold-1,2026-09-01T10:00:00Z,Seal\r\n");
  wxString error;
  CHECK(store.Load(&error));
  CHECK_EQ(store.All().size(), 1u);
  CHECK(store.Add(Make(1790949807, "Orca"), &error));

  ObservationStore reread(dir.Path());
  CHECK(reread.Load(&error));
  CHECK_EQ(reread.All().size(), 2u);
  CHECK_EQ(reread.All()[0].species, "Seal");
  CHECK(ReadText(reread.LogPath()).Contains("sighting_latitude"));
}

TEST(store_appends_after_missing_final_newline) {
  TempDir dir;
  ObservationStore store(dir.Path());
  wxString error;
  CHECK(store.Add(Make(1790949807, "Orca"), &error));
  wxString text = ReadText(store.LogPath());
  text.RemoveLast(2);  // strip the final CRLF, as a hand edit might
  WriteText(store.LogPath(), text);

  ObservationStore again(dir.Path());
  CHECK(again.Load(&error));
  CHECK(again.Add(Make(1790949907, "Minke"), &error));
  ObservationStore reread(dir.Path());
  CHECK(reread.Load(&error));
  CHECK_EQ(reread.All().size(), 2u);
}

TEST(store_import_media) {
  TempDir dir;
  const wxString src = dir.File("IMG 0042.JPG");
  WriteText(src, "jpeg bytes");
  ObservationStore store(dir.File("log"));
  wxString stored, error;
  CHECK(store.ImportMedia("20261002T140327Z-3f9a", src, &stored, &error));
  CHECK_EQ(stored, "media/20261002T140327Z-3f9a-IMG_0042.JPG");
  CHECK(wxFileName::FileExists(store.MediaPath(stored)));
  // A second import of the same name does not overwrite the first.
  wxString second;
  CHECK(store.ImportMedia("20261002T140327Z-3f9a", src, &second, &error));
  CHECK(second != stored);
  CHECK(store.DeleteMedia(second));
  CHECK(!wxFileName::FileExists(store.MediaPath(second)));
  CHECK(!store.DeleteMedia("../outside.jpg"));
}

TEST(store_media_path_absolute_passthrough) {
  ObservationStore store("/tmp/log");
  const wxString abs = wxFileName(wxFileName::GetTempDir(), "x.jpg").GetFullPath();
  CHECK_EQ(store.MediaPath(abs), abs);
  CHECK_EQ(store.MediaPath(""), "");
}

TEST(store_sanitize_file_name) {
  CHECK_EQ(SanitizeFileName("a/b\\c:d*e?.jpg"), "a_b_c_d_e_.jpg");
  CHECK_EQ(SanitizeFileName("..hidden"), "hidden");
  CHECK_EQ(SanitizeFileName(""), "media");
}
