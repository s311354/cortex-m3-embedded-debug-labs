#include "watchdog_supervisor.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/drivers/watchdog.h>

int watchdog_init(struct watchdog_ctx *ctx, const struct device *dev) {
    if (ctx == NULL || dev == NULL)
	return -EINVAL;

    if (k_is_in_isr())
	return -EWOULDBLOCK;

    if (ctx->state != WATCHDOG_UNINITIALIZED)
	return -EALREADY;

    if (!device_is_ready(dev))
	return -ENODEV;

    int rc = k_mutex_init(&ctx->lock);

    if (rc != 0)
	return rc;

    ctx->dev = dev;
    ctx->channel_id = -1;
    ctx->reported_mask = 0U; /* neither task reported */

    ctx->state = WATCHDOG_FAULTED;

    const struct wdt_timeout_cfg cfg = {
        .window = {
	    .min = 0U,
	    .max = WATCHDOG_TIMEOUT_MS
	},
	.callback = NULL,
	.flags = WDT_FLAG_RESET_SOC,
    };

    rc = wdt_install_timeout(dev, &cfg);

    if (rc < 0)
	return rc;

    ctx->channel_id = rc;

    rc = wdt_setup(dev, 0U);

    if (rc != 0)
	return rc;

    ctx->state = WATCHDOG_RUNNING;

    return 0;
}

int watchdog_report(struct watchdog_ctx *ctx, unsigned int task_id) {
    if (ctx == NULL || ctx->dev == NULL || task_id >= WATCHDOG_TASK_COUNT) {
        return -EINVAL;
    }

    if (k_is_in_isr())
	return -EWOULDBLOCK;

    int rc = k_mutex_lock(&ctx->lock, K_FOREVER);

    if (rc != 0)
	return rc;

    if (ctx->state != WATCHDOG_RUNNING) {
	rc = -EIO;
    } else {
        ctx->reported_mask |= (uint8_t)(1U << task_id);
	rc = 0;
    }

    k_mutex_unlock(&ctx->lock);
    return rc;
}

int watchdog_service(struct watchdog_ctx *ctx) {
    if (ctx == NULL || ctx->dev == NULL)
	return -EINVAL;

    if (k_is_in_isr())
	return -EWOULDBLOCK;

    int rc = k_mutex_lock(&ctx->lock, K_FOREVER);

    if (rc != 0)
	return rc;

    if (ctx->state != WATCHDOG_RUNNING) {
        rc = -EIO;
    } else if (ctx->reported_mask != WATCHDOG_REQUIRED_MASK) {
        rc = WATCHDOG_SERVICE_WAITING;
    } else {
        /* Hold the mutex through BOTH the feed and successful clearing */
        rc = wdt_feed(ctx->dev, ctx->channel_id);

	if (rc == 0) {
	    ctx->reported_mask = 0U;
	} else {
	    /* No clearing and no future feeds, including after -EAGAIN */
            ctx->state = WATCHDOG_FAULTED;
	}
    }

    k_mutex_unlock(&ctx->lock);
    return rc;
}

