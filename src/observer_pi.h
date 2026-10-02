// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_PI_H
#define OBSERVER_PI_H

#include <cstdint>
#include <memory>
#include <random>

#include <wx/bitmap.h>
#include <wx/string.h>
#include <wx/weakref.h>

#include "ocpn_api.h"

#include "brand.h"
#include "chart_marks.h"
#include "list_dialog.h"
#include "nmea.h"
#include "settings.h"
#include "store.h"

namespace observer {

/** The OpenCPN plugin: toolbar button, context menu, hotkey and dialogs. */
class ObserverPlugin : public opencpn_plugin_118, public SightingController {
public:
  explicit ObserverPlugin(void* ppimgr);
  ~ObserverPlugin() override;

  // opencpn_plugin
  int Init() override;
  bool DeInit() override;
  void LateInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;
  int GetPlugInVersionPost() override;
  const char* GetPlugInVersionPre() override;
  const char* GetPlugInVersionBuild() override;
  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

  int GetToolbarToolCount() override;
  void OnToolbarToolCallback(int id) override;
  void OnContextMenuItemCallback(int id) override;
  void PrepareContextMenu(int canvas_index) override;
  void ShowPreferencesDialog(wxWindow* parent) override;

  void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) override;
  void SetNMEASentence(wxString& sentence) override;
  void SetCursorLatLon(double lat, double lon) override;
  void SetCurrentViewPort(PlugIn_ViewPort& vp) override;
  void SetColorScheme(PI_ColorScheme cs) override;
  bool KeyboardEventHook(wxKeyEvent& event) override;

  // SightingController
  const ObservationStore& Store() const override { return *store_; }
  void NewSighting(wxWindow* parent) override;
  void EditSighting(wxWindow* parent, const wxString& id) override;
  void DeleteSighting(wxWindow* parent, const wxString& id) override;
  void ShowOnChart(const wxString& id) override;
  brand::Theme CurrentTheme() const override;

private:
  /** Time, position and instruments right now. */
  Observation Capture(bool at_cursor);

  void LogSighting(wxWindow* parent, bool at_cursor);
  bool SaveDialogResult(wxWindow* parent, Observation obs,
                        const wxString& new_media, bool media_removed,
                        bool editing);
  void ShowLog();
  void OpenStore();
  wxWindow* Parent() const;
  wxString DataPath(const wxString& file) const;
  wxString DefaultLogDir() const;
  void ShowError(wxWindow* parent, const wxString& message);

  Settings settings_;
  std::unique_ptr<ObservationStore> store_;
  ChartMarks marks_;
  NmeaTracker nmea_;
  // Cleared by wx if the dialog goes away with its parent.
  wxWeakRef<ListDialog> list_;

  int tool_id_ = -1;
  int menu_log_here_ = -1;
  int menu_show_log_ = -1;
  bool late_init_done_ = false;
  int color_scheme_ = PI_GLOBAL_COLOR_SCHEME_DAY;

  PlugIn_Position_Fix_Ex fix_{};
  bool have_fix_ = false;
  double cursor_lat_, cursor_lon_;  // NaN until the mouse is on the chart
  double menu_lat_, menu_lon_;      // where the context menu was opened
  double view_scale_ppm_ = 0;       // current chart scale, pixels per metre

  std::unique_ptr<wxBitmap> plugin_bitmap_;
  std::mt19937 rng_;
};

}  // namespace observer

#endif  // OBSERVER_PI_H
