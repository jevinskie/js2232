#!/usr/bin/env python3

import usb1

PACKET_SIZE = 64

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
        handle.bulkWrite(1, b"\xaa\x55\x00\xff" * (PACKET_SIZE // 4), timeout=1000)
        ibuf = handle.bulkRead(1, PACKET_SIZE, timeout=1000)
        print(ibuf.hex())
