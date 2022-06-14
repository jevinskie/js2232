#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <libusb-1.0/libusb.h>

constexpr int PACKET_SZ = 512;
constexpr int EP_SIZE   = 64;
static_assert(PACKET_SZ % EP_SIZE == 0, "Packet size not multiple of endpoint size");

using buf_t      = std::vector<uint8_t>;
using buf_ptr_t  = std::shared_ptr<buf_t>;
using buf_list_t = std::vector<buf_ptr_t>;

class xfer_t {
public:
    xfer_t(const buf_ptr_t &buf_ptr) : m_buf_ptr(buf_ptr) {
        assert(m_buf_ptr->size() <= EP_SIZE);
    }

    void on_complete(void) {
        fmt::print("{:s}\n", __PRETTY_FUNCTION__);
    }

    const buf_ptr_t &buf() const {
        return m_buf_ptr;
    }

private:
    const buf_ptr_t &m_buf_ptr;
};

using xfer_list_t = std::vector<xfer_t>;

libusb_context *usb_ctx          = nullptr;
libusb_device *dev               = nullptr;
libusb_device_handle *dev_handle = nullptr;

void hexdump(const std::string &desc, const buf_t &buf) {
    fmt::print("{:s}: ", desc);
    fmt::print("{:02x}", fmt::join(buf, " "));
    fmt::print("\n");
}

buf_t gen_aa5500ff_buf(size_t sz) {
    assert(sz % 4 == 0);
    const uint8_t aa5500ff[] = {0xaa, 0x55, 0x00, 0xff};
    buf_t res;
    res.reserve(sz);
    while (sz > 0) {
        std::copy(aa5500ff, aa5500ff + sizeof(aa5500ff), std::back_insert_iterator(res));
        sz -= 4;
    }
    return res;
}

buf_list_t chunk_buf(const buf_t &buf, size_t chunk_sz) {
    buf_list_t res;
    res.reserve(std::ceil(buf.size() / (double)chunk_sz));
    size_t sz     = buf.size();
    const auto *p = buf.data();
    while (sz > 0) {
        size_t cur_chunk_sz = std::min(sz, chunk_sz);
        auto cur_chunk      = std::make_shared<buf_t>();
        cur_chunk->reserve(cur_chunk_sz);
        std::copy(p, p + cur_chunk_sz, std::back_insert_iterator(*cur_chunk));
        res.emplace_back(cur_chunk);
        sz -= cur_chunk_sz;
        p += cur_chunk_sz;
    }
    return res;
}

xfer_list_t get_xfers(const buf_t &buf) {
    xfer_list_t res;
    res.reserve(std::ceil(buf.size() / (double)EP_SIZE));
    for (const buf_ptr_t &buf_ptr : chunk_buf(buf, EP_SIZE)) {
        res.emplace_back(xfer_t{buf_ptr});
    }
    return res;
}

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

    const auto obuf = gen_aa5500ff_buf(EP_SIZE);
    hexdump("obuf", obuf);
    const auto obuf_chunks = chunk_buf(obuf, 16);
    int i                  = 0;
    for (const auto &chunk : obuf_chunks) {
        hexdump(fmt::format("chunk {:d}", i), *chunk);
        ++i;
    }

    libusb_exit(usb_ctx);

    return 0;
}
