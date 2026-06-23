# agent-sudo — AGENTS.md

## What this is

Queue privileged commands and present a batch GUI for human approval. Two modes dispatched from a single entrypoint: `agent-sudo` (CLI queue) and `agent-sudo-flush` (GUI approve/execute). When a task needs root, queue all commands first, then flush once.

## Architecture

**Dual GUI implementations — Python is the active one:**

- `src/main.py` — PySide2/PyQt5 GUI. This is the canonical, working implementation. All tests target this.
- `src/gui/` — Qt6/C++ rewrite using `creeper-qt`. Work in progress, not yet functional as a drop-in replacement. Do not edit C++ and Python implementations to match each other; they will diverge during migration.

**C++ module layout** (`src/gui/`):

| File | Role |
|------|------|
| `src/main.cc` | App entrypoint — state, callbacks, creeper-qt declarative UI tree |
| `src/gui/command_card.h/cc` | `buildCommandCards()` — creates the scrollable card list from queue items |
| `src/gui/ring_countdown.h/cc` | RingCountdown widget (36×36 canvas countdown) |
| `src/gui/queue_io.h/cc` | Queue/history JSON I/O, `nowIso()`, `isAuthFail()`, `scrubPassword()` |
| `src/gui/executor.h/cc` | Post-GUI sudo subprocess execution |
| `src/gui/creeper-qt/` | Vendored creeper-qt 2.0 source (header-only lib + 17 .cc files) |

**Dispatch**: `main()` checks `os.path.basename(sys.argv[0])`. If it equals `"agent-sudo-flush"`, runs the GUI; otherwise CLI.

## Commands

```bash
# Queue a privileged command (always with --reason)
agent-sudo --reason "install build tools" -- apt install -y gcc make

# Submit all queued commands for approval (blocks, opens GUI)
agent-sudo-flush

# Exit code handling
# 0   = all checked commands executed
# 124 = execution timeout (300s)
# 126 = user rejected, window closed, or 60s auto-reject
# 127 = error (empty queue, DISPLAY unset, missing --reason, etc.)
```

## Tests

```bash
# Integration tests (requires agent-sudo + agent-sudo-flush on PATH)
bash src/test.sh

# Python unit/E2E tests — GUI tests need a virtual display
xvfb-run python3 src/e2e_test.py

# Stale — uses tkinter, references old clone path ~/agent-sudo. Do not rely on it.
# python3 src/functional_gui_test.py
```

`test.sh` section T3 asserts that AGENTS.md exists and mentions `agent-sudo` / `agent-sudo-flush`. Do not remove those references without updating the tests.

## Key paths

| Path | Purpose |
|------|---------|
| `~/.cache/agent-sudo/queue.json` | Pending command queue (version 1) |
| `~/.cache/agent-sudo/history.json` | Execution history (version 1) |
| `~/.cache/agent-sudo/` | Created with mode `0o700` |

All queue/history I/O uses `fcntl.flock` with `LOCK_EX` for concurrent safety. Do not bypass this with raw file reads.

## Dependencies

- Python 3.8+, PySide2 (`pip install PySide2` or `apt install python3-pyside2.qtwidgets`)
- `$DISPLAY` must be set for GUI; fall back to `pkexec` when $DISPLAY is empty
- C++ build (experimental): Qt6, CMake 3.22+, Eigen3, GCC 14+ (creeper-qt requires C++23 deducing-this)

## Conventions

- `--` is the delimiter between agent-sudo flags and the actual command. Everything after `--` is the command.
- `--reason` is required; omitting it exits 127.
- The GUI title is `"Agent 特权命令审批"` — human-facing, do not translate.
- The LLM prompt text area output format: `LLM_PROMPT: <text>` on stdout. Agents should check for this line after flush.
- Password is passed via stdin to `sudo -k -S`, then zeroed from memory with `ctypes.memset`.
- Queue writes are atomic (temp file + `os.replace`). History appends use flock but not atomic replacement.

## C++ build (experimental)

```bash
cmake -S . -B build
cmake --build build
```

The C++ binary outputs `agent-sudo-flush` and compiles creeper-qt from the vendored source.
