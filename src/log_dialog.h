// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_LOG_DIALOG_H
#define OBSERVER_LOG_DIALOG_H

#include <functional>

#include <wx/dialog.h>

#include "brand.h"
#include "geo.h"
#include "observation.h"
#include "settings.h"

class wxButton;
class wxChoice;
class wxCollapsiblePane;
class wxComboBox;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

namespace observer {

class ReadoutPanel;
class ReadoutTile;
class StatusIndicator;

/** Display helpers shared by the dialogs; they follow OpenCPN's units. */
LatLonFormat UserLatLonFormat();
wxString FormatTimeShort(int64_t utc);

/**
 * The quick sighting form. The time, position and instruments are captured
 * when the form opens and shown as readouts; the person adds what they
 * saw. Also used to edit a logged sighting.
 */
class LogDialog : public wxDialog {
public:
  using Recapture = std::function<Observation()>;

  /**
   * `initial` holds the captured or logged sighting. `recapture`, if set,
   * offers an "Update to now" button that captures again.
   */
  LogDialog(wxWindow* parent, const Observation& initial, bool editing,
            const Settings& settings, const wxString& media_path,
            brand::Theme theme, Recapture recapture = nullptr);

  /** The sighting as entered. Valid after ShowModal() returns wxID_OK. */
  const Observation& Result() const { return result_; }

  /** A newly chosen media file, absolute path, or "" if unchanged. */
  const wxString& NewMediaSource() const { return new_media_; }

  /** True if the person removed the media attachment. */
  bool MediaRemoved() const { return media_removed_; }

private:
  void BuildUi(const Settings& settings);
  void ShowCapture();
  void ShowMedia();
  bool Collect(wxString* error);
  void ApplyTheme();

  void OnRecapture(wxCommandEvent& event);
  void OnAttach(wxCommandEvent& event);
  void OnRemoveMedia(wxCommandEvent& event);
  void OnSave(wxCommandEvent& event);

  Observation obs_;
  Observation result_;
  bool editing_;
  brand::Theme theme_;
  Recapture recapture_;
  RangeUnit range_unit_;
  LatLonFormat latlon_format_;

  wxString media_path_;  // existing media, absolute
  wxString new_media_;
  bool media_removed_ = false;
  wxString lat_text_, lon_text_;  // as first shown, to spot edits

  ReadoutPanel* readouts_ = nullptr;
  ReadoutTile* time_tile_ = nullptr;
  ReadoutTile* pos_tile_ = nullptr;
  ReadoutTile* cog_tile_ = nullptr;
  ReadoutTile* sog_tile_ = nullptr;
  ReadoutTile* hdg_tile_ = nullptr;
  ReadoutTile* depth_tile_ = nullptr;
  ReadoutTile* temp_tile_ = nullptr;
  ReadoutTile* wind_tile_ = nullptr;
  StatusIndicator* status_ = nullptr;

  wxChoice* category_ = nullptr;
  wxComboBox* species_ = nullptr;
  wxSpinCtrl* count_ = nullptr;
  wxTextCtrl* description_ = nullptr;
  wxStaticText* media_label_ = nullptr;
  wxButton* media_remove_ = nullptr;
  wxCollapsiblePane* details_ = nullptr;
  wxComboBox* behaviour_ = nullptr;
  wxChoice* confidence_ = nullptr;
  wxTextCtrl* bearing_ = nullptr;
  wxTextCtrl* range_ = nullptr;
  wxChoice* range_unit_choice_ = nullptr;
  wxTextCtrl* lat_ = nullptr;
  wxTextCtrl* lon_ = nullptr;
  wxTextCtrl* observer_ = nullptr;
};

}  // namespace observer

#endif  // OBSERVER_LOG_DIALOG_H
