#!/usr/bin/env python3

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
        handle.bulkWrite(1, b"\xaa\x55\x00\xff", timeout=1000)
        ibuf = handle.bulkRead(1, 4, timeout=1000)
        print(ibuf.hex())
