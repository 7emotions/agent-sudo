#pragma once
// Material Icons Unicode codepoints.
// Using codepoints instead of ligature names avoids ligature-rendering issues
// with widget toolkits that don't enable OpenType GSUB features.

namespace icon {

// ── Action ──
inline constexpr auto kCheck     = "\uE5CA";
inline constexpr auto kClose     = "\uE5CD";
inline constexpr auto kPause     = "\uE034";
inline constexpr auto kPlayArrow = "\uE037";

// ── Selection ──
inline constexpr auto kSelectAll  = "\uE162";
inline constexpr auto kDeselect   = "\uE2BF";
inline constexpr auto kChecklist  = "\uE065";  // playlist_add_check

// ── UI ──
inline constexpr auto kPalette    = "\uE40A";
inline constexpr auto kLightMode  = "\uE3A8";  // wb_sunny
inline constexpr auto kDarkMode   = "\uE3AE";  // nights_stay
inline constexpr auto kTerminal   = "\uE0D9";

}
