// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "observer_pi.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/window.h>

#include "config.h"
#include "geo.h"
#include "log_dialog.h"
#include "prefs_dialog.h"
#include "time_util.h"

// OpenCPN loads the plugin through these two functions.
extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new observer::ObserverPlugin(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

namespace observer {

namespace {

// A GNSS fix older than this is not used as the sighting position.
constexpr int64_t kMaxFixAgeS = 60;
// Instrument values older than this are not recorded.
constexpr double kMaxInstrumentAgeS = 30;
// GNSS time is trusted for this long after the last RMC or ZDA.
constexpr double kMaxClockAgeS = 600;

wxString HotkeyLabel() {
#ifdef __WXOSX__
  return "Cmd+Shift+O";
#else
  return "Ctrl+Shift+O";
#endif
}

}  // namespace

ObserverPlugin::ObserverPlugin(void* ppimgr)
    : opencpn_plugin_118(ppimgr),
      cursor_lat_(NoValue()),
      cursor_lon_(NoValue()),
      menu_lat_(NoValue()),
      menu_lon_(NoValue()),
      rng_(std::random_device{}()) {}

ObserverPlugin::~ObserverPlugin() = default;

// ---- Identity -------------------------------------------------------------

int ObserverPlugin::GetAPIVersionMajor() { return OCPN_API_VERSION_MAJOR; }
int ObserverPlugin::GetAPIVersionMinor() { return OCPN_API_VERSION_MINOR; }
int ObserverPlugin::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int ObserverPlugin::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }
int ObserverPlugin::GetPlugInVersionPatch() { return PLUGIN_VERSION_PATCH; }
int ObserverPlugin::GetPlugInVersionPost() { return 0; }
const char* ObserverPlugin::GetPlugInVersionPre() { return PLUGIN_VERSION_PRE; }
const char* ObserverPlugin::GetPlugInVersionBuild() {
  return PLUGIN_VERSION_BUILD;
}

wxString ObserverPlugin::GetCommonName() { return PLUGIN_API_NAME; }

wxString ObserverPlugin::GetShortDescription() { return _(PKG_SUMMARY); }

wxString ObserverPlugin::GetLongDescription() {
  return wxString(_(PKG_DESCRIPTION)) + "\n\n" +
         wxString::Format(_("Click the binoculars on the toolbar, press %s, "
                            "or right-click the chart and choose \"Log "
                            "sighting here\"."),
                          HotkeyLabel());
}

wxBitmap* ObserverPlugin::GetPlugInBitmap() {
  if (!plugin_bitmap_) {
    // The plugin manager asks for this before Init(); resolve the path here.
    const wxString path = DataPath("observer_panel_icon.svg");
    wxBitmap bmp;
    if (wxFileName::FileExists(path)) bmp = GetBitmapFromSVGFile(path, 32, 32);
    plugin_bitmap_ = std::make_unique<wxBitmap>(
        bmp.IsOk() ? bmp : wxBitmap(32, 32));
  }
  return plugin_bitmap_.get();
}

wxString ObserverPlugin::DataPath(const wxString& file) const {
  wxString dir = GetPluginDataDir(PKG_NAME);
  if (dir.empty()) return wxString();
  wxFileName fn(dir, wxEmptyString);
  fn.AppendDir("data");
  fn.SetFullName(file);
  return fn.GetFullPath();
}

wxString ObserverPlugin::DefaultLogDir() const {
  wxFileName fn = wxFileName::DirName(GetWritableDocumentsDir());
  fn.AppendDir("Observer");
  return fn.GetPath();
}

// ---- Life cycle -----------------------------------------------------------

int ObserverPlugin::Init() {
  AddLocaleCatalog("opencpn-" PKG_NAME);

  settings_.Load(GetOCPNConfigObject(), DefaultLogDir());
  OpenStore();

  const wxString icon = DataPath("observer.svg");
  tool_id_ = InsertPlugInToolSVG(
      PLUGIN_API_NAME, icon, DataPath("observer_rollover.svg"),
      DataPath("observer_toggled.svg"), wxITEM_NORMAL, _("Log a sighting"),
      wxString::Format(_("Log a wildlife sighting (%s)"), HotkeyLabel()),
      nullptr, -1, 0, this);

  menu_log_here_ = AddCanvasContextMenuItem(
      new wxMenuItem(nullptr, wxID_ANY, _("Log sighting here")), this);
  menu_show_log_ = AddCanvasContextMenuItem(
      new wxMenuItem(nullptr, wxID_ANY, _("Sightings log")), this);

  return WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL | WANTS_PREFERENCES |
         WANTS_CONFIG | WANTS_NMEA_SENTENCES | WANTS_NMEA_EVENTS |
         WANTS_CURSOR_LATLON | INSTALLS_CONTEXTMENU_ITEMS | WANTS_LATE_INIT |
         WANTS_KEYBOARD_EVENTS | WANTS_ONPAINT_VIEWPORT;
}

void ObserverPlugin::LateInit() {
  // Waypoint support exists only from here on.
  late_init_done_ = true;
  marks_.RegisterIcon(DataPath("observer_mark.svg"));
  marks_.Sync(store_->All(), *store_, settings_.show_marks,
              settings_.show_mark_names);
}

bool ObserverPlugin::DeInit() {
  // Delete now, not with Destroy(): OpenCPN unloads the library straight
  // after DeInit, before wx would get round to a deferred deletion.
  if (list_) {
    ListDialog* list = list_.get();
    list_ = nullptr;
    delete list;
  }
  marks_.Clear();
  if (tool_id_ >= 0) RemovePlugInTool(tool_id_);
  if (menu_log_here_ >= 0) RemoveCanvasContextMenuItem(menu_log_here_);
  if (menu_show_log_ >= 0) RemoveCanvasContextMenuItem(menu_show_log_);
  tool_id_ = menu_log_here_ = menu_show_log_ = -1;
  settings_.Save(GetOCPNConfigObject());
  return true;
}

void ObserverPlugin::OpenStore() {
  store_ = std::make_unique<ObservationStore>(settings_.log_dir);
  wxString error;
  if (!store_->Load(&error)) {
    wxLogWarning("Observer: %s", error);
  } else if (!store_->LoadProblems().empty()) {
    wxLogWarning("Observer: %s: %s", store_->LogPath(),
                 store_->LoadProblems());
  }
}

// ---- Data from OpenCPN ----------------------------------------------------

void ObserverPlugin::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  fix_ = pfix;
  have_fix_ = true;
}

void ObserverPlugin::SetNMEASentence(wxString& sentence) {
  nmea_.Process(sentence, MonotonicSeconds(), NowEpoch());
}

void ObserverPlugin::SetCursorLatLon(double lat, double lon) {
  cursor_lat_ = lat;
  cursor_lon_ = lon;
}

void ObserverPlugin::SetCurrentViewPort(PlugIn_ViewPort& vp) {
  if (vp.bValid) view_scale_ppm_ = vp.view_scale_ppm;
}

void ObserverPlugin::PrepareContextMenu(int) {
  // The cursor is where the person right-clicked; it moves on to the menu.
  menu_lat_ = cursor_lat_;
  menu_lon_ = cursor_lon_;
}

void ObserverPlugin::SetColorScheme(PI_ColorScheme cs) {
  color_scheme_ = cs;
  if (list_) list_->ApplyTheme();
}

brand::Theme ObserverPlugin::CurrentTheme() const {
  return brand::ThemeForScheme(color_scheme_);
}

Observation ObserverPlugin::Capture(bool at_cursor) {
  Observation o;
  const int64_t now = NowEpoch();
  const double mono = MonotonicSeconds();

  // Prefer GNSS time when the computer clock disagrees with it, as it
  // often does on a boat far from a network. A GNSS date before this
  // release's year is wrong (a receiver hit by the week-number rollover,
  // or a replayed recording), so it is not trusted.
  const int64_t earliest = UtcToEpoch(PLUGIN_VERSION_MAJOR, 1, 1, 0, 0, 0);
  int64_t offset = 0;
  if (nmea_.ClockOffset(mono, kMaxClockAgeS, &offset) &&
      now + offset >= earliest) {
    o.utc = std::llabs(offset) >= 2 ? now + offset : now;
    o.time_source = "gnss";
  } else {
    o.utc = now;
    o.time_source = "system";
  }
  o.utc_offset_min = LocalOffsetMinutes(o.utc);
  o.id = MakeObservationId(o.utc, rng_());

  const bool fix_ok = have_fix_ && fix_.FixTime > 0 &&
                      IsValidPosition(fix_.Lat, fix_.Lon) &&
                      now - static_cast<int64_t>(fix_.FixTime) <= kMaxFixAgeS;
  if (fix_ok) {
    o.cog_deg = HasValue(fix_.Cog) ? fix_.Cog : NoValue();
    o.sog_kn = HasValue(fix_.Sog) ? fix_.Sog : NoValue();
    if (HasValue(fix_.Hdt)) {
      o.heading_deg = fix_.Hdt;
    } else if (HasValue(fix_.Hdm) && HasValue(fix_.Var)) {
      o.heading_deg = NormalizeDegrees(fix_.Hdm + fix_.Var);
    }
  }

  const bool menu_ok = IsValidPosition(menu_lat_, menu_lon_);
  const double click_lat = menu_ok ? menu_lat_ : cursor_lat_;
  const double click_lon = menu_ok ? menu_lon_ : cursor_lon_;
  if (at_cursor && IsValidPosition(click_lat, click_lon)) {
    o.lat = click_lat;
    o.lon = NormalizeLongitude(click_lon);
    o.position_source = "cursor";
  } else if (fix_ok) {
    o.lat = fix_.Lat;
    o.lon = NormalizeLongitude(fix_.Lon);
    o.position_source = "gnss";
    o.fix_age_s = static_cast<double>(
        std::max<int64_t>(0, now - static_cast<int64_t>(fix_.FixTime)));
  }

  const double heading_for_wind =
      HasValue(o.heading_deg) ? o.heading_deg : o.cog_deg;
  const Environment env =
      nmea_.Snapshot(mono, kMaxInstrumentAgeS, heading_for_wind, o.sog_kn);
  o.depth_m = env.depth_m;
  o.water_temp_c = env.water_temp_c;
  o.wind_speed_kn = env.wind_speed_kn;
  o.wind_dir_deg = env.wind_dir_deg;

  o.observer = settings_.observer;
  o.vessel = settings_.vessel;
  o.category = settings_.last_category;
  o.count = 1;
  return o;
}

// ---- Entry points ---------------------------------------------------------

int ObserverPlugin::GetToolbarToolCount() { return 1; }

void ObserverPlugin::OnToolbarToolCallback(int) {
  LogSighting(Parent(), false);
}

void ObserverPlugin::OnContextMenuItemCallback(int id) {
  if (id == menu_log_here_) {
    LogSighting(Parent(), true);
  } else if (id == menu_show_log_) {
    ShowLog();
  }
}

bool ObserverPlugin::KeyboardEventHook(wxKeyEvent& event) {
  if (!settings_.hotkey) return false;
  const bool chord = event.GetModifiers() == (wxMOD_CONTROL | wxMOD_SHIFT);
  if (!chord) return false;
  const int key = event.GetKeyCode();
  // Ctrl+O arrives as 15 in character events.
  if (key != 'O' && key != 'o' && key != 15) return false;
  if (event.GetEventType() == wxEVT_KEY_DOWN) {
    // Leave the key handler before opening a modal dialog.
    wxWindow* canvas = Parent();
    if (canvas) canvas->CallAfter([this]() { LogSighting(Parent(), false); });
  }
  return true;  // also swallows OpenCPN's own Ctrl+O
}

void ObserverPlugin::LogSighting(wxWindow* parent, bool at_cursor) {
  const Observation captured = Capture(at_cursor);
  LogDialog dlg(parent, captured, false, settings_, wxString(), CurrentTheme(),
                [this]() { return Capture(false); });
  if (dlg.ShowModal() != wxID_OK) return;
  SaveDialogResult(parent, dlg.Result(), dlg.NewMediaSource(), false, false);
}

void ObserverPlugin::NewSighting(wxWindow* parent) {
  LogSighting(parent, false);
}

void ObserverPlugin::EditSighting(wxWindow* parent, const wxString& id) {
  const Observation* existing = store_->Find(id);
  if (!existing) return;
  LogDialog dlg(parent, *existing, true, settings_,
                store_->MediaPath(existing->media), CurrentTheme());
  if (dlg.ShowModal() != wxID_OK) return;
  SaveDialogResult(parent, dlg.Result(), dlg.NewMediaSource(),
                   dlg.MediaRemoved(), true);
}

bool ObserverPlugin::SaveDialogResult(wxWindow* parent, Observation obs,
                                      const wxString& new_media,
                                      bool media_removed, bool editing) {
  wxString error;
  const wxString old_media = obs.media;
  if (!new_media.empty()) {
    if (settings_.copy_media) {
      wxString stored;
      if (!store_->ImportMedia(obs.id, new_media, &stored, &error)) {
        ShowError(parent, error);
        return false;
      }
      obs.media = stored;
    } else {
      obs.media = new_media;
    }
  } else if (media_removed) {
    obs.media.clear();
  }

  const bool ok = editing ? store_->Update(obs, &error)
                          : store_->Add(obs, &error);
  if (!ok) {
    if (obs.media != old_media) store_->DeleteMedia(obs.media);
    ShowError(parent, error);
    return false;
  }
  // A replaced or removed copy in media/ is no longer referenced.
  if (editing && obs.media != old_media) store_->DeleteMedia(old_media);

  settings_.RememberSpecies(obs.species);
  if (!editing) settings_.last_category = obs.category;
  settings_.Save(GetOCPNConfigObject());

  if (late_init_done_ && settings_.show_marks)
    marks_.Add(obs, *store_, settings_.show_mark_names);
  if (list_) list_->Reload();
  return true;
}

void ObserverPlugin::DeleteSighting(wxWindow* parent, const wxString& id) {
  const Observation* o = store_->Find(id);
  if (!o) return;
  const Observation copy = *o;
  wxString question =
      wxString::Format(_("Delete the sighting of %s at %s UTC?"), copy.Title(),
                       FormatTimeShort(copy.utc));
  const bool own_media = copy.media.StartsWith("media/");
  if (own_media) question += "\n\n" + _("Its photo or video in the log folder is deleted too.");
  if (OCPNMessageBox_PlugIn(parent, question, _("Observer"),
                            wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION) !=
      wxID_YES)
    return;

  wxString error;
  if (!store_->Remove(id, &error)) {
    ShowError(parent, error);
    return;
  }
  if (own_media) store_->DeleteMedia(copy.media);
  marks_.Remove(id);
  if (list_) list_->Reload();
}

void ObserverPlugin::ShowOnChart(const wxString& id) {
  const Observation* o = store_->Find(id);
  if (!o || !o->HasMarkPosition()) return;
  // Keep the current zoom, but no further out than about 1:200 000 so the
  // mark can be told from its neighbours.
  const double min_ppm = 0.02;
  const double scale =
      view_scale_ppm_ > 0 ? std::max(view_scale_ppm_, min_ppm) : min_ppm;
  JumpToPosition(o->MarkLat(), NormalizeLongitude(o->MarkLon()), scale);
}

void ObserverPlugin::ShowLog() {
  if (!list_) {
    // The primary canvas lives as long as OpenCPN; other canvases are
    // destroyed, with their child windows, when the layout changes.
    list_ = new ListDialog(GetOCPNCanvasWindow(), this);
  } else {
    list_->Reload();
    list_->ApplyTheme();
  }
  list_->Show();
  list_->Raise();
}

void ObserverPlugin::ShowPreferencesDialog(wxWindow* parent) {
  PrefsDialog dlg(parent, settings_, CurrentTheme());
  if (dlg.ShowModal() != wxID_OK) return;
  const Settings before = settings_;
  settings_ = dlg.Result();
  settings_.Save(GetOCPNConfigObject());

  if (settings_.log_dir != before.log_dir) OpenStore();
  if (late_init_done_) {
    marks_.Sync(store_->All(), *store_, settings_.show_marks,
                settings_.show_mark_names);
  }
  if (list_) list_->Reload();
}

// ---- Helpers --------------------------------------------------------------

wxWindow* ObserverPlugin::Parent() const {
  wxWindow* canvas = PluginGetFocusCanvas();
  return canvas ? canvas : GetOCPNCanvasWindow();
}

void ObserverPlugin::ShowError(wxWindow* parent, const wxString& message) {
  OCPNMessageBox_PlugIn(parent, message, _("Observer"), wxOK | wxICON_ERROR);
}

}  // namespace observer
