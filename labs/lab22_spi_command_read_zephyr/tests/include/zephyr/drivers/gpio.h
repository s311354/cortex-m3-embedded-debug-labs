/* Minimal source-level contract */
#ifndef LAB22_HOST_GPIO_H
#define LAB22_HOST_GPIO_H

#include <stdint.h>

#include <zephyr/device.h>

#define GPIO_ACTIVE_HIGH 0U
#define GPIO_ACTIVE_LOW (1U << 0)

struct gpio_dt_spec {
    const struct device *port;
    uint32_t pin;
    uint16_t dt_flags;
};

static inline bool gpio_is_ready_dt(const struct gpio_dt_spec * spec) {
    return spec != NULL && device_is_ready(spec->port);
}

#endif
