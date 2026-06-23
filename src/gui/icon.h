#pragma once
#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>

/// Central icon provider for agent-sudo GUI.
/// Stateless utility — all methods are static.
struct IconProvider {
    /// Initialize Material Icons font. Call once at startup.
    /// Returns true if the font was loaded successfully.
    static auto initFont() -> bool;

    /// Returns the application icon (key + sudo SVG).
    static auto appIcon() -> QPixmap;

    /// Returns a themed Material Icon pixmap at the given size and color.
    /// @param name   Material Icon codepoint name (e.g., "terminal", "check")
    /// @param color  Icon color (follows theme)
    /// @param size   Pixel size (default 20)
    static auto themedIcon(const QString& name,
                           const QColor& color,
                           int size = 20) -> QPixmap;

    /// Returns the Material Icons font.
    static auto font(int size = 20) -> QFont;

private:
    static inline QFont iconFont_{};
    static inline bool fontLoaded_ = false;

    /// App icon SVG data (embedded as raw string literal).
    static auto appIconSvg() -> const char*;
};
