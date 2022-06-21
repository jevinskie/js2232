#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(main);

#define SLEEP_TIME_MS 1000

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define USB_NODE DT_NODELABEL(usb)
PINCTRL_DT_DEFINE(USB_NODE);
// #define USB_NODE DT_ALIAS(usb)
// static const struct pinctrl_dev_config *usb_pcfg = PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(usb));

static void usb_bus_reset() {
    PINCTRL_DT_DEV_CONFIG_GET(USB_NODE);
}

void main(void) {
    int ret;

    printk("Hello World! %s\n", CONFIG_BOARD);

    usb_bus_reset();
    ret = usb_enable(nullptr);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB");
        return;
    }

    LOG_INF("entered main.");

    if (!device_is_ready(led.port)) {
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            return;
        }
        k_msleep(SLEEP_TIME_MS);
    }
}
