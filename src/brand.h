// Observer — wildlife sighting log for OpenCPN
// Copyright (C) 2026 SIGE
// SPDX-License-Identifier: GPL-3.0-or-later

// SIGE design tokens for the plugin's own drawing. OpenCPN's day, dusk
// and night colour schemes map to the SIGE Paper, Night and Helm (red
// night) palettes.

#ifndef OBSERVER_BRAND_H
#define OBSERVER_BRAND_H

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/string.h>

class wxWindow;
class wxStaticText;

namespace observer {
namespace brand {

enum class Theme { kPaper, kNight, kHelm };

struct Palette {
  wxColour paper;         ///< Ground
  wxColour paper_raised;  ///< Readout tiles
  wxColour ink;           ///< Values and headings
  wxColour ink_muted;     ///< Labels, units, stale values
  wxColour rule;          ///< Hairlines
  wxColour rule_strong;   ///< Meaningful borders
  wxColour signal;        ///< The mark's orange; fills only
  wxColour signal_ink;    ///< Orange words: eyebrows
  wxColour sea;           ///< Marine accent: sighting marks
  wxColour alert_warning;
  wxColour alert_caution;
  wxColour alert_normal;
  wxColour on_alert;
  wxColour mark_black;
};

/**
 * Theme for an OpenCPN PI_ColorScheme value. Day follows the desktop:
 * Paper on a light desktop, Night on a dark one, so labels stay legible on
 * native controls.
 */
Theme ThemeForScheme(int pi_color_scheme);

const Palette& PaletteFor(Theme theme);

/** IBM Plex Mono if installed, else the platform's monospace face. */
wxFont MonoFont(double point_size, wxFontWeight weight = wxFONTWEIGHT_NORMAL);

/** Archivo if installed, else the platform's GUI face. */
wxFont SansFont(double point_size, wxFontWeight weight = wxFONTWEIGHT_NORMAL);

/** The GUI font's point size, the base the type scale steps from. */
double BasePointSize();

/**
 * An uppercase `label` caption: mono, small, ink-muted.
 * Recoloured by ApplyLabelColours().
 */
wxStaticText* MakeLabel(wxWindow* parent, const wxString& text);

/** Measured or coded text (`data`): mono, as written, ink-muted. */
wxStaticText* MakeData(wxWindow* parent, const wxString& text);

/** An eyebrow above a title: mono, uppercase, signal-ink. */
wxStaticText* MakeEyebrow(wxWindow* parent, const wxString& text);

/** A heading in sans bold, ink. */
wxStaticText* MakeHeading(wxWindow* parent, const wxString& text);

/**
 * Recolour every label, eyebrow and heading made above inside `root`.
 * Call after OpenCPN's DimeWindow(), which resets control colours.
 */
void ApplyLabelColours(wxWindow* root, Theme theme);

}  // namespace brand
}  // namespace observer

#endif  // OBSERVER_BRAND_H
