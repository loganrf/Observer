// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "readout.h"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/gdicmn.h>
#include <wx/settings.h>
#include <wx/sizer.h>

namespace observer {

using brand::PaletteFor;

ReadoutTile::ReadoutTile(wxWindow* parent, const wxString& label)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
               wxFULL_REPAINT_ON_RESIZE | wxBORDER_NONE),
      label_(label.Upper()) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  const double base = brand::BasePointSize();
  label_font_ = brand::MonoFont(base * 0.8, wxFONTWEIGHT_MEDIUM);
  value_font_ = brand::MonoFont(base * 1.55, wxFONTWEIGHT_MEDIUM);
  unit_font_ = brand::MonoFont(base * 0.95);
  sub_font_ = brand::MonoFont(base * 0.8);
  lines_.Add("--");
  Bind(wxEVT_PAINT, &ReadoutTile::OnPaint, this);
}

void ReadoutTile::SetValue(const wxArrayString& lines, const wxString& unit,
                           const wxString& sub) {
  lines_ = lines;
  unit_ = unit;
  sub_ = sub;
  reason_.clear();
  stale_ = false;
  InvalidateBestSize();
  Refresh();
}

void ReadoutTile::SetValue(const wxString& value, const wxString& unit,
                           const wxString& sub) {
  wxArrayString lines;
  lines.Add(value);
  SetValue(lines, unit, sub);
}

void ReadoutTile::SetStale(const wxString& dashes, const wxString& reason) {
  lines_.Clear();
  lines_.Add(dashes);
  unit_.clear();
  sub_.clear();
  reason_ = reason.Upper();
  stale_ = true;
  InvalidateBestSize();
  Refresh();
}

void ReadoutTile::SetTheme(brand::Theme theme) {
  theme_ = theme;
  Refresh();
}

int ReadoutTile::Pad() const { return FromDIP(8); }

wxSize ReadoutTile::DoGetBestClientSize() const {
  wxClientDC dc(const_cast<ReadoutTile*>(this));
  dc.SetFont(label_font_);
  wxString label = label_;
  if (!reason_.empty()) label += wxString(L" \u00B7 ") + reason_;
  wxSize lsz = dc.GetTextExtent(label);

  dc.SetFont(value_font_);
  int vw = 0, vh = 0;
  for (const wxString& line : lines_) {
    const wxSize s = dc.GetTextExtent(line);
    vw = std::max(vw, s.x);
    vh += s.y;
  }
  dc.SetFont(unit_font_);
  const int uw = unit_.empty() ? 0 : dc.GetTextExtent(unit_).x + FromDIP(4);
  dc.SetFont(sub_font_);
  const wxSize ssz = sub_.empty() ? wxSize(0, 0) : dc.GetTextExtent(sub_);

  const int w = std::max({lsz.x, vw + uw, ssz.x, FromDIP(72)}) + 2 * Pad();
  const int h = Pad() + lsz.y + FromDIP(4) + vh +
                (sub_.empty() ? 0 : ssz.y + FromDIP(2)) + Pad();
  return wxSize(w, h);
}

void ReadoutTile::OnPaint(wxPaintEvent&) {
  wxAutoBufferedPaintDC dc(this);
  const brand::Palette& p = PaletteFor(theme_);
  const wxSize size = GetClientSize();
  const int pad = Pad();

  dc.SetBackground(wxBrush(p.paper_raised));
  dc.Clear();

  // Hairlines on the right and bottom; the panel draws top and left.
  dc.SetPen(wxPen(p.rule, 1));
  dc.DrawLine(size.x - 1, 0, size.x - 1, size.y);
  dc.DrawLine(0, size.y - 1, size.x, size.y - 1);

  dc.SetFont(label_font_);
  dc.SetTextForeground(p.ink_muted);
  wxString label = label_;
  if (!reason_.empty()) label += wxString(L" \u00B7 ") + reason_;
  const wxSize lsz = dc.GetTextExtent(label);
  dc.DrawText(label, pad, pad);

  // Values right-aligned, units after them, as on a SIGE instrument.
  dc.SetFont(unit_font_);
  const int uw = unit_.empty() ? 0 : dc.GetTextExtent(unit_).x;
  const int right = size.x - pad;
  int y = pad + lsz.y + FromDIP(4);
  for (size_t i = 0; i < lines_.size(); ++i) {
    dc.SetFont(value_font_);
    dc.SetTextForeground(stale_ ? p.ink_muted : p.ink);
    const wxSize vsz = dc.GetTextExtent(lines_[i]);
    const int unit_space = uw > 0 ? uw + FromDIP(4) : 0;
    dc.DrawText(lines_[i], right - unit_space - vsz.x, y);
    if (i == 0 && uw > 0) {
      dc.SetFont(unit_font_);
      dc.SetTextForeground(p.ink_muted);
      const int uh = dc.GetTextExtent(unit_).y;
      dc.DrawText(unit_, right - uw, y + vsz.y - uh - FromDIP(2));
    }
    y += vsz.y;
  }
  if (!sub_.empty()) {
    dc.SetFont(sub_font_);
    dc.SetTextForeground(p.ink_muted);
    const wxSize ssz = dc.GetTextExtent(sub_);
    dc.DrawText(sub_, right - ssz.x, y + FromDIP(2));
  }
}

ReadoutPanel::ReadoutPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  // An instrument grid: four columns, or two on a narrow (phone) screen.
  // Tiles draw their right and bottom hairlines; the 1 px inset leaves room
  // for the panel's own top and left ones.
  const int columns = wxGetDisplaySize().x < FromDIP(640) ? 2 : 4;
  grid_ = new wxFlexGridSizer(columns, 0, 0);
  for (int c = 0; c < columns; ++c) grid_->AddGrowableCol(c, 1);
  auto* outer = new wxBoxSizer(wxVERTICAL);
  outer->Add(grid_, 1, wxEXPAND | wxTOP | wxLEFT, 1);
  SetSizer(outer);
  Bind(wxEVT_PAINT, &ReadoutPanel::OnPaint, this);
}

ReadoutTile* ReadoutPanel::AddTile(const wxString& label) {
  auto* tile = new ReadoutTile(this, label);
  tile->SetTheme(theme_);
  grid_->Add(tile, 0, wxEXPAND);
  tiles_.push_back(tile);
  return tile;
}

void ReadoutPanel::SetTheme(brand::Theme theme) {
  theme_ = theme;
  for (ReadoutTile* t : tiles_) t->SetTheme(theme);
  Refresh();
}

void ReadoutPanel::OnPaint(wxPaintEvent&) {
  wxAutoBufferedPaintDC dc(this);
  const brand::Palette& p = PaletteFor(theme_);
  dc.SetBackground(wxBrush(p.paper_raised));
  dc.Clear();
  const wxSize size = GetClientSize();
  dc.SetPen(wxPen(p.rule, 1));
  dc.DrawLine(0, 0, size.x, 0);
  dc.DrawLine(0, 0, 0, size.y);
  dc.DrawLine(size.x - 1, 0, size.x - 1, size.y);
  dc.DrawLine(0, size.y - 1, size.x, size.y - 1);
}

StatusIndicator::StatusIndicator(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
               wxFULL_REPAINT_ON_RESIZE | wxBORDER_NONE) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  SetFont(brand::MonoFont(brand::BasePointSize() * 0.9, wxFONTWEIGHT_MEDIUM));
  Bind(wxEVT_PAINT, &StatusIndicator::OnPaint, this);
}

void StatusIndicator::Set(Level level, const wxString& text) {
  level_ = level;
  text_ = text;
  InvalidateBestSize();
  Refresh();
}

void StatusIndicator::SetTheme(brand::Theme theme) {
  theme_ = theme;
  Refresh();
}

wxSize StatusIndicator::DoGetBestClientSize() const {
  wxClientDC dc(const_cast<StatusIndicator*>(this));
  dc.SetFont(GetFont());
  const wxSize t = dc.GetTextExtent(text_.empty() ? wxString("X") : text_);
  const int shape = FromDIP(12);
  return wxSize(shape + FromDIP(8) + t.x, std::max(t.y, shape) + FromDIP(4));
}

void StatusIndicator::OnPaint(wxPaintEvent&) {
  wxAutoBufferedPaintDC dc(this);
  const brand::Palette& p = PaletteFor(theme_);
  dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
  dc.Clear();

  const wxSize size = GetClientSize();
  const int s = FromDIP(12);
  const int top = (size.y - s) / 2;
  wxColour fill;
  switch (level_) {
    case Level::kNormal: fill = p.alert_normal; break;
    case Level::kCaution: fill = p.alert_caution; break;
    case Level::kWarning: default: fill = p.alert_warning; break;
  }
  // The SIGE alert shapes carry the level even where colour cannot (Helm).
  dc.SetBrush(wxBrush(fill));
  dc.SetPen(theme_ == brand::Theme::kPaper ? wxPen(p.mark_black, 1)
                                           : wxPen(fill, 1));
  if (level_ == Level::kNormal) {
    dc.DrawEllipse(0, top, s, s);
  } else if (level_ == Level::kCaution) {
    const wxPoint diamond[] = {wxPoint(s / 2, top), wxPoint(s, top + s / 2),
                               wxPoint(s / 2, top + s), wxPoint(0, top + s / 2)};
    dc.DrawPolygon(4, diamond);
  } else {
    const wxPoint triangle[] = {wxPoint(s / 2, top), wxPoint(s, top + s),
                                wxPoint(0, top + s)};
    dc.DrawPolygon(3, triangle);
  }

  dc.SetFont(GetFont());
  dc.SetTextForeground(theme_ == brand::Theme::kPaper
                           ? wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)
                           : p.ink);
  const wxSize t = dc.GetTextExtent(text_);
  dc.DrawText(text_, s + FromDIP(8), (size.y - t.y) / 2);
}

}  // namespace observer
