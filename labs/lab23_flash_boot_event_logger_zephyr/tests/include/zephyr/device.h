/* Host-only test double */
#ifndef LAB23_HOST_DEVICE_H
#define LAB23_HOST_DEVICE_H

#include <stdbool.h>
#include <stddef.h>

struct device {
    bool ready;
};

static inline bool device_is_ready(const struct device *dev) {
    return dev != NULL && dev->ready;
}

#endif
