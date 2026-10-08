#include "spi_command.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>

int spi_client_ready(const struct spi_client *client) {
    if (client == NULL || client->bus == NULL || client->config == NULL)
	return -EINVAL;

    const struct spi_config *config = client->config;
    const spi_operation_t operation = config->operation;

    if (config->frequency == 0U)
	return -EINVAL;

    /* supported operations */
    const uint32_t allowed = SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_MODE_LOOP | SPI_CS_ACTIVE_HIGH;

    if (SPI_OP_MODE_GET(operation) != SPI_OP_MODE_CONTROLLER)
	return -ENOTSUP;

    if (SPI_WORD_SIZE_GET(operation) != 8U || ((uint32_t)operation & ~allowed) != 0U)
	return -ENOTSUP;

    if ((operation & SPI_HALF_DUPLEX) != 0U)
        return -ENOTSUP; 

    if ((operation & SPI_FRAME_FORMAT_TI) != 0U)
	return -ENOTSUP;

    if ((operation & SPI_HOLD_ON_CS) != 0U ||
	(operation & SPI_LOCK_ON) != 0U) {
        return -ENOTSUP;
    }

    if (!device_is_ready(client->bus))
	return -ENODEV;

    /* GPIO CS is optional */
    if (spi_cs_is_gpio(config)) {
        if (config->cs.gpio.port == NULL)
            return -EINVAL;

	if (!gpio_is_ready_dt(&config->cs.gpio))
            return -ENODEV;

	const bool gpio_active_low = (config->cs.gpio.dt_flags & GPIO_ACTIVE_LOW) != 0U;

	const bool gpio_active_high = (operation & SPI_CS_ACTIVE_HIGH) != 0U;

	if (gpio_active_low == gpio_active_high)
            return -EINVAL;
    } else if ((operation & SPI_MODE_LOOP) == 0U) {
        return -ENOTSUP;
    }

    return 0;
}


int spi_command_read(const struct spi_client *client, uint8_t command, uint8_t *reply, size_t reply_len) {
    if (reply == NULL || reply_len == 0U) 
	return -EINVAL;

    if (reply_len > SPI_COMMAND_MAX_REPLY)
	return -EMSGSIZE;

    const int rc = spi_client_ready(client);

    if (rc != 0)
	return rc;

    const struct spi_buf tx[] = {
        { .buf = &command, .len = 1U },
	{ .buf = NULL, .len = reply_len }, /* Transmit zero dummy bytes */
    };

    const struct spi_buf rx[] = {
        { .buf = NULL, .len = 1U }, /* Discard command-phase RX */
        { .buf = reply, .len = reply_len},
    };

    const struct spi_buf_set tx_set = {
        .buffers = tx,
	.count = 2U,
    };

    const struct spi_buf_set rx_set = {
        .buffers = rx,
	.count = 2U,
    };

    /* Synchronous call */
    return spi_transceive(client->bus, client->config, &tx_set, &rx_set);
}
