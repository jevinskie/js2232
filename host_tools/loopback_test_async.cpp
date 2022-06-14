#include <cassert>

#include <fmt/format.h>
#include <libusb-1.0/libusb.h>

libusb_context *usb_ctx          = nullptr;
libusb_device *dev               = nullptr;
libusb_device_handle *dev_handle = nullptr;

int main(int argc, const char **argv) {
    (void)argc, (void)argv;

    assert(!libusb_init(&usb_ctx));

    assert(!libusb_set_option(usb_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO));

    // discover devices
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(usb_ctx, &list);
    if (cnt < 0) {
        fmt::print("no devices error thingy\n");
        return -1;
    }

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device *cur_dev = list[i];
        libusb_device_descriptor cur_dev_desc;
        assert(!libusb_get_device_descriptor(cur_dev, &cur_dev_desc));
        if (cur_dev_desc.idVendor == 0x0404 && cur_dev_desc.idProduct == 0x6010 &&
            cur_dev_desc.iProduct) {
            libusb_device_handle *cur_dev_handle = nullptr;
            assert(!libusb_open(cur_dev, &cur_dev_handle));
            char prod_buf[32];
            assert(!libusb_get_string_descriptor_ascii(cur_dev_handle, cur_dev_desc.iProduct,
                                                       (uint8_t *)prod_buf, sizeof(prod_buf)));
            if (!strcmp(prod_buf, "js2232")) {
                dev        = cur_dev;
                dev_handle = cur_dev_handle;
                break;
            } else {
                libusb_close(cur_dev_handle);
            }
        }
    }

    libusb_free_device_list(list, 1);

    fmt::print("got 'em\n");

    libusb_exit(usb_ctx);

    return 0;
}
