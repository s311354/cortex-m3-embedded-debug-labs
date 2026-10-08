#ifndef LAB22_SPI_COMMAND_H
#define LAB22_SPI_COMMAND_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

#define SPI_COMMAND_MAX_REPLY 16U

/*
 * Application-owned, immutable config must remain valid across transfers
 */
struct spi_client {
    const struct device *bus;
    const struct spi_config *config;
};

int spi_client_ready(const struct spi_client *client);

int spi_command_read(const struct spi_client *client, uint8_t command, uint8_t *reply, size_t reply_len);

#endif
