// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

#include "brand.h"

#include <wx/fontenum.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/window.h>

namespace observer {
namespace brand {

namespace {

// Values from the SIGE design system tokens (tokens.json, version 2).
const Palette kPaper = {
    wxColour("#f6f3ee"), wxColour("#ffffff"), wxColour("#141312"),
    wxColour("#5c5751"), wxColour("#d8d1c6"), wxColour("#8a8279"),
    wxColour("#d65d2a"), wxColour("#b04a1c"), wxColour("#1e4e5c"),
    wxColour("#f0a81c"), wxColour("#f5d90a"), wxColour("#2a8a7a"),
    wxColour("#000000"), wxColour("#000000")};

const Palette kNight = {
    wxColour("#121110"), wxColour("#1c1a18"), wxColour("#f2ede5"),
    wxColour("#a39c92"), wxColour("#3a3632"), wxColour("#7a7268"),
    wxColour("#d65d2a"), wxColour("#e57a4c"), wxColour("#7fb3c2"),
    wxColour("#f0a81c"), wxColour("#f5d90a"), wxColour("#4fb8a4"),
    wxColour("#000000"), wxColour("#000000")};

// Helm: everything red on black to keep a dark-adapted crew's night vision.
const Palette kHelm = {
    wxColour("#000000"), wxColour("#140504"), wxColour("#ff4a2a"),
    wxColour("#e03a22"), wxColour("#3a0e08"), wxColour("#a8301a"),
    wxColour("#a8301a"), wxColour("#ff4a2a"), wxColour("#e03a22"),
    wxColour("#e03a22"), wxColour("#c0321c"), wxColour("#a8301a"),
    wxColour("#000000"), wxColour("#000000")};

const char* kLabelName = "sige-label";
const char* kEyebrowName = "sige-eyebrow";
const char* kHeadingName = "sige-heading";

bool HasFace(const wxString& face) {
  static int archivo = -1;
  static int plex = -1;
  int* cached = face == "Archivo" ? &archivo : &plex;
  if (*cached < 0) *cached = wxFontEnumerator::IsValidFacename(face) ? 1 : 0;
  return *cached == 1;
}

}  // namespace

Theme ThemeForScheme(int pi_color_scheme) {
  switch (pi_color_scheme) {
    case 2:  // PI_GLOBAL_COLOR_SCHEME_DUSK
      return Theme::kNight;
    case 3:  // PI_GLOBAL_COLOR_SCHEME_NIGHT
      return Theme::kHelm;
    default:
      return wxSystemSettings::GetAppearance().IsDark() ? Theme::kNight
                                                        : Theme::kPaper;
  }
}

const Palette& PaletteFor(Theme theme) {
  switch (theme) {
    case Theme::kNight:
      return kNight;
    case Theme::kHelm:
      return kHelm;
    case Theme::kPaper:
    default:
      return kPaper;
  }
}

double BasePointSize() {
  return wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT)
      .GetFractionalPointSize();
}

wxFont MonoFont(double point_size, wxFontWeight weight) {
  wxFontInfo info(point_size);
  info.Family(wxFONTFAMILY_TELETYPE).Weight(weight);
  if (HasFace("IBM Plex Mono")) info.FaceName("IBM Plex Mono");
  return wxFont(info);
}

wxFont SansFont(double point_size, wxFontWeight weight) {
  wxFontInfo info(point_size);
  info.Family(wxFONTFAMILY_SWISS).Weight(weight);
  if (HasFace("Archivo")) info.FaceName("Archivo");
  return wxFont(info);
}

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text) {
  auto* st = new wxStaticText(parent, wxID_ANY, text.Upper(),
                              wxDefaultPosition, wxDefaultSize, 0, kLabelName);
  st->SetFont(MonoFont(BasePointSize() * 0.85, wxFONTWEIGHT_MEDIUM));
  st->SetForegroundColour(PaletteFor(ThemeForScheme(1)).ink_muted);
  return st;
}

wxStaticText* MakeData(wxWindow* parent, const wxString& text) {
  auto* st = new wxStaticText(parent, wxID_ANY, text, wxDefaultPosition,
                              wxDefaultSize, 0, kLabelName);
  st->SetFont(MonoFont(BasePointSize() * 0.9));
  st->SetForegroundColour(PaletteFor(ThemeForScheme(1)).ink_muted);
  return st;
}

wxStaticText* MakeEyebrow(wxWindow* parent, const wxString& text) {
  auto* st = new wxStaticText(parent, wxID_ANY, text.Upper(),
                              wxDefaultPosition, wxDefaultSize, 0,
                              kEyebrowName);
  st->SetFont(MonoFont(BasePointSize() * 0.85, wxFONTWEIGHT_MEDIUM));
  st->SetForegroundColour(PaletteFor(ThemeForScheme(1)).signal_ink);
  return st;
}

wxStaticText* MakeHeading(wxWindow* parent, const wxString& text) {
  auto* st = new wxStaticText(parent, wxID_ANY, text, wxDefaultPosition,
                              wxDefaultSize, 0, kHeadingName);
  st->SetFont(SansFont(BasePointSize() * 1.45, wxFONTWEIGHT_BOLD));
  st->SetForegroundColour(PaletteFor(ThemeForScheme(1)).ink);
  return st;
}

void ApplyLabelColours(wxWindow* root, Theme theme) {
  if (!root) return;
  const Palette& p = PaletteFor(theme);
  for (wxWindow* child : root->GetChildren()) {
    const wxString name = child->GetName();
    if (name == kLabelName) {
      child->SetForegroundColour(p.ink_muted);
    } else if (name == kEyebrowName) {
      child->SetForegroundColour(p.signal_ink);
    } else if (name == kHeadingName) {
      child->SetForegroundColour(p.ink);
    }
    ApplyLabelColours(child, theme);
  }
  root->Refresh();
}

}  // namespace brand
}  // namespace observer
