#!/usr/bin/env python3
"""Send key events to agent-sudo GUI window via XSendEvent (window-targeted)."""

import sys
import time
import argparse
import array
from Xlib import X, XK, display
from Xlib.display import Display
from Xlib.protocol.event import KeyPress, KeyRelease


def extract_pid(prop_value):
    if isinstance(prop_value, int):
        return prop_value
    if isinstance(prop_value, (list, tuple, array.array)):
        return int(prop_value[0])
    return int(prop_value)


def find_window_by_pid(dpy, pid, timeout=10.0):
    net_wm_pid_atom = dpy.intern_atom("_NET_WM_PID")
    net_client_list = dpy.intern_atom("_NET_CLIENT_LIST")
    start = time.time()
    while time.time() - start < timeout:
        root = dpy.screen().root
        window_ids = root.get_full_property(net_client_list, X.AnyPropertyType)
        if window_ids:
            for win_id in window_ids.value:
                win = dpy.create_resource_object("window", win_id)
                try:
                    pid_prop = win.get_full_property(net_wm_pid_atom, X.AnyPropertyType)
                    if pid_prop and pid_prop.value is not None:
                        try:
                            wpid = extract_pid(pid_prop.value)
                        except (ValueError, IndexError, TypeError):
                            continue
                        if wpid == pid:
                            return win
                except Exception:
                    pass
        time.sleep(0.3)
    return None


def find_window_by_class(dpy, wm_class_substr, timeout=10.0):
    net_client_list = dpy.intern_atom("_NET_CLIENT_LIST")
    start = time.time()
    while time.time() - start < timeout:
        root = dpy.screen().root
        window_ids = root.get_full_property(net_client_list, X.AnyPropertyType)
        if window_ids:
            for win_id in window_ids.value:
                win = dpy.create_resource_object("window", win_id)
                try:
                    cls = win.get_wm_class()
                    if cls and wm_class_substr in str(cls):
                        return win
                except Exception:
                    pass
        time.sleep(0.3)
    return None


def _make_key_event(dpy, win, keycode, press, state=0):
    cls = KeyPress if press else KeyRelease
    return cls(
        detail=keycode,
        time=X.CurrentTime,
        root=dpy.screen().root,
        window=win,
        child=X.NONE,
        root_x=0, root_y=0,
        event_x=0, event_y=0,
        state=state,
        same_screen=1,
    )


def send_key_to_window(dpy, win, keysym):
    keycode = dpy.keysym_to_keycode(keysym)
    if keycode == 0:
        return False
    press = _make_key_event(dpy, win, keycode, True)
    win.send_event(press, X.KeyPressMask)
    dpy.flush()
    time.sleep(0.05)
    release = _make_key_event(dpy, win, keycode, False)
    win.send_event(release, X.KeyReleaseMask)
    dpy.flush()
    return True


def send_text_to_window(dpy, win, text):
    for char in text:
        ks = XK.string_to_keysym(char)
        if ks == 0:
            continue
        shift_needed = char.isupper() or char in '~!@#$%^&*()_+{}|:"<>?'
        state = X.ShiftMask if shift_needed else 0

        if shift_needed:
            shift_kc = dpy.keysym_to_keycode(XK.XK_Shift_L)
            if shift_kc:
                s = _make_key_event(dpy, win, shift_kc, True, 0)
                win.send_event(s, X.KeyPressMask)
                dpy.flush()

        kc = dpy.keysym_to_keycode(ks)
        if kc:
            p = _make_key_event(dpy, win, kc, True, state)
            win.send_event(p, X.KeyPressMask)
            dpy.flush()
            time.sleep(0.04)
            r = _make_key_event(dpy, win, kc, False, state)
            win.send_event(r, X.KeyReleaseMask)
            dpy.flush()

        if shift_needed:
            shift_kc = dpy.keysym_to_keycode(XK.XK_Shift_L)
            if shift_kc:
                s = _make_key_event(dpy, win, shift_kc, False, 0)
                win.send_event(s, X.KeyReleaseMask)
                dpy.flush()


def main():
    parser = argparse.ArgumentParser(description="Send keys to agent-sudo GUI")
    parser.add_argument("--action", choices=["reject", "accept", "type"], required=True)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--text", default="")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    dpy = Display()
    win = find_window_by_pid(dpy, args.pid, args.timeout)
    if win is None:
        win = find_window_by_class(dpy, "agent-sudo-flush", args.timeout)
    if win is None:
        print(f"ERROR: window not found within {args.timeout}s", file=sys.stderr)
        dpy.close()
        sys.exit(1)

    win.configure(stack_mode=X.Above)
    dpy.flush()
    time.sleep(0.3)

    if args.action == "reject":
        ok = send_key_to_window(dpy, win, XK.XK_Escape)
        if ok:
            print(f"Escape -> PID={args.pid}")
        else:
            sys.exit(1)

    elif args.action == "accept":
        ok = send_key_to_window(dpy, win, XK.XK_Return)
        if ok:
            print(f"Enter -> PID={args.pid}")
        else:
            sys.exit(1)

    elif args.action == "type":
        send_text_to_window(dpy, win, args.text)
        print(f"Text -> PID={args.pid}")

    dpy.close()


if __name__ == "__main__":
    main()
