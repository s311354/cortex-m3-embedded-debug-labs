#ifndef SPI_BITBANG_H
#define SPI_BITBANG_H

#include <stdint.h>
#include <stddef.h>
#include "CM3DS_MPS2.h"

enum spi_mode {
    SPI_MODE_0 = 0, /* CPOL = 0, CPHA = 0 */
    SPI_MODE_1 = 1, /* CPOL = 0, CPHA = 1 */
    SPI_MODE_2 = 2, /* CPOL = 1, CPHA = 0 */
    SPI_MODE_3 = 3  /* CPOL = 1, CPHA = 1 */
};

enum spi_bit_order {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST = 1
};

enum spi_status {
    SPI_OK = 0,
    SPI_ERROR_INVALID_ARGUMENT = -1
};

struct spi_bitbang_ops {
    void (*set_sclk)(void *context, uint8_t level);
    void (*set_mosi)(void *context, uint8_t level);
    void (*set_cs)(void *context, uint8_t level);
    uint8_t (*get_miso)(void *context);
    void (*delay_half_cycle)(void *context);
};

struct spi_bitbang_bus {
    const struct spi_bitbang_ops *ops;
    void *context;

    enum spi_mode mode;
    enum spi_bit_order bit_order;
    uint8_t cs_active_low;
};

int spi_bitbang_init(struct spi_bitbang_bus *bus);

void spi_bitbang_select(struct spi_bitbang_bus *bus);
void spi_bitbang_deselect(struct spi_bitbang_bus *bus);

uint8_t spi_bitbang_transfer_byte(struct spi_bitbang_bus *bus, uint8_t tx_byte);

int spi_bitbang_transfer(struct spi_bitbang_bus *bus,
		         const uint8_t *tx_buffer,
			 uint8_t *rx_buffer,
			 size_t length);
#endif
