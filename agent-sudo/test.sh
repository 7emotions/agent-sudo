#!/usr/bin/env bash
# agent-sudo regression test
# Usage: bash test.sh    # requires agent-sudo + agent-sudo-flush on PATH
set -euo pipefail

PASS=0; FAIL=0
RED='\033[31m'; GREEN='\033[32m'; NC='\033[0m'

assert() {
    local desc="$1"; shift
    if "$@"; then
        echo -e "  ${GREEN}✓${NC} $desc"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $desc"
        FAIL=$((FAIL + 1))
    fi
}

section() { echo; echo "── $1 ──"; }

# ── Setup: isolated test environment ──
REAL_HOME="$HOME"
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT
export HOME="$TMPDIR"
export XDG_CACHE_HOME="$HOME/.cache"
mkdir -p "$XDG_CACHE_HOME/agent-sudo"
Q="$XDG_CACHE_HOME/agent-sudo/queue.json"
H="$XDG_CACHE_HOME/agent-sudo/history.json"
AGENTS_MD="$REAL_HOME/.config/opencode/AGENTS.md"

echo "agent-sudo regression test"
echo "Temp dir: $TMPDIR"

# ═══════════════════════════════════════════
# Section T1: agent-sudo CLI (10 assertions)
# ═══════════════════════════════════════════
section "T1: agent-sudo CLI"

# T1.1: --version prints version and exits 0
assert "T1.1 --version prints version" bash -c 'agent-sudo --version 2>&1 | grep -q "agent-sudo v"'

# Queue one command for T1.2–T1.9
agent-sudo --reason "install build tools" -- apt install -y gcc make >/dev/null 2>&1

# T1.2: queue file exists after queuing
assert "T1.2 queue file exists" test -f "$Q"

# T1.3: schema version is 1
assert "T1.3 schema version is 1" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert d.get('version') == 1, f'got {d.get(\"version\")!r}'
"

# T1.4: one item queued
assert "T1.4 one item queued" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert len(d['items']) == 1, f'got {len(d[\"items\"])}'
"

# T1.5: reason stored correctly
assert "T1.5 reason stored" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert d['items'][0]['reason'] == 'install build tools', f'got {d[\"items\"][0][\"reason\"]!r}'
"

# T1.6: command stored as string
assert "T1.6 command stored as string" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert 'apt install -y gcc make' in d['items'][0]['command'], f'got {d[\"items\"][0][\"command\"]!r}'
"

# T1.7: status is pending
assert "T1.7 status is pending" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert d['items'][0]['status'] == 'pending', f'got {d[\"items\"][0][\"status\"]!r}'
"

# T1.8: cwd is absolute path
assert "T1.8 cwd is absolute path" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
cwd = d['items'][0]['cwd']
assert cwd.startswith('/'), f'cwd={cwd!r} not absolute'
"

# T1.9: timestamp is present
assert "T1.9 timestamp present" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert 'timestamp' in d['items'][0], 'timestamp missing'
"

# T1.10: queue second command, verify auto-increment IDs
agent-sudo --reason "verify auto-increment" -- echo "second command" >/dev/null 2>&1
assert "T1.10 IDs are 1 and 2" python3 -c "
import json
with open('$Q') as f:
    d = json.load(f)
assert len(d['items']) == 2, f'got {len(d[\"items\"])}'
ids = [it['id'] for it in d['items']]
assert ids == [1, 2], f'got ids={ids}'
"

# ═══════════════════════════════════════════
# Section T2: Symlinks and PATH (5 assertions)
# ═══════════════════════════════════════════
section "T2: Symlinks and PATH"

# T2.1: agent-sudo on PATH
assert "T2.1 agent-sudo on PATH" bash -c 'which agent-sudo >/dev/null'

# T2.2: agent-sudo-flush on PATH
assert "T2.2 agent-sudo-flush on PATH" bash -c 'which agent-sudo-flush >/dev/null'

# T2.3: agent-sudo is symlink
assert "T2.3 agent-sudo is symlink" bash -c 'test -L "$(which agent-sudo)"'

# T2.4: agent-sudo-flush is symlink
assert "T2.4 agent-sudo-flush is symlink" bash -c 'test -L "$(which agent-sudo-flush)"'

# T2.5: main.py is executable
assert "T2.5 main.py executable" bash -c 'test -x "$(readlink -f "$(which agent-sudo)")"'

# ═══════════════════════════════════════════
# Section T3: AGENTS.md (3 assertions)
# ═══════════════════════════════════════════
section "T3: AGENTS.md"

# T3.1: AGENTS.md mentions agent-sudo
assert "T3.1 AGENTS.md mentions agent-sudo" bash -c "grep -q 'agent-sudo' \"$AGENTS_MD\""

# T3.2: AGENTS.md mentions agent-sudo-flush
assert "T3.2 AGENTS.md mentions agent-sudo-flush" bash -c "grep -q 'agent-sudo-flush' \"$AGENTS_MD\""

# T3.3: backup file exists
assert "T3.3 backup file exists" bash -c "ls \"$AGENTS_MD\".bak.* >/dev/null 2>&1"

# ═══════════════════════════════════════════
# Section T4: Edge cases — no GUI (3 assertions)
# ═══════════════════════════════════════════
section "T4: Edge cases (pre-GUI code paths)"

# Reset queue so T4.2 sees an empty queue
rm -f "$Q"

# T4.1: DISPLAY unset → exits 127 with "ERROR"
assert "T4.1 DISPLAY unset exits 127" bash -c '
OUT=$(DISPLAY= agent-sudo-flush 2>&1); RC=$?
[ $RC -eq 127 ] && echo "$OUT" | grep -q "ERROR"
'

# T4.2: DISPLAY set but queue empty → exits 127 with "ERROR"
assert "T4.2 empty queue exits 127" bash -c '
OUT=$(DISPLAY=:0 agent-sudo-flush 2>&1); RC=$?
[ $RC -eq 127 ] && echo "$OUT" | grep -q "ERROR"
'

# T4.3: --reason missing → exits 127
assert "T4.3 --reason missing exits 127" bash -c '
agent-sudo -- echo x >/dev/null 2>&1; [ $? -eq 127 ]
'

# ═══════════════════════════════════════════
# Final Report
# ═══════════════════════════════════════════
echo
echo "──────────────────────────────────────"
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
if [ "$FAIL" -gt 0 ]; then
    echo -e "${RED}REGRESSION DETECTED${NC}"
    exit 1
else
    echo -e "${GREEN}ALL CLEAN${NC}"
    exit 0
fi
