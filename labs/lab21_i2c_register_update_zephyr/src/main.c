#include "reg_access.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#define SENSOR_NODE DT_NODELABEL(sensor0)
#define I2C_NODE    DT_NODELABEL(i2c_shield1)

#define TEST_REG \
	((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_reg))

#define TEST_MASK \
	((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_mask))

#define TEST_VALUE \
	((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_value))

#define LAB21_NOT_RUN (-999)

BUILD_ASSERT(DT_NODE_HAS_STATUS(SENSOR_NODE, okay),
		"sensor0 must be enabled");
BUILD_ASSERT(DT_NODE_HAS_STATUS(I2C_NODE, okay),
		"i2c_shield1 must be enabled");

enum lab21_stage {
    LAB21_STAGE_RESET = 0,
    LAB21_STAGE_BUS_READY,
    LAB21_STAGE_READ_ORIGINAL,
    LAB21_STAGE_DIRECT_WRITE,
    LAB21_STAGE_VERIFY_WRITE,
    LAB21_STAGE_RESTORE_AFTER_WRITE,
    LAB21_STAGE_UPDATE,
    LAB21_STAGE_VERIFY_UPDATE,
    LAB21_STAGE_NOOP_UPDATE,
    LAB21_STAGE_FINAL_RESTORE,
    LAB21_STAGE_DONE,
    LAB21_STAGE_ERROR
};

static const struct i2c_client g_sensor = {
    .bus = DEVICE_DT_GET(DT_BUS(SENSOR_NODE)),
    .addr = DT_REG_ADDR(SENSOR_NODE),
};

/*
 * GDB-visible state
 */
volatile enum lab21_stage g_lab21_stage;

volatile uintptr_t g_i2c_mmio_base;
volatile uint16_t g_sensor_addr;

volatile uint8_t g_test_reg;
volatile uint8_t g_test_mask;
volatile uint8_t g_test_value;

volatile uint8_t g_original_value;
volatile uint8_t g_direct_write_value;
volatile uint8_t g_after_direct_write;

volatile uint8_t g_expected_update_value;
volatile uint8_t g_after_update;

volatile int g_read_original_rc = LAB21_NOT_RUN;

volatile int g_direct_write_rc = LAB21_NOT_RUN;
volatile int g_verify_write_rc = LAB21_NOT_RUN;
volatile int g_restore_after_write_rc = LAB21_NOT_RUN;

volatile int g_update_rc = LAB21_NOT_RUN;
volatile int g_verify_update_rc = LAB21_NOT_RUN;

volatile int g_noop_update_rc = LAB21_NOT_RUN;
volatile int g_final_restore_rc = LAB21_NOT_RUN;

__attribute__((noinline))
void lab21_checkpoint(void) {
    __asm__ volatile("" ::: "memory");
}

static void lab21_error(void) {
    g_lab21_stage = LAB21_STAGE_ERROR;

    lab21_checkpoint();

    for (;;)
	k_sleep(K_FOREVER);
}

int main(void) {
    uint8_t value;

    g_lab21_stage = LAB21_STAGE_RESET;

    /* on MPS2/AN385: 0x4002A000 */
    g_i2c_mmio_base = (uintptr_t) DT_REG_ADDR(I2C_NODE);

    g_sensor_addr = g_sensor.addr;

    g_test_reg = TEST_REG;
    g_test_mask = TEST_MASK;
    g_test_value = TEST_VALUE;

    /* Controller readiness */
    if (!device_is_ready(g_sensor.bus))
	lab21_error();

    g_lab21_stage = LAB21_STAGE_BUS_READY;
    lab21_checkpoint();

    /* Test 1: reg_read_byte() */
    g_lab21_stage = LAB21_STAGE_READ_ORIGINAL;
    lab21_checkpoint();

    g_read_original_rc = reg_read_byte(&g_sensor, TEST_REG, &value);

    if (g_read_original_rc != 0)
	lab21_error();

    g_original_value = value;

    /* Test 2: reg_write_byte() */
    g_direct_write_value = (uint8_t)(g_original_value ^ TEST_MASK);

    g_lab21_stage = LAB21_STAGE_DIRECT_WRITE;
    lab21_checkpoint();

    g_direct_write_rc = reg_write_byte(&g_sensor, TEST_REG, g_direct_write_value);

    if (g_direct_write_rc != 0)
	lab21_error();

    g_lab21_stage = LAB21_STAGE_VERIFY_WRITE;
    lab21_checkpoint();

    g_verify_write_rc = reg_read_byte(&g_sensor, TEST_REG, &value);

    if (g_verify_write_rc != 0)
	lab21_error();

    g_after_direct_write = value;

    if (g_after_direct_write != g_direct_write_value)
	lab21_error();

    /* Restore original state before testing update() */
    g_lab21_stage = LAB21_STAGE_RESTORE_AFTER_WRITE;
    lab21_checkpoint();

    g_restore_after_write_rc = reg_write_byte(&g_sensor, TEST_REG, g_original_value);

    if (g_restore_after_write_rc != 0)
	lab21_error();

    /* Test 3: reg_update_byte() */
    g_expected_update_value = (uint8_t)((g_original_value & (uint8_t)~TEST_MASK) | (TEST_VALUE & TEST_MASK));

    g_lab21_stage = LAB21_STAGE_UPDATE;
    lab21_checkpoint();

    g_update_rc = reg_update_byte(&g_sensor, TEST_REG, TEST_MASK, TEST_VALUE);

    if (g_update_rc != 0)
	lab21_error();

    /* Verify against the physical sensor */
    g_lab21_stage = LAB21_STAGE_VERIFY_UPDATE;
    lab21_checkpoint();

    g_verify_update_rc = reg_read_byte(&g_sensor, TEST_REG, &value);

    if (g_verify_update_rc != 0)
	lab21_error();

    g_after_update = value;

    if (g_after_update != g_expected_update_value) {
        lab21_error();
    }

    /* Test 4: redundant-write suppression */

    g_lab21_stage = LAB21_STAGE_NOOP_UPDATE;
    lab21_checkpoint();

    g_noop_update_rc = reg_update_byte(&g_sensor, TEST_REG, TEST_MASK, g_after_update);

    if (g_noop_update_rc != 0)
	lab21_error();

    /* Leave physical hardware in its original state */
    g_lab21_stage = LAB21_STAGE_FINAL_RESTORE;
    lab21_checkpoint();

    g_final_restore_rc = reg_write_byte(&g_sensor, TEST_REG, g_original_value);

    if (g_final_restore_rc != 0)
        lab21_error();

    /* Success */
    g_lab21_stage = LAB21_STAGE_DONE;
    lab21_checkpoint();

    for (;;)
	k_sleep(K_FOREVER);

    return 0;
}
