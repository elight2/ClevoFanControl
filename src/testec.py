#!/usr/bin/env python3
"""CLI test tool for the clevoec .so (libclevoec).

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

# Fan index -> tachometer address, same values as EC_RPM1/EC_RPM2 in libclevoec.h.
RPM_ADDR = {1: 0xD0, 2: 0xD2}

lib = ctypes.CDLL(LIB_PATH, use_errno=True)
# init_perm: 0 on success, -1 on failure with errno set
lib.init_perm.restype = ctypes.c_int
lib.init_perm.argtypes = []
# fix_fan_duty/release_fan_duty return void: a failed write cannot be reported
lib.fix_fan_duty.restype = None
lib.fix_fan_duty.argtypes = [ctypes.c_ubyte, ctypes.c_ubyte]
lib.release_fan_duty.restype = None
lib.release_fan_duty.argtypes = [ctypes.c_ubyte]
lib.get_rpm.restype = ctypes.c_ushort
lib.get_rpm.argtypes = [ctypes.c_ubyte]


def fan(arg):
    if arg not in ("1", "2"):
        sys.exit(f"bad fan {arg}: must be 1 or 2")
    return int(arg)


def cmd_set(index, arg):
    try:
        duty = int(arg)
    except ValueError:
        sys.exit(f"bad duty {arg}: must be -1 or 0-255")
    if duty != -1 and not 0 <= duty <= 255:
        sys.exit(f"bad duty {duty}: must be -1 or 0-255")
    if duty == -1:
        lib.release_fan_duty(index)
        print(f"fan {index} auto")
    else:
        lib.fix_fan_duty(index, duty)
        print(f"fan {index} duty {duty}")


def cmd_get(index):
    print(f"fan {index} rpm {lib.get_rpm(RPM_ADDR[index])}")


def main(argv):
    if len(argv) == 4 and argv[1] == "s":
        cmd_set(fan(argv[2]), argv[3])
    elif len(argv) == 3 and argv[1] == "g":
        cmd_get(fan(argv[2]))
    else:
        sys.exit(__doc__.strip())


if __name__ == "__main__":
    if lib.init_perm() != 0:
        # init_perm returns -1 and leaves the reason in errno
        err = ctypes.get_errno()
        sys.exit(f"init_perm failed: {os.strerror(err)} (errno {err})")
    main(sys.argv)
