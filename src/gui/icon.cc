#include "icon.h"
#include <QFontDatabase>
#include <QPainter>
#include <QSvgRenderer>

auto IconProvider::initFont() -> bool {
    if (fontLoaded_) return true;
    if (!QFontDatabase::hasFamily("Material Icons"))
        return false;
    iconFont_ = QFont("Material Icons");
    iconFont_.setStyleStrategy(QFont::PreferQuality);
    fontLoaded_ = true;
    return true;
}

auto IconProvider::font(int size) -> QFont {
    initFont();
    QFont f = iconFont_;
    f.setPixelSize(size);
    return f;
}

auto IconProvider::themedIcon(const QString& name,
                              const QColor& color,
                              int size) -> QPixmap {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(color);
    p.setFont(font(size));
    // Map icon name to Material Icons unicode codepoint
    // (the font maps ligature names — use the name as text directly)
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, name);
    p.end();
    return pm;
}

auto IconProvider::appIcon() -> QPixmap {
    // Render embedded SVG to QPixmap at 64x64 (standard app icon size)
    QSvgRenderer renderer { QByteArray(appIconSvg()) };
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p);
    p.end();
    return pm;
}

auto IconProvider::appIconSvg() -> const char* {
    // Key + sudo icon. Primary color #0059C7, white details.
    return R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <circle cx="28" cy="30" r="10" fill="none" stroke="#0059C7" stroke-width="3.5"/>
  <circle cx="28" cy="30" r="3" fill="#0059C7"/>
  <path d="M36 30h14l-3-3 3-3h-14" fill="none" stroke="#0059C7" stroke-width="3"
        stroke-linecap="round" stroke-linejoin="round"/>
  <rect x="6" y="48" width="52" height="10" rx="3" fill="#0059C7" opacity="0.15"/>
  <text x="32" y="56.5" text-anchor="middle" fill="#0059C7"
        font-size="7" font-weight="bold" font-family="sans-serif">sudo</text>
</svg>
    )svg";
}
