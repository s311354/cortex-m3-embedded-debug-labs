/* Host-only test double */
#ifndef LAB24_HOST_DEVICE_H
#define LAB24_HOST_DEVICE_H

#include <stdbool.h>
#include <stddef.h>

struct device {
    bool ready;
    size_t size;
};

static inline bool device_is_ready(const struct device *dev) {
    return dev != NULL && dev->ready;
}


#endif
