#include "watchdog_supervisor.h"

#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define WDT_NODE       DT_ALIAS(watchdog0)
#define WORKER_STACK   1024U
#define WORKER_PRIO    5
#define SERVICE_MS     50U
#define NOT_RUN        (-999)

BUILD_ASSERT(DT_NODE_HAS_STATUS(WDT_NODE, okay), "watchdog0 must be enabled");
BUILD_ASSERT(DT_NODE_HAS_COMPAT(WDT_NODE, arm_cmsdk_watchdog), "CMSDK watchdog required");
BUILD_ASSERT(DT_REG_ADDR(WDT_NODE) == 0x40008000U, "MPS2 watchdog mapping differs");

static const struct device *const g_wdt = DEVICE_DT_GET(WDT_NODE);

static struct watchdog_ctx g_watchdog;
static struct k_thread g_workers[WATCHDOG_TASK_COUNT];
K_THREAD_STACK_ARRAY_DEFINE(g_worker_stacks, WATCHDOG_TASK_COUNT, WORKER_STACK);

/* atomics are for cross-thread demo telemetry/stall injection, not core policy */
atomic_t g_effective_stall_mask;
atomic_t g_observed_stall_mask;
atomic_t g_report_calls[WATCHDOG_TASK_COUNT];

volatile uint32_t g_auto_stall_after_feeds = 5U; /* Set 0 to stay healthy */
volatile uint32_t g_stall_mask = 0U; /* bit 0: task0, bit 1: task1 */

enum lab25_stage {
    LAB25_STAGE_RESET = 0,
    LAB25_STAGE_INIT,
    LAB25_STAGE_RUNNING,
    LAB25_STAGE_WAITING_FOR_RESET,
    LAB25_STAGE_ERROR,
};

volatile enum lab25_stage g_lab25_stage;

volatile int g_init_rc = NOT_RUN;
volatile int g_service_rc = NOT_RUN;
volatile int g_error_rc = NOT_RUN;
volatile int g_report_rc[WATCHDOG_TASK_COUNT] = {NOT_RUN, NOT_RUN};

volatile uint32_t g_work_units[WATCHDOG_TASK_COUNT];
volatile uint32_t g_successful_feeds;
volatile uint32_t g_waiting_services;
volatile uintptr_t g_watchdog_mmio_base;

__attribute__((noinline)) void lab25_checkpoint(void) {
    __asm__ volatile ("nop");
}

static void halt_without_feeding(int rc) {
    g_error_rc = rc;
    g_lab25_stage = LAB25_STAGE_ERROR;
    printk("LAB25 ERROR rc=%d; no more software feeds\n", rc);
    for (;;)
	k_sleep(K_FOREVER);
}

static void heartbeat_worker(void *arg, void *unused1, void *unused2) {
    const unsigned int id = (unsigned int)(uintptr_t) arg;
    const uint32_t period_ms = id == 0U ? 200U : 300U;
    uint32_t work_units = 0U;

    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);

    for (;;) {
        if (atomic_test_bit(&g_effective_stall_mask, id))  {
            /* Simulate a task that makes no further progress or reports */
	    atomic_set_bit(&g_observed_stall_mask, id);
	} else {
	    /* Placeholder for one completed unit of the worker's real work */
            ++work_units;
	    g_work_units[id] = work_units;

	    const int rc = watchdog_report(&g_watchdog, id);

	    g_report_rc[id] = rc;

	    if (rc != 0) {
                for (;;)
	            k_sleep(K_FOREVER);
	    }
	    atomic_inc(&g_report_calls[id]);
	}

	k_msleep(period_ms);
    }
}

int main(void) {
    g_watchdog_mmio_base = DT_REG_ADDR(WDT_NODE);

    /* Test 1. */
    g_lab25_stage = LAB25_STAGE_INIT;
    printk("LAB25 BOOT watchdog=0x%lx\n", (unsigned long)g_watchdog_mmio_base);
    lab25_checkpoint();

    g_init_rc = watchdog_init(&g_watchdog, g_wdt);

    if (g_init_rc != 0)
	halt_without_feeding(g_init_rc);

    for (unsigned int id = 0U; id < WATCHDOG_TASK_COUNT; ++id) {
        k_thread_create(&g_workers[id], g_worker_stacks[id],
			K_THREAD_STACK_SIZEOF(g_worker_stacks[id]),
			heartbeat_worker, (void *)(uintptr_t)id, NULL, NULL,
			WORKER_PRIO, 0U, K_NO_WAIT);
    }

    /* Test 2. */
    g_lab25_stage = LAB25_STAGE_RUNNING;
    printk("LAB25 START channel=%d reload_ms=%u\n", g_watchdog.channel_id, WATCHDOG_TIMEOUT_MS);
    lab25_checkpoint();

    uint32_t previous_stall_mask = 0U;

    for (;;) {
        if (g_auto_stall_after_feeds != 0U &&
	    g_successful_feeds >= g_auto_stall_after_feeds) {
	    /* The automatic demonstration stalls task 1 */
	    g_stall_mask |= 2U;
	}

	const uint32_t requested = g_stall_mask & WATCHDOG_REQUIRED_MASK;
	atomic_set(&g_effective_stall_mask, (atomic_val_t)requested);

	if (requested != previous_stall_mask) {
            previous_stall_mask = requested;

	    g_lab25_stage = requested != 0U ? LAB25_STAGE_WAITING_FOR_RESET : LAB25_STAGE_RUNNING;

	    printk("LAB25 STALL mask=%u feeds=%u\n", requested, g_successful_feeds);
	    lab25_checkpoint();
	}

	g_service_rc = watchdog_service(&g_watchdog);

	if (g_service_rc == 0) {
	    ++g_successful_feeds;
	    printk("LAB25 FEED %u\n", g_successful_feeds);
	} else if (g_service_rc == WATCHDOG_SERVICE_WAITING) {
	    ++g_waiting_services;
	} else {
	    halt_without_feeding(g_service_rc);
	}

	k_msleep(SERVICE_MS);
    }

    return 0;
}

