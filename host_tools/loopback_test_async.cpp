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

constexpr int PACKET_SZ = 128;
constexpr int EP_SIZE   = 64;
static_assert(PACKET_SZ % EP_SIZE == 0, "Packet size not multiple of endpoint size");

using buf_t      = std::vector<uint8_t>;
using buf_ptr_t  = std::shared_ptr<buf_t>;
using buf_list_t = std::vector<buf_ptr_t>;

class xfer_t {
public:
    xfer_t(libusb_device_handle *dev_handle, int ep, buf_ptr_t buf_ptr, int timeout = 1000,
           buf_ptr_t gold = nullptr)
        : m_buf_ptr(buf_ptr), m_usb_xfer(libusb_alloc_transfer(0)), m_in_cb(false), m_ep(ep),
          m_dev_handle(dev_handle), m_timeout(timeout), m_gold(gold) {
        assert(m_buf_ptr->size() <= EP_SIZE);
        assert(m_usb_xfer);
        assert(m_dev_handle);
        libusb_fill_bulk_transfer(m_usb_xfer, m_dev_handle, m_ep, buf_ptr->data(), buf_ptr->size(),
                                  completed_c_cb, this, m_timeout);
    }

    ~xfer_t() {
        libusb_free_transfer(m_usb_xfer);
    }

    bool is_in() const {
        return m_ep & 0x80;
    }

    int status() const {
        assert(m_in_cb);
        return m_usb_xfer->status;
    }

    bool completed() const {
        return status() == LIBUSB_TRANSFER_COMPLETED;
    }

    void submit() {
        libusb_submit_transfer(m_usb_xfer);
    }

    void on_complete() {
        m_in_cb = true;
        fmt::print("{:s} status: {:d}\n", __PRETTY_FUNCTION__, status());
        m_in_cb = false;
    }

    buf_ptr_t &buf() {
        return m_buf_ptr;
    }

    static void completed_c_cb(struct libusb_transfer *transfer) {
        auto *thiz = (xfer_t *)transfer->user_data;
        thiz->completed();
    }

private:
    buf_ptr_t m_buf_ptr;
    libusb_transfer *m_usb_xfer;
    bool m_in_cb;
    const int m_ep;
    libusb_device_handle *m_dev_handle;
    int m_timeout;
    buf_ptr_t m_gold;
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

xfer_list_t get_xfers(libusb_device_handle *dev_handle, int ep, const buf_t &buf) {
    xfer_list_t res;
    res.reserve(std::ceil(buf.size() / (double)EP_SIZE) * 2);
    for (auto &buf_ptr : chunk_buf(buf, EP_SIZE)) {
        res.emplace_back(xfer_t{dev_handle, ep, buf_ptr});
        auto in_buf_ptr = std::make_shared<buf_t>(buf_ptr->size());
        res.emplace_back(xfer_t{dev_handle, ep | 0x80, in_buf_ptr, 1000, buf_ptr});
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

    fmt::print("dev count: {:d}\n", cnt);

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device *cur_dev = list[i];
        libusb_device_descriptor cur_dev_desc;
        assert(!libusb_get_device_descriptor(cur_dev, &cur_dev_desc));
        if (cur_dev_desc.idVendor == 0x0403 && cur_dev_desc.idProduct == 0x6010) {
            fmt::print("iProduct: {:d}\n", cur_dev_desc.iProduct);
            libusb_device_handle *cur_dev_handle = nullptr;
            assert(!libusb_open(cur_dev, &cur_dev_handle));
            char prod_buf[32] = {};
            assert(libusb_get_string_descriptor_ascii(cur_dev_handle, cur_dev_desc.iProduct,
                                                      (uint8_t *)prod_buf, sizeof(prod_buf)) >= 0);
            prod_buf[sizeof(prod_buf) - 1] = '\0';
            fmt::print("cur_dev_handle: {:p} prod_buf: {:s}\n", fmt::ptr(cur_dev_handle), prod_buf);
            if (!strcmp(prod_buf, "js2232")) {
                assert(!libusb_set_auto_detach_kernel_driver(cur_dev_handle, true));
                dev        = cur_dev;
                dev_handle = cur_dev_handle;
                break;
            } else {
                libusb_close(cur_dev_handle);
                cur_dev_handle = nullptr;
            }
        }
    }

    libusb_free_device_list(list, 1);

    if (!dev_handle) {
        fmt::print("didn't get 'em\n");
        return -1;
    }
    fmt::print("got 'em\n");

    if (false) {
        const auto obuf = gen_aa5500ff_buf(EP_SIZE);
        hexdump("obuf", obuf);
        const auto obuf_chunks = chunk_buf(obuf, 16);
        int i                  = 0;
        for (const auto &chunk : obuf_chunks) {
            hexdump(fmt::format("chunk {:d}", i), *chunk);
            ++i;
        }
    }

    int i = 0;
    while (true) {
        fmt::print("xfer {:d}\n", i);
        const auto obuf = gen_aa5500ff_buf(PACKET_SZ);
        auto xfer_list  = get_xfers(dev_handle, 1, obuf);
        for (auto &xfer : xfer_list) {
            xfer.submit();
        }
        if (i > 1) {
            break;
        }
        ++i;
    }

    libusb_exit(usb_ctx);

    return 0;
}
