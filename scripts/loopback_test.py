#!/usr/bin/env python3

import argparse
import random
import sys
import time

import usb1

PACKET_SIZE = 1024

parser = argparse.ArgumentParser(description="USB device benchmark")
parser.add_argument("--loop-bulk", action="store_true", help="Do loopback bulk test")
parser.add_argument("--out-bulk", action="store_true", help="Do OUT bulk test")
parser.add_argument("--in-bulk", action="store_true", help="Do IN bulk test")
args = parser.parse_args()

with usb1.USBContext() as ctx:
    handle = None
    for dev in ctx.getDeviceIterator(skip_on_error=True):
        if dev.getVendorID() != 0x0403 and dev.getProductID() != 0x6010:
            continue
        try:
            if dev.getProduct() != "js2232":
                continue
        except usb1.USBErrorAccess:
            pass
        handle = dev.open()
        break
    assert handle
    handle.setAutoDetachKernelDriver(True)
    with handle.claimInterface(0):
        handle.clearHalt(1)
        handle.clearHalt(0x81)
        nbytes = 0
        tstart = time.time()
        try:
            while True:
                if args.loop_bulk:
                    obuf = random.randbytes(PACKET_SIZE)
                    handle.bulkWrite(1, obuf, timeout=1000)
                    ibuf = handle.bulkRead(1, PACKET_SIZE, timeout=1000)
                    ibuf_inv = bytes([b ^ 0xFF for b in ibuf])
                    assert ibuf_inv == obuf
                    nbytes += len(obuf) * 2

                elif args.out_bulk:
                    obuf = random.randbytes(PACKET_SIZE)
                    handle.bulkWrite(1, obuf, timeout=1000)
                    nbytes += len(obuf)
                elif args.in_bulk:
                    ibuf = handle.bulkRead(1, PACKET_SIZE, timeout=1000)
                    assert len(ibuf) == PACKET_SIZE
                    nbytes += len(ibuf)
                else:
                    raise RuntimeError("must select a test mode")
                if nbytes % (16 * 1024) == 0:
                    print(".", end="", flush=True)
        except KeyboardInterrupt:
            pass
        tend = time.time()
        kbits_per_second = (nbytes * 8 / 1000) / (tend - tstart)
        print()
        print(f"Loopback test speed: {kbits_per_second:,.2f} Kbps")
