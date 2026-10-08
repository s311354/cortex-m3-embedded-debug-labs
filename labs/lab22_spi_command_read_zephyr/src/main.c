/*
 * Real Zephyr SPI -> PL022 MMIO INTERNAL LOOPBACK test, not a sensor model
 */

#include "spi_command.h"

#include <errno.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define TARGET_NODE DT_NODELABEL(lab22_target)
#define BUS_NODE    DT_BUS(TARGET_NODE)

#define NOT_RUN     (-999)
#define COMMAND     0x9fU
#define SHORT_REPLY 3U
#define FILL        0xccU

BUILD_ASSERT(DT_NODE_HAS_STATUS(TARGET_NODE, okay), "lab22_target must be enabled");
BUILD_ASSERT(DT_REG_ADDR(BUS_NODE) == 0x40026000U, "This is lab's MPS2 Shield0 mapping is 0x40026000");
BUILD_ASSERT(SPI_COMMAND_MAX_REPLY >= SHORT_REPLY, "Reply limit too small");

/* Current Zephyr API */
static const struct spi_config g_spi_config = SPI_CONFIG_DT(TARGET_NODE,
	            SPI_OP_MODE_CONTROLLER | SPI_TRANSFER_MSB | SPI_WORD_SET(8) | SPI_MODE_LOOP);

static const struct spi_client g_sensor = {
    .bus = DEVICE_DT_GET(BUS_NODE),
    .config = &g_spi_config,
};

/* Application owns the receive buffer, not the helper */
static uint8_t g_storage[SPI_COMMAND_MAX_REPLY + 2U];

enum lab22_stage {
    LAB22_STAGE_RESET = 0,
    LAB22_STAGE_READY,
    LAB22_STAGE_SHORT_REPLY,
    LAB22_STAGE_MAX_REPLY,
    LAB22_STAGE_INVALID_ARGS,
    LAB22_STAGE_DONE,
    LAB22_STAGE_ERROR,
};

volatile enum lab22_stage g_lab22_stage;
volatile enum lab22_stage g_lab22_failed_stage;

volatile int g_ready_rc = NOT_RUN;
volatile int g_short_rc = NOT_RUN;
volatile int g_max_rc = NOT_RUN;

volatile int g_null_rc = NOT_RUN;
volatile int g_zero_rc = NOT_RUN;
volatile int g_oversize_rc = NOT_RUN;

volatile int g_short_check = NOT_RUN;
volatile int g_max_check = NOT_RUN;
volatile int g_invalid_check = NOT_RUN;

volatile uintptr_t g_spi_mmio_base;
volatile uint32_t g_spi_cr0;
volatile uint32_t g_spi_cr1;
volatile uint32_t g_spi_cpsr;

volatile uint8_t g_command;

volatile uint8_t g_short_reply[SHORT_REPLY];
volatile uint8_t g_max_reply[SPI_COMMAND_MAX_REPLY];

__attribute__((noinline))
void lab22_checkpoint(void) {
    __asm__ volatile ("nop");
}

static void lab22_fail(void) {
    g_lab22_failed_stage = g_lab22_stage;
    g_lab22_stage = LAB22_STAGE_ERROR;

    lab22_checkpoint();

    for (;;) {
       k_sleep(K_FOREVER);
    }
}

static void prepare_storage(void) {
    memset(g_storage, FILL, sizeof(g_storage));

    g_storage[0] = 0xa5U;
    g_storage[sizeof(g_storage) - 1U] = 0x5aU;
}

static int verify_storage(size_t len) {
    if (g_storage[0] != 0xa5U || g_storage[sizeof(g_storage) - 1U] != 0x5aU)
        return -1;

    for (size_t i = 0U; i < SPI_COMMAND_MAX_REPLY; ++i) {
	/* PL022 internal loopback */
        if (g_storage[i + 1U] != (i < len ? 0x00U : FILL)) {
	    return -1;
	}
    }

    return 0;
}

int main(void) {
    g_lab22_stage = LAB22_STAGE_RESET;

    g_spi_mmio_base = DT_REG_ADDR(BUS_NODE);
    g_command = COMMAND;

    /* Test1. Validate client */
    g_lab22_stage = LAB22_STAGE_READY;
    lab22_checkpoint();

    g_ready_rc = spi_client_ready(&g_sensor);

    if (g_ready_rc != 0)
	lab22_fail();

    /* Test2. Read three reply bytes */
    g_lab22_stage = LAB22_STAGE_SHORT_REPLY;
    prepare_storage();
    lab22_checkpoint();

    g_short_rc = spi_command_read(&g_sensor, COMMAND, &g_storage[1], SHORT_REPLY);
    g_short_check = verify_storage(SHORT_REPLY);

    for (size_t i = 0U; i < SHORT_REPLY; ++i) 
	g_short_reply[i] = g_storage[i + 1U];

    /* Configuration/status registers are safe for observation */
    g_spi_cr0 = sys_read32(g_spi_mmio_base + 0x00U);
    g_spi_cr1 = sys_read32(g_spi_mmio_base + 0x04U);
    g_spi_cpsr = sys_read32(g_spi_mmio_base + 0x10U);

    if (g_short_rc != 0 || g_short_check != 0 || (g_spi_cr1 & 3U) != 3U)
	lab22_fail();

    /* Test3. Maximum allowed reply */
    g_lab22_stage = LAB22_STAGE_MAX_REPLY;
    prepare_storage();
    lab22_checkpoint();

    g_max_rc = spi_command_read(&g_sensor, COMMAND, &g_storage[1], SPI_COMMAND_MAX_REPLY);

    g_max_check = verify_storage(SPI_COMMAND_MAX_REPLY);

    for (size_t i = 0U; i < SPI_COMMAND_MAX_REPLY; ++i) {
        g_max_reply[i] = g_storage[i + 1U];
    }

    if (g_max_rc != 0 || g_max_check != 0)
	lab22_fail();

    /* Test4. Argument/reply-size checks */
    g_lab22_stage = LAB22_STAGE_INVALID_ARGS;
    prepare_storage();
    lab22_checkpoint();

    g_null_rc = spi_command_read(&g_sensor, COMMAND, NULL, 1U);
    g_zero_rc = spi_command_read(&g_sensor, COMMAND, &g_storage[1], 0U);
    g_oversize_rc = spi_command_read(&g_sensor, COMMAND, &g_storage[1], SPI_COMMAND_MAX_REPLY + 1U);

    g_invalid_check = verify_storage(0U);

    if (g_null_rc != -EINVAL || g_zero_rc != -EINVAL || g_oversize_rc != -EMSGSIZE || g_invalid_check != 0)
	lab22_fail();

    /* Success */
    g_lab22_stage = LAB22_STAGE_DONE;
    lab22_checkpoint();

    for(;;) {
        k_sleep(K_FOREVER);
    }

    return 0;
}




