// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "log_dialog.h"

#include <cmath>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/collpane.h>
#include <wx/combobox.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ocpn_plugin.h"

#include "readout.h"
#include "time_util.h"

namespace observer {

namespace {

const wchar_t* kDegT = L"\u00B0T";
const wchar_t* kDegC = L"\u00B0C";
const wchar_t* kEllipsis = L"\u2026";

wxString Angle(double deg) {
  const long d = std::lround(NormalizeDegrees(deg)) % 360;
  return wxString::Format("%03ld", d);
}

wxString OneDecimal(double v) { return wxString::Format("%.1f", v); }

wxString StatusText(const Observation& o, bool editing,
                    StatusIndicator::Level* level) {
  wxString pos;
  if (o.position_source == "gnss") {
    const long age = HasValue(o.fix_age_s) ? std::lround(o.fix_age_s) : 0;
    *level = age <= 10 ? StatusIndicator::Level::kNormal
                       : StatusIndicator::Level::kCaution;
    pos = wxString::Format(_("GNSS fix, %ld s old"), age);
  } else if (o.position_source == "cursor") {
    *level = StatusIndicator::Level::kCaution;
    pos = _("Position from the chart cursor");
  } else if (o.position_source == "manual") {
    *level = StatusIndicator::Level::kCaution;
    pos = _("Position entered by hand");
  } else {
    *level = StatusIndicator::Level::kWarning;
    pos = _("No position fix. Enter one under More details");
  }
  const wxString time = o.time_source == "gnss"
                            ? _("time from GNSS")
                            : _("time from the computer clock");
  wxString text = pos + wxString(L" \u00B7 ") + time;
  if (editing) text = _("Logged:") + " " + text;
  return text;
}

}  // namespace

LatLonFormat UserLatLonFormat() {
  switch (GetLatLonFormat()) {
    case 1:
      return LatLonFormat::kDecimalDegrees;
    case 2:
      return LatLonFormat::kDegreesMinutesSeconds;
    default:
      return LatLonFormat::kDegreesDecimalMinutes;
  }
}

wxString FormatTimeShort(int64_t utc) {
  int y, mo, d, h, mi, s;
  EpochToUtc(utc, &y, &mo, &d, &h, &mi, &s);
  return wxString::Format("%04d-%02d-%02d %02d:%02d", y, mo, d, h, mi);
}

LogDialog::LogDialog(wxWindow* parent, const Observation& initial,
                     bool editing, const Settings& settings,
                     const wxString& media_path, brand::Theme theme,
                     Recapture recapture)
    : wxDialog(parent, wxID_ANY,
               editing ? _("Edit sighting") : _("Log a sighting"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      obs_(initial),
      editing_(editing),
      theme_(theme),
      recapture_(std::move(recapture)),
      range_unit_(settings.range_unit),
      latlon_format_(UserLatLonFormat()),
      media_path_(media_path) {
  BuildUi(settings);
  ShowCapture();
  ShowMedia();
  ApplyTheme();
  Fit();
  SetMinSize(GetSize());
  CentreOnParent();
  species_->SetFocus();
}

void LogDialog::BuildUi(const Settings& settings) {
  const int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);

  // Header: eyebrow and title, as SIGE pages open.
  const wxString eyebrow = _("Marine") + wxString(L" \u00B7 ") +
                           (editing_ ? _("Edit sighting") : _("Sighting"));
  top->Add(brand::MakeEyebrow(this, eyebrow),
           0, wxLEFT | wxRIGHT | wxTOP, 2 * gap);
  top->Add(brand::MakeHeading(this, editing_ ? obs_.Title()
                                             : wxString(_("Log a sighting"))),
           0, wxLEFT | wxRIGHT | wxBOTTOM, 2 * gap);

  // Captured readouts.
  readouts_ = new ReadoutPanel(this);
  time_tile_ = readouts_->AddTile(_("Time"));
  pos_tile_ = readouts_->AddTile(_("Position"));
  cog_tile_ = readouts_->AddTile(_("COG"));
  sog_tile_ = readouts_->AddTile(_("SOG"));
  hdg_tile_ = readouts_->AddTile(_("HDG"));
  depth_tile_ = readouts_->AddTile(_("Depth"));
  temp_tile_ = readouts_->AddTile(_("Water"));
  wind_tile_ = readouts_->AddTile(_("Wind"));
  top->Add(readouts_, 0, wxEXPAND | wxLEFT | wxRIGHT, 2 * gap);

  auto* status_row = new wxBoxSizer(wxHORIZONTAL);
  status_ = new StatusIndicator(this);
  status_row->Add(status_, 1, wxALIGN_CENTER_VERTICAL);
  if (recapture_) {
    auto* again = new wxButton(this, wxID_ANY, _("Update to now"));
    again->Bind(wxEVT_BUTTON, &LogDialog::OnRecapture, this);
    status_row->Add(again, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
  }
  top->Add(status_row, 0, wxEXPAND | wxALL, 2 * gap);
  top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 2 * gap);

  // What was seen.
  auto* form = new wxFlexGridSizer(2, gap, 2 * gap);
  form->AddGrowableCol(1, 1);

  form->Add(brand::MakeLabel(this, _("Category")), 0, wxALIGN_CENTER_VERTICAL);
  auto* cat_row = new wxBoxSizer(wxHORIZONTAL);
  wxArrayString categories = settings.categories;
  if (!obs_.category.empty() && categories.Index(obs_.category) == wxNOT_FOUND)
    categories.Add(obs_.category);
  category_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           categories);
  const wxString want = editing_ ? obs_.category
                                 : (obs_.category.empty()
                                        ? settings.last_category
                                        : obs_.category);
  const int cat_index = categories.Index(want);
  category_->SetSelection(cat_index == wxNOT_FOUND ? 0 : cat_index);
  cat_row->Add(category_, 1, wxALIGN_CENTER_VERTICAL);
  cat_row->Add(brand::MakeLabel(this, _("Count")), 0,
               wxALIGN_CENTER_VERTICAL | wxLEFT, 2 * gap);
  count_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                          wxDefaultSize, wxSP_ARROW_KEYS, 0, 100000,
                          obs_.count);
  count_->SetToolTip(_("Number of animals. 0 if unknown."));
  cat_row->Add(count_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
  form->Add(cat_row, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Species")), 0, wxALIGN_CENTER_VERTICAL);
  species_ = new wxComboBox(this, wxID_ANY, obs_.species, wxDefaultPosition,
                            wxSize(FromDIP(320), -1), settings.recent_species,
                            wxCB_DROPDOWN);
  species_->AutoComplete(settings.recent_species);
  species_->SetHint(_("Humpback whale"));
  form->Add(species_, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Description")), 0, wxALIGN_TOP | wxTOP,
            FromDIP(4));
  description_ = new wxTextCtrl(this, wxID_ANY, obs_.description,
                                wxDefaultPosition,
                                wxSize(-1, FromDIP(72)), wxTE_MULTILINE);
  description_->SetHint(_("What you saw: markings, group, direction of travel"));
  form->Add(description_, 1, wxEXPAND);

  form->Add(brand::MakeLabel(this, _("Media")), 0, wxALIGN_CENTER_VERTICAL);
  auto* media_row = new wxBoxSizer(wxHORIZONTAL);
  auto* attach = new wxButton(this, wxID_ANY,
                              _("Attach photo or video") + kEllipsis);
  attach->Bind(wxEVT_BUTTON, &LogDialog::OnAttach, this);
  media_row->Add(attach, 0, wxALIGN_CENTER_VERTICAL);
  media_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxST_ELLIPSIZE_MIDDLE);
  media_row->Add(media_label_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
  media_remove_ = new wxButton(this, wxID_ANY, _("Remove"));
  media_remove_->Bind(wxEVT_BUTTON, &LogDialog::OnRemoveMedia, this);
  media_row->Add(media_remove_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
  form->Add(media_row, 1, wxEXPAND);

  top->Add(form, 0, wxEXPAND | wxALL, 2 * gap);

  // Optional detail, folded away so the quick path stays short.
  details_ = new wxCollapsiblePane(this, wxID_ANY, _("More details"));
  wxWindow* pane = details_->GetPane();
  auto* dform = new wxFlexGridSizer(4, gap, 2 * gap);
  dform->AddGrowableCol(1, 1);
  dform->AddGrowableCol(3, 1);

  dform->Add(brand::MakeLabel(pane, _("Behaviour")), 0,
             wxALIGN_CENTER_VERTICAL);
  behaviour_ = new wxComboBox(pane, wxID_ANY, obs_.behaviour, wxDefaultPosition,
                              wxDefaultSize, settings.behaviours,
                              wxCB_DROPDOWN);
  dform->Add(behaviour_, 1, wxEXPAND);
  dform->Add(brand::MakeLabel(pane, _("Confidence")), 0,
             wxALIGN_CENTER_VERTICAL);
  wxArrayString confidences;
  confidences.Add(_("Not stated"));
  for (const wxString& c : Settings::Confidences()) confidences.Add(c);
  confidence_ = new wxChoice(pane, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             confidences);
  const int conf = confidences.Index(obs_.confidence);
  confidence_->SetSelection(conf == wxNOT_FOUND ? 0 : conf);
  dform->Add(confidence_, 1, wxEXPAND);

  dform->Add(brand::MakeLabel(pane, _("Bearing") + " " + kDegT), 0,
             wxALIGN_CENTER_VERTICAL);
  bearing_ = new wxTextCtrl(pane, wxID_ANY,
                            HasValue(obs_.bearing_deg) ? Angle(obs_.bearing_deg)
                                                       : wxString());
  bearing_->SetHint(_("to the animal"));
  dform->Add(bearing_, 1, wxEXPAND);
  dform->Add(brand::MakeLabel(pane, _("Range")), 0, wxALIGN_CENTER_VERTICAL);
  auto* range_row = new wxBoxSizer(wxHORIZONTAL);
  wxString range_text;
  if (HasValue(obs_.range_m)) {
    range_text = range_unit_ == RangeUnit::kNauticalMiles
                     ? wxString::Format("%.2f", obs_.range_m / kMetresPerNm)
                     : wxString::Format("%.0f", obs_.range_m);
  }
  range_ = new wxTextCtrl(pane, wxID_ANY, range_text);
  range_row->Add(range_, 1, wxALIGN_CENTER_VERTICAL);
  wxArrayString units;
  units.Add("m");
  units.Add("NM");
  range_unit_choice_ = new wxChoice(pane, wxID_ANY, wxDefaultPosition,
                                    wxDefaultSize, units);
  range_unit_choice_->SetSelection(static_cast<int>(range_unit_));
  range_row->Add(range_unit_choice_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT,
                 FromDIP(4));
  dform->Add(range_row, 1, wxEXPAND);

  dform->Add(brand::MakeLabel(pane, _("Latitude")), 0,
             wxALIGN_CENTER_VERTICAL);
  lat_ = new wxTextCtrl(pane, wxID_ANY);
  dform->Add(lat_, 1, wxEXPAND);
  dform->Add(brand::MakeLabel(pane, _("Longitude")), 0,
             wxALIGN_CENTER_VERTICAL);
  lon_ = new wxTextCtrl(pane, wxID_ANY);
  dform->Add(lon_, 1, wxEXPAND);

  dform->Add(brand::MakeLabel(pane, _("Observer")), 0,
             wxALIGN_CENTER_VERTICAL);
  observer_ = new wxTextCtrl(pane, wxID_ANY, obs_.observer);
  dform->Add(observer_, 1, wxEXPAND);
  dform->AddSpacer(0);
  dform->AddSpacer(0);

  auto* pane_sizer = new wxBoxSizer(wxVERTICAL);
  pane_sizer->Add(dform, 1, wxEXPAND | wxTOP, gap);
  pane->SetSizer(pane_sizer);
  details_->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [this](wxCollapsiblePaneEvent&) {
    SetMinSize(wxDefaultSize);
    Fit();
    Layout();
  });
  top->Add(details_, 0, wxEXPAND | wxLEFT | wxRIGHT, 2 * gap);
  if (editing_) details_->Expand();

  // Buttons: one primary action.
  auto* buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->AddStretchSpacer(1);
  auto* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
  buttons->Add(cancel, 0, wxRIGHT, gap);
  auto* save = new wxButton(this, wxID_OK,
                            editing_ ? _("Save changes") : _("Save sighting"));
  save->SetDefault();
  save->Bind(wxEVT_BUTTON, &LogDialog::OnSave, this);
  buttons->Add(save, 0);
  top->Add(buttons, 0, wxEXPAND | wxALL, 2 * gap);

  // Ctrl+Enter saves from anywhere, the multi-line description included.
  wxAcceleratorEntry accel(wxACCEL_CTRL, WXK_RETURN, wxID_OK);
  SetAcceleratorTable(wxAcceleratorTable(1, &accel));
  Bind(wxEVT_MENU, &LogDialog::OnSave, this, wxID_OK);

  SetSizer(top);
}

void LogDialog::ShowCapture() {
  const wxString no_data = _("No data");

  int y, mo, d, h, mi, s;
  EpochToUtc(obs_.utc, &y, &mo, &d, &h, &mi, &s);
  time_tile_->SetValue(wxString::Format("%02d:%02d:%02d", h, mi, s), "UTC",
                       wxString::Format("%04d-%02d-%02d", y, mo, d));

  if (IsValidPosition(obs_.lat, obs_.lon)) {
    wxArrayString lines;
    lines.Add(FormatLatitude(obs_.lat, latlon_format_));
    lines.Add(FormatLongitude(obs_.lon, latlon_format_));
    pos_tile_->SetValue(lines, wxString());
    lat_text_ = lines[0];
    lon_text_ = lines[1];
  } else {
    pos_tile_->SetStale("--", _("No fix"));
    lat_text_.clear();
    lon_text_.clear();
  }
  lat_->ChangeValue(lat_text_);
  lon_->ChangeValue(lon_text_);

  if (HasValue(obs_.cog_deg))
    cog_tile_->SetValue(Angle(obs_.cog_deg), kDegT);
  else
    cog_tile_->SetStale("---", no_data);

  if (HasValue(obs_.sog_kn))
    sog_tile_->SetValue(OneDecimal(toUsrSpeed_Plugin(obs_.sog_kn)),
                        getUsrSpeedUnit_Plugin());
  else
    sog_tile_->SetStale("--.-", no_data);

  if (HasValue(obs_.heading_deg))
    hdg_tile_->SetValue(Angle(obs_.heading_deg), kDegT);
  else
    hdg_tile_->SetStale("---", no_data);

  if (HasValue(obs_.depth_m))
    depth_tile_->SetValue(OneDecimal(obs_.depth_m), "m",
                          wxString::Format("%.0f ft", obs_.depth_m / 0.3048));
  else
    depth_tile_->SetStale("--.-", no_data);

  if (HasValue(obs_.water_temp_c)) {
    const wxString unit = getUsrTempUnit_Plugin();
    temp_tile_->SetValue(OneDecimal(toUsrTemp_Plugin(obs_.water_temp_c)),
                         unit.empty() ? wxString(kDegC) : unit);
  } else {
    temp_tile_->SetStale("--.-", no_data);
  }

  if (HasValue(obs_.wind_speed_kn)) {
    const wxString from =
        HasValue(obs_.wind_dir_deg)
            ? _("from") + " " + Angle(obs_.wind_dir_deg) + kDegT
            : wxString();
    wind_tile_->SetValue(wxString::Format("%.0f", obs_.wind_speed_kn), "kn",
                         from);
  } else {
    wind_tile_->SetStale("--", no_data);
  }

  StatusIndicator::Level level = StatusIndicator::Level::kWarning;
  const wxString status = StatusText(obs_, editing_, &level);
  status_->Set(level, status);
  readouts_->Layout();
}

void LogDialog::ShowMedia() {
  wxString shown;
  if (!new_media_.empty()) {
    shown = wxFileName(new_media_).GetFullName();
  } else if (!media_removed_ && !media_path_.empty()) {
    shown = wxFileName(media_path_).GetFullName();
  }
  media_label_->SetLabel(shown.empty() ? _("None") : shown);
  media_remove_->Enable(!shown.empty());
  Layout();
}

void LogDialog::ApplyTheme() {
  DimeWindow(this);  // OpenCPN's dusk and night colours for the controls
  readouts_->SetTheme(theme_);
  status_->SetTheme(theme_);
  brand::ApplyLabelColours(this, theme_);
}

void LogDialog::OnRecapture(wxCommandEvent&) {
  if (!recapture_) return;
  // What the person typed lives in the controls and is kept; only the
  // measured values and the time are replaced.
  obs_ = recapture_();
  ShowCapture();
  Layout();
}

void LogDialog::OnAttach(wxCommandEvent&) {
  wxString path;
  const wxString wildcard =
      _("Photos and videos") +
      " (*.jpg;*.jpeg;*.png;*.heic;*.webp;*.mp4;*.mov;*.m4v)|"
      "*.jpg;*.jpeg;*.png;*.heic;*.webp;*.mp4;*.mov;*.m4v;"
      "*.JPG;*.JPEG;*.PNG;*.HEIC;*.WEBP;*.MP4;*.MOV;*.M4V|" +
      _("All files") + " (*.*)|*.*";
  const int rc = PlatformFileSelectorDialog(
      this, &path, _("Attach photo or video"), wxEmptyString, wxEmptyString,
      wildcard);
  if (rc != wxID_OK || path.empty()) return;
  new_media_ = path;
  media_removed_ = false;
  ShowMedia();
}

void LogDialog::OnRemoveMedia(wxCommandEvent&) {
  new_media_.clear();
  media_removed_ = !media_path_.empty();
  ShowMedia();
}

bool LogDialog::Collect(wxString* error) {
  Observation o = obs_;
  if (category_->GetSelection() != wxNOT_FOUND)
    o.category = category_->GetString(category_->GetSelection());
  o.species = species_->GetValue();
  o.species.Trim(true).Trim(false);
  o.count = count_->GetValue();
  o.description = description_->GetValue();
  o.description.Trim(true);
  o.behaviour = behaviour_->GetValue();
  o.behaviour.Trim(true).Trim(false);
  o.confidence = confidence_->GetSelection() > 0
                     ? confidence_->GetString(confidence_->GetSelection())
                     : wxString();
  o.observer = observer_->GetValue();
  o.observer.Trim(true).Trim(false);

  // Position: only reparse what the person changed, to keep full precision.
  const wxString lat_text = lat_->GetValue();
  const wxString lon_text = lon_->GetValue();
  if (lat_text != lat_text_ || lon_text != lon_text_) {
    double lat, lon;
    if (lat_text.Strip(wxString::both).empty() &&
        lon_text.Strip(wxString::both).empty()) {
      o.lat = NoValue();
      o.lon = NoValue();
      o.position_source = "none";
    } else if (!ParseCoordinate(lat_text, true, &lat)) {
      *error = _("The latitude is not one I can read. Try 47 40.890 N or "
                 "47.6815.");
      return false;
    } else if (!ParseCoordinate(lon_text, false, &lon)) {
      *error = _("The longitude is not one I can read. Try 122 25.164 W or "
                 "-122.4194.");
      return false;
    } else {
      o.lat = lat;
      o.lon = lon;
      o.position_source = "manual";
      o.fix_age_s = NoValue();
    }
  }

  // Bearing and range to the animal place the mark where it was seen.
  double bearing = NoValue();
  double range = NoValue();
  const wxString bearing_text = bearing_->GetValue().Strip(wxString::both);
  const wxString range_text = range_->GetValue().Strip(wxString::both);
  if (!bearing_text.empty() &&
      (!ParseNumber(bearing_text, &bearing) || bearing < 0 || bearing > 360)) {
    *error = _("Bearing must be a number of degrees from 0 to 360.");
    return false;
  }
  if (!range_text.empty()) {
    wxString t = range_text;
    t.Replace(",", ".");
    if (!ParseNumber(t, &range) || range < 0) {
      *error = _("Range must be a positive number.");
      return false;
    }
    if (range_unit_choice_->GetSelection() == 1) range *= kMetresPerNm;
  }
  if (HasValue(bearing) != HasValue(range)) {
    *error = _("Give both a bearing and a range, or neither.");
    return false;
  }
  o.bearing_deg = HasValue(bearing) ? NormalizeDegrees(bearing) : NoValue();
  o.range_m = range;
  o.UpdateSightingPosition();

  result_ = o;
  return true;
}

void LogDialog::OnSave(wxCommandEvent&) {
  wxString error;
  if (!Collect(&error)) {
    if (!details_->IsExpanded()) {
      details_->Expand();
      Fit();
    }
    OCPNMessageBox_PlugIn(this, error, _("Observer"), wxOK | wxICON_WARNING);
    return;
  }
  EndModal(wxID_OK);
}

}  // namespace observer
