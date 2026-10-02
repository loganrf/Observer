// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

// A stand-in for OpenCPN: implements the plugin API calls Observer makes,
// loads the built plugin with dlopen() and drives it.
//
//   stub_host <plugin.so> <data-dir> <work-dir> [mode]
//
// Modes:
//   smoke        load, Init, LateInit, feed data, DeInit (default)
//   save         log a sighting through the form; check the CSV and mark
//   log|list|prefs [day|dusk|night]
//                open that dialog and save a screenshot to
//                <work-dir>/<mode>-<scheme>.png (needs an X display and
//                ImageMagick's `import`)
//
// The smoke mode proves every symbol the plugin needs resolves against an
// OpenCPN-shaped host; CI runs it under xvfb-run.

#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include <set>

#include <wx/app.h>
#include <wx/bmpbndl.h>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/msgdlg.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include <cstdint>

#include "ocpn_plugin.h"

// ---- Host state -----------------------------------------------------------

namespace host {
wxFrame* frame = nullptr;
wxFileConfig* config = nullptr;
wxString data_dir;   // contains share/opencpn/plugins/observer_pi/data
wxString docs_dir;
int scheme = PI_GLOBAL_COLOR_SCHEME_DAY;
std::set<wxString> waypoints;
std::set<wxString> icons;
int next_id = 1;
}  // namespace host

// ---- Plugin base classes (OpenCPN defines these) --------------------------

opencpn_plugin::~opencpn_plugin() {}
int opencpn_plugin::Init() { return 0; }
bool opencpn_plugin::DeInit() { return true; }
int opencpn_plugin::GetAPIVersionMajor() { return 1; }
int opencpn_plugin::GetAPIVersionMinor() { return 18; }
int opencpn_plugin::GetPlugInVersionMajor() { return 0; }
int opencpn_plugin::GetPlugInVersionMinor() { return 0; }
wxBitmap* opencpn_plugin::GetPlugInBitmap() { return nullptr; }
wxString opencpn_plugin::GetCommonName() { return ""; }
wxString opencpn_plugin::GetShortDescription() { return ""; }
wxString opencpn_plugin::GetLongDescription() { return ""; }
void opencpn_plugin::SetDefaults() {}
int opencpn_plugin::GetToolbarToolCount() { return 0; }
int opencpn_plugin::GetToolboxPanelCount() { return 0; }
void opencpn_plugin::SetupToolboxPanel(int, wxNotebook*) {}
void opencpn_plugin::OnCloseToolboxPanel(int, int) {}
void opencpn_plugin::ShowPreferencesDialog(wxWindow*) {}
bool opencpn_plugin::RenderOverlay(wxMemoryDC*, PlugIn_ViewPort*) {
  return false;
}
void opencpn_plugin::SetCursorLatLon(double, double) {}
void opencpn_plugin::SetCurrentViewPort(PlugIn_ViewPort&) {}
void opencpn_plugin::SetPositionFix(PlugIn_Position_Fix&) {}
void opencpn_plugin::SetNMEASentence(wxString&) {}
void opencpn_plugin::SetAISSentence(wxString&) {}
void opencpn_plugin::ProcessParentResize(int, int) {}
void opencpn_plugin::SetColorScheme(PI_ColorScheme) {}
void opencpn_plugin::OnToolbarToolCallback(int) {}
void opencpn_plugin::OnContextMenuItemCallback(int) {}
void opencpn_plugin::UpdateAuiStatus() {}
wxArrayString opencpn_plugin::GetDynamicChartClassNameArray() {
  return wxArrayString();
}

opencpn_plugin_18::opencpn_plugin_18(void* p) : opencpn_plugin(p) {}
opencpn_plugin_18::~opencpn_plugin_18() {}
bool opencpn_plugin_18::RenderOverlay(wxDC&, PlugIn_ViewPort*) { return false; }
bool opencpn_plugin_18::RenderGLOverlay(wxGLContext*, PlugIn_ViewPort*) {
  return false;
}
void opencpn_plugin_18::SetPluginMessage(wxString&, wxString&) {}
void opencpn_plugin_18::SetPositionFixEx(PlugIn_Position_Fix_Ex&) {}

opencpn_plugin_19::opencpn_plugin_19(void* p) : opencpn_plugin_18(p) {}
opencpn_plugin_19::~opencpn_plugin_19() {}
void opencpn_plugin_19::OnSetupOptions() {}

opencpn_plugin_110::opencpn_plugin_110(void* p) : opencpn_plugin_19(p) {}
opencpn_plugin_110::~opencpn_plugin_110() {}
void opencpn_plugin_110::LateInit() {}

opencpn_plugin_111::opencpn_plugin_111(void* p) : opencpn_plugin_110(p) {}
opencpn_plugin_111::~opencpn_plugin_111() {}

opencpn_plugin_112::opencpn_plugin_112(void* p) : opencpn_plugin_111(p) {}
opencpn_plugin_112::~opencpn_plugin_112() {}
bool opencpn_plugin_112::MouseEventHook(wxMouseEvent&) { return false; }
void opencpn_plugin_112::SendVectorChartObjectInfo(wxString&, wxString&,
                                                   wxString&, double, double,
                                                   double, int) {}

opencpn_plugin_113::opencpn_plugin_113(void* p) : opencpn_plugin_112(p) {}
opencpn_plugin_113::~opencpn_plugin_113() {}
bool opencpn_plugin_113::KeyboardEventHook(wxKeyEvent&) { return false; }
void opencpn_plugin_113::OnToolbarToolDownCallback(int) {}
void opencpn_plugin_113::OnToolbarToolUpCallback(int) {}

opencpn_plugin_114::opencpn_plugin_114(void* p) : opencpn_plugin_113(p) {}
opencpn_plugin_114::~opencpn_plugin_114() {}

opencpn_plugin_115::opencpn_plugin_115(void* p) : opencpn_plugin_114(p) {}
opencpn_plugin_115::~opencpn_plugin_115() {}

opencpn_plugin_116::opencpn_plugin_116(void* p) : opencpn_plugin_115(p) {}
opencpn_plugin_116::~opencpn_plugin_116() {}
bool opencpn_plugin_116::RenderGLOverlayMultiCanvas(wxGLContext*,
                                                    PlugIn_ViewPort*, int) {
  return false;
}
bool opencpn_plugin_116::RenderOverlayMultiCanvas(wxDC&, PlugIn_ViewPort*,
                                                  int) {
  return false;
}
void opencpn_plugin_116::PrepareContextMenu(int) {}

opencpn_plugin_117::opencpn_plugin_117(void* p) : opencpn_plugin_116(p) {}
int opencpn_plugin_117::GetPlugInVersionPatch() { return 0; }
int opencpn_plugin_117::GetPlugInVersionPost() { return 0; }
const char* opencpn_plugin_117::GetPlugInVersionPre() { return ""; }
const char* opencpn_plugin_117::GetPlugInVersionBuild() { return ""; }
void opencpn_plugin_117::SetActiveLegInfo(Plugin_Active_Leg_Info&) {}

opencpn_plugin_118::opencpn_plugin_118(void* p) : opencpn_plugin_117(p) {}
bool opencpn_plugin_118::RenderGLOverlayMultiCanvas(wxGLContext*,
                                                    PlugIn_ViewPort*, int,
                                                    int) {
  return false;
}
bool opencpn_plugin_118::RenderOverlayMultiCanvas(wxDC&, PlugIn_ViewPort*, int,
                                                  int) {
  return false;
}

PlugIn_Waypoint_Ex::PlugIn_Waypoint_Ex(double lat, double lon,
                                       const wxString& icon_ident,
                                       const wxString& wp_name,
                                       const wxString& GUID, const double,
                                       const bool, const int, const double,
                                       const wxColor) {
  InitDefaults();
  m_lat = lat;
  m_lon = lon;
  IconName = icon_ident;
  m_MarkName = wp_name;
  m_GUID = GUID;
}
PlugIn_Waypoint_Ex::~PlugIn_Waypoint_Ex() {}
void PlugIn_Waypoint_Ex::InitDefaults() {
  m_HyperlinkList = nullptr;
  IsVisible = true;
  IsNameVisible = false;
}

// ---- API functions --------------------------------------------------------

int InsertPlugInToolSVG(wxString, wxString svg, wxString, wxString, wxItemKind,
                        wxString, wxString, wxObject*, int, int,
                        opencpn_plugin*) {
  if (!wxFileName::FileExists(svg)) {
    std::fprintf(stderr, "toolbar icon missing: %s\n", svg.utf8_str().data());
    std::exit(3);
  }
  return host::next_id++;
}
void RemovePlugInTool(int) {}
int AddCanvasContextMenuItem(wxMenuItem* item, opencpn_plugin*) {
  delete item;
  return host::next_id++;
}
void RemoveCanvasContextMenuItem(int) {}
wxFileConfig* GetOCPNConfigObject() { return host::config; }
wxWindow* GetOCPNCanvasWindow() { return host::frame; }
wxWindow* PluginGetFocusCanvas() { return host::frame; }
bool AddLocaleCatalog(wxString) { return true; }
void JumpToPosition(double, double, double) {}
int GetLatLonFormat() { return 0; }
wxString GetWritableDocumentsDir() { return host::docs_dir; }
double toUsrSpeed_Plugin(double v, int) { return v; }
double toUsrTemp_Plugin(double v, int) { return v; }
wxString getUsrSpeedUnit_Plugin(int) { return "kn"; }
wxString getUsrTempUnit_Plugin(int) { return wxString(L"\u00B0C"); }

wxString GetPluginDataDir(const char* name) {
  wxFileName fn = wxFileName::DirName(host::data_dir);
  fn.AppendDir(name);
  return fn.GetPath();
}

wxBitmap GetBitmapFromSVGFile(wxString filename, unsigned int w,
                              unsigned int h) {
  return wxBitmapBundle::FromSVGFile(filename, wxSize(w, h))
      .GetBitmap(wxSize(w, h));
}

bool AddCustomWaypointIcon(wxBitmap* image, wxString key, wxString) {
  if (!image || !image->IsOk()) return false;
  host::icons.insert(key);
  return true;
}

bool AddSingleWaypointEx(PlugIn_Waypoint_Ex* wp, bool permanent) {
  if (permanent || host::waypoints.count(wp->m_GUID)) return false;
  if (host::icons.count(wp->IconName) == 0) {
    std::fprintf(stderr, "unknown waypoint icon %s\n",
                 wp->IconName.utf8_str().data());
  }
  host::waypoints.insert(wp->m_GUID);
  return true;
}

bool DeleteSingleWaypoint(wxString& guid) {
  return host::waypoints.erase(guid) > 0;
}

int OCPNMessageBox_PlugIn(wxWindow* parent, const wxString& message,
                          const wxString& caption, int style, int, int) {
  std::fprintf(stderr, "message box: %s\n", message.utf8_str().data());
  return (style & wxYES_NO) ? wxID_YES : wxID_OK;
}

int PlatformDirSelectorDialog(wxWindow*, wxString*, wxString, wxString) {
  return wxID_CANCEL;
}

int PlatformFileSelectorDialog(wxWindow*, wxString*, wxString, wxString,
                               wxString, wxString) {
  return wxID_CANCEL;
}

// A rough stand-in for OpenCPN's dusk and night control colours.
void DimeWindow(wxWindow* win) {
  if (!win) return;
  if (host::scheme == PI_GLOBAL_COLOR_SCHEME_DUSK ||
      host::scheme == PI_GLOBAL_COLOR_SCHEME_NIGHT) {
    const bool night = host::scheme == PI_GLOBAL_COLOR_SCHEME_NIGHT;
    win->SetBackgroundColour(night ? wxColour(20, 20, 20)
                                   : wxColour(64, 64, 64));
    win->SetForegroundColour(night ? wxColour(140, 30, 20)
                                   : wxColour(200, 200, 200));
  }
  for (wxWindow* child : win->GetChildren()) DimeWindow(child);
}

// ---- Driver ---------------------------------------------------------------

namespace {

void WriteFixtureLog(const wxString& dir) {
  wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  wxFFile f(wxFileName(dir, "observations.csv").GetFullPath(), "wb");
  f.Write(wxString::FromUTF8(
      "id,utc_time,local_time,species,category,count,latitude,longitude,"
      "media,description\r\n"
      "20261001T091500Z-0001,2026-10-01T09:15:00Z,2026-10-01T02:15:00-07:00,"
      "Humpback whale,Whale,2,47.6815,-122.4194,media/a.jpg,Mother and calf\r\n"
      "20261001T112000Z-0002,2026-10-01T11:20:00Z,2026-10-01T04:20:00-07:00,"
      "Dall's porpoise,Porpoise,6,47.7012,-122.4410,,Bow-riding\r\n"
      "20261002T070500Z-0003,2026-10-02T07:05:00Z,2026-10-02T00:05:00-07:00,"
      "Rhinoceros auklet,Seabird,14,47.7300,-122.4700,,Rafting\r\n"));
}

PlugIn_Position_Fix_Ex Fix() {
  PlugIn_Position_Fix_Ex fix{};
  fix.Lat = 47.6815;
  fix.Lon = -122.4194;
  fix.Cog = 182.5;
  fix.Sog = 5.2;
  fix.Var = 15.0;
  fix.Hdm = 165.0;
  fix.Hdt = std::nan("");
  fix.FixTime = std::time(nullptr) - 1;
  fix.nSats = 9;
  return fix;
}

wxString WithChecksum(const wxString& body) {
  unsigned sum = 0;
  for (size_t i = 1; i < body.length(); ++i) sum ^= body[i].GetValue() & 0xFF;
  return body + wxString::Format("*%02X", sum);
}

void Feed(opencpn_plugin_118* p) {
  PlugIn_Position_Fix_Ex fix = Fix();
  p->SetPositionFixEx(fix);
  for (const char* s : {"$SDDPT,42.5,0.0,", "$YXMTW,12.3,C",
                        "$WIMWD,270.0,T,255.0,M,14.0,N,7.2,M"}) {
    wxString sentence = WithChecksum(s);
    p->SetNMEASentence(sentence);
  }
}

class HostApp : public wxApp {
public:
  bool OnInit() override;
  int OnRun() override {
    const int rc = wxApp::OnRun();
    return exit_code_ != 0 ? exit_code_ : rc;
  }
  int OnExit() override;

private:
  void Shoot();
  void Save();
  void Finish(int code);

  wxString log_dir_;
  void* handle_ = nullptr;
  opencpn_plugin_118* plugin_ = nullptr;
  destroy_t* destroy_ = nullptr;
  wxString mode_;
  wxString shot_;
  wxTimer timer_;
  int exit_code_ = 0;
};

bool HostApp::OnInit() {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: stub_host <plugin.so> <data-dir> <work-dir> [mode] "
                 "[day|dusk|night]\n");
    return false;
  }
  const wxString lib = argv[1];
  host::data_dir = argv[2];
  const wxString work = argv[3];
  mode_ = argc > 4 ? argv[4] : wxString("smoke");
  const wxString scheme = argc > 5 ? argv[5] : wxString("day");
  shot_ = wxFileName(work, mode_ + "-" + scheme + ".png").GetFullPath();

  wxFileName::Mkdir(work, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  host::docs_dir = work;
  host::config = new wxFileConfig(
      "observer-host", "", wxFileName(work, "opencpn.conf").GetFullPath(), "",
      wxCONFIG_USE_LOCAL_FILE);
  const wxString log_dir = wxFileName(work, "log").GetFullPath();
  log_dir_ = log_dir;
  host::config->Write("/PlugIns/Observer/LogDir", log_dir);
  host::config->Write("/PlugIns/Observer/Observer", "Logan");
  host::config->Write("/PlugIns/Observer/LastCategory", "Whale");
  host::config->Write("/PlugIns/Observer/RecentSpecies",
                      "Humpback whale;Orca;Dall's porpoise");
  if (mode_ == "list") WriteFixtureLog(log_dir);

  host::frame = new wxFrame(nullptr, wxID_ANY, "OpenCPN stub host",
                            wxPoint(0, 0), wxSize(1100, 760));
  host::frame->SetBackgroundColour(wxColour("#cfd8dc"));
  host::frame->Show();

  handle_ = dlopen(lib.utf8_str().data(), RTLD_NOW | RTLD_GLOBAL);
  if (!handle_) {
    std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
    std::exit(2);
  }
  auto* create = reinterpret_cast<create_t*>(dlsym(handle_, "create_pi"));
  destroy_ = reinterpret_cast<destroy_t*>(dlsym(handle_, "destroy_pi"));
  if (!create || !destroy_) {
    std::fprintf(stderr, "create_pi/destroy_pi not exported\n");
    std::exit(2);
  }
  plugin_ = dynamic_cast<opencpn_plugin_118*>(create(nullptr));
  if (!plugin_) {
    std::fprintf(stderr, "plugin is not an opencpn_plugin_118\n");
    std::exit(2);
  }
  std::printf("loaded %s %d.%d.%d API %d.%d\n",
              plugin_->GetCommonName().utf8_str().data(),
              plugin_->GetPlugInVersionMajor(),
              plugin_->GetPlugInVersionMinor(),
              plugin_->GetPlugInVersionPatch(), plugin_->GetAPIVersionMajor(),
              plugin_->GetAPIVersionMinor());
  if (!plugin_->GetPlugInBitmap() || !plugin_->GetPlugInBitmap()->IsOk()) {
    std::fprintf(stderr, "no plugin bitmap\n");
    std::exit(3);
  }

  const int caps = plugin_->Init();
  if (caps & WANTS_LATE_INIT) plugin_->LateInit();
  host::scheme = scheme == "night"  ? PI_GLOBAL_COLOR_SCHEME_NIGHT
                 : scheme == "dusk" ? PI_GLOBAL_COLOR_SCHEME_DUSK
                                    : PI_GLOBAL_COLOR_SCHEME_DAY;
  plugin_->SetColorScheme(static_cast<PI_ColorScheme>(host::scheme));
  Feed(plugin_);
  std::printf("chart marks after LateInit: %zu\n", host::waypoints.size());

  if (mode_ == "smoke") {
    CallAfter([this]() { Finish(0); });
    return true;
  }

  timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
    if (mode_ == "save")
      Save();
    else
      Shoot();
  });
  timer_.StartOnce(1200);
  CallAfter([this]() {
    if (mode_ == "log" || mode_ == "save") {
      plugin_->OnToolbarToolCallback(1);
    } else if (mode_ == "list") {
      plugin_->OnContextMenuItemCallback(3);  // second context menu item
    } else if (mode_ == "prefs") {
      plugin_->ShowPreferencesDialog(host::frame);
    }
  });
  return true;
}

void HostApp::Shoot() {
  wxExecute("import -window root \"" + shot_ + "\"", wxEXEC_SYNC);
  std::printf("screenshot %s\n", shot_.utf8_str().data());
  for (wxWindow* w : wxTopLevelWindows) {
    auto* dlg = wxDynamicCast(w, wxDialog);
    if (dlg && dlg->IsModal()) dlg->EndModal(wxID_CANCEL);
  }
  CallAfter([this]() { Finish(0); });
}

void HostApp::Save() {
  wxDialog* form = nullptr;
  for (wxWindow* w : wxTopLevelWindows) {
    auto* dlg = wxDynamicCast(w, wxDialog);
    if (dlg && dlg->IsModal()) form = dlg;
  }
  if (!form) {
    std::fprintf(stderr, "no sighting form open\n");
    CallAfter([this]() { Finish(5); });
    return;
  }
  // The first combo box on the form is the species.
  wxWindowList pending(form->GetChildren());
  for (auto it = pending.begin(); it != pending.end(); ++it) {
    if (auto* combo = wxDynamicCast(*it, wxComboBox)) {
      combo->SetValue("Orca");
      break;
    }
  }
  wxCommandEvent click(wxEVT_BUTTON, wxID_OK);
  click.SetEventObject(form->FindWindow(wxID_OK));
  form->FindWindow(wxID_OK)->GetEventHandler()->ProcessEvent(click);

  CallAfter([this]() {
    wxFFile f(wxFileName(log_dir_, "observations.csv").GetFullPath(), "rb");
    wxString text;
    if (!f.IsOpened() || !f.ReadAll(&text, wxConvUTF8)) {
      std::fprintf(stderr, "observations.csv was not written\n");
      Finish(6);
      return;
    }
    const bool row_ok = text.Contains(",Orca,") && text.Contains("47.681500") &&
                        text.Contains(",Whale,") && text.Contains(",gnss,") &&
                        text.Contains(",42.5,12.3,14.0,270,Logan,");
    std::printf("logged sighting: %s\n", row_ok ? "ok" : "unexpected row");
    if (!row_ok) std::fprintf(stderr, "%s\n", text.utf8_str().data());
    const bool mark_ok = host::waypoints.size() == 1;
    std::printf("chart marks after save: %zu\n", host::waypoints.size());
    Finish(row_ok && mark_ok ? 0 : 7);
  });
}

void HostApp::Finish(int code) {
  exit_code_ = code;
  timer_.Stop();
  if (plugin_) {
    plugin_->DeInit();
    destroy_(plugin_);
    plugin_ = nullptr;
  }
  std::printf("chart marks after DeInit: %zu\n", host::waypoints.size());
  if (!host::waypoints.empty() && exit_code_ == 0) exit_code_ = 4;
  host::frame->Destroy();
}

int HostApp::OnExit() {
  delete host::config;
  host::config = nullptr;
  return exit_code_;
}

}  // namespace

wxIMPLEMENT_APP(HostApp);
