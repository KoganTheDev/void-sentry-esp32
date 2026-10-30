"""Read the board's serial log for N seconds without resetting it.

DTR and RTS are held low while the port opens, so the CH340 does not pulse EN and the board keeps running.

Usage:
    python read_serial.py COM3 30

Output: the raw serial lines, unmodified, on stdout.
Needs pyserial (the PlatformIO penv Python has it).
"""
import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser(description="Read the board's serial log without resetting it (DTR/RTS low).")
    ap.add_argument("port", help="serial port, e.g. COM3 (see `pio device list`)")
    ap.add_argument("seconds", type=float, help="how long to read")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = args.port, args.baud, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    end = time.time() + args.seconds
    while time.time() < end:
        line = s.readline()
        if line:
            sys.stdout.write(line.decode("utf-8", "replace"))
    s.close()


if __name__ == "__main__":
    main()
