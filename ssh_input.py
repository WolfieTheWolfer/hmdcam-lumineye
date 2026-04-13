#!/usr/bin/env python3
"""
Virtual joystick bridge for hmdcam InputListener.
Run this over SSH to control hmdcam from your keyboard.

Keys:
  Arrow keys -> navigation
  Enter      -> OK (Space in imgui)
  ESC        -> Back
  m          -> Menu (Compose/Power)
  q          -> Quit this script
"""

import evdev
from evdev import UInput, ecodes as e
import tty
import sys
import termios
import time

# Create virtual joystick device
# Must appear as a joystick to pass InputListener's ID_INPUT_JOYSTICK filter
cap = {
    e.EV_KEY: [
        e.BTN_A,    # -> kButtonDown  (arrow down)
        e.BTN_B,    # -> kButtonRight (arrow right)
        e.BTN_X,    # -> kButtonLeft  (arrow left)
        e.BTN_C,    # -> kButtonUp    (arrow up)
        e.BTN_TL2,  # -> kButtonPower (menu)
        e.BTN_Y,    # -> kButtonBack  (ESC)
        e.BTN_Z,    # -> kButtonOK    (Enter/Space)
    ],
}

def press(ui, code):
    ui.write(e.EV_KEY, code, 1)
    ui.syn()
    time.sleep(0.05)
    ui.write(e.EV_KEY, code, 0)
    ui.syn()

def getch():
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == '\x1b':
            ch2 = sys.stdin.read(2)
            return '\x1b' + ch2
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)

def main():
    try:
        ui = UInput(cap, name='hmdcam-virtual-joystick', vendor=0x054c, product=0x0001)
    except Exception as ex:
        print(f"Failed to create uinput device: {ex}")
        print("Try: sudo python3 this_script.py")
        return

    print("Virtual joystick created. Controls:")
    print("  Arrow keys = navigate")
    print("  Enter      = OK/select")
    print("  ESC        = back")
    print("  m          = open menu")
    print("  q          = quit")
    print()

    # Wait for udev to pick up the device
    time.sleep(1)
    print("Ready. Press keys to control hmdcam.")

    while True:
        ch = getch()

        if ch == 'q':
            break
        elif ch == '\x1b[A':  # up arrow
            press(ui, e.BTN_C)
        elif ch == '\x1b[B':  # down arrow
            press(ui, e.BTN_A)
        elif ch == '\x1b[C':  # right arrow
            press(ui, e.BTN_B)
        elif ch == '\x1b[D':  # left arrow
            press(ui, e.BTN_X)
        elif ch in ('\r', '\n'):  # Enter
            press(ui, e.BTN_Z)
        elif ch == '\x1b':   # ESC
            press(ui, e.BTN_Y)
        elif ch in ('m', 'M'):  # menu
            press(ui, e.BTN_TL2)

    ui.close()
    print("Done.")

if __name__ == '__main__':
    main()