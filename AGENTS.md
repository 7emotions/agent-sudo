# agent-sudo — AGENTS.md

## What this is

Queue privileged commands and present a batch GUI for human approval. Single C++23/Qt6 binary dispatched by `argv[0]`: invoked as `agent-sudo` it queues commands; as `agent-sudo-flush` it opens the GUI.

## Commands

```bash
agent-sudo --reason "why this needs root" -- <command>
agent-sudo-flush
```

Exit codes: 0=executed, 124=timeout(30s), 126=rejected/timeout, 127=error.

`--` separates flags from command. `--reason` is required.

## Build & install

```bash
cmake -S . -B build && cmake --build build
sudo cp build/agent-sudo-flush /usr/local/bin/
sudo ln -sf /usr/local/bin/agent-sudo-flush /usr/local/bin/agent-sudo
```

Requires: Qt6 (Widgets, Network, Svg, Multimedia), Eigen3, CMake 3.22+, GCC 14+.

## Architecture

| File | Role |
|------|------|
| `src/main.cc` | Entrypoint — CLI dispatch, QSettings, creeper-qt declarative UI |
| `src/gui/command_card.cc/h` | `buildCommandCards()` — scrollable command cards with danger detection |
| `src/gui/queue_io.cc/h` | Queue/history JSON I/O |
| `src/gui/executor.cc/h` | `execCommands()` — stdin-based sudo subprocess |
| `src/gui/ring_countdown.cc/h` | 36×36 countdown ring (theme-aware) |
| `src/gui/icon.cc/h` | IconProvider — Material Icons Round font + app SVG |
| `src/gui/sound.cc/h` | SoundManager — QSoundEffect per event, QSettings configurable |
| `src/gui/theme_transition.cc/h` | ColorScheme interpolation for animated theme switches |
| `src/gui/tr.h` | `tr::t(key)` — JSON-based i18n lookup |
| `src/gui/creeper-qt/` | Vendored creeper-qt (MIT, original author @creeper5820) |

## Key paths

| Path | Purpose |
|------|---------|
| `~/.cache/agent-sudo/queue.json` | Command queue (version 1) |
| `~/.cache/agent-sudo/history.json` | Execution history (version 1) |
| `~/.config/agent-sudo/theme.conf` | QSettings — theme, sound, language |

## Conventions

- GUI title: `"Agent 特权命令审批"` — human-facing, never translate
- `LLM_PROMPT: <text>` on stdout when user types in the collapsible text area
- Empty password on execute → silently ignored (no retry)
- Auth failure → queue cleared, exit 126
- No Python dependency — the old `src/main.py`, `e2e_test.py`, `functional_gui_test.py`, `test.sh` are deleted
- Material Icons font bundled as Qt resource, no system font dependency
