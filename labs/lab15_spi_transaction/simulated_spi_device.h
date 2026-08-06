#ifndef SIMULATED_SPI_DEVICE_H
#define SIMULATED_SPI_DEVICE_H

#include <stdint.h>

#include "spi_bitbang.h"

struct simulated_spi_device {
    volatile uint8_t sclk;
    volatile uint8_t mosi;
    volatile uint8_t miso;
    volatile uint8_t cs;

    volatile uint32_t clock_edge_count;
    volatile uint32_t delay_count;

    uint8_t selected;

    uint8_t command_received;
    uint8_t response_byte_index;
    uint8_t response_bit_index;
    
    uint8_t response[3];
};

void simulated_spi_device_init(struct simulated_spi_device *device);

const struct spi_bitbang_ops *simulated_spi_device_get_ops(void);

#endif
