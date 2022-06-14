#include <cassert>

#include <fmt/format.h>
#include <libusb-1.0/libusb.h>

consteval bool is_le() noexcept {
    return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
}

template <typename T> T constexpr byteswap(T v) noexcept {
    static_assert(sizeof(v) == 1 || sizeof(v) == 2 || sizeof(v) == 4 || sizeof(v) == 8 ||
                      sizeof(v) == 16,
                  "Unsupported size");
    static_assert(std::is_integral_v<T>, "Unsupported, non-integral type");
    if constexpr (sizeof(v) == 1)
        return v;
    else if constexpr (sizeof(v) == 2)
        return __builtin_bswap16(v);
    else if constexpr (sizeof(v) == 4)
        return __builtin_bswap32(v);
    else if constexpr (sizeof(v) == 8)
        return __builtin_bswap64(v);
    else if constexpr (sizeof(v) == 16)
        return __builtin_bswap128(v);
}

template <typename T> T constexpr to_be(T v) noexcept {
    if (!is_le())
        return v;
    return byteswap(v);
}

template <typename T> T constexpr to_le(T v) noexcept {
    if (!is_le())
        return byteswap(v);
    return v;
}

libusb_context *usb_ctx = nullptr;

int main(int argc, const char **argv) {
    (void)argc, (void)argv;

    assert(!libusb_init(&usb_ctx));

    assert(!libusb_set_option(usb_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO));

    // discover devices
    libusb_device **list;
    libusb_device *found = nullptr;
    ssize_t cnt          = libusb_get_device_list(usb_ctx, &list);
    ssize_t i            = 0;
    int err              = 0;
    if (cnt < 0) {
        fmt::print("no devices error thingy\n");
        return -1;
    }

    for (i = 0; i < cnt; i++) {
        libusb_device *device = list[i];
        libusb_device_descriptor dev_desc;
        assert(!libusb_get_device_descriptor(device, &dev_desc));
        fmt::print("hi {:04x}:{:04x}\n", dev_desc.idVendor, dev_desc.idProduct);
        if (false) {
            found = device;
            break;
        }
    }

    if (found) {
        libusb_device_handle *handle;

        err = libusb_open(found, &handle);
        if (err) {
            fmt::print("error!\n");
            return -1;
        }
        // etc
    }

    libusb_free_device_list(list, 1);

    libusb_exit(usb_ctx);

    return 0;
}
