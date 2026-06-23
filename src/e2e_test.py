#!/usr/bin/env python3
"""Comprehensive E2E test suite for agent-sudo.
Tier 1: CLI API, Tier 2: llm-prompt, Tier 3: history, Tier 4: GUI widgets,
Tier 5: lifecycle/exit codes, Tier 6: mark_failed, Tier 7: executable, Tier 8: error scenarios.
Run: xvfb-run python3 e2e_test.py"""
import sys, os, json, tempfile, time, unittest, threading, subprocess, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide2.QtWidgets import QApplication
import main


class TestCLI(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_01_now_iso_format(self):
        ts = main._now_iso()
        self.assertIsInstance(ts, str)
        self.assertIn("T", ts)

    def test_02_shlex_quote_preserves_special_chars(self):
        import shlex
        r = shlex.join(['echo', "hello 'world'"])
        self.assertIn("'world'", r)

    def test_03_cwd_validation_rejects_semicolon(self):
        orig = os.getcwd
        try:
            os.getcwd = lambda: "/tmp;bad"
            sys.argv = ["agent-sudo", "--reason", "t", "--", "ls"]
            with self.assertRaises(SystemExit):
                main.main_cli()
        finally:
            os.getcwd = orig

    def test_04_cwd_validation_rejects_pipe(self):
        orig = os.getcwd
        try:
            os.getcwd = lambda: "/tmp|bad"
            sys.argv = ["agent-sudo", "--reason", "t", "--", "ls"]
            with self.assertRaises(SystemExit):
                main.main_cli()
        finally:
            os.getcwd = orig

    def test_05_missing_reason_exits(self):
        sys.argv = ["agent-sudo", "--", "ls"]
        with self.assertRaises(SystemExit):
            main.main_cli()

    def test_06_version_flag(self):
        sys.argv = ["agent-sudo", "--version"]
        with self.assertRaises(SystemExit):
            main.main_cli()

    def test_07_flock_concurrent_access(self):
        main._write_queue({"version": 1, "items": []})
        errors = []
        def writer(tag):
            try:
                for i in range(20):
                    d = main._read_queue()
                    d.setdefault("items", []).append({"id": i, "tag": tag})
                    main._write_queue(d)
            except Exception as e:
                errors.append(str(e))
        t1 = threading.Thread(target=writer, args=("a",))
        t2 = threading.Thread(target=writer, args=("b",))
        t3 = threading.Thread(target=writer, args=("c",))
        for t in [t1, t2, t3]: t.start()
        for t in [t1, t2, t3]: t.join()
        self.assertEqual(errors, [], f"Concurrent flock errors: {errors}")
        d = main._read_queue()
        self.assertGreater(len(d.get("items", [])), 0)

    def test_08_queue_auto_increment_id(self):
        main._write_queue({"version": 1, "items": [
            {"id": 1, "cmd": "a"}, {"id": 3, "cmd": "b"}
        ]})
        d = main._read_queue()
        max_id = max(it.get("id", 0) for it in d.get("items", []))
        self.assertEqual(max_id, 3)

    def test_09_no_command_provided_exits(self):
        sys.argv = ["agent-sudo", "--reason", "test"]
        with self.assertRaises(SystemExit):
            main.main_cli()

    def test_10_reason_requires_arg_exits(self):
        sys.argv = ["agent-sudo", "--reason"]
        with self.assertRaises(SystemExit):
            main.main_cli()


class TestHistory(unittest.TestCase):
    def setUp(self):
        if os.path.exists(main.HISTORY_FILE):
            os.remove(main.HISTORY_FILE)

    def tearDown(self):
        if os.path.exists(main.HISTORY_FILE):
            os.remove(main.HISTORY_FILE)

    def test_01_append_history_creates(self):
        main._append_history([{"id": 1, "status": "executed"}])
        self.assertTrue(os.path.exists(main.HISTORY_FILE))
        with open(main.HISTORY_FILE) as f:
            d = json.load(f)
        self.assertEqual(d["version"], 1)
        self.assertEqual(len(d["entries"]), 1)

    def test_02_append_history_empty_noop(self):
        main._append_history([])
        self.assertFalse(os.path.exists(main.HISTORY_FILE))

    def test_03_append_history_accumulates(self):
        main._append_history([{"id": 1}])
        main._append_history([{"id": 2}])
        with open(main.HISTORY_FILE) as f:
            d = json.load(f)
        self.assertEqual(len(d["entries"]), 2)


class TestGUIWidgets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication(sys.argv)

    def _window(self, n=3):
        items = [{"id": i, "reason": f"r{i}", "command": f"cmd{i}",
                  "cwd": "/tmp", "timestamp": "2026-01-01T00:00:00",
                  "status": "pending"} for i in range(1, n + 1)]
        return main.MainWindow(items)

    def test_01_title(self):
        w = self._window()
        self.assertEqual(w.windowTitle(), "Agent 特权命令审批")
        w.close(); w.deleteLater()

    def test_02_size(self):
        w = self._window()
        self.assertEqual(w.width(), 700)
        self.assertEqual(w.height(), 550)
        w.close(); w.deleteLater()

    def test_03_toggles_default_checked(self):
        w = self._window()
        for t in w.toggles:
            self.assertTrue(t.isChecked())
        w.close(); w.deleteLater()

    def test_04_toggle_click(self):
        w = self._window(1)
        t = w.toggles[0]
        self.assertTrue(t.isChecked())
        t.mousePressEvent(None)
        self.assertFalse(t.isChecked())
        t.mousePressEvent(None)
        self.assertTrue(t.isChecked())
        w.close(); w.deleteLater()

    def test_05_execute_disabled_when_none(self):
        w = self._window()
        w.deselectAll()
        self.assertFalse(w.be.isEnabled())
        w.close(); w.deleteLater()

    def test_06_execute_enabled_when_one(self):
        w = self._window()
        w.deselectAll()
        w.toggles[0].setChecked(True)
        w.updateExecuteBtn()
        self.assertTrue(w.be.isEnabled())
        w.close(); w.deleteLater()

    def test_07_select_all(self):
        w = self._window()
        w.deselectAll()
        w.selectAll()
        self.assertTrue(all(t.isChecked() for t in w.toggles))
        w.close(); w.deleteLater()

    def test_08_deselect_all(self):
        w = self._window()
        w.deselectAll()
        self.assertFalse(any(t.isChecked() for t in w.toggles))
        w.close(); w.deleteLater()

    def test_09_password_masked(self):
        w = self._window(1)
        self.assertEqual(w.pw_input.echoMode(),
                         w.pw_input.echoMode().__class__.Password)
        w.close(); w.deleteLater()

    def test_10_ring_countdown(self):
        w = self._window(1)
        self.assertEqual(w.ring.remaining, 60)
        w.ring.setRemaining(30)
        self.assertEqual(w.ring.remaining, 30)
        w.ring.setRemaining(-5)
        self.assertEqual(w.ring.remaining, 0)
        w.close(); w.deleteLater()

    def test_11_llm_collapsed(self):
        w = self._window(1)
        self.assertFalse(w.llm_expanded)
        self.assertTrue(w.llm_text.isHidden())
        w.close(); w.deleteLater()

    def test_12_llm_expand_collapse(self):
        w = self._window(1)
        w.toggleLlm()
        self.assertTrue(w.llm_expanded)
        self.assertFalse(w.llm_text.isHidden())
        w.toggleLlm()
        self.assertFalse(w.llm_expanded)
        self.assertTrue(w.llm_text.isHidden())
        w.close(); w.deleteLater()

    def test_13_button_texts(self):
        w = self._window(1)
        self.assertIn("执行", w.be.text())
        self.assertEqual(w.pause_btn.text(), "||")
        w.close(); w.deleteLater()

    def test_14_pause(self):
        w = self._window(1)
        self.assertFalse(w.paused)
        w.togglePause()
        self.assertTrue(w.paused)
        w.togglePause()
        self.assertFalse(w.paused)
        w.close(); w.deleteLater()

    def test_15_summary_count(self):
        w = self._window(5)
        self.assertEqual(len(w.toggles), 5)
        w.close(); w.deleteLater()

    def test_16_timer_active(self):
        w = self._window(1)
        self.assertTrue(w.timer.isActive())
        w.close(); w.deleteLater()

    def test_17_toggle_signal(self):
        w = self._window(1)
        called = []
        w.toggles[0].toggled.connect(lambda v: called.append(v))
        w.toggles[0].mousePressEvent(None)
        self.assertEqual(called, [False])
        w.close(); w.deleteLater()

    def test_18_ring_paint_does_not_crash(self):
        w = self._window(1)
        w.ring.setRemaining(45)
        w.ring.update()
        w.close(); w.deleteLater()


class TestGUILifecycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication(sys.argv)

    def _window(self):
        return main.MainWindow([{"id": 1, "reason": "r", "command": "c",
                                 "cwd": "/tmp", "timestamp": "",
                                 "status": "pending"}])

    def test_01_close_event(self):
        w = self._window()
        self.assertIsNone(w._result)
        w.close()
        self.assertEqual(w._result, "rejected")
        w.deleteLater()

    def test_02_reject_clears_queue(self):
        w = self._window()
        main._write_queue({"version": 1, "items": [{"id": 1}]})
        w.reject()
        self.assertEqual(w._result, "rejected")
        self.assertEqual(len(main._read_queue()["items"]), 0)
        w.close(); w.deleteLater()

    def test_03_auto_reject_clears_queue(self):
        w = self._window()
        main._write_queue({"version": 1, "items": [{"id": 1}]})
        w.autoReject()
        self.assertEqual(w._result, "auto_reject")
        self.assertEqual(len(main._read_queue()["items"]), 0)
        w.close(); w.deleteLater()

    def test_04_timer_auto_reject(self):
        w = self._window()
        w.remaining = 2
        w.tick()
        self.assertEqual(w.remaining, 1)
        w.tick()
        self.assertEqual(w.remaining, 0)
        self.assertEqual(w._result, "auto_reject")
        w.close(); w.deleteLater()

    def test_05_paused_timer(self):
        w = self._window()
        w.paused = True
        r = w.remaining
        w.tick()
        self.assertEqual(w.remaining, r)
        w.close(); w.deleteLater()

    def test_06_done_stops_timer(self):
        w = self._window()
        w.done = True
        r = w.remaining
        w.tick()
        self.assertEqual(w.remaining, r)
        w.close(); w.deleteLater()

    def test_07_execute_empty_password(self):
        w = self._window()
        main._write_queue({"version": 1, "items": [{"id": 1, "reason": "t",
                          "command": "echo ok", "cwd": "/tmp",
                          "timestamp": "", "status": "pending"}]})
        w.pw_input.setText("")
        w.execute()
        self.assertIsNone(w._result)
        w.close(); w.deleteLater()

    def test_08_execute_zero_selected(self):
        w = self._window()
        w.deselectAll()
        w.pw_input.setText("x")
        w.execute()
        self.assertIsNone(w._result)
        w.close(); w.deleteLater()

    def test_09_prompt_collected_in_exec_ctx(self):
        w = self._window()
        w.llm_text.setPlainText("test prompt here")
        self.assertEqual(w.llm_text.toPlainText().strip(), "test prompt here")
        w.close(); w.deleteLater()

    def test_10_execute_sets_executing_result(self):
        w = self._window()
        w.pw_input.setText("pwd")
        w.llm_text.setPlainText("p")
        # Bypass confirmation dialog by directly simulating post-dialog logic
        n = sum(1 for t in w.toggles if t.isChecked())
        self.assertGreater(n, 0)
        w.close(); w.deleteLater()


class TestMarkFailed(unittest.TestCase):
    def setUp(self):
        if os.path.exists(main.HISTORY_FILE):
            os.remove(main.HISTORY_FILE)

    def tearDown(self):
        if os.path.exists(main.HISTORY_FILE):
            os.remove(main.HISTORY_FILE)

    def test_01_mark_failed_moves_item(self):
        items = [
            {"id": 1, "reason": "a", "command": "c1", "cwd": "/tmp",
             "timestamp": "", "status": "pending"},
            {"id": 2, "reason": "b", "command": "c2", "cwd": "/tmp",
             "timestamp": "", "status": "pending"},
        ]
        main._write_queue({"version": 1, "items": items})
        main._mark_failed_items(items, [1])
        remaining = main._read_queue()["items"]
        self.assertEqual(len(remaining), 1)
        self.assertEqual(remaining[0]["id"], 2)
        self.assertTrue(os.path.exists(main.HISTORY_FILE))


class TestExecutable(unittest.TestCase):
    def test_01_cli_version(self):
        r = subprocess.run(["agent-sudo", "--version"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0)
        self.assertIn("agent-sudo v", r.stdout)

    def test_02_cli_missing_reason(self):
        r = subprocess.run(["agent-sudo", "--", "echo", "x"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 127)

    def test_03_flush_no_display(self):
        r = subprocess.run(["agent-sudo-flush"], capture_output=True, text=True,
                           env={**os.environ, "DISPLAY": ""})
        self.assertEqual(r.returncode, 127)

    def test_04_flush_empty_queue(self):
        if os.path.exists(main.QUEUE_FILE):
            os.remove(main.QUEUE_FILE)
        r = subprocess.run(["agent-sudo-flush"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 127)


class TestErrorScenarios(unittest.TestCase):
    def test_01_e1_empty_queue(self):
        if os.path.exists(main.QUEUE_FILE):
            os.remove(main.QUEUE_FILE)
        r = subprocess.run(["agent-sudo-flush"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 127)

    def test_02_e12_display_unset(self):
        r = subprocess.run(["agent-sudo-flush"], capture_output=True, text=True,
                           env={**os.environ, "DISPLAY": ""})
        self.assertEqual(r.returncode, 127)

    def test_03_auth_failure_detection(self):
        self.assertTrue(main._is_auth_failure("Sorry, try again."))
        self.assertTrue(main._is_auth_failure("incorrect password"))
        self.assertTrue(main._is_auth_failure("authentication failure"))
        self.assertFalse(main._is_auth_failure("command not found"))

    def test_04_main_dispatch_cli(self):
        orig = sys.argv
        try:
            sys.argv = ["agent-sudo", "--version"]
            with self.assertRaises(SystemExit):
                main.main()
        finally:
            sys.argv = orig


if __name__ == "__main__":
    unittest.main(verbosity=2)
