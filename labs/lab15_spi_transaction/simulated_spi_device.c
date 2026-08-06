#include "simulated_spi_device.h"

static void simulated_set_sclk(void *context, uint8_t level) {
    struct simulated_spi_device *device;

    device = (struct simulated_spi_device *) context;

    if (device->sclk != level) {
        device->clock_edge_count++;
    }

    device->sclk = level;
}

static void simulated_set_mosi(void *context, uint8_t level) {
    struct simulated_spi_device *device;

    device = (struct simulated_spi_device *) context;
    device->mosi = level;
}

static void simulated_set_cs(void *context, uint8_t level) {
    struct simulated_spi_device *device;

    device = (struct simulated_spi_device *) context;
    device->cs = level;

    if (level == 0U) {
        device->selected = 1U;
	device->command_received = 0U;
	device->response_byte_index = 0U;
	device->response_bit_index = 0U;
    } else {
        device->selected = 0U;
	device->miso = 1U;
    }
}

static uint8_t simulated_get_miso(void *context) {
    struct simulated_spi_device *device;
    uint8_t response_byte;
    uint8_t bit;

    device = (struct simulated_spi_device *) context;

    if (device->selected == 0U) {
        return 1U;
    }

    if (device->command_received == 0U) {
        bit = 1U;

	device->response_bit_index++;

	if (device->response_bit_index == 8U) {
	    device->command_received = 1U;
	    device->response_bit_index = 0U;
	}

	device->miso = bit;

	return bit;
    }

    if (device->response_byte_index >= 3U) {
        device->miso = 1U;
	return 1U;
    }


    response_byte = device->response[device->response_byte_index];

    bit = (uint8_t)((response_byte >> (7U - device->response_bit_index)) & 1U);

    device->response_bit_index++;

    if (device->response_bit_index == 8U) {
        device->response_bit_index = 0U;
	device->response_byte_index++;
    }

    device->miso = bit;

    return bit;
}

static void simulated_delay_half_cycle(void *context) {
    struct simulated_spi_device *device;

    device = (struct simulated_spi_device *) context;

    device->delay_count++;

    __asm volatile ("nop");
}

static const struct spi_bitbang_ops g_simulated_spi_ops = {
    .set_sclk = simulated_set_sclk,
    .set_mosi = simulated_set_mosi,
    .set_cs = simulated_set_cs,
    .get_miso = simulated_get_miso,
    .delay_half_cycle = simulated_delay_half_cycle
};

void simulated_spi_device_init(struct simulated_spi_device *device) {
    if (device == 0)
        return;

    device->sclk = 0U;
    device->mosi = 0U;
    device->miso = 1U;
    device->cs = 1U;

    device->clock_edge_count = 0U;
    device->delay_count = 0U;

    device->selected = 0U;
    device->command_received = 0U;
    device->response_byte_index = 0U;
    device->response_bit_index = 0U;

    device->response[0] = 0xEFU;
    device->response[1] = 0x40U;
    device->response[2] = 0x18U;
}

const struct spi_bitbang_ops *simulated_spi_device_get_ops(void) {
    return &g_simulated_spi_ops;
}
