// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "prefs_dialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>

#include "ocpn_api.h"

#include "config.h"

namespace observer {

namespace {

wxString Lines(const wxArrayString& items) {
  wxString out;
  for (const wxString& item : items) out << item << "\n";
  return out;
}

wxArrayString FromLines(const wxString& text) {
  wxArrayString out;
  wxStringTokenizer tok(text, "\r\n", wxTOKEN_STRTOK);
  while (tok.HasMoreTokens()) {
    wxString item = tok.GetNextToken();
    item.Trim(true).Trim(false);
    if (!item.empty() && out.Index(item) == wxNOT_FOUND) out.Add(item);
  }
  return out;
}

}  // namespace

PrefsDialog::PrefsDialog(wxWindow* parent, const Settings& settings,
                         brand::Theme theme)
    : wxDialog(parent, wxID_ANY, _("Observer preferences"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      settings_(settings) {
  const int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);
  top->Add(brand::MakeEyebrow(this, _("Marine") + wxString(L" \u00B7 ") +
                                        PLUGIN_API_NAME),
           0, wxLEFT | wxRIGHT | wxTOP, 2 * gap);
  top->Add(brand::MakeHeading(this, _("Preferences")), 0,
           wxLEFT | wxRIGHT | wxBOTTOM, 2 * gap);

  auto* form = new wxFlexGridSizer(2, gap, 2 * gap);
  form->AddGrowableCol(1, 1);

  form->Add(brand::MakeLabel(this, _("Log folder")), 0,
            wxALIGN_CENTER_VERTICAL);
  auto* dir_row = new wxBoxSizer(wxHORIZONTAL);
  log_dir_ = new wxTextCtrl(this, wxID_ANY, settings.log_dir, wxDefaultPosition,
                            wxSize(FromDIP(380), -1));
  log_dir_->SetToolTip(_("Folder for observations.csv and media"));
  dir_row->Add(log_dir_, 1, wxALIGN_CENTER_VERTICAL);
  auto* browse = new wxButton(this, wxID_ANY,
                              _("Browse") + wxString(L"\u2026"));
  browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    wxString dir;
    if (PlatformDirSelectorDialog(this, &dir, _("Log folder"),
                                  log_dir_->GetValue()) == wxID_OK &&
        !dir.empty())
      log_dir_->SetValue(dir);
  });
  dir_row->Add(browse, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
  form->Add(dir_row, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Observer")), 0, wxALIGN_CENTER_VERTICAL);
  observer_ = new wxTextCtrl(this, wxID_ANY, settings.observer);
  observer_->SetHint(_("Your name, added to each sighting"));
  form->Add(observer_, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Vessel")), 0, wxALIGN_CENTER_VERTICAL);
  vessel_ = new wxTextCtrl(this, wxID_ANY, settings.vessel);
  form->Add(vessel_, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Range unit")), 0,
            wxALIGN_CENTER_VERTICAL);
  wxArrayString units;
  units.Add(_("Metres"));
  units.Add(_("Nautical miles"));
  range_unit_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             units);
  range_unit_->SetSelection(static_cast<int>(settings.range_unit));
  form->Add(range_unit_, 0);

  form->AddSpacer(0);
  auto* checks = new wxBoxSizer(wxVERTICAL);
  show_marks_ = new wxCheckBox(this, wxID_ANY, _("Show sightings on the chart"));
  show_marks_->SetValue(settings.show_marks);
  checks->Add(show_marks_, 0, wxBOTTOM, gap);
  show_names_ = new wxCheckBox(this, wxID_ANY, _("Show species names on marks"));
  show_names_->SetValue(settings.show_mark_names);
  checks->Add(show_names_, 0, wxBOTTOM | wxLEFT, gap);
  copy_media_ = new wxCheckBox(
      this, wxID_ANY, _("Copy photos and videos into the log folder"));
  copy_media_->SetValue(settings.copy_media);
  checks->Add(copy_media_, 0, wxBOTTOM, gap);
  hotkey_ = new wxCheckBox(this, wxID_ANY,
                           _("Ctrl+Shift+O logs a sighting from the chart"));
  hotkey_->SetValue(settings.hotkey);
  checks->Add(hotkey_, 0);
  form->Add(checks, 1, wxEXPAND);
  show_marks_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
    show_names_->Enable(show_marks_->GetValue());
  });
  show_names_->Enable(settings.show_marks);

  form->Add(brand::MakeLabel(this, _("Categories")), 0, wxALIGN_TOP | wxTOP,
            FromDIP(4));
  categories_ = new wxTextCtrl(this, wxID_ANY, Lines(settings.categories),
                               wxDefaultPosition, wxSize(-1, FromDIP(110)),
                               wxTE_MULTILINE);
  categories_->SetToolTip(_("One per line, in the order they appear"));
  form->Add(categories_, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Behaviours")), 0, wxALIGN_TOP | wxTOP,
            FromDIP(4));
  behaviours_ = new wxTextCtrl(this, wxID_ANY, Lines(settings.behaviours),
                               wxDefaultPosition, wxSize(-1, FromDIP(110)),
                               wxTE_MULTILINE);
  behaviours_->SetToolTip(_("One per line"));
  form->Add(behaviours_, 1, wxEXPAND);

  top->Add(form, 1, wxEXPAND | wxLEFT | wxRIGHT, 2 * gap);

  auto* bottom = new wxBoxSizer(wxHORIZONTAL);
  bottom->Add(brand::MakeData(this, PLUGIN_VERSION), 0,
              wxALIGN_CENTER_VERTICAL);
  bottom->AddStretchSpacer(1);
  bottom->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0,
              wxALIGN_CENTER_VERTICAL);
  top->Add(bottom, 0, wxEXPAND | wxALL, 2 * gap);
  SetSizerAndFit(top);

  DimeWindow(this);
  brand::ApplyLabelColours(this, theme);
  CentreOnParent();
}

Settings PrefsDialog::Result() const {
  Settings s = settings_;
  s.log_dir = log_dir_->GetValue();
  s.log_dir.Trim(true).Trim(false);
  if (s.log_dir.empty()) s.log_dir = settings_.log_dir;
  s.observer = observer_->GetValue();
  s.observer.Trim(true).Trim(false);
  s.vessel = vessel_->GetValue();
  s.vessel.Trim(true).Trim(false);
  s.range_unit = range_unit_->GetSelection() == 1 ? RangeUnit::kNauticalMiles
                                                  : RangeUnit::kMetres;
  s.show_marks = show_marks_->GetValue();
  s.show_mark_names = show_names_->GetValue();
  s.copy_media = copy_media_->GetValue();
  s.hotkey = hotkey_->GetValue();
  const wxArrayString cats = FromLines(categories_->GetValue());
  s.categories = cats.empty() ? Settings::DefaultCategories() : cats;
  const wxArrayString behs = FromLines(behaviours_->GetValue());
  s.behaviours = behs.empty() ? Settings::DefaultBehaviours() : behs;
  return s;
}

}  // namespace observer
