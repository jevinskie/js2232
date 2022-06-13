#!/usr/bin/env python3

import random
import time

import usb1

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
        print(f"dev: {dev}")
        handle = dev.open()
        break
    print(f"handle: {handle}")
    assert handle
    handle.setAutoDetachKernelDriver(True)
    with handle.claimInterface(0):
        try:
            while True:
                ibuf = handle.bulkRead(1, 64, timeout=30)
        except:
            pass
        print("flushed")
        i = 0
        while True:
            obuf = random.randbytes(64)
            # print(f"obuf:     {obuf.hex()}")
            handle.bulkWrite(1, obuf, timeout=1000)
            ibuf = handle.bulkRead(1, 64, timeout=1000)
            # print(f"ibuf:     {ibuf.hex()}")
            # assert ibuf == obuf
            ibuf_inv = bytes([b ^ 0xFF for b in ibuf])
            # print(f"ibuf_inv: {ibuf_inv.hex()}")
            assert ibuf_inv == obuf
            if i % 1024 == 0:
                print(".", end="", flush=True)
            # if i == 12:
            #     break
            i += 1
