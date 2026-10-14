#include "fault_log.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define EEPROM_NODE DT_NODELABEL(fault_eeprom)
#define BUS_NODE DT_BUS(EEPROM_NODE)

#define NOT_RUN (-999)

BUILD_ASSERT(DT_NODE_HAS_STATUS(EEPROM_NODE, okay), "fault_eeprom must be enabled");
BUILD_ASSERT(DT_ON_BUS(EEPROM_NODE, i2c), "fault eeprom must use I2C");
BUILD_ASSERT(DT_REG_ADDR(BUS_NODE) == 0x4002a000U, "Expected MPS2 Shield1");
BUILD_ASSERT(DT_PROP(EEPROM_NODE, pagesize) == 8U, "Commit isolation requires 8-byte pages");
BUILD_ASSERT(DT_PROP(EEPROM_NODE, size) >= FAULT_LOG_BYTES, "Need 256 reserved bytes");
BUILD_ASSERT(DT_PROP(EEPROM_NODE, address_width) == 8U, "AT24C02 profile uses one address byte");

static const struct device *const g_eeprom = DEVICE_DT_GET(EEPROM_NODE);
static struct fault_log g_log;
static struct fault_log g_recovered_log;

enum lab24_stage {
    LAB24_STAGE_RESET = 0,
    LAB24_STAGE_INIT,
    LAB24_STAGE_READ_INITIAL,
    LAB24_STAGE_APPEND,
    LAB24_STAGE_RECOVER,
    LAB24_STAGE_VERIFY,
    LAB24_STAGE_DONE,
    LAB24_STAGE_ERROR,
};

volatile enum lab24_stage g_lab24_stage;
volatile enum lab24_stage g_failed_stage;

volatile int g_init_rc = NOT_RUN;
volatile int g_initial_latest_rc = NOT_RUN;
volatile int g_append_rc = NOT_RUN;
volatile int g_recover_rc = NOT_RUN;
volatile int g_latest_rc = NOT_RUN;
volatile int g_compare_rc = NOT_RUN;

volatile uint32_t g_initial_sequence;
volatile uint32_t g_committed_sequence;

volatile uint8_t g_initial_slot = FAULT_LOG_NO_SLOT;
volatile uint8_t g_committed_slot = FAULT_LOG_NO_SLOT;

volatile uint32_t g_initial_fault_code;
volatile uint32_t g_latest_fault_code;

volatile uint32_t g_fault_to_append = 0x10010001U;
volatile uint32_t g_attempted_fault_code;
volatile uintptr_t g_i2c_mmio_base;

volatile size_t g_eeprom_size;

__attribute__((noinline)) void lab24_checkpoint(void) {
    __asm__ volatile ("" ::: "memory");
}

static void fail_lab(void) {
    g_failed_stage = g_lab24_stage;
    g_lab24_stage = LAB24_STAGE_ERROR;

    printk("LAB24 ERROR stage=%u init=%d initial=%d append=%d recover=%d latest=%d\n",
		    (unsigned int) g_failed_stage, g_init_rc, g_initial_latest_rc, g_append_rc, g_recover_rc, g_latest_rc);

    lab24_checkpoint();
    for (;;) {
        k_sleep(K_FOREVER);
    }
}

int main(void) {
    uint32_t value = 0U;
    g_i2c_mmio_base = DT_REG_ADDR(BUS_NODE);
    printk("LAB24 BOOT\n");

    g_lab24_stage = LAB24_STAGE_INIT;
    lab24_checkpoint();

    g_init_rc = fault_log_init(&g_log, g_eeprom);

    if (g_init_rc != 0)
	fail_lab();

    g_eeprom_size = eeprom_get_size(g_eeprom);
    g_initial_sequence = g_log.sequence;
    g_initial_slot = g_log.newest_slot;

    g_lab24_stage = LAB24_STAGE_READ_INITIAL;
    lab24_checkpoint();

    g_initial_latest_rc = fault_log_latest(&g_log, &value);

    if (g_initial_latest_rc != 0 && g_initial_latest_rc != -ENOENT)
	fail_lab();

    if (g_initial_latest_rc == 0)
	g_initial_fault_code = value;

    printk("LAB24 RECOVER rc=%d seq=%u slot=%u code=0x%08x\n", 
		    g_initial_latest_rc, g_initial_sequence, (unsigned int)g_initial_slot, g_initial_fault_code);

    g_lab24_stage = LAB24_STAGE_APPEND;
    lab24_checkpoint();

    g_attempted_fault_code = g_fault_to_append;
    g_append_rc = fault_log_append(&g_log, g_attempted_fault_code);

    if (g_append_rc != 0)
	fail_lab();

    g_lab24_stage = LAB24_STAGE_RECOVER;
    lab24_checkpoint();

    g_recover_rc = fault_log_init(&g_recovered_log, g_eeprom);

    if (g_recover_rc != 0)
	fail_lab();

    g_lab24_stage = LAB24_STAGE_VERIFY;
    lab24_checkpoint();

    g_latest_rc = fault_log_latest(&g_recovered_log, &value);

    if (g_latest_rc != 0)
	fail_lab();

    g_latest_fault_code = value;
    g_committed_sequence = g_recovered_log.sequence;
    g_committed_slot = g_recovered_log.newest_slot;

    const uint32_t expected_sequence = g_initial_slot == FAULT_LOG_NO_SLOT ? 1U : g_initial_sequence + 1U;
    const uint8_t expected_slot = g_initial_slot == FAULT_LOG_NO_SLOT ? 0U : (uint8_t) ((g_initial_slot + 1U) % FAULT_LOG_SLOT_COUNT);

    g_compare_rc = (value == g_attempted_fault_code && g_committed_sequence == expected_sequence && g_committed_slot == expected_slot) ? 0 : -EIO;

    if (g_compare_rc != 0)
	fail_lab();

    g_lab24_stage = LAB24_STAGE_DONE;

    printk("LAB24 DONE seq=%u slot=%u code=0x%08x\n",
		    g_committed_sequence, (unsigned int) g_committed_slot, g_latest_fault_code);

    lab24_checkpoint();

    for (;;) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
