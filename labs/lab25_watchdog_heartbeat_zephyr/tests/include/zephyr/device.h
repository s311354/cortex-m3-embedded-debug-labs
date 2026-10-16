/* Host-only test double */
#ifndef TEST_DEVICE_H
#define TEST_DEVICE_H

#include <stdbool.h>

struct device {
    bool ready;
};

static inline bool device_is_ready(const struct device *dev) {
    return dev !=0 && dev->ready;
}

#endif
