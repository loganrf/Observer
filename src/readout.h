// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

// SIGE readout tiles: a label, a value and a unit, one quantity per tile,
// divided by hairlines. Stale values show dashes and say why.

#ifndef OBSERVER_READOUT_H
#define OBSERVER_READOUT_H

#include <vector>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/window.h>

#include "brand.h"

namespace observer {

class ReadoutTile : public wxWindow {
public:
  ReadoutTile(wxWindow* parent, const wxString& label);

  /** Show a value; `lines` holds one or two value lines. */
  void SetValue(const wxArrayString& lines, const wxString& unit,
                const wxString& sub = wxString());
  void SetValue(const wxString& value, const wxString& unit,
                const wxString& sub = wxString());

  /** Show dashes in place of a value, with the reason in the label. */
  void SetStale(const wxString& dashes, const wxString& reason);

  void SetTheme(brand::Theme theme);

protected:
  wxSize DoGetBestClientSize() const override;

private:
  void OnPaint(wxPaintEvent& event);
  int Pad() const;

  wxString label_;
  wxString reason_;
  wxArrayString lines_;
  wxString unit_;
  wxString sub_;
  bool stale_ = true;
  brand::Theme theme_ = brand::Theme::kPaper;
  wxFont label_font_, value_font_, unit_font_, sub_font_;
};

/** A grid of readout tiles that fills its width. */
class ReadoutPanel : public wxPanel {
public:
  explicit ReadoutPanel(wxWindow* parent);

  ReadoutTile* AddTile(const wxString& label);
  void SetTheme(brand::Theme theme);

private:
  void OnPaint(wxPaintEvent& event);

  wxFlexGridSizer* grid_ = nullptr;
  std::vector<ReadoutTile*> tiles_;
  brand::Theme theme_ = brand::Theme::kPaper;
};

/** Fix state as an alert indicator: colour, shape and word together. */
class StatusIndicator : public wxWindow {
public:
  enum class Level { kNormal, kCaution, kWarning };

  explicit StatusIndicator(wxWindow* parent);

  void Set(Level level, const wxString& text);
  void SetTheme(brand::Theme theme);

protected:
  wxSize DoGetBestClientSize() const override;

private:
  void OnPaint(wxPaintEvent& event);

  Level level_ = Level::kWarning;
  wxString text_;
  brand::Theme theme_ = brand::Theme::kPaper;
};

}  // namespace observer

#endif  // OBSERVER_READOUT_H
