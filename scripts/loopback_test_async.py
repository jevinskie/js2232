#!/usr/bin/env python3

import random
import time
from collections import deque
from functools import partial

import usb1
from more_itertools import chunked

PACKET_SIZE = 128
EP_SIZE = 64
assert PACKET_SIZE % EP_SIZE == 0

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
        try:
            while True:
                ibuf = handle.bulkRead(1, EP_SIZE, timeout=30)
        except:
            pass
        nbytes = 0
        tstart = time.time()
        queue = deque()

        def xfer_out_cb(xfer_in, xfer_out):
            # print(f"out status: {xfer.getStatus()}")
            assert xfer_out.getStatus() == usb1.TRANSFER_COMPLETED
            # xfer_out.submit()
            xfer_in.submit()

        def xfer_in_cb(xfer):
            assert xfer.getStatus() == usb1.TRANSFER_COMPLETED
            obuf = queue.pop()
            obuf_inv = bytes([b ^ 0xFF for b in obuf])
            ibuf = xfer.getBuffer()[: xfer.getActualLength()]
            # print(f"ibuf: {ibuf.hex(' ')}")
            assert obuf_inv == ibuf
            # xfer.submit()

        try:
            while nbytes < 25600:
                # obuf = random.randbytes(PACKET_SIZE)
                obuf = b"\xaa\x55\x00\xff" * (PACKET_SIZE // 4)
                obuf_chunks = chunked(obuf, EP_SIZE)
                xfer_list = []
                for chunk in obuf_chunks:
                    xfer_out = handle.getTransfer()
                    xfer_in = handle.getTransfer()
                    xfer_out.setBulk(
                        1,
                        len(chunk),
                        callback=partial(xfer_out_cb, xfer_in),
                        timeout=1000,
                    )
                    xfer_out.setBuffer(chunk)
                    xfer_list.append(xfer_out)
                    xfer_in.setBulk(0x81, len(chunk), callback=xfer_in_cb, timeout=1000)
                    xfer_list.append(xfer_in)
                    xfer_out.submit()
                    queue.appendleft(chunk)
                while any(x.isSubmitted() for x in xfer_list):
                    try:
                        ctx.handleEvents()
                    except usb1.USBErrorInterrupted:
                        pass
                time.sleep(0.01)
                nbytes += len(obuf)
                if nbytes % (1024 * EP_SIZE) == 0:
                    print(".", end="", flush=True)
        except KeyboardInterrupt:
            pass
        tend = time.time()
        kbits_per_second = (nbytes * 8 / 1000) / (tend - tstart)
        print()
        print(f"Loopback test speed: {kbits_per_second:,.2f} Kbps")
