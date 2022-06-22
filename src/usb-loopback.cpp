#undef NDEBUG
#define __ASSERT_ON 1
#include <assert.h>

#include <zephyr/init.h>

#include <usb_descriptor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(js2232_usb);

constexpr int IF0_IN_EP_ADDR  = 0x81;
constexpr int IF0_OUT_EP_ADDR = 0x01;

constexpr int IF0_IN_EP_IDX  = 0;
constexpr int IF0_OUT_EP_IDX = 1;

constexpr uint8_t REQ_SET_TEST_MODE = 0x42;

enum test_mode_t : uint16_t {
    INVALID_MODE,
    LOOPBACK_BULK,
    OUT_BULK,
    IN_BULK,
};

test_mode_t test_mode = INVALID_MODE;

__attribute__((aligned(4))) static uint8_t loopback_buf[CONFIG_JS2232_BULK_EP_MPS];
BUILD_ASSERT(sizeof(loopback_buf) == CONFIG_JS2232_BULK_EP_MPS);

struct usb_loopback_config {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_in_ep;
    struct usb_ep_descriptor if0_out_ep;
} __packed;

USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_loopback_config loopback_cfg = {
    /* Interface descriptor 0 */
    .if0 =
        {
            .bLength            = sizeof(struct usb_if_descriptor),
            .bDescriptorType    = USB_DESC_INTERFACE,
            .bInterfaceNumber   = 0,
            .bAlternateSetting  = 0,
            .bNumEndpoints      = 2,
            .bInterfaceClass    = USB_BCC_VENDOR,
            .bInterfaceSubClass = 0xFF,
            .bInterfaceProtocol = 0xFF,
            .iInterface         = 0,
        },

    /* Data Endpoint IN 1 */
    .if0_in_ep =
        {
            .bLength          = sizeof(struct usb_ep_descriptor),
            .bDescriptorType  = USB_DESC_ENDPOINT,
            .bEndpointAddress = IF0_IN_EP_ADDR,
            .bmAttributes     = USB_DC_EP_BULK,
            .wMaxPacketSize   = sys_cpu_to_le16(CONFIG_JS2232_BULK_EP_MPS),
            .bInterval        = 0x00,
        },

    /* Data Endpoint OUT 1 */
    .if0_out_ep =
        {
            .bLength          = sizeof(struct usb_ep_descriptor),
            .bDescriptorType  = USB_DESC_ENDPOINT,
            .bEndpointAddress = IF0_OUT_EP_ADDR,
            .bmAttributes     = USB_DC_EP_BULK,
            .wMaxPacketSize   = sys_cpu_to_le16(CONFIG_JS2232_BULK_EP_MPS),
            .bInterval        = 0x00,
        },
};

USBD_DEVICE_DESCR_DEFINE(primary)
struct usb_common_descriptor common_desc = {
    /* Device descriptor */
    .device_descriptor =
        {
            .bLength            = sizeof(struct usb_device_descriptor),
            .bDescriptorType    = USB_DESC_DEVICE,
            .bcdUSB             = sys_cpu_to_le16(USB_SRN_2_0),
            .bDeviceClass       = 0,
            .bDeviceSubClass    = 0,
            .bDeviceProtocol    = 0,
            .bMaxPacketSize0    = USB_MAX_CTRL_MPS,
            .idVendor           = sys_cpu_to_le16((uint16_t)CONFIG_USB_DEVICE_VID),
            .idProduct          = sys_cpu_to_le16((uint16_t)CONFIG_USB_DEVICE_PID),
            .bcdDevice          = sys_cpu_to_le16((uint16_t)CONFIG_JS2232_BCDDEVICE),
            .iManufacturer      = 1,
            .iProduct           = 2,
            .iSerialNumber      = 3,
            .bNumConfigurations = 1,
        },
    /* Configuration descriptor */
    .cfg_descr =
        {
            .bLength             = sizeof(struct usb_cfg_descriptor),
            .bDescriptorType     = USB_DESC_CONFIGURATION,
            .wTotalLength        = 0,
            .bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .iConfiguration      = 0,
            .bmAttributes        = USB_SCD_RESERVED,
            .bMaxPower           = 500 / 2,
        },
};

#pragma GCC push_options
#pragma GCC optimize(2)
static void invert_buf(uint8_t *buf, uint16_t len) {
    uint8_t *p8 = buf;
    while (len && (uintptr_t)p8 & 0b11) {
        *p8 = *p8 ^ 0xff;
        ++p8;
        len -= sizeof(*p8);
    }
    uint32_t *p32 = (uint32_t *)p8;
    while (len >= 4) {
        *p32 = *p32 ^ 0xffffffff;
        ++p32;
        len -= sizeof(*p32);
    }
    p8 = (uint8_t *)p32;
    while (len) {
        *p8 = *p8 ^ 0xff;
        ++p8;
        len -= sizeof(*p8);
    }
}

void invert_buf_align32(uint8_t *buf, uint32_t len) {
    __builtin_assume_aligned(buf, 4);
    uint32_t *p32 = (uint32_t *)buf;
    while (len >= 4) {
        *p32 = *p32 ^ 0xffffffff;
        ++p32;
        len -= sizeof(*p32);
    }
}
#pragma GCC pop_options

static void xfer_rx_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status) {
    int res;
    uint32_t ret_bytes;
    switch (test_mode) {
    case LOOPBACK_BULK:
        res = usb_read(IF0_OUT_EP_ADDR, loopback_buf, 64, &ret_bytes);
        // assert(!res && ret_bytes == 64);
        if (res || ret_bytes != 64) {
            LOG_INF("read fail res: %d ret_bytes: %u", res, ret_bytes);
        }
        invert_buf_align32(loopback_buf, 64);
        res = usb_write(IF0_IN_EP_ADDR, loopback_buf, 64, &ret_bytes);
        // assert(res == 0 && ret_bytes == 64);
        if (res || ret_bytes != 64) {
            LOG_INF("write fail res: %d ret_bytes: %u", res, ret_bytes);
        }
        break;
    case OUT_BULK:
        res = usb_read(IF0_OUT_EP_ADDR, loopback_buf, 64, &ret_bytes);
        if (res || ret_bytes != 64) {
            LOG_INF("read fail res: %d ret_bytes: %u", res, ret_bytes);
        }
        break;
    case IN_BULK:
        assert(!"Invalid test mode");
        break;
    default:
        assert(!"Invalid test mode");
        break;
    }
}

static void xfer_tx_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status) {
    int res            = -1;
    uint32_t ret_bytes = 0;
    // LOG_INF("tx_cb: ep: 0x%02x status: %d", ep, status);
    switch (test_mode) {
    case LOOPBACK_BULK:
        break;
    case OUT_BULK:
        assert(!"Invalid test mode");
        break;
    case IN_BULK:
        res = usb_write(IF0_IN_EP_ADDR, loopback_buf, 64, &ret_bytes);
        if (res || ret_bytes != 64) {
            LOG_INF("write fail res: %d ret_bytes: %u", res, ret_bytes);
        }
        break;
    default:
        assert(!"Invalid test mode");
        break;
    }
}

static struct usb_ep_cfg_data ep_cfg[] = {
    {
        .ep_cb   = xfer_rx_cb,
        .ep_addr = IF0_OUT_EP_ADDR,
    },
    {
        .ep_cb   = xfer_tx_cb,
        .ep_addr = IF0_IN_EP_ADDR,
    },
};

static void loopback_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
                               const uint8_t *param) {
    ARG_UNUSED(cfg);

    LOG_INF("%s: status: %d", __FUNCTION__, status);

    switch (status) {
    case USB_DC_INTERFACE:
        LOG_INF("USB interface configured");
        break;
    case USB_DC_SET_HALT:
        LOG_INF("Set Feature ENDPOINT_HALT");
        break;
    case USB_DC_CLEAR_HALT:
        LOG_INF("Clear Feature ENDPOINT_HALT");
        break;
    case USB_DC_CONFIGURED:
        LOG_INF("Configured");
        break;
    default:
        break;
    }
}

static int loopback_vendor_handler(struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {
    int res            = -1;
    uint32_t ret_bytes = 0;
    LOG_INF("Class request: bRequest 0x%x bmRequestType 0x%x len %d", setup->bRequest,
            setup->bmRequestType, *len);

    if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_DEVICE ||
        setup->RequestType.type != USB_REQTYPE_TYPE_VENDOR) {
        return -ENOTSUP;
    }

    if (setup->bRequest == REQ_SET_TEST_MODE) {
        test_mode = (test_mode_t)setup->wValue;
        memset(loopback_buf, 0, sizeof(loopback_buf));
        switch (test_mode) {
        case LOOPBACK_BULK:
            LOG_INF("Test mode: loopback bulk");
            break;
        case OUT_BULK:
            LOG_INF("Test mode: out bulk");
            break;
        case IN_BULK:
            LOG_INF("Test mode: in bulk");
            res = usb_write(IF0_IN_EP_ADDR, loopback_buf, 64, &ret_bytes);
            if (res || ret_bytes != 64) {
                LOG_INF("write fail res: %d ret_bytes: %u", res, ret_bytes);
            }
            break;
        default:
            assert(!"Invalid test mode");
            break;
        }
        return 0;
    }

    return -ENOTSUP;
}

static void loopback_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
    ARG_UNUSED(head);

    LOG_INF("%s: bInterfaceNumber: %d", __PRETTY_FUNCTION__, bInterfaceNumber);
    loopback_cfg.if0.bInterfaceNumber = bInterfaceNumber;
}

USBD_DEFINE_CFG_DATA(loopback_config) = {
    .usb_device_description = nullptr,
    .interface_descriptor   = &loopback_cfg.if0,
    .interface_config       = loopback_interface_config,
    .cb_usb_status          = loopback_status_cb,
    .interface =
        {
            .class_handler  = nullptr,
            .vendor_handler = loopback_vendor_handler,
            .custom_handler = nullptr,
        },
    .num_endpoints = ARRAY_SIZE(ep_cfg),
    .endpoint      = ep_cfg,
};
