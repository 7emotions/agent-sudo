---
name: agent-sudo
description: Queue privileged commands for user approval via GUI. Use when a task requires root (apt install, systemctl, writes to /etc, etc.) and no passwordless sudo is available. Queue all commands first, then run agent-sudo-flush to present them in a GUI for user approval.
---

# agent-sudo

Queue privileged commands and present them as a batch for user approval via a GUI window.

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
