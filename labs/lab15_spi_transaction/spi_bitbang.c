#include <stddef.h>
#include <stdint.h>

#include "spi_bitbang.h"


static uint8_t spi_get_cpol(enum spi_mode mode) {
    return (mode == SPI_MODE_2 || mode == SPI_MODE_3) ? 1U : 0U;
}

static uint8_t spi_get_cpha(enum spi_mode mode) {
    return (mode == SPI_MODE_1 || mode == SPI_MODE_3) ? 1U : 0U;
}

static uint8_t spi_get_tx_bit(uint8_t value, uint8_t bit_index, enum spi_bit_order order) {
    if (order == SPI_MSB_FIRST) {
        return (uint8_t)((value >> (7U - bit_index)) & 1U);
    }

    return (uint8_t) ((value >> bit_index) & 1U);
}

static void spi_store_rx_bit(uint8_t *value, uint8_t bit_index, uint8_t bit, enum spi_bit_order order) {
    uint8_t shift;

    shift = (order == SPI_MSB_FIRST) ?
	    (uint8_t) (7U - bit_index) :
	    bit_index;

    if (bit != 0U) {
        *value |= (uint8_t)(1U << shift);
    }
}

int spi_bitbang_init(struct spi_bitbang_bus *bus) {
    uint8_t idle_level;

    if (bus == 0 ||
	bus->ops == 0 ||
	bus->ops->set_sclk == 0 ||
	bus->ops->set_mosi == 0 ||
	bus->ops->set_cs == 0 ||
	bus->ops->get_miso == 0 ||
	bus->ops->delay_half_cycle == 0) {
        return SPI_ERROR_INVALID_ARGUMENT;
    }

    idle_level = spi_get_cpol(bus->mode);

    bus->ops->set_sclk(bus->context, idle_level);
    bus->ops->set_mosi(bus->context, 0U);

    spi_bitbang_select(bus);

    return SPI_OK;
}

void spi_bitbang_select(struct spi_bitbang_bus *bus) {
    uint8_t active_level;

    active_level = (bus->cs_active_low != 0U) ? 0U : 1U;
    bus->ops->set_cs(bus->context, active_level);
}

void spi_bitbang_deselect(struct spi_bitbang_bus *bus) {
    uint8_t inactive_level;
    uint8_t idle_level;

    inactive_level = (bus->cs_active_low != 0U) ? 1U : 0U;
    idle_level = spi_get_cpol(bus->mode);

    bus->ops->set_sclk(bus->context, idle_level);
    bus->ops->set_cs(bus->context, inactive_level);
}

uint8_t spi_bitbang_transfer_byte(struct spi_bitbang_bus *bus, uint8_t tx_byte) {
    uint8_t rx_byte;
    uint8_t tx_bit;
    uint8_t rx_bit;

    uint8_t cpol;
    uint8_t cpha;
    uint8_t idle_level;
    uint8_t active_level;

    rx_byte = 0U;

    cpol = spi_get_cpol(bus->mode);
    cpha = spi_get_cpha(bus->mode);

    idle_level = cpol;
    active_level = (uint8_t)(cpol ^ 1U);

    for (uint8_t bit_index = 0U; bit_index < 8U; ++bit_index) {
        tx_bit = spi_get_tx_bit(tx_byte, bit_index, bus->bit_order);

	if (cpha == 0U) {
	    bus->ops->set_mosi(bus->context, tx_bit);
	    bus->ops->delay_half_cycle(bus->context);

	    bus->ops->set_sclk(bus->context, active_level);

	    rx_bit = bus->ops->get_miso(bus->context);

	    spi_store_rx_bit(&rx_byte, bit_index, rx_bit, bus->bit_order);

	    bus->ops->delay_half_cycle(bus->context);
	    bus->ops->set_sclk(bus->context, idle_level);
	} else {

	    bus->ops->set_sclk(bus->context, active_level);
	    bus->ops->set_mosi(bus->context, tx_bit);

	    bus->ops->delay_half_cycle(bus->context);
	    bus->ops->set_sclk(bus->context, idle_level);

	    rx_bit = bus->ops->get_miso(bus->context);

	    spi_store_rx_bit(&rx_byte, bit_index, rx_bit, bus->bit_order);

	    bus->ops->delay_half_cycle(bus->context);
	}
    }

    return rx_byte;
}

int spi_bitbang_transfer(struct spi_bitbang_bus *bus,
		         const uint8_t *tx_buffer,
			 uint8_t *rx_buffer,
			 size_t length) {

    uint8_t tx_byte;
    uint8_t rx_byte;

    if (bus == 0 ||
        (tx_buffer == 0 && rx_buffer == 0)) {
        return SPI_ERROR_INVALID_ARGUMENT;
    }

    spi_bitbang_select(bus);

    for (size_t index = 0U; index < length; ++index) {
        tx_byte = (tx_buffer != 0) ? tx_buffer[index] : 0xFFU;

	rx_byte = spi_bitbang_transfer_byte(bus, tx_byte);

	if (rx_buffer != 0) {
	    rx_buffer[index] = rx_byte;
	}
    }

    spi_bitbang_deselect(bus);

    return SPI_OK;
}
