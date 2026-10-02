// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OBSERVER_PREFS_DIALOG_H
#define OBSERVER_PREFS_DIALOG_H

#include <wx/dialog.h>

#include "brand.h"
#include "settings.h"

class wxCheckBox;
class wxChoice;
class wxTextCtrl;

namespace observer {

/** Observer's preferences, opened from OpenCPN's plugin manager. */
class PrefsDialog : public wxDialog {
public:
  PrefsDialog(wxWindow* parent, const Settings& settings, brand::Theme theme);

  /** The edited settings. Valid after ShowModal() returns wxID_OK. */
  Settings Result() const;

private:
  Settings settings_;
  wxTextCtrl* log_dir_ = nullptr;
  wxTextCtrl* observer_ = nullptr;
  wxTextCtrl* vessel_ = nullptr;
  wxCheckBox* show_marks_ = nullptr;
  wxCheckBox* show_names_ = nullptr;
  wxCheckBox* copy_media_ = nullptr;
  wxCheckBox* hotkey_ = nullptr;
  wxChoice* range_unit_ = nullptr;
  wxTextCtrl* categories_ = nullptr;
  wxTextCtrl* behaviours_ = nullptr;
};

}  // namespace observer

#endif  // OBSERVER_PREFS_DIALOG_H
