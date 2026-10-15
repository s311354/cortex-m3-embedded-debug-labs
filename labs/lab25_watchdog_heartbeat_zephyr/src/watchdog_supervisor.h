#ifndef LAB25_WATCHDOG_SUPERVISOR_H
#define LAB25_WATCHDOG_SUPERVISOR_H

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#define WATCHDOG_TASK_COUNT       2U
#define WATCHDOG_REQUIRED_MASK    0x03U
#define WATCHDOG_TIMEOUT_MS       2000U
#define WATCHDOG_SERVICE_WAITING  1

enum watchdog_state {
    WATCHDOG_UNINITIALIZED = 0,
    WATCHDOG_RUNNING,
    WATCHDOG_FAULTED,
};

struct watchdog_ctx {
    const struct device *dev;

    int channel_id;
    uint8_t reported_mask;
    enum watchdog_state state;
    struct k_mutex lock;
};

int watchdog_init(struct watchdog_ctx *ctx, const struct device *dev);

int watchdog_report(struct watchdog_ctx *ctx, unsigned int task_id);

int watchdog_service(struct watchdog_ctx *ctx);


#endif
