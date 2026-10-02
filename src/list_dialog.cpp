// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "list_dialog.h"

#include <algorithm>

#include <wx/button.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/mimetype.h>
#include <wx/srchctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

#include "ocpn_plugin.h"

#include "config.h"
#include "export.h"
#include "geo.h"
#include "log_dialog.h"
#include "store.h"

namespace observer {

namespace {

enum Column { kColTime, kColSpecies, kColCount, kColCategory, kColPosition,
              kColMedia };

enum { kExportCsv = wxID_HIGHEST + 1, kExportGeoJson, kExportGpx };

bool Matches(const Observation& o, const wxString& needle) {
  if (needle.empty()) return true;
  for (const wxString* field : {&o.species, &o.category, &o.description,
                                &o.behaviour, &o.observer}) {
    if (field->Lower().Contains(needle)) return true;
  }
  return false;
}

bool OpenPath(const wxString& path) {
  // wxLaunchDefaultApplication handles files and folders on every desktop.
  return wxLaunchDefaultApplication(path);
}

}  // namespace

ListDialog::ListDialog(wxWindow* parent, SightingController* controller)
    : wxDialog(parent, wxID_ANY, _("Sightings"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      controller_(controller) {
  const int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);

  top->Add(brand::MakeEyebrow(this, _("Marine") + wxString(L" \u00B7 ") +
                                        _("Wildlife log")),
           0, wxLEFT | wxRIGHT | wxTOP, 2 * gap);
  auto* head = new wxBoxSizer(wxHORIZONTAL);
  head->Add(brand::MakeHeading(this, _("Sightings")), 0,
            wxALIGN_CENTER_VERTICAL);
  head->AddStretchSpacer(1);
  search_ = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(FromDIP(220), -1));
  search_->ShowCancelButton(true);
  search_->SetDescriptiveText(_("Search species, notes"));
  head->Add(search_, 0, wxALIGN_CENTER_VERTICAL);
  top->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 2 * gap);

  list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                         wxSize(FromDIP(720), FromDIP(320)),
                         wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
  list_->AppendColumn(_("Time (UTC)"), wxLIST_FORMAT_LEFT, FromDIP(140));
  list_->AppendColumn(_("Species"), wxLIST_FORMAT_LEFT, FromDIP(170));
  list_->AppendColumn(_("Count"), wxLIST_FORMAT_RIGHT, FromDIP(60));
  list_->AppendColumn(_("Category"), wxLIST_FORMAT_LEFT, FromDIP(110));
  list_->AppendColumn(_("Position"), wxLIST_FORMAT_LEFT, FromDIP(220));
  list_->AppendColumn(_("Media"), wxLIST_FORMAT_LEFT, FromDIP(60));
  top->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 2 * gap);

  summary_ = brand::MakeLabel(this, wxEmptyString);
  top->Add(summary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 2 * gap);

  auto* actions = new wxBoxSizer(wxHORIZONTAL);
  auto add_button = [&](const wxString& label, void (ListDialog::*handler)(
                                                   wxCommandEvent&)) {
    auto* b = new wxButton(this, wxID_ANY, label);
    b->Bind(wxEVT_BUTTON, handler, this);
    actions->Add(b, 0, wxRIGHT, gap);
    return b;
  };
  add_button(_("Log sighting"), &ListDialog::OnNew);
  edit_ = add_button(_("Edit"), &ListDialog::OnEdit);
  delete_ = add_button(_("Delete"), &ListDialog::OnDelete);
  chart_ = add_button(_("Show on chart"), &ListDialog::OnChart);
  media_ = add_button(_("Open media"), &ListDialog::OnMedia);
  actions->AddStretchSpacer(1);
  add_button(_("Export") + wxString(L"\u2026"), &ListDialog::OnExport);
  add_button(_("Open folder"), &ListDialog::OnFolder);
  auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
  actions->Add(close, 0);
  top->Add(actions, 0, wxEXPAND | wxALL, 2 * gap);

  SetSizerAndFit(top);
  SetEscapeId(wxID_CLOSE);

  search_->Bind(wxEVT_TEXT, &ListDialog::OnSearch, this);
  list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &ListDialog::OnActivated, this);
  list_->Bind(wxEVT_LIST_ITEM_SELECTED,
              [this](wxListEvent&) { UpdateButtons(); });
  list_->Bind(wxEVT_LIST_ITEM_DESELECTED,
              [this](wxListEvent&) { UpdateButtons(); });
  Bind(wxEVT_CLOSE_WINDOW, &ListDialog::OnClose, this);

  Reload();
  ApplyTheme();
  CentreOnParent();
}

std::vector<Observation> ListDialog::Visible() const {
  const wxString needle = search_->GetValue().Lower().Strip(wxString::both);
  std::vector<Observation> out;
  for (const Observation& o : controller_->Store().All())
    if (Matches(o, needle)) out.push_back(o);
  std::sort(out.begin(), out.end(),
            [](const Observation& a, const Observation& b) {
              return a.utc > b.utc;  // newest first
            });
  return out;
}

void ListDialog::Reload() {
  const wxString selected = SelectedId();
  const std::vector<Observation> rows = Visible();
  const LatLonFormat fmt = UserLatLonFormat();

  list_->Freeze();
  list_->DeleteAllItems();
  ids_.clear();
  long select_row = -1;
  for (const Observation& o : rows) {
    const long row = list_->InsertItem(list_->GetItemCount(),
                                       FormatTimeShort(o.utc));
    list_->SetItem(row, kColSpecies, o.species.empty() ? o.Title() : o.species);
    list_->SetItem(row, kColCount,
                   o.count > 0 ? wxString::Format("%d", o.count) : "");
    list_->SetItem(row, kColCategory, o.category);
    list_->SetItem(row, kColPosition,
                   o.HasMarkPosition()
                       ? FormatLatitude(o.MarkLat(), fmt) + "  " +
                             FormatLongitude(o.MarkLon(), fmt)
                       : wxString("--"));
    list_->SetItem(row, kColMedia, o.media.empty() ? "" : _("Yes"));
    ids_.push_back(o.id);
    if (o.id == selected) select_row = row;
  }
  if (select_row >= 0) {
    list_->SetItemState(select_row, wxLIST_STATE_SELECTED,
                        wxLIST_STATE_SELECTED);
    list_->EnsureVisible(select_row);
  }
  list_->Thaw();

  const int total = static_cast<int>(controller_->Store().All().size());
  const int shown = static_cast<int>(rows.size());
  const wxString count =
      shown == total ? wxString::Format(_("%d sightings"), total)
                     : wxString::Format(_("%d of %d sightings"), shown, total);
  summary_->SetLabel(count.Upper() + wxString(L" \u00B7 ") +
                     controller_->Store().LogPath());
  UpdateButtons();
}

void ListDialog::ApplyTheme() {
  DimeWindow(this);
  brand::ApplyLabelColours(this, controller_->CurrentTheme());
}

wxString ListDialog::SelectedId() const {
  if (!list_) return wxString();
  const long row = list_->GetNextItem(-1, wxLIST_NEXT_ALL,
                                      wxLIST_STATE_SELECTED);
  if (row < 0 || static_cast<size_t>(row) >= ids_.size()) return wxString();
  return ids_[static_cast<size_t>(row)];
}

void ListDialog::UpdateButtons() {
  const wxString id = SelectedId();
  const Observation* o = id.empty() ? nullptr : controller_->Store().Find(id);
  edit_->Enable(o != nullptr);
  delete_->Enable(o != nullptr);
  chart_->Enable(o && o->HasMarkPosition());
  media_->Enable(o && !o->media.empty());
}

void ListDialog::OnSearch(wxCommandEvent&) { Reload(); }

void ListDialog::OnActivated(wxListEvent&) {
  const wxString id = SelectedId();
  if (!id.empty()) controller_->EditSighting(this, id);
}

void ListDialog::OnNew(wxCommandEvent&) { controller_->NewSighting(this); }

void ListDialog::OnEdit(wxCommandEvent&) {
  const wxString id = SelectedId();
  if (!id.empty()) controller_->EditSighting(this, id);
}

void ListDialog::OnDelete(wxCommandEvent&) {
  const wxString id = SelectedId();
  if (!id.empty()) controller_->DeleteSighting(this, id);
}

void ListDialog::OnChart(wxCommandEvent&) {
  const wxString id = SelectedId();
  if (!id.empty()) controller_->ShowOnChart(id);
}

void ListDialog::OnMedia(wxCommandEvent&) {
  const Observation* o = controller_->Store().Find(SelectedId());
  if (!o || o->media.empty()) return;
  const wxString path = controller_->Store().MediaPath(o->media);
  if (!wxFileName::FileExists(path)) {
    OCPNMessageBox_PlugIn(this,
                          wxString::Format(_("The file is missing:\n%s"), path),
                          _("Observer"), wxOK | wxICON_WARNING);
    return;
  }
  OpenPath(path);
}

void ListDialog::OnExport(wxCommandEvent& event) {
  wxMenu menu;
  menu.Append(kExportCsv, _("CSV spreadsheet"));
  menu.Append(kExportGeoJson, _("GeoJSON for GIS"));
  menu.Append(kExportGpx, _("GPX waypoints"));
  auto* button = wxDynamicCast(event.GetEventObject(), wxWindow);
  const int choice = button ? button->GetPopupMenuSelectionFromUser(menu)
                            : GetPopupMenuSelectionFromUser(menu);
  if (choice == wxID_NONE) return;

  const std::vector<Observation> rows = Visible();
  const ObservationStore& store = controller_->Store();
  const MediaResolver resolve = [&store](const wxString& m) {
    return store.MediaPath(m);
  };
  wxString text, ext, wildcard;
  switch (choice) {
    case kExportCsv:
      text = ExportCsv(rows, resolve);
      ext = "csv";
      wildcard = "CSV (*.csv)|*.csv";
      break;
    case kExportGeoJson:
      text = ExportGeoJson(rows, resolve);
      ext = "geojson";
      wildcard = "GeoJSON (*.geojson)|*.geojson";
      break;
    case kExportGpx:
      text = ExportGpx(rows, resolve, wxString("Observer ") + PLUGIN_VERSION);
      ext = "gpx";
      wildcard = "GPX (*.gpx)|*.gpx";
      break;
    default:
      return;
  }

  wxString path;
  const wxString suggested = "sightings." + ext;
  if (PlatformFileSelectorDialog(this, &path, _("Export sightings"),
                                 store.Dir(), suggested, wildcard) != wxID_OK ||
      path.empty())
    return;
  if (wxFileName(path).GetExt().empty()) path += "." + ext;

  wxFFile f;
  if (!f.Open(path, "wb") || !f.Write(text, wxConvUTF8) || !f.Close()) {
    OCPNMessageBox_PlugIn(this,
                          wxString::Format(_("Cannot write %s"), path),
                          _("Observer"), wxOK | wxICON_ERROR);
    return;
  }
  summary_->SetLabel(
      wxString::Format(_("Exported %d sightings"), static_cast<int>(rows.size()))
          .Upper() +
      wxString(L" \u00B7 ") + path);
}

void ListDialog::OnFolder(wxCommandEvent&) {
  const wxString dir = controller_->Store().Dir();
  if (!wxFileName::DirExists(dir))
    wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  OpenPath(dir);
}

void ListDialog::OnClose(wxCloseEvent&) { Hide(); }

}  // namespace observer
