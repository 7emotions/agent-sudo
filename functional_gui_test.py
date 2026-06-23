#!/usr/bin/env python3
"""Functional GUI tests for agent-sudo. Run: xvfb-run python3 functional_gui_test.py"""
import sys, os, json, tempfile, unittest
sys.path.insert(0, os.path.expanduser("/home/ubuntu/.ws/pkagt/agent-sudo"))
import tkinter as tk
import main


class TestGUIFunctional(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.queue_path = os.path.join(self.tmpdir, "queue.json")
        with open(self.queue_path, "w") as f:
            json.dump({"version": 1, "items": [
                {"id": 1, "reason": "安装依赖", "command": "apt install -y gcc",
                 "cwd": "/tmp", "timestamp": "2026-01-01T00:00:00", "status": "pending"},
                {"id": 2, "reason": "启动服务", "command": "systemctl start nginx",
                 "cwd": "/tmp", "timestamp": "2026-01-01T00:00:01", "status": "pending"},
                {"id": 3, "reason": "写入配置", "command": "echo x > /etc/hosts",
                 "cwd": "/tmp", "timestamp": "2026-01-01T00:00:02", "status": "pending"},
            ]}, f)

    def tearDown(self):
        import shutil; shutil.rmtree(self.tmpdir, ignore_errors=True)

    # ------------------------------------------------------------------
    # test_01: Window title
    # ------------------------------------------------------------------
    def test_01_window_title(self):
        root = tk.Tk()
        try:
            main.build_gui(root, self.queue_path)
            root.update_idletasks()
            self.assertEqual(root.title(), "Agent 特权命令审批")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_02: Summary shows correct item count
    # ------------------------------------------------------------------
    def test_02_summary_shows_correct_count(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            self.assertIn("共 3 条", widgets["summary_label"].cget("text"))
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_03: All items checked by default
    # ------------------------------------------------------------------
    def test_03_all_checked_by_default(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            for var in widgets["check_vars"]:
                self.assertTrue(var.get(), "all check_vars should default to True")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_04: Execute disabled when no items checked
    # ------------------------------------------------------------------
    def test_04_execute_disabled_when_none_checked(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            # Uncheck all
            for var in widgets["check_vars"]:
                var.set(False)
            # programmatic var.set() does not fire checkbox command;
            # call _update_execute_btn explicitly
            main._update_execute_btn(widgets["check_vars"], widgets["execute_btn"])
            root.update_idletasks()
            self.assertEqual(widgets["execute_btn"].cget("state"), "disabled")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_05: Execute enabled when at least one checked
    # ------------------------------------------------------------------
    def test_05_execute_enabled_when_one_checked(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            # Uncheck all first
            for var in widgets["check_vars"]:
                var.set(False)
            # Re-check first item
            widgets["check_vars"][0].set(True)
            main._update_execute_btn(widgets["check_vars"], widgets["execute_btn"])
            root.update_idletasks()
            self.assertEqual(widgets["execute_btn"].cget("state"), "normal")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_06: Password field uses bullet masking
    # ------------------------------------------------------------------
    def test_06_password_field_masked(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            self.assertEqual(widgets["password_entry"].cget("show"), "\u25cf")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_07: Countdown label format
    # ------------------------------------------------------------------
    def test_07_countdown_label_format(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            text = widgets["countdown_label"].cget("text")
            self.assertRegex(text, r"\u23f1 \d+s")
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_08: LLM prompt body collapsed on startup
    # ------------------------------------------------------------------
    def test_08_llm_prompt_initially_collapsed(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            # llm_prompt_body should NOT be packed (pack_forget'd);
            # pack_info() raises TclError when widget is not managed
            with self.assertRaises(tk.TclError):
                widgets["llm_prompt_body"].pack_info()
            # Disclosure label should show collapsed indicator
            self.assertIn("\u25b6", widgets["llm_disclosure_label"].cget("text"))
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_09: All control buttons exist with correct texts
    # ------------------------------------------------------------------
    def test_09_select_all_deselect_all_buttons_exist(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            self.assertEqual(widgets["select_all_btn"].cget("text"), "全选")
            self.assertEqual(widgets["deselect_all_btn"].cget("text"), "取消全选")
            self.assertEqual(widgets["reject_btn"].cget("text"), "拒绝")
            self.assertIn("执行", widgets["execute_btn"].cget("text"))
        finally:
            root.destroy()

    # ------------------------------------------------------------------
    # test_10: Reject button exists and has a command configured
    #          (cannot invoke — handler calls os._exit(126))
    # ------------------------------------------------------------------
    def test_10_reject_button_exists_and_configured(self):
        root = tk.Tk()
        try:
            widgets = main.build_gui(root, self.queue_path)
            root.update_idletasks()
            self.assertEqual(widgets["reject_btn"].cget("text"), "拒绝")
            # Verify a command is bound (not None / empty)
            cmd = widgets["reject_btn"].cget("command")
            self.assertIsNotNone(cmd, "reject button must have a command")
        finally:
            root.destroy()


if __name__ == "__main__":
    unittest.main(verbosity=2)
