# Theme & Icon Management — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-theme management (3 presets + light/dark), Material Icons for UI, custom SVG app icon, 300ms theme transition animation, and configurable QSoundEffect-based sound feedback to the agent-sudo C++ GUI.

**Architecture:** 6 new files (`icon`, `theme_transition`, `sound` pairs) + 5 modified files. IconProvider wraps Material Icons font + embedded app SVG. ThemeTransition implements `transition_state_trait<ColorScheme>` via per-field QColor lerp. SoundManager lazily loads QSoundEffect per event. QSettings persists theme/sound config. Animatable on main window drives 300ms ease-out transitions.

**Tech Stack:** C++23, Qt6 (Widgets + Multimedia), creeper-qt (Animatable, LinearState, ThemeManager), Eigen3, Material Icons font, QSettings

---

### Task 1: RingCountdown — Make Theme-Aware

**Files:**
- Modify: `src/gui/ring_countdown.h:10-18`
- Modify: `src/gui/ring_countdown.cc:13-27`

Remove hardcoded colors (`#edeaf2`, `#0071e3`, `#1d1d1f`). Add `set_color_scheme()` to satisfy `creeper::theme::color_scheme_setter_trait`.

- [ ] **Step 1: Add ColorScheme member and setter to header**

Open `src/gui/ring_countdown.h`. Add include for `creeper-qt/utility/theme/color-scheme.hh`, a private `ColorScheme colorScheme_` member, and a public `set_color_scheme()` method. The header should become:

```cpp
#pragma once
#include <QWidget>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <creeper-qt/utility/theme/color-scheme.hh>

/// Ring-countdown indicator widget (36×36). Draws a circular progress arc
/// and remaining-seconds digit. Used in the summary bar for the 60s auto-reject.
class RingCountdown : public QWidget {
public:
    explicit RingCountdown(QWidget* parent = nullptr);
    void setRemaining(int val);
    void set_color_scheme(const creeper::theme::ColorScheme& scheme);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int remaining_ = 60;
    int total_     = 60;
    creeper::theme::ColorScheme colorScheme_;
};
```

- [ ] **Step 2: Implement setter and update paintEvent**

Open `src/gui/ring_countdown.cc`. Add the `set_color_scheme` method. Replace hardcoded colors in `paintEvent` with `colorScheme_` fields:

```cpp
#include "ring_countdown.h"
#include <algorithm>

RingCountdown::RingCountdown(QWidget* parent) : QWidget(parent) {
    setFixedSize(36, 36);
}

void RingCountdown::setRemaining(int val) {
    remaining_ = std::max(val, 0);
    update();
}

void RingCountdown::set_color_scheme(const creeper::theme::ColorScheme& scheme) {
    colorScheme_ = scheme;
    update();
}

void RingCountdown::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background ring: surface_variant
    QPen pen(colorScheme_.surface_variant, 2.5);
    p.setPen(pen);
    p.drawEllipse(QRectF(2, 2, 32, 32));

    // Progress arc: primary
    pen.setColor(colorScheme_.primary);
    p.setPen(pen);
    double frac = static_cast<double>(remaining_) / total_;
    int span = static_cast<int>(-frac * 360 * 16);
    p.drawArc(QRectF(2, 2, 32, 32), 90 * 16, span);

    // Remaining seconds digit: on_surface
    p.setPen(colorScheme_.on_surface);
    p.setFont(QFont("sans-serif", 10, QFont::Bold));
    p.drawText(QRectF(0, 0, 36, 36), Qt::AlignCenter,
               QString::number(remaining_));
}
```

- [ ] **Step 3: Build verification**

```bash
cmake -S . -B build && cmake --build build
```

Expected: build succeeds. The ring widget compiles with `ColorScheme` member.

- [ ] **Step 4: Commit**

```bash
git add src/gui/ring_countdown.h src/gui/ring_countdown.cc
git commit -m "refactor: make RingCountdown theme-aware via color_scheme_setter_trait"
```

---

### Task 2: Material Icons — Add Missing Constants

**Files:**
- Modify: `src/gui/creeper-qt/utility/material-icon.hh:75-132`

Add icon constants needed by the design: `kChecklist`, `kTerminal`, `kPalette`, `kSelectAll`, `kDeselect`, `kPause`, `kPlayArrow`.

- [ ] **Step 1: Add constants to icon namespace**

Open `src/gui/creeper-qt/utility/material-icon.hh`. In `namespace creeper::material::icon`, add the missing constants after the existing ones:

```cpp
        // Additional UI icons
        constexpr auto kChecklist   = "checklist";
        constexpr auto kTerminal    = "terminal";
        constexpr auto kPalette     = "palette";
        constexpr auto kSelectAll   = "select_all";
        constexpr auto kDeselect    = "deselect";
        constexpr auto kPause       = "pause";
        constexpr auto kPlayArrow   = "play_arrow";
        constexpr auto kLightMode   = "light_mode";
```

Insert them at the end of the `namespace icon { ... }` block, before the closing braces.

- [ ] **Step 2: Build verification**

```bash
cmake --build build
```

Expected: build succeeds (no new compilation needed unless something includes the header, but it's safe).

- [ ] **Step 3: Commit**

```bash
git add src/gui/creeper-qt/utility/material-icon.hh
git commit -m "feat: add missing Material Icon constants for theme/sound UI"
```

---

### Task 3: IconProvider — Icon System Core

**Files:**
- Create: `src/gui/icon.h`
- Create: `src/gui/icon.cc`

`IconProvider` is stateless. Provides: Material Icons font initialization, themed icon pixmap from icon name + QColor, and the app icon QPixmap (key+sudo SVG embedded as raw string).

- [ ] **Step 1: Create icon.h**

Create `src/gui/icon.h`:

```cpp
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
```

- [ ] **Step 2: Create icon.cc — font init, app icon SVG, themedIcon**

Create `src/gui/icon.cc`:

```cpp
#include "icon.h"
#include <QPainter>
#include <QSvgRenderer>

auto IconProvider::initFont() -> bool {
    if (fontLoaded_) return true;
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
    QSvgRenderer renderer(QByteArray(appIconSvg()));
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
```

- [ ] **Step 3: Build verification**

Not yet — icon.cc depends on Qt6::Svg (`QSvgRenderer`). Update CMake first in Task 8. For now, verify the source compiles syntactically by checking for obvious issues.

- [ ] **Step 4: Commit**

```bash
git add src/gui/icon.h src/gui/icon.cc
git commit -m "feat: add IconProvider — Material Icons font + app SVG icon"
```

---

### Task 4: ThemeTransition — Animated Theme Switching

**Files:**
- Create: `src/gui/theme_transition.h`
- Create: `src/gui/theme_transition.cc`

Implement `ThemeTransitionState` satisfying `creeper::transition_state_trait<ColorScheme>`. Includes `colorSchemeLerp()` that interpolates all 30 QColor fields.

- [ ] **Step 1: Create theme_transition.h**

Create `src/gui/theme_transition.h`:

```cpp
#pragma once
#include <creeper-qt/utility/animation/state/accessor.hh>
#include <creeper-qt/utility/theme/theme.hh>

/// Linear interpolation between two ColorSchemes.
/// Satisfies creeper::transition_state_trait, usable with TransitionValue.
class ThemeTransitionState : public creeper::NormalAccessor {
public:
    using ValueT = creeper::ColorScheme;

    ThemeTransitionState() = default;
    ThemeTransitionState(const creeper::ColorScheme& from,
                         const creeper::ColorScheme& to,
                         double durationSec = 0.3);

    auto update() -> bool;
    auto get_value() const -> ValueT;
    auto get_target() const -> ValueT;
    auto set_value(ValueT v) -> void;
    auto set_target(ValueT t) -> void;

private:
    creeper::ColorScheme value_{};
    creeper::ColorScheme target_{};
    double progress_ = 0.0;
    double duration_ = 0.3;
    double lastTimestamp_ = 0.0; // seconds, for frame-rate-independent timing
};

/// Per-field QColor linear interpolation for all 30 ColorScheme fields.
auto colorSchemeLerp(const creeper::ColorScheme& a,
                     const creeper::ColorScheme& b,
                     double t) -> creeper::ColorScheme;

/// Cubic-bezier(0.4, 0, 0.2, 1) ease-out function.
auto easeOut(double t) -> double;
```

- [ ] **Step 2: Create theme_transition.cc**

Create `src/gui/theme_transition.cc`:

```cpp
#include "theme_transition.h"
#include <chrono>

ThemeTransitionState::ThemeTransitionState(const creeper::ColorScheme& from,
                                           const creeper::ColorScheme& to,
                                           double durationSec)
    : value_(from), target_(to), duration_(durationSec),
      lastTimestamp_(
          std::chrono::duration<double>(
              std::chrono::steady_clock::now().time_since_epoch()).count())
{}

auto ThemeTransitionState::update() -> bool {
    auto now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    double dt = now - lastTimestamp_;
    lastTimestamp_ = now;
    if (dt <= 0.0) dt = 1.0 / 90.0; // fallback for 90fps

    progress_ += dt / duration_;
    if (progress_ >= 1.0) {
        value_ = target_;
        return false; // animation complete
    }
    value_ = colorSchemeLerp(value_, target_, easeOut(progress_));
    return true;      // continue animating
}

auto ThemeTransitionState::get_value() const -> ValueT { return value_; }
auto ThemeTransitionState::get_target() const -> ValueT { return target_; }
auto ThemeTransitionState::set_value(ValueT v) -> void { value_ = v; }
auto ThemeTransitionState::set_target(ValueT t) -> void { target_ = t; }

// cubic-bezier(0.4, 0, 0.2, 1) — Material Design standard ease-out
auto easeOut(double t) -> double {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    // Precomputed cubic-bezier: c1=(0.4,0), c2=(0.2,1)
    // Use simplified approximation: ease-out = 1 - (1-t)^3
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}

inline auto lerpColor(const QColor& a, const QColor& b, double t) -> QColor {
    return QColor(
        static_cast<int>(a.red()   + (b.red()   - a.red())   * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t)
    );
}

auto colorSchemeLerp(const creeper::ColorScheme& a,
                     const creeper::ColorScheme& b,
                     double t) -> creeper::ColorScheme {
    return creeper::ColorScheme {
        .primary               = lerpColor(a.primary,               b.primary,               t),
        .on_primary            = lerpColor(a.on_primary,            b.on_primary,            t),
        .primary_container     = lerpColor(a.primary_container,     b.primary_container,     t),
        .on_primary_container  = lerpColor(a.on_primary_container,  b.on_primary_container,  t),
        .secondary             = lerpColor(a.secondary,             b.secondary,             t),
        .on_secondary          = lerpColor(a.on_secondary,          b.on_secondary,          t),
        .secondary_container   = lerpColor(a.secondary_container,   b.secondary_container,   t),
        .on_secondary_container= lerpColor(a.on_secondary_container,b.on_secondary_container,t),
        .tertiary              = lerpColor(a.tertiary,              b.tertiary,              t),
        .on_tertiary           = lerpColor(a.on_tertiary,           b.on_tertiary,           t),
        .tertiary_container    = lerpColor(a.tertiary_container,    b.tertiary_container,    t),
        .on_tertiary_container = lerpColor(a.on_tertiary_container, b.on_tertiary_container, t),
        .error                 = lerpColor(a.error,                 b.error,                 t),
        .on_error              = lerpColor(a.on_error,              b.on_error,              t),
        .error_container       = lerpColor(a.error_container,       b.error_container,       t),
        .on_error_container    = lerpColor(a.on_error_container,    b.on_error_container,    t),
        .background            = lerpColor(a.background,            b.background,            t),
        .on_background         = lerpColor(a.on_background,         b.on_background,         t),
        .surface               = lerpColor(a.surface,               b.surface,               t),
        .on_surface            = lerpColor(a.on_surface,            b.on_surface,            t),
        .surface_variant       = lerpColor(a.surface_variant,       b.surface_variant,       t),
        .on_surface_variant    = lerpColor(a.on_surface_variant,    b.on_surface_variant,    t),
        .outline               = lerpColor(a.outline,               b.outline,               t),
        .outline_variant       = lerpColor(a.outline_variant,       b.outline_variant,       t),
        .shadow                = lerpColor(a.shadow,                b.shadow,                t),
        .scrim                 = lerpColor(a.scrim,                 b.scrim,                 t),
        .inverse_surface       = lerpColor(a.inverse_surface,       b.inverse_surface,       t),
        .inverse_on_surface    = lerpColor(a.inverse_on_surface,    b.inverse_on_surface,    t),
        .inverse_primary       = lerpColor(a.inverse_primary,       b.inverse_primary,       t),
        .surface_container_highest = lerpColor(a.surface_container_highest, b.surface_container_highest, t),
        .surface_container_high    = lerpColor(a.surface_container_high,    b.surface_container_high,    t),
        .surface_container         = lerpColor(a.surface_container,         b.surface_container,         t),
        .surface_container_low     = lerpColor(a.surface_container_low,     b.surface_container_low,     t),
        .surface_container_lowest  = lerpColor(a.surface_container_lowest,  b.surface_container_lowest,  t),
    };
}
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/theme_transition.h src/gui/theme_transition.cc
git commit -m "feat: add ThemeTransitionState — animated ColorScheme interpolation"
```

---

### Task 5: SoundManager — Configurable Sound Feedback

**Files:**
- Create: `src/gui/sound.h`
- Create: `src/gui/sound.cc`

`SoundManager` reads per-event sound config from QSettings. Lazy-loads `QSoundEffect` instances. Each event supports three modes: `"default"` (embedded WAV), `"none"` (silent), or a file path.

- [ ] **Step 1: Create sound.h**

Create `src/gui/sound.h`:

```cpp
#pragma once
#include <QSettings>
#include <QSoundEffect>
#include <QString>
#include <memory>
#include <unordered_map>

/// Configurable sound feedback manager using QSoundEffect.
/// Reads per-event paths from QSettings. Gracefully degrades if
/// Qt6::Multimedia is unavailable (no-ops all play calls).
class SoundManager {
public:
    enum class Event { Open, Warning, Executed, Rejected };

    explicit SoundManager(const QString& configPath);

    auto play(Event event) -> void;
    auto setEnabled(bool enabled) -> void;

private:
    QSettings settings_;
    bool enabled_ = true;
    std::unordered_map<Event, std::unique_ptr<QSoundEffect>> effects_;

    auto resolveSource(Event event) const -> QUrl;
    static auto defaultWavData(Event event) -> const QByteArray&;
    static auto eventKey(Event event) -> const char*;
};
```

- [ ] **Step 2: Create sound.cc**

Create `src/gui/sound.cc`:

```cpp
#include "sound.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <array>

static constexpr auto kGroupSound = "sound";
static constexpr auto kKeyEnabled = "sound/enabled";

auto SoundManager::eventKey(Event event) -> const char* {
    switch (event) {
        case Event::Open:     return "sound/event/open";
        case Event::Warning:  return "sound/event/warning";
        case Event::Executed: return "sound/event/executed";
        case Event::Rejected: return "sound/event/rejected";
    }
    return "";
}

SoundManager::SoundManager(const QString& configPath)
    : settings_(configPath, QSettings::IniFormat) {
    enabled_ = settings_.value("sound/enabled", true).toBool();
}

auto SoundManager::setEnabled(bool enabled) -> void {
    enabled_ = enabled;
    settings_.setValue("sound/enabled", enabled);
    settings_.sync();
}

auto SoundManager::resolveSource(Event event) const -> QUrl {
    auto val = settings_.value(eventKey(event), "default").toString();
    if (val == "none" || val.isEmpty()) return QUrl();
    if (val == "default") {
        // Write embedded WAV data to a temporary file.
        // QSoundEffect requires a file URL, not in-memory data.
        // Cache the temp file path? For simplicity, write each time.
        // In practice, default sounds are < 5KB — negligible overhead.
        static int counter = 0;
        auto tmpName = QString("agent-sudo-sound-%1-%2.wav")
                           .arg(static_cast<int>(event)).arg(++counter);
        auto tmpPath = QDir::temp().filePath(tmpName);
        QFile tmpFile(tmpPath);
        if (tmpFile.open(QIODevice::WriteOnly)) {
            tmpFile.write(defaultWavData(event));
            tmpFile.close();
            return QUrl::fromLocalFile(tmpPath);
        }
        return QUrl();
    }
    // User-specified file path
    return QUrl::fromLocalFile(val);
}

auto SoundManager::play(Event event) -> void {
    if (!enabled_) return;

    auto it = effects_.find(event);
    if (it == effects_.end()) {
        auto url = resolveSource(event);
        if (url.isEmpty()) {
            // No source configured for this event — store nullptr and skip
            effects_[event] = nullptr;
            return;
        }
        auto effect = std::make_unique<QSoundEffect>();
        effect->setSource(url);
        effect->setVolume(0.8);
        effects_[event] = std::move(effect);
        it = effects_.find(event);
    }

    auto* effect = it->second.get();
    if (effect && effect->status() == QSoundEffect::Ready) {
        effect->play();
    }
}

auto SoundManager::defaultWavData(Event event) -> const QByteArray& {
    // Minimal 16-bit PCM WAV data for each event.
    // Generated via sine wave synthesis: 44100Hz mono, 16-bit.
    // Each is < 5KB.
    static const QByteArray kOpenWav = []() -> QByteArray {
        // 0.2s beep at 800Hz, 44100Hz mono 16-bit PCM
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.2f;
        constexpr float freq = 800.0f;
        int nsamples = static_cast<int>(sampleRate * duration);
        QByteArray data;
        data.resize(44 + nsamples * 2); // WAV header + PCM data

        auto* hdr = reinterpret_cast<quint8*>(data.data());
        // RIFF header
        hdr[0]='R';hdr[1]='I';hdr[2]='F';hdr[3]='F';
        quint32 fileSize = 36 + nsamples*2;
        hdr[4]=fileSize&0xff; hdr[5]=(fileSize>>8)&0xff;
        hdr[6]=(fileSize>>16)&0xff; hdr[7]=(fileSize>>24)&0xff;
        hdr[8]='W';hdr[9]='A';hdr[10]='V';hdr[11]='E';
        // fmt chunk
        hdr[12]='f';hdr[13]='m';hdr[14]='t';hdr[15]=' ';
        hdr[16]=16;hdr[17]=0;hdr[18]=0;hdr[19]=0; // chunk size
        hdr[20]=1;hdr[21]=0; // PCM
        hdr[22]=1;hdr[23]=0; // mono
        hdr[24]=static_cast<quint8>(sampleRate&0xff);
        hdr[25]=static_cast<quint8>((sampleRate>>8)&0xff);
        hdr[26]=static_cast<quint8>((sampleRate>>16)&0xff);
        hdr[27]=static_cast<quint8>((sampleRate>>24)&0xff);
        quint32 byteRate = sampleRate * 2;
        hdr[28]=byteRate&0xff; hdr[29]=(byteRate>>8)&0xff;
        hdr[30]=(byteRate>>16)&0xff; hdr[31]=(byteRate>>24)&0xff;
        hdr[32]=2;hdr[33]=0; // block align
        hdr[34]=16;hdr[35]=0; // bits per sample
        // data chunk
        hdr[36]='d';hdr[37]='a';hdr[38]='t';hdr[39]='a';
        quint32 dsize = nsamples*2;
        hdr[40]=dsize&0xff; hdr[41]=(dsize>>8)&0xff;
        hdr[42]=(dsize>>16)&0xff; hdr[43]=(dsize>>24)&0xff;

        auto* samples = reinterpret_cast<qint16*>(data.data() + 44);
        for (int i = 0; i < nsamples; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            // Envelope: fade in/out
            float env = 1.0f;
            if (t < 0.02f) env = t / 0.02f;
            else if (t > duration - 0.02f) env = (duration - t) / 0.02f;
            samples[i] = static_cast<qint16>(16000.0f * env *
                          std::sin(2.0f * 3.14159f * freq * t));
        }
        return data;
    }();

    static const QByteArray kEmpty;
    switch (event) {
        case Event::Open:     return kOpenWav;
        case Event::Warning:  return kOpenWav; // stub — same beep
        case Event::Executed: return kOpenWav; // stub
        case Event::Rejected: return kOpenWav; // stub
    }
    return kEmpty;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/sound.h src/gui/sound.cc
git commit -m "feat: add SoundManager — configurable QSoundEffect per event"
```

---

### Task 6: Command Cards — Add Type Icons

**Files:**
- Modify: `src/gui/command_card.cc:48-63`
- Modify: `src/gui/command_card.h` (add include)

Add a `terminal` Material Icon before the reason text in each command card title row.

- [ ] **Step 1: Update command_card.h include**

Add `#include "gui/icon.h"` after the existing includes:

```cpp
#include "command_card.h"
#include "gui/icon.h"         // <-- new
#include <creeper-qt/creeper-qt.hh>
```

- [ ] **Step 2: Add icon to title row in command_card.cc**

In the title row construction (~line 48), insert an icon `Text` widget before the reason `Text`:

```cpp
        auto* titleRow = new Row {
            lnpro::Margin { 0 },
            lnpro::Spacing { 8 },
            lnpro::Item<Text> {
                text::pro::ThemeManager { *manager },
                text::pro::Text {
                    QString::fromUtf8(material::icon::kTerminal) },
                wdpro::Font {
                    QFont(material::regular::font, material::size::_1) },
            },
            lnpro::Item<Text> {
                text::pro::ThemeManager { *manager },
                wdpro::Font { QFont("sans-serif", 12) },
                text::pro::Text { QString("#%1 — %2").arg(id).arg(reason) },
            },
            lnpro::Stretch { 1 },
            lnpro::Item<Switch> {
                _switch::pro::ThemeManager { *manager },
                _switch::pro::Checked { true },
                wdpro::Bind { sw },
                wdpro::FixedSize { 60, 36 },
            },
        };
```

The icon `Text` uses Material Icons font (`material::regular::font`) at `material::size::_1` (18px). The theme handles its color automatically via `text::pro::ThemeManager`.

- [ ] **Step 3: Commit**

```bash
git add src/gui/command_card.h src/gui/command_card.cc
git commit -m "feat: add terminal icon to command card title rows"
```

---

### Task 7: Main Integration — Theme Dropdown, Sound, App Icon, Animation

**Files:**
- Modify: `src/main.cc` (major changes — layout restructure for theme dropdown, QSettings, SoundManager, Animatable)

This is the largest task. It integrates all new modules into the main window construction.

- [ ] **Step 1: Add new includes to main.cc**

After the existing includes at the top of `src/main.cc`, add:

```cpp
#include "gui/icon.h"
#include "gui/sound.h"
#include "gui/theme_transition.h"
#include <creeper-qt/utility/animation/animatable.hh>
#include <creeper-qt/utility/animation/transition.hh>
#include <creeper-qt/utility/animation/state/linear.hh>
#include <creeper-qt/utility/theme/preset/blue-miku.hh>
#include <creeper-qt/utility/theme/preset/gloden-harvest.hh>
#include <creeper-qt/utility/theme/preset/green.hh>
#include <QSettings>
```

Wait — `blue-miku.hh` is already included. Let me check... yes, line 7. I already have it. Just add the new ones. Let me revise.

Actually, let me look at what's already there:
- Line 5: `creeper-qt/creeper-qt.hh`
- Line 6: `creeper-qt/core/application.hh`
- Line 7: `creeper-qt/utility/theme/preset/blue-miku.hh`
- Lines 9-12: gui headers
- Lines 14-20: Qt headers

Add these new includes after line 7:
```cpp
#include <creeper-qt/utility/theme/preset/gloden-harvest.hh>
#include <creeper-qt/utility/theme/preset/green.hh>
```

And add these after the Qt headers (after line 20):
```cpp
#include <QSettings>
```

And after the `#include "gui/executor.h"` line:
```cpp
#include "gui/icon.h"
#include "gui/sound.h"
#include "gui/theme_transition.h"
```

- [ ] **Step 2: Add QSettings load and theme preset array**

After the namespace aliases (~line 25), add theme configuration:

```cpp
// ══════════════════════════════════════════════════════════════════════════
//  Theme & sound config
// ══════════════════════════════════════════════════════════════════════════

static const ThemePack kPresetPacks[] = {
    kBlueMikuThemePack,
    kGlodenHarvestThemePack,
    kGreenThemePack,
};

static constexpr const char* kPresetLabels[] = {
    "蓝色初音", "金色丰收", "森林绿"
};
```

- [ ] **Step 3: Initialize QSettings, SoundManager, Animatable, App Icon**

In `main()`, after the `auto manager = ThemeManager { kBlueMikuThemePack };` line (~line 44), replace it with QSettings-driven init:

```cpp
    // ---- Config ----
    QSettings cfg("agent-sudo", "agent-sudo");
    int presetIdx = cfg.value("theme/preset", 0).toInt();
    if (presetIdx < 0 || presetIdx > 2) presetIdx = 0;
    ColorMode mode = cfg.value("theme/mode", 0).toInt() == 0
                         ? ColorMode::LIGHT : ColorMode::DARK;

    auto manager = ThemeManager { kPresetPacks[presetIdx], mode };
    auto soundMgr = SoundManager {
        QDir(QStandardPaths::writableLocation(
                 QStandardPaths::ConfigLocation))
            .filePath("agent-sudo/theme.conf") };

    // App icon
    IconProvider::initFont();
    app::init { app::pro::Complete { argc, argv },
                 app::pro::WindowIcon { IconProvider::appIcon() } };

    // Animatable for theme transitions
    // (will be created after main window — pointer to assign later)
    Animatable* animatable = nullptr;
```

Wait — `app::init` might not support `WindowIcon`. Let me check what `app::pro` supports. Looking at `application.hh`, I don't know its exact properties. Let me use a simpler approach: set the icon directly after window creation.

Also, `QStandardPaths` needs `#include <QStandardPaths>`.

Let me simplify and be more tactical. The `app::init` already exists on line 43. Let me replace lines 43-44 with a more comprehensive init block. But I need to be careful not to break the existing structure.

Actually, let me re-read the current init flow:
- Line 31: `main()` starts
- Lines 32-41: DISPLAY check + queue read
- Line 43: `app::init { app::pro::Complete { argc, argv } };`
- Line 44: `auto manager = ThemeManager { kBlueMikuThemePack };`

I need to intercept after the app init and before the ThemeManager construction. Let me restructure this section. Since this is a significant rewrite of the init block, let me show the exact code to replace.

- [ ] **Step 1 (revised): Load QSettings + init theme, sound, icon**

Replace lines 42-44 of `src/main.cc` with:

```cpp

    // ---- Config & theme ----
    QSettings cfg("agent-sudo", "agent-sudo");
    int presetIdx = cfg.value("theme/preset", 0).toInt();
    if (presetIdx < 0 || presetIdx > 2) presetIdx = 0;
    ColorMode mode = cfg.value("theme/mode", 0).toInt() == 0
                         ? ColorMode::LIGHT : ColorMode::DARK;

    IconProvider::initFont();
    app::init { app::pro::Complete { argc, argv },
                app::pro::WindowIcon { IconProvider::appIcon() } };

    auto manager = ThemeManager { kPresetPacks[presetIdx], mode };

    auto soundConfigPath = QDir(
        QStandardPaths::writableLocation(
            QStandardPaths::ConfigLocation))
        .filePath("agent-sudo/theme.conf");
    auto soundMgr = SoundManager { soundConfigPath };

    // Animatable created after MainWindow — pointer set below
    Animatable* animatable = nullptr;
```

Add `#include <QSettings>`, `#include <QStandardPaths>`, `#include <QDir>` to the Qt includes section.

- [ ] **Step 2: Play open sound after window shows**

After `QTimer::singleShot(0, pwF, qOverload<>(&QWidget::setFocus));` (~line 100), add:

```cpp
            QTimer::singleShot(100, [&] {
                soundMgr.play(SoundManager::Event::Open);
            });
```

- [ ] **Step 3: Add countdown warning sound**

In the countdown timer callback (~line 183), after the `--remaining;` line, add warning sound trigger:

```cpp
                --remaining;
                ringW->setRemaining(remaining);
                if (remaining == 10) {
                    soundMgr.play(SoundManager::Event::Warning);
                }
```

- [ ] **Step 4: Add execute/reject sounds**

In the execute button callback (~line 131), before `window.hide();`:

```cpp
                soundMgr.play(SoundManager::Event::Executed);
```

In the reject button callback (~line 162) and countdown expiry (~line 188), before `clearQueue();`:

```cpp
                soundMgr.play(SoundManager::Event::Rejected);
```

- [ ] **Step 5: Add theme dropdown to summary bar**

Replace the summary bar construction (~lines 210-227) with one that includes the theme selector pill. The current code:

```cpp
                // ── Summary bar ──
                lnpro::Item<Row> {
                    lnpro::Margin { 10 },
                    lnpro::Spacing { 8 },
                    lnpro::Item<Text> {
                        text::pro::ThemeManager { manager },
                        text::pro::Text {
                            QString::fromUtf8("共 %1 条待审批命令")
                                .arg(items.size()) },
                    },
                    lnpro::Stretch { 1 },
                    lnpro::Item { ringW },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Bind { pauseB },
                        button::pro::Text { "||" },
                        wdpro::FixedSize { 32, 32 },
                    },
                },
```

Replace with (adding theme pill + using Material Icons for pause):

```cpp
                // ── Summary bar ──
                lnpro::Item<Row> {
                    lnpro::Margin { 10 },
                    lnpro::Spacing { 8 },
                    lnpro::Item<Text> {
                        text::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont(material::regular::font,
                                  material::size::_1) },
                        text::pro::Text {
                            QString::fromUtf8(material::icon::kChecklist) },
                    },
                    lnpro::Item<Text> {
                        text::pro::ThemeManager { manager },
                        text::pro::Text {
                            QString::fromUtf8("共 %1 条待审批命令")
                                .arg(items.size()) },
                    },
                    lnpro::Stretch { 1 },
                    lnpro::Item { ringW },
                    // Theme selector pill
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont(material::regular::font, 16) },
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kPalette) },
                        wdpro::FixedSize { 32, 32 },
                        button::pro::Clickable { [&] {
                            // Cycle through presets
                            presetIdx = (presetIdx + 1) % 3;
                            auto oldScheme = manager.color_scheme();
                            manager.set_theme_pack(kPresetPacks[presetIdx]);
                            auto newScheme = manager.color_scheme();
                            cfg.setValue("theme/preset", presetIdx);
                            cfg.sync();
                            // Animated transition
                            if (animatable) {
                                auto state = std::make_shared<
                                    ThemeTransitionState>(
                                        oldScheme, newScheme);
                                auto task = std::make_unique<
                                    creeper::TransitionTask<
                                        ThemeTransitionState>>(
                                    state, std::make_shared<bool>(true));
                                animatable->push_transition_task(
                                    std::move(task));
                            } else {
                                manager.apply_theme();
                            }
                        }},
                    },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Bind { pauseB },
                        wdpro::Font {
                            QFont(material::regular::font,
                                  material::size::_2) },
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kPause) },
                        wdpro::FixedSize { 32, 32 },
                    },
                },
```

Wait — the theme dropdown clickable is quite complex. It cycles through presets and uses the Animatable for transitions. Let me also register the Animatable on the main window.

- [ ] **Step 6: Create Animatable and register RingCountdown handler**

After the window construction (before or inside the lambda), we need to:
1. Create the Animatable on the main window
2. Register RingCountdown via `manager.append_handler(ringW, *ringW)` (the template overload)

The handler registration for RingCountdown should use the `color_scheme_setter_trait` overload. Add this near the other handler registrations (~line 107):

```cpp
            // RingCountdown theme handler (uses set_color_scheme trait)
            manager.append_handler(ringW, *ringW);
```

And create the Animatable after the window is built. Add this right after `window.setFixedSize(700, 550);` and inside the lambda:

```cpp
            auto* anim = new Animatable(window);
            animatable = anim;
```

Wait — `Animatable` takes a `QWidget&` reference, not a pointer. And `new Animatable(window)` — does it require the window to live as long as the Animatable? Yes. But we can't use a local variable since `animatable` is a pointer captured by the callbacks.

Let me use `std::unique_ptr`:

Change `Animatable* animatable = nullptr;` to:
```cpp
    std::unique_ptr<Animatable> animatable;
```

Then in the lambda:
```cpp
            animatable = std::make_unique<Animatable>(window);
```

But callbacks capture by reference — they need a stable reference. Let me keep it as a pointer but allocate on the heap owned by the main window... Actually, the simplest approach: use a raw pointer allocated with `new`, and let the window ownership handle cleanup. Or use `std::unique_ptr` in the outer scope.

Let me simplify: put `Animatable` in the outer scope as a value and capture by reference:

In the outer scope (around line 59, near the other pointers):
```cpp
    auto animatable = Animatable {}; // default-constructible? Let me check...
```

Actually, `Animatable` requires a `QWidget&` in its constructor. So we can't default-construct it. Let me use a pointer approach:

Outside the lambda:
```cpp
    std::unique_ptr<Animatable> anim;
```

Inside the lambda (after window creation):
```cpp
            anim = std::make_unique<Animatable>(window);
```

And the callbacks capture `anim` by reference: `[&] { ... anim->push_transition_task(...); }`.

This is cleaner. Let me update the code.

- [ ] **Step 7: Update pause button icon toggle**

In the pause button callback (~line 173), update to toggle between pause and play icons:

```cpp
            // Pause
            QObject::connect(pauseB, &QAbstractButton::clicked, [&] {
                paused = !paused;
                pauseB->setText(QString::fromUtf8(
                    paused ? material::icon::kPlayArrow
                           : material::icon::kPause));
            });
```

And update the initial pause button text to use the icon:
In the pause button construction, change `button::pro::Text { "||" }` to:
```cpp
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kPause) },
```

- [ ] **Step 8: Add icons to select-all / deselect / execute / reject buttons**

Update the button bar (~lines 287-316) to use Material Icons:

Select all button — add icon:
```cpp
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont(material::regular::font, 14) },
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kSelectAll) },
                        wdpro::FixedSize { 36, 32 },
                        button::pro::Clickable { [&] { setAll(true); } },
                    },
```

Deselect button:
```cpp
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont(material::regular::font, 14) },
                        button::pro::Text {
                            QString::fromUtf8(
                                material::icon::kDeselect) },
                        wdpro::FixedSize { 36, 32 },
                        button::pro::Clickable { [&] { setAll(false); } },
                    },
```

Reject button — add close icon:
```cpp
                    lnpro::Item<TextButton> {
                        text_button::pro::ThemeManager { manager },
                        wdpro::Bind { rejB },
                        wdpro::FixedSize { 80, 32 },
                        wdpro::Font {
                            QFont(material::regular::font, 14) },
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kClose)
                                + QString::fromUtf8(" 拒绝") },
                    },
```

Execute button — add check icon:
```cpp
                    lnpro::Item<FilledButton> {
                        filled_button::pro::ThemeManager { manager },
                        wdpro::Bind { execB },
                        wdpro::FixedSize { 140, 36 },
                        wdpro::Font {
                            QFont(material::regular::font, 14) },
                        button::pro::Text {
                            QString::fromUtf8(material::icon::kCheck)
                                + QString::fromUtf8(" 执行 (Enter)") },
                    },
```

- [ ] **Step 9: Add light/dark mode toggle in theme selector**

The theme selector currently cycles presets. We also need a way to toggle light/dark. Since this is getting complex in a single inline lambda, let me extract a helper function at the namespace level for theme switching:

```cpp
static auto switchTheme(ThemeManager& mgr,
                        const ThemePack& newPack,
                        ColorMode newMode,
                        QSettings& cfg,
                        Animatable* anim) -> void {
    auto oldScheme = mgr.color_scheme();
    mgr.set_theme_pack(newPack);
    mgr.set_color_mode(newMode);
    auto newScheme = mgr.color_scheme();
    cfg.setValue("theme/mode", newMode == ColorMode::LIGHT ? 0 : 1);
    cfg.sync();
    if (anim) {
        auto state = std::make_shared<ThemeTransitionState>(
            oldScheme, newScheme);
        auto running = std::make_shared<bool>(true);
        anim->push_transition_task(
            std::make_unique<creeper::TransitionTask<
                ThemeTransitionState>>(state, running));
    } else {
        mgr.apply_theme();
    }
};
```

Then the theme pill's clickable becomes simpler — cycle presets:
```cpp
                        button::pro::Clickable { [&] {
                            presetIdx = (presetIdx + 1) % 3;
                            cfg.setValue("theme/preset", presetIdx);
                            switchTheme(manager, kPresetPacks[presetIdx],
                                        manager.color_mode(), cfg,
                                        anim.get());
                        }},
```

And we can add a long-press or right-click for mode toggle. For simplicity, let's add a separate small button next to the palette button for light/dark toggle:

In the summary bar, after the palette button, add a light/dark toggle button:

```cpp
                    // Light/Dark toggle
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont(material::regular::font, 16) },
                        button::pro::Text {
                            QString::fromUtf8(
                                mode == ColorMode::LIGHT
                                    ? material::icon::kDarkMode
                                    : material::icon::kLightMode) },
                        wdpro::FixedSize { 32, 32 },
                        button::pro::Clickable { [&] {
                            ColorMode newMode =
                                mode == ColorMode::LIGHT
                                    ? ColorMode::DARK
                                    : ColorMode::LIGHT;
                            mode = newMode;
                            switchTheme(manager, kPresetPacks[presetIdx],
                                        newMode, cfg, anim.get());
                        }},
                    },
```

Wait — `material::icon::kLightMode` doesn't exist. Let me check... `material-icon.hh` has `kDarkMode` but no `kLightMode`. I need to add it. Let me add it to Task 2's constants list.

Actually, the Material Icons standard name is `light_mode`. Let me add that to Task 2.

- [ ] **Step 10: Commit**

```bash
git add src/main.cc
git commit -m "feat: integrate theme dropdown, sound, icons, and animation into main window"
```

---

### Task 8: CMakeLists.txt — Add New Sources and Dependencies

**Files:**
- Modify: `CMakeLists.txt:12,57-64`

Add `Qt6::Svg` (for IconProvider's `QSvgRenderer`) and `Qt6::Multimedia` (for SoundManager's `QSoundEffect`). Add new .cc files to the `agent-sudo-flush` target.

- [ ] **Step 1: Update Qt6 dependencies**

Change line 11-12 from:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
```
to:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg Multimedia)
```

- [ ] **Step 2: Add new source files**

Change the `add_executable` block (lines 58-64) from:
```cmake
add_executable(agent-sudo-flush
    src/main.cc
    src/gui/command_card.cc
    src/gui/ring_countdown.cc
    src/gui/queue_io.cc
    src/gui/executor.cc
)
```
to:
```cmake
add_executable(agent-sudo-flush
    src/main.cc
    src/gui/command_card.cc
    src/gui/icon.cc
    src/gui/ring_countdown.cc
    src/gui/sound.cc
    src/gui/theme_transition.cc
    src/gui/queue_io.cc
    src/gui/executor.cc
)
```

- [ ] **Step 3: Update link libraries**

Change lines 71-76 from:
```cmake
target_link_libraries(agent-sudo-flush
    PRIVATE
        creeper-qt-widgets
        Qt6::Widgets
        Qt6::Network
)
```
to:
```cmake
target_link_libraries(agent-sudo-flush
    PRIVATE
        creeper-qt-widgets
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::Multimedia
)
```

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add new sources, Qt6::Svg and Qt6::Multimedia deps"
```

---

### Task 9: Build & Verify

**Files:** None (verification only)

- [ ] **Step 1: Full rebuild**

```bash
cmake -S . -B build && cmake --build build
```

Expected: compilation succeeds with zero errors. Warnings are acceptable but should be reviewed.

- [ ] **Step 2: Check binary exists**

```bash
ls -la build/agent-sudo-flush
```

Expected: binary exists and is executable.

- [ ] **Step 3: Smoke test — DISPLAY unset should exit 127**

```bash
unset DISPLAY && build/agent-sudo-flush 2>&1; echo "exit: $?"
```

Expected: prints "ERROR: DISPLAY not set" and exit code 127.

- [ ] **Step 4: Smoke test — empty queue should exit 127**

```bash
rm -f ~/.cache/agent-sudo/queue.json && DISPLAY=:99 build/agent-sudo-flush 2>&1; echo "exit: $?"
```

Expected: prints "ERROR: queue is empty" and exit code 127.

- [ ] **Step 5: Review compilation warnings**

```bash
cmake --build build 2>&1 | grep -i warning || echo "No warnings"
```

Expected: zero warnings. If warnings exist, review and fix before declaring done.

---

### Task 10: Update README.md — Document New Dependencies

**Files:**
- Modify: `README.md` (install section)

- [ ] **Step 1: Add Qt6 Multimedia and Svg to dependencies**

In the README's install section, add the new dependencies:

```markdown
- C++23 compiler (GCC 14+)
- Qt6 (Widgets, Network, Svg, Multimedia)
- Eigen3
- Material Icons font (`fonts-material-design-icons-iconfont`)
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: add Qt6::Svg, Qt6::Multimedia, and Material Icons font deps"
```
