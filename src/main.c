#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(main);

#define SLEEP_TIME_MS 1000

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void main(void) {
    int ret;

    printk("Hello World! %s\n", CONFIG_BOARD);

    ret = usb_enable(NULL);
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
