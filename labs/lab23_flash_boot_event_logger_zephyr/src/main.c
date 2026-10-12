#include "boot_log.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define BOOTLOG_PARTITION DT_NODELABEL(bootlog_partition)
#define BOOTLOG_FLASH     DT_MTD_FROM_FIXED_PARTITION(BOOTLOG_PARTITION)
#define BOOTLOG_BASE      ((off_t)DT_REG_ADDR(BOOTLOG_PARTITION))
#define BOOTLOG_BYTES     ((size_t)DT_REG_SIZE(BOOTLOG_PARTITION))
#define LAB23_NOT_RUN     (-999)

BUILD_ASSERT(DT_NODE_EXISTS(BOOTLOG_FLASH), "bootlog partition must belong to a flash driver");
BUILD_ASSERT(DT_NODE_HAS_STATUS(BOOTLOG_FLASH, okay), "MPS2 flash simulator must be enabled");
BUILD_ASSERT((BOOTLOG_BYTES % BOOT_LOG_RECORD_SIZE) == 0U, "bootlog partition needs whole 16-byte slots");
BUILD_ASSERT(BOOTLOG_BYTES >= 2U * BOOT_LOG_RECORD_SIZE, "bootlog demo requires at least two slots");

enum lab23_stage {
    LAB23_STAGE_RESET = 0,
    LAB23_STAGE_INIT,
    LAB23_STAGE_EMPTY_CHECK,
    LAB23_STAGE_APPEND_FIRST,
    LAB23_STAGE_APPEND_SECOND,
    LAB23_STAGE_RECOVER,
    LAB23_STAGE_READ_LAST,
    LAB23_STAGE_DONE,
    LAB23_STAGE_ERROR
};

static struct boot_log g_log;
static struct boot_log g_recovered_log;
static const struct device *const g_flash = DEVICE_DT_GET(BOOTLOG_FLASH);

static const uint8_t g_record_a[BOOT_LOG_RECORD_SIZE] = {
    0x42, 0x4f, 0x4f, 0x54, 0x01, 0x00, 0x00, 0x01,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};

static const uint8_t g_record_b[BOOT_LOG_RECORD_SIZE] = {
    0x42, 0x4f, 0x4f, 0x54, 0x02, 0x00, 0x00, 0x02,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xf0, 0x0f
};

volatile enum lab23_stage g_lab23_stage;
volatile enum lab23_stage g_lab23_failed_stage;

volatile int g_init_rc = LAB23_NOT_RUN;
volatile int g_empty_rc = LAB23_NOT_RUN;
volatile int g_append_a_rc = LAB23_NOT_RUN;
volatile int g_append_b_rc = LAB23_NOT_RUN;
volatile int g_recover_rc = LAB23_NOT_RUN;
volatile int g_read_last_rc = LAB23_NOT_RUN;
volatile int g_compare_rc = LAB23_NOT_RUN;

volatile size_t g_initial_next_slot;
volatile size_t g_recovered_next_slot;
volatile size_t g_capacity_slots;

volatile uint8_t g_erase_value;
volatile uint8_t g_last_record[BOOT_LOG_RECORD_SIZE];

__attribute__ ((noinline)) void lab23_checkpoint(void) {
    __asm__ volatile ("nop");
}

static void halt_lab(void) {
    lab23_checkpoint();
    for (;;) {
        k_sleep(K_FOREVER);
    }
}

static void fail_lab(void) {
    g_lab23_failed_stage = g_lab23_stage;
    g_lab23_stage = LAB23_STAGE_ERROR;
    halt_lab();
}

int main(void) {
    uint8_t last[BOOT_LOG_RECORD_SIZE];
    g_lab23_stage = LAB23_STAGE_RESET;
    g_lab23_failed_stage = LAB23_STAGE_RESET;

    /* TEST1. */
    g_lab23_stage = LAB23_STAGE_INIT;
    lab23_checkpoint();

    g_init_rc = boot_log_init(&g_log, g_flash, BOOTLOG_BASE, BOOTLOG_BYTES);

    if (g_init_rc != 0)
	fail_lab();

    g_initial_next_slot = g_log.next_slot;
    g_capacity_slots = g_log.region_size / BOOT_LOG_RECORD_SIZE;
    g_erase_value = g_log.erase_value;

    /* TEST2. */
    g_lab23_stage = LAB23_STAGE_EMPTY_CHECK;
    lab23_checkpoint();

    if (g_log.next_slot == 0U) {
        g_empty_rc = boot_log_read_last(&g_log, last);

	if (g_empty_rc != !ENOENT )
            fail_lab();
    }

    if (g_capacity_slots - g_initial_next_slot < 2U) {
        fail_lab();
    }

    /* TEST3. */
    g_lab23_stage = LAB23_STAGE_APPEND_FIRST;
    lab23_checkpoint();

    g_append_a_rc = boot_log_append(&g_log, g_record_a);

    if (g_append_a_rc != 0) 
	fail_lab();

    g_lab23_stage = LAB23_STAGE_APPEND_SECOND;
    lab23_checkpoint();

    g_append_b_rc = boot_log_append(&g_log, g_record_b);

    if (g_append_b_rc != 0)
	fail_lab();

    /* TEST4. */
    g_lab23_stage = LAB23_STAGE_RECOVER;
    lab23_checkpoint();

    g_recover_rc = boot_log_init(&g_recovered_log, g_flash, BOOTLOG_BASE, BOOTLOG_BYTES);

    if (g_recover_rc != 0 || g_recovered_log.next_slot != g_initial_next_slot + 2U) {
        fail_lab();
    }

    g_recovered_next_slot = g_recovered_log.next_slot;

    /* TEST5. */
    g_lab23_stage = LAB23_STAGE_READ_LAST;
    lab23_checkpoint();

    g_read_last_rc = boot_log_read_last(&g_recovered_log, last);

    if (g_read_last_rc != 0)
	fail_lab();

    g_compare_rc = memcmp(last, g_record_b, sizeof(last));
    memcpy((void *)g_last_record, last, sizeof(last));

    if (g_compare_rc != 0)
	fail_lab();

    g_lab23_stage = LAB23_STAGE_DONE;
    halt_lab();
    return 0;
}

