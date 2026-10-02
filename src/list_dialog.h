// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_LIST_DIALOG_H
#define OBSERVER_LIST_DIALOG_H

#include <vector>

#include <wx/dialog.h>
#include <wx/srchctrl.h>

#include "brand.h"
#include "observation.h"

class wxButton;
class wxListCtrl;
class wxListEvent;
class wxStaticText;

namespace observer {

class ObservationStore;

/** What the log window asks the plugin to do. */
class SightingController {
public:
  virtual ~SightingController() = default;
  virtual const ObservationStore& Store() const = 0;
  virtual void NewSighting(wxWindow* parent) = 0;
  virtual void EditSighting(wxWindow* parent, const wxString& id) = 0;
  virtual void DeleteSighting(wxWindow* parent, const wxString& id) = 0;
  virtual void ShowOnChart(const wxString& id) = 0;
  virtual brand::Theme CurrentTheme() const = 0;
};

/** The sightings log: browse, search, edit, delete and export. */
class ListDialog : public wxDialog {
public:
  ListDialog(wxWindow* parent, SightingController* controller);

  /** Re-read the store, keeping the selection where possible. */
  void Reload();
  void ApplyTheme();

private:
  wxString SelectedId() const;
  std::vector<Observation> Visible() const;
  void UpdateButtons();

  void OnSearch(wxCommandEvent& event);
  void OnActivated(wxListEvent& event);
  void OnNew(wxCommandEvent& event);
  void OnEdit(wxCommandEvent& event);
  void OnDelete(wxCommandEvent& event);
  void OnChart(wxCommandEvent& event);
  void OnMedia(wxCommandEvent& event);
  void OnExport(wxCommandEvent& event);
  void OnFolder(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  SightingController* controller_;
  wxSearchCtrl* search_ = nullptr;
  wxListCtrl* list_ = nullptr;
  wxStaticText* summary_ = nullptr;
  wxButton* edit_ = nullptr;
  wxButton* delete_ = nullptr;
  wxButton* chart_ = nullptr;
  wxButton* media_ = nullptr;
  std::vector<wxString> ids_;  // row -> observation id
};

}  // namespace observer

#endif  // OBSERVER_LIST_DIALOG_H
