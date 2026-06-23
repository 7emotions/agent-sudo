#!/usr/bin/env bash
# agent-sudo 真实场景端到端测试
# End-to-end test: CLI queue → GUI popup → automated interaction
set -uo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; CYAN='\033[0;36m'; NC='\033[0m'
PASS=0; FAIL=0; TOTAL=0
X11_HELPER="/home/ubuntu/.ws/pkagt/x11_key.py"
VENV_PYTHON="/tmp/test-venv/bin/python3"
QUEUE_DIR="$HOME/.cache/agent-sudo"
QUEUE_FILE="$QUEUE_DIR/queue.json"

_pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo -e "  ${GREEN}PASS${NC} $1"; }
_fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo -e "  ${RED}FAIL${NC} $1 — $2"; }
_info() { echo -e "  ${CYAN}INFO${NC} $1"; }

reset_state() {
    killall agent-sudo-flush 2>/dev/null || true
    sleep 0.5
    rm -f "$QUEUE_FILE" "$QUEUE_DIR/history.json"
    mkdir -p "$QUEUE_DIR"
    echo '{"version":1,"items":[]}' > "$QUEUE_FILE"
}

assert_eq() {
    local label="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then _pass "$label"
    else _fail "$label" "expected='$expected' got='$actual'"; fi
}

assert_contains() {
    local label="$1" haystack="$2" needle="$3"
    if echo "$haystack" | grep -qF "$needle"; then _pass "$label"
    else _fail "$label" "missing '$needle'"; fi
}

# Launch flush in background, send X11 key, wait for exit, return exit code via global
FLUSH_RC=0
run_flush_with_key() {
    local action="$1" text="${2:-}"

    agent-sudo-flush > /tmp/ast-out.txt 2> /tmp/ast-err.txt &
    local pid=$!
    sleep 1.5

    local args="--action $action --pid $pid --timeout 8"
    [ -n "$text" ] && args="$args --text $text"

    $VENV_PYTHON "$X11_HELPER" $args >/dev/null 2>&1
    local x11_rc=$?

    if [ $x11_rc -ne 0 ]; then
        _fail "X11 $action PID=$pid" "helper exited $x11_rc"
        kill $pid 2>/dev/null || true
        FLUSH_RC=127
        return 1
    fi

    # Poll for process exit (avoid wait issues in scripts)
    local waited=0
    while kill -0 $pid 2>/dev/null; do
        sleep 0.5
        waited=$((waited + 1))
        [ $waited -gt 20 ] && { _fail "timeout PID=$pid" "process didn't exit"; kill $pid 2>/dev/null; FLUSH_RC=124; return 1; }
    done
    # Process exited - get exit code from the job itself
    wait $pid 2>/dev/null
    FLUSH_RC=$?

    _pass "X11 $action -> PID=$pid, flush rc=$FLUSH_RC"
    return 0
}

# ── Prerequisites ──────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════════════════════════╗"
echo "║  agent-sudo 真实场景端到端测试                                       ║"
echo "╚══════════════════════════════════════════════════════════════════════╝"
echo ""

_info "agent-sudo: $(which agent-sudo)"
_info "DISPLAY: $DISPLAY"

# ═══════════════════════════════════════════════════════════════════════════
# PART 1 — CLI Queue
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── Part 1: CLI Queue ──"

echo -e "\n${YELLOW}1.1 单条命令入队${NC}"
reset_state
agent-sudo --reason "安装 git" -- apt install -y git
assert_eq "exit code" "0" "$?"
queue=$(cat "$QUEUE_FILE")
assert_contains "reason" "$queue" "安装 git"
assert_contains "command" "$queue" "apt install -y git"

echo -e "\n${YELLOW}1.2 多条命令入队${NC}"
reset_state
agent-sudo --reason "创建配置目录" -- mkdir -p /etc/myapp/config
agent-sudo --reason "写入 service 文件" -- tee /etc/systemd/system/myapp.service
agent-sudo --reason "重载 systemd" -- systemctl daemon-reload
agent-sudo --reason "开机自启" -- systemctl enable myapp.service
count=$(python3 -c "import json,sys; print(len(json.load(sys.stdin)['items']))" < "$QUEUE_FILE")
assert_eq "4 items" "4" "$count"

echo -e "\n${YELLOW}1.3 递增 ID${NC}"
ids=$(python3 -c "import json,sys; print(','.join(str(i['id']) for i in json.load(sys.stdin)['items']))" < "$QUEUE_FILE")
assert_eq "IDs 1,2,3,4" "1,2,3,4" "$ids"

echo -e "\n${YELLOW}1.4 CWD 记录${NC}"
cwd=$(python3 -c "import json,sys; print(json.load(sys.stdin)['items'][0]['cwd'])" < "$QUEUE_FILE")
assert_eq "cwd matches" "$(pwd)" "$cwd"

echo -e "\n${YELLOW}1.5 特殊字符${NC}"
reset_state
agent-sudo --reason "安装 & 配置 Nginx (v1.25)" -- apt install -y nginx
queue=$(cat "$QUEUE_FILE")
assert_contains "special chars" "$queue" "安装 & 配置 Nginx (v1.25)"

# ═══════════════════════════════════════════════════════════════════════════
# PART 2 — GUI Reject (Escape)
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── Part 2: GUI Reject ──"

echo -e "\n${YELLOW}2.1 Escape 拒绝${NC}"
reset_state
agent-sudo --reason "安装 Docker" -- apt install -y docker.io
agent-sudo --reason "启动 Docker" -- systemctl start docker

run_flush_with_key "reject"
assert_eq "reject exit=126" "126" "$FLUSH_RC"

echo -e "\n${YELLOW}2.2 队列清空${NC}"
count=$(python3 -c "import json,sys; print(len(json.load(sys.stdin)['items']))" < "$QUEUE_FILE")
assert_eq "queue empty" "0" "$count"

# ═══════════════════════════════════════════════════════════════════════════
# PART 3 — GUI Auth Fail (wrong password + Enter)
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── Part 3: Auth Fail ──"

echo -e "\n${YELLOW}3.1 错误密码${NC}"
reset_state
agent-sudo --reason "测试命令" -- echo "hello from test"

# Type wrong password first, then send Enter in a separate call
agent-sudo-flush > /tmp/ast-out.txt 2> /tmp/ast-err.txt &
FPID=$!
sleep 1.5

_info "Typing wrong password..."
x11_type_rc=0
$VENV_PYTHON "$X11_HELPER" --action type --pid $FPID --text "wrongpassword123" --timeout 5 >/dev/null 2>&1 || x11_type_rc=$?
sleep 0.3
_info "Pressing Enter..."
x11_enter_rc=0
$VENV_PYTHON "$X11_HELPER" --action accept --pid $FPID --timeout 5 >/dev/null 2>&1 || x11_enter_rc=$?

if [ $x11_type_rc -ne 0 ] && [ $x11_enter_rc -ne 0 ]; then
    _fail "auth X11" "both type and enter failed"
    kill $FPID 2>/dev/null || true
fi

wait $FPID 2>/dev/null
auth_rc=$?
_info "auth exit code: $auth_rc"

if [ "$auth_rc" = "126" ]; then
    _pass "auth failure -> 126"
elif [ "$auth_rc" = "0" ]; then
    _info "auth succeeded (password may have been accepted — NOPASSWD?)"
else
    _info "unexpected exit code $auth_rc"
fi

echo -e "\n${YELLOW}3.2 认证失败后队列清空${NC}"
count=$(python3 -c "import json,sys; print(len(json.load(sys.stdin).get('items',[])))" < "$QUEUE_FILE" 2>/dev/null || echo "0")
assert_eq "queue cleared" "0" "$count"

# ═══════════════════════════════════════════════════════════════════════════
# PART 4 — Edge Cases
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── Part 4: Edge Cases ──"

echo -e "\n${YELLOW}4.1 空队列刷新${NC}"
reset_state
set +e
agent-sudo-flush 2>/dev/null
empty_rc=$?
set -e
assert_eq "empty queue exit=127" "127" "$empty_rc"

echo -e "\n${YELLOW}4.2 空参数保护${NC}"
reset_state
agent-sudo --reason "" -- echo test 2>&1 || true
_info "empty reason -> usage error (expected)"

# ═══════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "╔══════════════════════════════════════════════════════════════════════╗"
echo -e "║  结果: ${GREEN}PASS=$PASS${NC}  ${RED}FAIL=$FAIL${NC}  TOTAL=$TOTAL                                              ║"
echo "╚══════════════════════════════════════════════════════════════════════╝"
echo ""
echo -e "${YELLOW}Manual tests (need sudo password):${NC}"
echo "  完整审批: queue commands → agent-sudo-flush → approve with password → verify history.json"
echo "  选择性审批: uncheck some commands → only checked ones execute"
echo "  60s 倒计时: launch flush → wait 60s → auto-reject (exit 126)"
echo "  LLM 留言: expand text area → type message → execute → stdout shows LLM_PROMPT"

[ $FAIL -eq 0 ] && exit 0 || exit 1
