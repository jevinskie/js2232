#!/usr/bin/env python3

import argparse
import random
import time
from enum import IntEnum

import usb1

REQ_SET_TEST_MODE = 0x42
REQ_SET_PACKET_SZ = 0x43


class test_mode_t(IntEnum):
    INVALID_MODE = 0
    LOOPBACK_BULK = 1
    OUT_BULK = 2
    IN_BULK = 3


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
        cfg0 = next(dev.iterConfigurations())
        interface = cfg0[args.interface]
        setting = interface[0]
        if args.in_test:
            in_ep = setting[0]
            assert in_ep.getAddress() & 0x80
        elif args.out_test:
            out_ep = setting[0]
            assert not out_ep.getAddress() & 0x80
        else:
            in_ep, out_ep = [None] * 2
            ep_iter = iter(setting)
            while not all((in_ep, out_ep)):
                ep = next(ep_iter)
                if not ep.getAddress() & 0x80:
                    out_ep = ep
                    out_ep_addr = ep.getAddress()
                else:
                    in_ep = ep
                    in_ep_addr = ep.getAddress() ^ 0x80
        with handle.claimInterface(args.interface):
            if out_ep:
                handle.clearHalt(out_ep.getAddress())
            if in_ep:
                handle.clearHalt(in_ep.getAddress())
            test_mode = test_mode_t.INVALID_MODE
            if args.loop_test:
                test_mode = test_mode_t.LOOPBACK_BULK
            elif args.out_test:
                test_mode = test_mode_t.OUT_BULK
            elif args.in_test:
                test_mode = test_mode_t.IN_BULK
            else:
                raise RuntimeError("must select a test mode")
            handle.controlWrite(
                usb1.RECIPIENT_DEVICE | usb1.REQUEST_TYPE_VENDOR,
                REQ_SET_PACKET_SZ,
                args.pkt_sz,
                0,
                b"",
                timeout=100,
            )
            handle.controlWrite(
                usb1.RECIPIENT_DEVICE | usb1.REQUEST_TYPE_VENDOR,
                REQ_SET_TEST_MODE,
                test_mode,
                0,
                b"",
                timeout=100,
            )
            nbytes = 0
            tstart = time.time()
            try:
                while True:
                    obuf = bytearray(random.randbytes(args.pkt_sz))
                    # print(f"obuf: {obuf.hex(' ')}")
                    handle.bulkWrite(out_ep_addr, obuf, timeout=1000)
                    ibuf = handle.bulkRead(in_ep_addr, args.pkt_sz, timeout=1000)
                    ibuf_inv = bytes([b ^ 0xFF for b in ibuf])
                    # print(f"Ibuf: {ibuf_inv.hex(' ')}")
                    assert ibuf_inv == obuf
                    nbytes += len(obuf) * 2
                    if nbytes % (16 * 1024) == 0:
                        print(".", end="", flush=True)
                    time.sleep(0.001)
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
    mode.add_argument(
        "-i", "--in", dest="in_test", action="store_true", help="In (device-to-host) test"
    )
    mode.add_argument(
        "-o", "--out", dest="out_test", action="store_true", help="Out (host-to-device) test"
    )
    mode.add_argument("-l", "--loop", dest="loop_test", action="store_true", help="Loopback test")
    parser.add_argument("-v", "--vid", type=any_int, default=0x0403, help="Vendor ID")
    parser.add_argument("-p", "--pid", type=any_int, default=0x6010, help="Product ID")
    parser.add_argument("-m", "--manufacturer", default="Chewing", help="Manufacturer match")
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
