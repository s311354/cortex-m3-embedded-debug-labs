#ifndef LAB26_HOST_DEVICE_H_
#define LAB26_HOST_DEVICE_H_

#include <stdbool.h>

struct device {
    bool ready;
};

static inline bool device_is_ready(const struct device *dev) {
    return dev != 0 && dev->ready;
}

#endif
