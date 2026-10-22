/* Host-only subset reviewed against Zephyr drivers/spi.h */
#ifndef LAB22_HOST_SPI_H
#define LAB22_HOST_SPI_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifndef CONFIG_SPI_EXTENDED_MODES
typedef uint32_t spi_operation_t;
#else
typedef uint16_t spi_operation_t;
#endif

#define SPI_OP_MODE_CONTROLLER   0U
#define SPI_OP_MODE_PERIPHERAL   (1U << 0)
#define SPI_OP_MODE_GET(op)      ((op) & 1U)

#define SPI_MODE_CPOL            (1U << 1)
#define SPI_MODE_CPHA            (1U << 2)
#define SPI_MODE_LOOP            (1U << 3)

#define SPI_TRANSFER_MSB         0U
#define SPI_TRANSFER_LSB         (1U << 4)

#define SPI_WORD_SET(n)          ((n) << 5)
#define SPI_WORD_SIZE_GET(op)     (((op) >> 5) & 0x3fU)

#define SPI_FULL_DUPLEX          0U
#define SPI_HALF_DUPLEX          (1U << 11)
#define SPI_HOLD_ON_CS           (1U << 12)
#define SPI_LOCK_ON              (1U << 13)
#define SPI_CS_ACTIVE_HIGH       (1U << 14)
#define SPI_FRAME_FORMAT_TI      (1U << 15)
#define SPI_FRAME_FORMAT_MOTOROLA 0U

#define SPI_LINES_SINGLE         0U
#define SPI_LINES_DUAL           (1U << 16)
#define SPI_LINES_QUAD           (2U << 16)
#define SPI_LINES_OCTAL          (3U << 16)


/* only members used by the helper or forwarding tests are modeled */
struct spi_cs_control {
    struct gpio_dt_spec gpio;
    uint32_t delay;
    bool cs_is_gpio;
};

struct spi_config {
    uint32_t frequency;
    spi_operation_t operation;
    uint16_t peripheral;
    struct spi_cs_control cs;
    uint16_t word_delay;
};

struct spi_buf {
    void *buf;
    size_t len;
};

struct spi_buf_set {
    const struct spi_buf *buffers;
    size_t count;
};

static inline bool spi_cs_is_gpio(const struct spi_config *config) {
    return config->cs.cs_is_gpio;
}

int spi_transceive(const struct device *dev, const struct spi_config * config, const struct spi_buf_set *tx, const struct spi_buf_set *rx);



#endif
