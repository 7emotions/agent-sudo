---
name: agent-sudo
description: Queue privileged commands for user approval via GUI. Use when a task requires root (apt install, systemctl, writes to /etc, etc.) and no passwordless sudo is available. Queue all commands first, then run agent-sudo-flush to present them in a GUI for user approval.
---

# agent-sudo

Queue privileged commands and present them as a batch for user approval via a GUI window. Built with C++23 / Qt6 / creeper-qt.

## Dependencies

- Qt6 (Widgets, Network, Svg, Multimedia)
- Eigen3
- CMake 3.22+, GCC 14+
- Material Icons Round font (bundled in binary)

## Features

- Theme management (3 presets + light/dark, animated transitions)
- Sound feedback (configurable per event via QSettings)
- Danger level detection (safe/warning/danger icons)
- Keyboard shortcuts (Esc=reject, A=select all, C=deselect)
- Multi-language support (zh/en/ja/ko/fr/de/es/pt)
- Click card to toggle selection

## Pattern

```
# 1. Queue each privileged command with a reason
agent-sudo --reason "为什么需要这条命令" -- <command>
agent-sudo --reason "为什么需要这条命令" -- <command>

# 2. Run flush — blocks until user approves or rejects
agent-sudo-flush
```

## Exit codes

| Code | Meaning |
|------|---------|
| 0    | All checked commands executed successfully |
| 124  | Execution timed out (300s) |
| 126  | User rejected, window closed, or auto-reject after 60s |
| 127  | Error (empty queue, DISPLAY unset, etc.) |

## LLM Prompt

Users can type a prompt in the GUI's collapsible text area. If they do, stdout will contain:

```
LLM_PROMPT: <text>
```

Read this line and incorporate it as guidance. It only appears when the user actually types something.

## Detection

```bash
which agent-sudo agent-sudo-flush  # must both be on PATH
echo $DISPLAY                       # must not be empty
```

If either check fails, fall back to `pkexec`.

## Example

```bash
# Agent decides to install packages and enable a service
agent-sudo --reason "install build tools" -- apt install -y gcc make cmake
agent-sudo --reason "enable and start nginx" -- systemctl enable --now nginx

# Present for approval
agent-sudo-flush

# 0 → success, continue
# 126 → user said no, stop and report
# 124 → timeout, check queue.json for state
```

## Queue and history

- Queue: `~/.cache/agent-sudo/queue.json`
- History: `~/.cache/agent-sudo/history.json`

## Configuration

`~/.config/agent-sudo/theme.conf` (QSettings INI format):

```ini
[theme]
preset=0   ; 0=BlueMiku, 1=GoldenHarvest, 2=Green
mode=0     ; 0=Light, 1=Dark

[sound]
enabled=true
event\open=default       ; default/none/<wav-path>
event\warning=default
event\executed=default
event\rejected=none

lang=zh   ; en/ja/ko/fr/de/es/pt
```
