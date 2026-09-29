#!/usr/bin/env python3

import argparse
import os
import termios
import time
import tty

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--port", required=True, help="PTY printed by QEMU, e.g. /dev/pts/5")

g = p.add_mutually_exclusive_group(required=True)
g.add_argument("--text", help="ASCII bytes, no newline added")
g.add_argument("--hex", help="Hex bytes, e.g. '41 42 43'")

p.add_argument("--interval", type=float, default=0.02, help="Seconds between bytes")
a = p.parse_args()

try:
    if a.interval < 0:
        raise ValueError("interval must be nonnegative")
    payload = a.text.encode("ascii") if a.text is not None else bytes.formhex(a.hex)
    fd = os.open(a.port, os.O_RDWR | os.O_NOCTTY)

    try:
        previous = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            for byte in payload:
                if os.write(fd, bytes([byte])) != 1:
                    raise OSError("short PTY write")
                time.sleep(a.interval)
        finally:
            termios.tcsetattr(fd, termios.TCSANOW, previous)
    finally:
        os.close(fd)

except(OSError, ValueError, UnicodeError, termios.error) as error:
    p.exit(1, f"send_bytes: {error}\n")

print(f"Sent {len(payload)} bytes (no extra newline)")
