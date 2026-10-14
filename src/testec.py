#!/usr/bin/env python3
"""CLI test tool for the clevoec .so.

usage:
  testec.py s <fan 1|2> <duty 0-255>   set fan speed
  testec.py s <fan 1|2> a              set fan to auto
  testec.py g <fan 1|2>                read fan rpm
"""
import ctypes
import os
import sys

LIB_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "build", "libclevoec.so"
)
AUTO = -1

lib = ctypes.CDLL(LIB_PATH)
lib.init.restype = ctypes.c_int
lib.init.argtypes = []
lib.set_fan_speed.restype = ctypes.c_int
lib.set_fan_speed.argtypes = [ctypes.c_int, ctypes.c_int]
lib.get_rpm.restype = ctypes.c_int
lib.get_rpm.argtypes = [ctypes.c_int]


def check(rc):
    if rc < 0:
        sys.exit(f"{os.strerror(-rc)} (errno {-rc})")
    return rc


def fan(arg):
    if arg not in ("1", "2"):
        sys.exit(f"bad fan {arg}: must be 1 or 2")
    return int(arg)


def cmd_set(index, arg):
    if arg == "a":
        duty, shown = AUTO, "auto"
    else:
        try:
            duty = int(arg)
        except ValueError:
            sys.exit(f"bad duty {arg}: must be 0-255 or a")
        if not 0 <= duty <= 255:
            sys.exit(f"bad duty {arg}: must be 0-255 or a")
        shown = f"duty {duty}"
    check(lib.set_fan_speed(duty, index))
    print(f"fan {index} {shown}")


def cmd_get(index):
    print(f"fan {index} rpm {lib.get_rpm(index)}")


def main(argv):
    if len(argv) == 4 and argv[1] == "s":
        cmd_set(fan(argv[2]), argv[3])
    elif len(argv) == 3 and argv[1] == "g":
        cmd_get(fan(argv[2]))
    else:
        sys.exit(__doc__.strip())


if __name__ == "__main__":
    check(lib.init())
    main(sys.argv)
