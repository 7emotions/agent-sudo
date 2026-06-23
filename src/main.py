#!/usr/bin/env python3
"""agent-sudo: CLI queue + PySide2 GUI for privileged command approval.
Dispatch: agent-sudo=CLI, agent-sudo-flush=GUI (native Qt5).
"""
import sys, os, json, shlex, tempfile, fcntl, signal, ctypes, subprocess, time
from datetime import datetime, timezone
from PySide2.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QPushButton, QCheckBox, QLineEdit, QScrollArea,
    QFrame, QTextEdit)
from PySide2.QtCore import Qt, QTimer, QRectF, Signal, Property, QEasingCurve, QPropertyAnimation
from PySide2.QtGui import QFont, QColor, QPainter, QPen, QBrush, QPainterPath

QUEUE_DIR = os.path.expanduser("~/.cache/agent-sudo")
QUEUE_FILE = os.path.join(QUEUE_DIR, "queue.json")
HISTORY_FILE = os.path.join(QUEUE_DIR, "history.json")

# ---------------------------------------------------------------------------
# Queue I/O (shared by CLI and GUI)
# ---------------------------------------------------------------------------

def _read_queue():
    """Read queue.json with flock(LOCK_EX). Returns {"version": 1, "items": [...]}."""
    return _read_queue_from(QUEUE_FILE)


def _read_queue_from(path):
    """Read queue from a specific path with flock(LOCK_EX)."""
    if not os.path.exists(path):
        return {"version": 1, "items": []}
    with open(path, "r") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            data = json.load(f)
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)
    if not isinstance(data, dict) or data.get("version") != 1:
        return {"version": 1, "items": []}
    return data


def _write_queue(data):
    """Write queue.json with flock(LOCK_EX)."""
    fd = os.open(QUEUE_FILE, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX)
        os.ftruncate(fd, 0)
        os.lseek(fd, 0, os.SEEK_SET)
        content = json.dumps(data, ensure_ascii=False, indent=2).encode()
        os.write(fd, content)
        os.fsync(fd)
    finally:
        fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)


def _atomic_queue_update(items):
    """Write queue items atomically via temp file + os.replace."""
    tmp = tempfile.NamedTemporaryFile(
        mode="w", dir=QUEUE_DIR, delete=False, prefix=".queue-"
    )
    try:
        json.dump({"version": 1, "items": items}, tmp, ensure_ascii=False, indent=2)
        tmp.flush()
        os.fsync(tmp.fileno())
        tmp.close()
        os.replace(tmp.name, QUEUE_FILE)
    except Exception:
        try:
            os.unlink(tmp.name)
        except OSError:
            pass
        raise


def _append_history(entries):
    """Append entries to history.json with flock."""
    if not entries:
        return
    if os.path.exists(HISTORY_FILE):
        with open(HISTORY_FILE, "r") as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            try:
                hist = json.load(f)
            finally:
                fcntl.flock(f, fcntl.LOCK_UN)
    else:
        hist = {"version": 1, "entries": []}
    hist["entries"].extend(entries)
    with open(HISTORY_FILE, "w") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            json.dump(hist, f, ensure_ascii=False, indent=2)
            f.flush()
            os.fsync(f.fileno())
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def _now_iso():
    return datetime.now(timezone.utc).isoformat()


# ---------------------------------------------------------------------------
# CLI mode (agent-sudo)
# ---------------------------------------------------------------------------

def main_cli():
    if len(sys.argv) >= 2 and sys.argv[1] == "--version":
        print("agent-sudo v1.0.0")
        sys.exit(0)

    # Parse --reason
    args = sys.argv[1:]
    try:
        reason_idx = args.index("--reason")
    except ValueError:
        print("ERROR: --reason is required", file=sys.stderr)
        sys.exit(127)

    if reason_idx + 1 >= len(args):
        print("ERROR: --reason requires an argument", file=sys.stderr)
        sys.exit(127)

    reason = args[reason_idx + 1]
    remaining = args[reason_idx + 2:]

    # Find -- delimiter
    if "--" in remaining:
        dash_idx = remaining.index("--")
        cmd_args = remaining[dash_idx + 1:]
    else:
        cmd_args = remaining

    if not cmd_args:
        print("ERROR: no command provided", file=sys.stderr)
        sys.exit(127)

    # Validate cwd
    cwd = os.getcwd()
    if not os.path.isabs(cwd) or not os.path.isdir(cwd):
        print("ERROR: invalid cwd", file=sys.stderr)
        sys.exit(127)

    for bad in (";", "|", "&", "$", "`", "\\", "\n"):
        if bad in cwd:
            print(f"ERROR: cwd contains forbidden character", file=sys.stderr)
            sys.exit(127)

    # Check existing queue for bad cwd items
    existing = _read_queue()
    for item in existing.get("items", []):
        item_cwd = item.get("cwd", "")
        for bad in (";", "|", "&", "$", "`", "\\", "\n"):
            if bad in item_cwd:
                print(f"ERROR: queue.json contains item with invalid cwd", file=sys.stderr)
                sys.exit(127)

    max_id = max((it.get("id", 0) for it in existing.get("items", [])), default=0)

    command_str = shlex.join(cmd_args)

    new_item = {
        "id": max_id + 1,
        "reason": reason,
        "command": command_str,
        "cwd": cwd,
        "timestamp": _now_iso(),
        "status": "pending",
    }

    existing.setdefault("items", []).append(new_item)
    _write_queue(existing)

    total = len(existing["items"])
    print(f"Queued 1 command (total: {total})")
    sys.exit(0)


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _is_auth_failure(text):
    keywords = (
        "incorrect password", "authentication failure",
        "密码错误", "Sorry, try again"
    )
    return any(
        kw.lower() in text.lower() if kw.isascii() else kw in text
        for kw in keywords
    )


def _mark_failed_items(items, selected_ids):
    remaining = [it for it in items if it["id"] not in selected_ids]
    for it in items:
        if it["id"] in selected_ids:
            it["status"] = "failed"
            it["failed_at"] = _now_iso()
    try:
        _atomic_queue_update(remaining)
        failed = [it for it in items if it["id"] in selected_ids]
        if failed:
            _append_history(failed)
    except OSError:
        pass


# ---------------------------------------------------------------------------
# QSS Stylesheet — Apple macOS Soft design system
# ---------------------------------------------------------------------------

QSS = """
QMainWindow, QWidget { background: #f0eef7; }
QScrollArea { background: #faf9fc; border: 1px solid #edeaf2; border-radius: 12px; }
QFrame#summaryBar { background: #faf9fc; border-bottom: 1px solid #edeaf2; padding: 10px 20px; }
QLabel#summaryLabel { font-size: 15px; font-weight: bold; color: #1d1d1f; background: transparent; }
QFrame#card { background: #ffffff; border: 1px solid #e2e0eb; border-radius: 10px; margin: 4px 8px; }
QLineEdit#pwInput { background: #ffffff; border: 1px solid #e2e0eb; border-radius: 8px; padding: 8px 12px; font-size: 14px; }
QLineEdit#pwInput:focus { border: 1.5px solid #0071e3; }
QPushButton#btnSecondary { background: #ffffff; border: 1px solid #e2e0eb; border-radius: 8px; padding: 6px 16px; font-size: 13px; color: #1d1d1f; }
QPushButton#btnSecondary:hover { background: #f5f5f7; }
QPushButton#btnReject { background: #ffffff; border: 1px solid #e2e0eb; border-radius: 8px; padding: 6px 16px; font-size: 13px; color: #ff453a; }
QPushButton#btnReject:hover { background: #fff0ef; }
QPushButton#btnPrimary { background: #0071e3; border: none; border-radius: 8px; padding: 8px 22px; font-size: 13px; font-weight: bold; color: white; }
QPushButton#btnPrimary:hover { background: #0077ed; }
QPushButton#btnPrimary:disabled { background: #80b8f1; color: rgba(255,255,255,0.6); }
QFrame#pwFrame { background: #faf9fc; }
QLabel#pwLabel { font-size: 13px; color: #424245; background: transparent; }
QFrame#btnBar { background: #faf9fc; border-top: 1px solid #edeaf2; padding: 12px 20px; }
QFrame#llmFrame { background: #faf9fc; border-top: 1px solid #edeaf2; }
QLabel#llmDisclosure { font-size: 13px; color: #424245; background: transparent; }
QTextEdit#llmText { background: #ffffff; border: 1px solid #e2e0eb; border-radius: 8px; font-size: 12px; font-family: monospace; }
QLabel#cardTitle { font-size: 14px; color: #1d1d1f; background: transparent; }
QLabel#cardCmd { font-size: 11px; color: #6e6e73; font-family: monospace; background: transparent; }
"""


# ---------------------------------------------------------------------------
# RingCountdown — Canvas ring countdown widget (36×36)
# ---------------------------------------------------------------------------

class RingCountdown(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(36, 36)
        self.remaining = 60
        self.total = 60

    def setRemaining(self, val):
        self.remaining = max(val, 0)
        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        pen = QPen(QColor("#edeaf2"), 2.5)
        p.setPen(pen)
        p.drawEllipse(QRectF(2, 2, 32, 32))
        pen.setColor(QColor("#0071e3"))
        p.setPen(pen)
        span = int(-(self.remaining / self.total) * 360 * 16)
        p.drawArc(QRectF(2, 2, 32, 32), 90 * 16, span)
        p.setPen(QColor("#1d1d1f"))
        p.setFont(QFont("Courier", 9, QFont.Weight.Bold))
        p.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, str(self.remaining))
        p.end()


# ---------------------------------------------------------------------------
# ToggleSwitch — Apple pill toggle switch (51×31)
# ---------------------------------------------------------------------------

class ToggleSwitch(QWidget):
    toggled = Signal(bool)

    def __init__(self, parent=None, checked=True):
        super().__init__(parent)
        self.setFixedSize(51, 31)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self._checked = checked
        self._knob_x = 22.0 if checked else 2.0
        self._anim = QPropertyAnimation(self, b"knob_x")

    def _get_knob_x(self):
        return self._knob_x

    def _set_knob_x(self, val):
        self._knob_x = val
        self.update()

    knob_x = Property(float, _get_knob_x, _set_knob_x)

    def isChecked(self):
        return self._checked

    def setChecked(self, val):
        self._checked = bool(val)
        target = 22.0 if self._checked else 2.0
        self._anim.stop()
        self._anim.setDuration(180)
        self._anim.setStartValue(self._knob_x)
        self._anim.setEndValue(float(target))
        self._anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._anim.start()

    def mousePressEvent(self, event):
        self._checked = not self._checked
        self.setChecked(self._checked)
        self.toggled.emit(self._checked)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        track = QColor("#30d158") if self._checked else QColor("#e9e9eb")
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(track)
        p.drawRoundedRect(QRectF(0, 0, 51, 31), 15.5, 15.5)
        x = float(self._knob_x)
        p.setBrush(Qt.GlobalColor.white)
        p.drawEllipse(QRectF(x, 2, 27, 27))
        p.end()


# ---------------------------------------------------------------------------
# MainWindow — PySide2 Qt5 GUI
# ---------------------------------------------------------------------------

class MainWindow(QMainWindow):
    def __init__(self, queue_items):
        super().__init__()
        self.queue_items = queue_items
        self.toggles = []
        self.paused = False
        self.remaining = 60
        self.done = False
        self.llm_expanded = False
        self._closed = False
        self._result = None

        self.setWindowTitle("Agent 特权命令审批")
        self.setFixedSize(700, 550)
        self.setStyleSheet(QSS)
        screen = QApplication.primaryScreen().geometry()
        self.move((screen.width() - 700) // 2, (screen.height() - 550) // 2)

        central = QWidget(objectName="central")
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # ---- Summary bar ----
        sb = QFrame(objectName="summaryBar")
        sb_layout = QHBoxLayout(sb)
        sb_layout.addWidget(QLabel(f"共 {len(queue_items)} 条待审批命令", objectName="summaryLabel"))
        sb_layout.addStretch()
        self.ring = RingCountdown()
        sb_layout.addWidget(self.ring)
        self.pause_btn = QPushButton("||")
        self.pause_btn.setFixedSize(32, 32)
        self.pause_btn.setStyleSheet("QPushButton { font-size: 16px; color: #6e6e73; border: 1px solid #e2e0eb; border-radius: 8px; background: #ffffff; } QPushButton:hover { background: #f5f5f7; }")
        self.pause_btn.clicked.connect(self.togglePause)
        sb_layout.addWidget(self.pause_btn)
        layout.addWidget(sb)

        # ---- Scroll area with cards ----
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll_widget = QWidget()
        scroll_layout = QVBoxLayout(scroll_widget)
        scroll_layout.setContentsMargins(8, 8, 8, 8)
        scroll_layout.setSpacing(4)

        for item in queue_items:
            card = QFrame(objectName="card")
            card_layout = QVBoxLayout(card)
            card_layout.setContentsMargins(14, 10, 14, 10)
            row = QHBoxLayout()
            row.addWidget(QLabel(f"#{item['id']} — {item['reason']}", objectName="cardTitle"))
            row.addStretch()
            toggle = ToggleSwitch()
            toggle.toggled.connect(self.updateExecuteBtn)
            self.toggles.append(toggle)
            row.addWidget(toggle)
            card_layout.addLayout(row)
            card_layout.addWidget(QLabel(f"→ {item['command']}", objectName="cardCmd"))
            scroll_layout.addWidget(card)

        scroll_layout.addStretch()
        scroll.setWidget(scroll_widget)
        layout.addWidget(scroll, 1)

        # ---- LLM disclosure ----
        llm_frame = QFrame(objectName="llmFrame")
        llm_outer = QVBoxLayout(llm_frame)
        llm_outer.setContentsMargins(16, 6, 16, 0)
        self.llm_header_btn = QPushButton("▶ 备注本次执行上下文 (LLM Prompt)")
        self.llm_header_btn.setFlat(True)
        self.llm_header_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.llm_header_btn.setStyleSheet("QPushButton { text-align: left; font-size: 13px; color: #424245; background: transparent; border: none; padding: 4px 0; } QPushButton:hover { color: #1d1d1f; }")
        self.llm_header_btn.clicked.connect(self.toggleLlm)
        llm_outer.addWidget(self.llm_header_btn)
        self.llm_text = QTextEdit(objectName="llmText")
        self.llm_text.setMaximumHeight(0)
        self.llm_text.setVisible(False)
        llm_outer.addWidget(self.llm_text)
        layout.addWidget(llm_frame)

        # ---- Password ----
        pw_frame = QFrame(objectName="pwFrame")
        pw_layout = QHBoxLayout(pw_frame)
        pw_layout.setContentsMargins(20, 8, 20, 8)
        pw_layout.addWidget(QLabel("sudo 密码:", objectName="pwLabel"))
        self.pw_input = QLineEdit(objectName="pwInput")
        self.pw_input.setEchoMode(QLineEdit.EchoMode.Password)
        self.pw_input.setPlaceholderText("输入 sudo 密码")
        pw_layout.addWidget(self.pw_input)
        layout.addWidget(pw_frame)

        # ---- Button bar ----
        bb = QFrame(objectName="btnBar")
        bb_layout = QHBoxLayout(bb)
        bb_layout.setContentsMargins(0, 0, 0, 0)
        ba = QPushButton("全选", objectName="btnSecondary")
        ba.clicked.connect(self.selectAll)
        bb_layout.addWidget(ba)
        bd = QPushButton("取消全选", objectName="btnSecondary")
        bd.clicked.connect(self.deselectAll)
        bb_layout.addWidget(bd)
        bb_layout.addStretch()
        br = QPushButton("拒绝", objectName="btnReject")
        br.clicked.connect(self.reject)
        bb_layout.addWidget(br)
        self.be = QPushButton("执行 (Enter)", objectName="btnPrimary")
        self.be.clicked.connect(self.execute)
        self.be.setDefault(True)
        bb_layout.addWidget(self.be)
        layout.addWidget(bb)

        self.pw_input.returnPressed.connect(self.execute)
        QTimer.singleShot(0, self.pw_input.setFocus)

        # ---- Countdown timer ----
        self.timer = QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(1000)

    # ---- Event handlers ----

    def toggleLlm(self):
        self.llm_expanded = not self.llm_expanded
        arrow = "▼" if self.llm_expanded else "▶"
        self.llm_header_btn.setText(f"{arrow} 备注本次执行上下文 (LLM Prompt)")

        if self.llm_expanded:
            self.llm_text.setMaximumHeight(80)
            self.llm_text.setVisible(True)
        else:
            self.llm_text.setMaximumHeight(0)
            self.llm_text.setVisible(False)

    def selectAll(self):
        for t in self.toggles:
            t.setChecked(True)
        self.updateExecuteBtn()

    def deselectAll(self):
        for t in self.toggles:
            t.setChecked(False)
        self.updateExecuteBtn()

    def updateExecuteBtn(self):
        n = sum(1 for t in self.toggles if t.isChecked())
        self.be.setEnabled(n > 0)

    def togglePause(self):
        self.paused = not self.paused
        self.pause_btn.setText(">" if self.paused else "||")

    def tick(self):
        if self.done or self.paused:
            return
        self.remaining -= 1
        self.ring.setRemaining(self.remaining)
        if self.remaining <= 0:
            self.done = True
            self.autoReject()

    def autoReject(self):
        self._result = "auto_reject"
        p = self.llm_text.toPlainText().strip()
        if p:
            print(f"LLM_PROMPT: {p}", flush=True)
        _write_queue({"version": 1, "items": []})
        QApplication.quit()

    def reject(self):
        self.done = True
        self._result = "rejected"
        p = self.llm_text.toPlainText().strip()
        if p:
            print(f"LLM_PROMPT: {p}", flush=True)
        _write_queue({"version": 1, "items": []})
        QApplication.quit()

    def execute(self):
        n = sum(1 for t in self.toggles if t.isChecked())
        if n == 0:
            return

        pw = self.pw_input.text()
        if not pw:
            return

        self.done = True

        # Collect execution context, store on window for main_flush
        prompt = self.llm_text.toPlainText().strip()
        selected_items = []
        script_lines = ["set -e"]
        for i, (item, toggle) in enumerate(zip(self.queue_items, self.toggles)):
            if toggle.isChecked():
                selected_items.append(item)
                script_lines.append(f"echo '=== [{item['id']}] {item['reason']} ==='")
                script_lines.append(f"cd {shlex.quote(item['cwd'])} && {item['command']}")

        self._exec_ctx = {
            "password": pw,
            "script": "\n".join(script_lines),
            "items": self.queue_items,
            "selected": selected_items,
            "prompt": prompt,
        }
        if prompt:
            print(f"LLM_PROMPT: {prompt.strip()}", flush=True)
        self._result = "executing"
        self.hide()
        QTimer.singleShot(0, QApplication.quit)

    def closeEvent(self, event):
        if not self._closed and self._result is None:
            self._closed = True
            self.timer.stop()
            self._result = "rejected"
            QApplication.quit()
        event.accept()


# ---------------------------------------------------------------------------
# GUI mode (agent-sudo-flush) — PySide2 Qt5
# ---------------------------------------------------------------------------

def main_flush():
    os.environ["QT_FILESYSTEMMODEL_WATCH_FILES"] = "0"
    
    if not os.environ.get("DISPLAY"):
        print("ERROR: DISPLAY not set, cannot open GUI", file=sys.stderr)
        sys.exit(127)

    data = _read_queue()
    if not data.get("items"):
        print("ERROR: queue is empty", file=sys.stderr)
        sys.exit(127)

    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    window = MainWindow(data.get("items", []))
    window.show()

    signal.signal(signal.SIGTERM, lambda s, f: os._exit(128 + signal.SIGTERM))
    signal.signal(signal.SIGINT, lambda s, f: os._exit(128 + signal.SIGINT))

    app.exec_()

    rc = getattr(window, '_result', 'unknown')

    if rc == "executing":
        ctx = getattr(window, '_exec_ctx', {})
        pw = ctx.get("password", "")
        script = ctx.get("script", "")
        selected = ctx.get("selected", [])
        items = ctx.get("items", [])

        try:
            proc = subprocess.Popen(
                ["sudo", "-k", "-S", "bash", "-c", script],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, bufsize=1)
        except OSError:
            print("ERROR: cannot start sudo", flush=True)
            sys.exit(127)

        try:
            proc.stdin.write(pw + "\n")
            proc.stdin.close()
        except (BrokenPipeError, OSError):
            pass

        try:
            pw_ba = bytearray(pw, "utf-8")
            ctypes.memset(id(pw_ba), 0, len(pw_ba))
        except Exception:
            pass
        del pw

        output_text = []
        sudo_prefixes = ("[sudo]", "sudo:", "Sorry", "password")
        for line in proc.stdout:
            stripped = line.lstrip()
            if not any(stripped.startswith(p) for p in sudo_prefixes):
                sys.stdout.write(line)
                sys.stdout.flush()
            output_text.append(line)

        try:
            proc.wait(timeout=300)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()
            _mark_failed_items(items, [it['id'] for it in selected])
            print("ERROR: execution timed out", flush=True)
            sys.exit(124)

        rc2 = proc.returncode
        full_output = "".join(output_text)
        if rc2 == 1 and _is_auth_failure(full_output):
            print("ERROR: authentication failed", flush=True)
            sys.exit(126)
        elif rc2 == 0:
            for it in selected:
                it["status"] = "executed"
                it["completed_at"] = _now_iso()
            _atomic_queue_update([])
            _append_history(selected)
            print(f"EXECUTED: {len(selected)} commands", flush=True)
            sys.exit(0)
        else:
            _mark_failed_items(items, [it['id'] for it in selected])
            print(f"ERROR: commands failed with exit code {rc2}", flush=True)
            sys.exit(rc2)

    if rc in ("rejected", "auto_reject"):
        print("REJECTED: user dismissed" if rc == "rejected" else "REJECTED: auto-reject after 60s")
        sys.exit(126)
    elif rc == "executed":
        sys.exit(0)
    elif rc == "timeout":
        print("ERROR: execution timed out")
        sys.exit(124)
    elif rc == "failed":
        sys.exit(1)
    else:
        print("REJECTED: window closed")
        sys.exit(126)


# ---------------------------------------------------------------------------
# Main dispatch
# ---------------------------------------------------------------------------

def main():
    os.makedirs(QUEUE_DIR, mode=0o700, exist_ok=True)
    prog = os.path.basename(sys.argv[0])
    if prog == "agent-sudo-flush":
        main_flush()
    else:
        main_cli()


if __name__ == "__main__":
    main()
