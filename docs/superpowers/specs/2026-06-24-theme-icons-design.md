# Theme & Icon Management — Design Spec

Date: 2026-06-24
Status: approved
Scope: agent-sudo C++ rewrite branch

## Decisions

| Item | Choice |
|---|---|
| Scope | Full — multi-theme + icon system + animation + sound |
| Icon tech | Hybrid: Material Icons font for UI, custom SVG for app icon |
| App icon metaphor | Key + sudo (shield variant) |
| Theme switch UI | Dropdown selector (3 presets + light/dark toggle) |
| Theme persistence | QSettings (`~/.config/agent-sudo/theme.conf`) |
| Theme transition | 300ms ease-out color gradient (Animatable + LinearState) |
| Sound | QSoundEffect, 5 events, per-event configurable path or "default"/"none" |

## Architecture

### New Files (6)

| File | Purpose |
|---|---|
| `src/gui/icon.h` | IconProvider class — Material Icons font init + app SVG icon |
| `src/gui/icon.cc` | Implementation: `initFont()`, `appIcon()`, `themedIcon(name, color)` |
| `src/gui/theme_transition.h` | `ThemeTransitionState` — interpolates `ColorScheme` per `transition_state_trait` |
| `src/gui/theme_transition.cc` | `colorSchemeLerp(a, b, t)` — per-field QColor lerp for all 30 fields |
| `src/gui/sound.h` | `SoundManager` — QSoundEffect pool, per-event config, enable/disable |
| `src/gui/sound.cc` | Implementation: `play(Event)`, `resolveSource()`, embedded default WAV data |

### Modified Files (5)

| File | Changes |
|---|---|
| `src/main.cc` | Theme dropdown UI (pill + popup), QSettings load/save, Animatable for transition, SoundManager integration, app icon |
| `src/gui/command_card.cc` | Add Material Icon (`terminal`) before reason text in each card |
| `src/gui/ring_countdown.h` | Add `set_color_scheme(ColorScheme)`, implement `color_scheme_setter_trait` |
| `src/gui/ring_countdown.cc` | Replace hardcoded `#edeaf2`/`#0071e3`/`#1d1d1f` with `ColorScheme::primary`, `::surface_variant`, `::on_surface` |
| `CMakeLists.txt` | Add new .cc files + `Qt6::Multimedia` dependency |

### Dependencies

- **Qt6::Widgets** (existing)
- **Qt6::Network** (existing)
- **Qt6::Multimedia** (new — for QSoundEffect)
- **Eigen3** (existing)
- **creeper-qt vendored** (existing)
- **Material Icons font** — system package `fonts-material-design-icons-iconfont`

## Icon System

### App Icon

Custom SVG rendered to QPixmap, set via `QApplication::setWindowIcon()`. The SVG uses the "key + sudo" metaphor: a key shape with "sudo" text underneath. Colors derived from the active theme's primary color.

SVG embedded as C++ raw string literal (no external file dependency).

### UI Icons

All UI icons use Material Icons font via existing `material-icon.hh` constants:

| Location | Icon | Constant |
|---|---|---|
| Window title bar icon | (app icon SVG) | — |
| Summary bar command count | checklist | `kChecklist` (add to constants) |
| Command card type indicator | terminal | `icon::kTerminal` (add) |
| Theme selector pill | palette | `icon::kPalette` (add) |
| Pause/resume button | pause / play_arrow | `icon::kPause` / `kPlayArrow` (add) |
| Select all / deselect | select_all / deselect | `icon::kSelectAll` / `kDeselect` (add) |
| Execute button | check | `icon::kCheck` |
| Reject button | close | `icon::kClose` |

Missing icon constants will be added to `material-icon.hh`:
- `kChecklist`, `kTerminal`, `kPalette`, `kSelectAll`, `kDeselect`

## Theme System

### Theme Presets

| Index | Name | File |
|---|---|---|
| 0 | Blue Miku (default) | `preset/blue-miku.hh` |
| 1 | Golden Harvest | `preset/gloden-harvest.hh` |
| 2 | Forest Green | `preset/green.hh` |

### QSettings Schema

```ini
[theme]
preset=0   ; 0=BlueMiku, 1=GoldenHarvest, 2=Green
mode=0     ; 0=Light, 1=Dark

[sound]
enabled=true
event\open=default
event\warning=default
event\executed=default
event\rejected=none
```

### Theme Dropdown UI

A pill-shaped button in the summary bar (between countdown and pause button). On click, shows a popup with:
- 3 theme preset entries (name + color dot, checkmark on active)
- Divider
- Light / Dark mode toggle row

Selecting a preset or toggling mode triggers the theme transition animation.

### Theme Transition Animation

300ms, cubic-bezier(0.4, 0, 0.2, 1) easing (Material Design standard ease-out).

Data flow:
1. User selects new theme/mode → snapshot current `ColorScheme`
2. Set new target on `ThemeManager` (`set_theme_pack` / `set_color_mode`)
3. Create `ThemeTransitionState(old, new)` implementing `transition_state_trait<ColorScheme>`
4. Push as `TransitionTask` to main window's `Animatable`
5. 90fps frame loop: `colorSchemeLerp(old, new, progress)` → QColor lerp on all 30 fields
6. Each frame: `manager.apply_theme()` broadcasts intermediate scheme to all registered handlers
7. On completion: final apply + QSettings write

`ThemeTransitionState`:
```cpp
struct ThemeTransitionState : NormalAccessor {
    using ValueT = ColorScheme;
    ColorScheme value;   // current interpolated
    ColorScheme target;  // target
    double progress = 0.0;
    double duration = 0.3;  // seconds

    auto update() -> bool;  // increments progress, returns false when done
    auto get_value() const -> ColorScheme { return value; }
    auto get_target() const -> ColorScheme { return target; }
    auto set_target(ColorScheme t) -> void { target = t; progress = 0.0; }
    auto set_value(ColorScheme v) -> void { value = v; }
};
```

## Sound System

### Events

| Event | Trigger | Default behavior |
|---|---|---|
| `Open` | GUI initialized | Short chime (200ms) |
| `Warning` | Countdown ≤ 10s, fires once | Alert triple-beep |
| `Executed` | User clicks Execute | Success chime |
| `Rejected` | User clicks Reject or countdown expires | Short close sound |

### Configuration

Per-event value can be:
- `"default"` — use built-in WAV (compiled into binary, < 5KB each)
- `"none"` — silent for this event
- Absolute path to a `.wav` file (e.g., `/usr/share/sounds/freedesktop/stereo/message.oga`)

`sound/enabled=false` disables all sounds globally.

### Implementation

`SoundManager` owns a `std::unordered_map<Event, std::unique_ptr<QSoundEffect>>`. Effects are lazily created on first `play()`. `resolveSource(Event)` reads QSettings, returns `QUrl` (file path or embedded resource). Graceful degradation: if `Qt6::Multimedia` is unavailable at runtime, all `play()` calls are no-ops.

### Built-in Sound Data

Four small WAV files (≤ 5KB each) embedded as `constexpr std::array<uint8_t>`:
- `kDefaultOpenWav` — short pip (~200ms)
- `kDefaultWarningWav` — triple beep (~500ms)
- `kDefaultExecutedWav` — success chime (~400ms)
- `kDefaultRejectedWav` — close click (~150ms)

Generated programmatically (sine wave synthesis) to avoid copyright issues.

## RingCountdown Theme Integration

Currently hardcoded:
- Background ring: `QColor("#edeaf2")` → `ColorScheme::surface_variant`
- Progress arc: `QColor("#0071e3")` → `ColorScheme::primary`
- Text: `QColor("#1d1d1f")` → `ColorScheme::on_surface`

New method:
```cpp
class RingCountdown : public QWidget {
    // ...
    void set_color_scheme(const creeper::theme::ColorScheme& scheme);
private:
    creeper::theme::ColorScheme colorScheme_;
};
```

Registered as `color_scheme_setter_trait` via `manager.append_handler(ringW, *ringW)` — the existing template overload handles this.

## Risks & Mitigations

1. **Material Icons font missing**: Document dependency on `fonts-material-design-icons-iconfont`. Graceful fallback: icons render as empty/placeholder text.
2. **Qt6::Multimedia missing**: SoundManager degrades to no-op. `find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)` — if Multimedia is missing at build time, CMake fails. User installs `qt6-multimedia-dev` or we make it optional.
3. **Animation performance**: 30 QColor lerps @ 90fps ≈ negligible. Main window 700×550, repaint is cheap.
4. **Intermediate ColorScheme validity**: During animation, on_primary may not contrast properly with primary. 300ms duration makes this imperceptible.
5. **QSettings cross-platform**: Works on Linux (`~/.config/agent-sudo/theme.conf`). No macOS/Windows paths needed for now.
