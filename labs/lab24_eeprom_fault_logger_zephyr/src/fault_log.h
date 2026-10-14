#ifndef LAB24_FAULT_LOG_H
#define LAB24_FAULT_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>

#define FAULT_LOG_SLOT_COUNT     8U
#define FAULT_LOG_SLOT_SIZE      32U
#define FAULT_LOG_BYTES          (FAULT_LOG_SLOT_COUNT * FAULT_LOG_SLOT_SIZE)
#define FAULT_LOG_BODY_SIZE      16U
#define FAULT_LOG_COMMIT_OFFSET  24U
#define FAULT_LOG_COMMIT_SIZE    8U
#define FAULT_LOG_NO_SLOT        UINT8_MAX

struct fault_log {
    const struct device *dev;
    uint32_t sequence;
    uint8_t newest_slot;
    bool initialized;
};

int fault_log_init(struct fault_log *log, const struct device *dev);

int fault_log_append(struct fault_log *log, uint32_t fault_code);

int fault_log_latest(struct fault_log *log, uint32_t *fault_code);


#endif
