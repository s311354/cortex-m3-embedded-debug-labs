#ifndef HOST_ZEPHYR_DEVICE_H
#define HOST_ZEPHYR_DEVICE_H

#include <stdbool.h>

struct device {
    bool ready;
};

static inline bool device_is_ready(const struct device *dev) {
    return dev != NULL && dev->ready;
}

#endif
