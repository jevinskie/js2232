
#include <zephyr/init.h>

#include <usb_descriptor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>

#define LOG_LEVEL CONFIG_USB_DEVICE_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(js2232_usb);

#define IF0_IN_EP_ADDR 0x81
#define IF0_OUT_EP_ADDR 0x01

#define IF0_IN_EP_IDX 0
#define IF0_OUT_EP_IDX 1

#define IF1_IN_EP_ADDR 0x82
#define IF1_OUT_EP_ADDR 0x02

#define IF1_IN_EP_IDX 0
#define IF1_OUT_EP_IDX 1

static uint8_t loopback_buf[1024];
BUILD_ASSERT(sizeof(loopback_buf) == CONFIG_USB_REQUEST_BUFFER_SIZE);

struct usb_loopback_config {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_in_ep;
    struct usb_ep_descriptor if0_out_ep;
    struct usb_if_descriptor if1;
    struct usb_ep_descriptor if1_in_ep;
    struct usb_ep_descriptor if1_out_ep;
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

    /* Data Endpoint OUT 2 */
    .if0_out_ep =
        {
            .bLength          = sizeof(struct usb_ep_descriptor),
            .bDescriptorType  = USB_DESC_ENDPOINT,
            .bEndpointAddress = IF0_OUT_EP_ADDR,
            .bmAttributes     = USB_DC_EP_BULK,
            .wMaxPacketSize   = sys_cpu_to_le16(CONFIG_JS2232_BULK_EP_MPS),
            .bInterval        = 0x00,
        },

    /* Interface descriptor 1 */
    .if1 =
        {
            .bLength            = sizeof(struct usb_if_descriptor),
            .bDescriptorType    = USB_DESC_INTERFACE,
            .bInterfaceNumber   = 1,
            .bAlternateSetting  = 0,
            .bNumEndpoints      = 2,
            .bInterfaceClass    = USB_BCC_VENDOR,
            .bInterfaceSubClass = 0xFF,
            .bInterfaceProtocol = 0xFF,
            .iInterface         = 0,
        },

    /* Data Endpoint IN 3 */
    .if1_in_ep =
        {
            .bLength          = sizeof(struct usb_ep_descriptor),
            .bDescriptorType  = USB_DESC_ENDPOINT,
            .bEndpointAddress = IF1_IN_EP_ADDR,
            .bmAttributes     = USB_DC_EP_BULK,
            .wMaxPacketSize   = sys_cpu_to_le16(CONFIG_JS2232_BULK_EP_MPS),
            .bInterval        = 0x00,
        },

    /* Data Endpoint OUT 4 */
    .if1_out_ep =
        {
            .bLength          = sizeof(struct usb_ep_descriptor),
            .bDescriptorType  = USB_DESC_ENDPOINT,
            .bEndpointAddress = IF1_OUT_EP_ADDR,
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
            .bNumInterfaces      = 2,
            .bConfigurationValue = 1,
            .iConfiguration      = 0,
            .bmAttributes        = USB_SCD_RESERVED,
            .bMaxPower           = 500 / 2,
        },
};

static void loopback_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status) {
    uint32_t bytes_to_read;

    usb_read(ep, NULL, 0, &bytes_to_read);
    LOG_DBG("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
    usb_read(ep, loopback_buf, bytes_to_read, NULL);
}

static void loopback_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status) {
    if (usb_write(ep, loopback_buf, CONFIG_JS2232_BULK_EP_MPS, NULL)) {
        LOG_DBG("ep 0x%x", ep);
    }
}

/* usb.rst endpoint configuration start */
static struct usb_ep_cfg_data ep_cfg[] = {
    {
        .ep_cb   = loopback_out_cb,
        .ep_addr = IF0_OUT_EP_ADDR,
    },
    {
        .ep_cb   = loopback_in_cb,
        .ep_addr = IF0_IN_EP_ADDR,
    },
    {
        .ep_cb   = loopback_out_cb,
        .ep_addr = IF1_OUT_EP_ADDR,
    },
    {
        .ep_cb   = loopback_in_cb,
        .ep_addr = IF1_IN_EP_ADDR,
    },
};
/* usb.rst endpoint configuration end */

static void loopback_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
                               const uint8_t *param) {
    ARG_UNUSED(cfg);

    switch (status) {
    case USB_DC_INTERFACE:
        loopback_in_cb(ep_cfg[IF0_IN_EP_IDX].ep_addr, USB_DC_EP_SETUP);
        LOG_DBG("USB interface configured");
        break;
    case USB_DC_SET_HALT:
        LOG_DBG("Set Feature ENDPOINT_HALT");
        break;
    case USB_DC_CLEAR_HALT:
        LOG_DBG("Clear Feature ENDPOINT_HALT");
        if (*param == ep_cfg[IF0_IN_EP_IDX].ep_addr) {
            loopback_in_cb(ep_cfg[IF0_IN_EP_IDX].ep_addr, USB_DC_EP_SETUP);
        }
        break;
    default:
        break;
    }
}

/* usb.rst vendor handler start */
static int loopback_vendor_handler(struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {
    LOG_DBG("Class request: bRequest 0x%x bmRequestType 0x%x len %d", setup->bRequest,
            setup->bmRequestType, *len);

    if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_DEVICE) {
        return -ENOTSUP;
    }

    if (usb_reqtype_is_to_device(setup) && setup->bRequest == 0x5b) {
        LOG_DBG("Host-to-Device, data %p", *data);
        /*
         * Copy request data in loopback_buf buffer and reuse
         * it later in control device-to-host transfer.
         */
        memcpy(loopback_buf, *data, MIN(sizeof(loopback_buf), setup->wLength));
        return 0;
    }

    if ((usb_reqtype_is_to_host(setup)) && (setup->bRequest == 0x5c)) {
        LOG_DBG("Device-to-Host, wLength %d, data %p", setup->wLength, *data);
        *data = loopback_buf;
        *len  = MIN(sizeof(loopback_buf), setup->wLength);
        return 0;
    }

    return -ENOTSUP;
}
/* usb.rst vendor handler end */

static void loopback_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
    ARG_UNUSED(head);

    loopback_cfg.if0.bInterfaceNumber = bInterfaceNumber;
}

USBD_DEFINE_CFG_DATA(loopback_config) = {
    .usb_device_description = NULL,
    .interface_descriptor   = &loopback_cfg.if0,
    .interface_config       = loopback_interface_config,
    .cb_usb_status          = loopback_status_cb,
    .interface =
        {
            .class_handler  = NULL,
            .vendor_handler = loopback_vendor_handler,
            .custom_handler = NULL,
        },
    .num_endpoints = ARRAY_SIZE(ep_cfg),
    .endpoint      = ep_cfg,
};
