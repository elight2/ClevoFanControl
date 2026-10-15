#!/usr/bin/env python3
"""CLI test tool for the clevoec .so.

usage:
  testec.py s <fan 1|2> <-1|0-255>   set fan speed, -1 = auto
  testec.py g <fan 1|2>              read fan rpm
"""
import ctypes
import os
import sys

LIB_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "build", "libclevoec.so"
)

lib = ctypes.CDLL(LIB_PATH)
lib.ec_init.restype = ctypes.c_int
lib.ec_init.argtypes = []
lib.set_fan_speed.restype = ctypes.c_int
lib.set_fan_speed.argtypes = [ctypes.c_int, ctypes.c_int]
lib.get_rpm.restype = ctypes.c_int
lib.get_rpm.argtypes = [ctypes.c_int]

# so returns -1 for bad duty and -2 for bad index; ec_init returns -errno
ERR = {-1: "bad duty: must be -1 or 0-255", -2: "bad index: must be 1 or 2"}


def check(rc, libcall=False):
    if rc < 0:
        if libcall and rc in ERR:
            sys.exit(ERR[rc])
        sys.exit(f"{os.strerror(-rc)} (errno {-rc})")
    return rc


def fan(arg):
    if arg not in ("1", "2"):
        sys.exit(f"bad fan {arg}: must be 1 or 2")
    return int(arg)


def cmd_set(index, arg):
    try:
        duty = int(arg)
    except ValueError:
        sys.exit(f"bad duty {arg}: must be -1 or 0-255")
    check(lib.set_fan_speed(duty, index), libcall=True)
    print(f"fan {index} {'auto' if duty == -1 else f'duty {duty}'}")


def cmd_get(index):
    print(f"fan {index} rpm {check(lib.get_rpm(index), libcall=True)}")


def main(argv):
    if len(argv) == 4 and argv[1] == "s":
        cmd_set(fan(argv[2]), argv[3])
    elif len(argv) == 3 and argv[1] == "g":
        cmd_get(fan(argv[2]))
    else:
        sys.exit(__doc__.strip())


if __name__ == "__main__":
    check(lib.ec_init())
    main(sys.argv)
