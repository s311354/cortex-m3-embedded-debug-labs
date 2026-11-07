#ifndef LAB26_DMA_SIM_H_
#define LAB26_DMA_SIM_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>

/* Software-only controller */
const struct device *dma_sim_device(void);
int dma_sim_wait(k_timeout_t timeout);

#endif
