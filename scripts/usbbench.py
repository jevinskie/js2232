#!/usr/bin/env python3

import argparse
import random
import time

import usb1

PACKET_SIZE = 64


def main(args):
    if args.async_xfers:
        raise NotImplementedError()
    with usb1.USBContext() as ctx:
        handle = None
        for dev in ctx.getDeviceIterator(skip_on_error=True):
            if dev.getVendorID() != args.vid and dev.getProductID() != args.pid:
                continue
            try:
                if args.product not in dev.getProduct():
                    continue
                if args.manufacturer not in dev.getManufacturer():
                    continue
            except usb1.USBErrorAccess:
                pass
            handle = dev.open()
            break
        assert handle
        handle.setAutoDetachKernelDriver(True)
        with handle.claimInterface(args.interface):
            handle.clearHalt(args.out_ep)
            handle.clearHalt(args.in_ep)
            nbytes = 0
            tstart = time.time()
            try:
                while True:
                    obuf = bytearray(random.randbytes(PACKET_SIZE))
                    print(f"obuf: {obuf.hex(' ')}")
                    handle.bulkWrite(1, obuf, timeout=1000)
                    ibuf = handle.bulkRead(2, PACKET_SIZE, timeout=1000)
                    ibuf_inv = bytes([b ^ 0xFF for b in ibuf])
                    print(f"Ibuf: {ibuf_inv.hex(' ')}")
                    assert ibuf_inv == obuf
                    nbytes += len(obuf) * 2
                    if nbytes % (16 * 1024) == 0:
                        print(".", end="", flush=True)
            except KeyboardInterrupt:
                pass
            tend = time.time()
            kbits_per_second = (nbytes * 8 / 1000) / (tend - tstart)
            print()
            print(f"Loopback test speed: {kbits_per_second:,.2f} Kbps")


def any_int(s):
    return int(s, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser("usbbench")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("-i", "--in", action="store_true", help="In (device-to-host) test")
    mode.add_argument("-o", "--out", action="store_true", help="Out (host-to-device) test")
    mode.add_argument("-l", "--loop", action="store_true", help="Loopback test")
    parser.add_argument("-v", "--vid", type=any_int, default=0x0403, help="Vendor ID")
    parser.add_argument("-p", "--pid", type=any_int, default=0x6010, help="Product ID")
    parser.add_argument("-m", "--manufacturer", default="Tractor", help="Manufacturer match")
    parser.add_argument("-P", "--product", default="js2232", help="Product match")
    parser.add_argument("-f", "--interface", type=any_int, default=0, help="Interface number")
    parser.add_argument("-S", "--pkt-sz", type=any_int, default=64, help="Packet size")
    parser.add_argument("-I", "--in-ep", type=any_int, default=0x82, help="In endpoint")
    parser.add_argument("-O", "--out-ep", type=any_int, default=0x01, help="Out endpoint")
    parser.add_argument(
        "-s", "--sz", type=any_int, default=8 * 1024 * 1024, help="Overall test size"
    )
    parser.add_argument("-a", "--async-xfers", action="store_true", help="Async transfer mode")
    parser.add_argument("-c", "--check", action="store_true", help="Verify data")
    parser.add_argument("-V", "--verbose", action="store_true", help="Be more verbose")
    args = parser.parse_args()
    main(args)
