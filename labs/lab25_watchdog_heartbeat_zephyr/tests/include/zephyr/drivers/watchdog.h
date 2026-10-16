/* Host-only API contract double, not a watchdog peripheral model */
#ifndef TEST_WATCHDOG_H
#define TEST_WATCHDOG_H

#include <stdint.h>
#include <zephyr/device.h>

#define WDT_FLAG_RESET_SOC 2U

struct wdt_window {
    uint32_t min;
    uint32_t max;
};

typedef void (*wdt_callback_t) (const struct device *dev, int);

struct wdt_timeout_cfg {
    struct wdt_window window;
    wdt_callback_t callback;
    uint8_t flags;
};

int wdt_install_timeout(const struct device *, const struct wdt_timeout_cfg *);
int wdt_setup(const struct device *, uint8_t);
int wdt_feed(const struct device *, int);

#endif
