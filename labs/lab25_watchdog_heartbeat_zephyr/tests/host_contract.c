#define _POSIX_C_SOURCE 200809L
#include "watchdog_supervisor.h"

#include <zephyr/drivers/watchdog.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Thread_local _Bool mock_isr_context;
static _Atomic unsigned long checks;
static unsigned long cases;

#define CHECK(expr) do { \
	atomic_fetch_add(&checks, 1UL); \
	if (!(expr)) { \
		fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device bus = { .ready = true };

static struct {
    int install_result, setup_result, feed_result;
    unsigned int install_calls, setup_calls, feed_calls;
    struct watchdog_ctx *ctx;
} mock;

/* Gate used to suspend a fake driver feed while another thread reports */
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static bool pause_feed, feed_entered, release_feed, reporter_started;

int wdt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg) {
    CHECK(dev == &bus);

    CHECK(cfg->window.min == 0U);
    CHECK(cfg->window.max == WATCHDOG_TIMEOUT_MS);
    CHECK(cfg->callback == NULL);
    CHECK(cfg->flags == WDT_FLAG_RESET_SOC);

    CHECK(mock.setup_calls == 0U);
    CHECK(mock.ctx->state == WATCHDOG_FAULTED);
    ++mock.install_calls;

    return mock.install_result;
}

int wdt_setup(const struct device *dev, uint8_t options) {
    CHECK(dev == &bus);
    CHECK(options == 0U);

    CHECK(mock.install_calls == 1U);
    CHECK(mock.ctx->channel_id == mock.install_result);
    ++mock.setup_calls;
    return mock.setup_result;
}

int wdt_feed(const struct device *dev, int channel) {
    CHECK(dev == &bus);
    CHECK(channel == mock.install_result);

    CHECK(mock.ctx->reported_mask == WATCHDOG_REQUIRED_MASK);
    CHECK(mock.ctx->state == WATCHDOG_RUNNING);
    CHECK(mock.setup_calls == 1U);
    ++mock.feed_calls;

    CHECK(pthread_mutex_lock(&gate) == 0);
    if (pause_feed) {
        feed_entered = true;
	CHECK(pthread_cond_broadcast(&condition) == 0);
	while (!release_feed) {
	    CHECK(pthread_cond_wait(&condition, &gate) == 0);
	}
    }
    CHECK(pthread_mutex_unlock(&gate) == 0);
    return mock.feed_result;
}

static void reset_mock(struct watchdog_ctx *ctx) {
    bus.ready = true;

    memset(&mock, 0, sizeof(mock));
    mock.install_result = 7;
    mock.ctx = ctx;
    mock_isr_context = false;
    pause_feed = feed_entered = release_feed = reporter_started = false;
}

static void dispose(struct watchdog_ctx *ctx) {
    if (ctx->dev != NULL)
        CHECK(pthread_mutex_destroy(&ctx->lock.native) == 0);
}

static void test_validation(void) {
    struct watchdog_ctx ctx = {0};
    reset_mock(&ctx);

    CHECK(watchdog_init(NULL, &bus) == -EINVAL);
    CHECK(watchdog_init(&ctx, NULL) == -EINVAL);

    CHECK(watchdog_report(NULL, 0U) == -EINVAL);
    CHECK(watchdog_service(NULL) == -EINVAL);
    CHECK(watchdog_report(&ctx, 0U) == -EINVAL);
    CHECK(watchdog_service(&ctx) == -EINVAL);
    CHECK(mock.install_calls == 0U);

    bus.ready = false;
    CHECK(watchdog_init(&ctx, &bus) == -ENODEV);
    CHECK(ctx.state == WATCHDOG_UNINITIALIZED);

    bus.ready = true;
    mock_isr_context = true;
    CHECK(watchdog_init(&ctx, &bus) == -EWOULDBLOCK);
    CHECK(mock.install_calls == 0U);

    mock_isr_context = false;
    CHECK(watchdog_init(&ctx, &bus) == 0);
    CHECK(ctx.reported_mask == 0U);
    CHECK(ctx.channel_id == 7);
    CHECK(watchdog_init(&ctx, &bus) == -EALREADY);
    CHECK(mock.install_calls == 1U && mock.setup_calls == 1U);
    CHECK(watchdog_report(&ctx, 2U) == -EINVAL);
    CHECK(watchdog_report(&ctx, UINT_MAX) == -EINVAL);

    mock_isr_context = true;
    CHECK(watchdog_report(&ctx, 0U) == -EWOULDBLOCK);
    CHECK(watchdog_service(&ctx) == -EWOULDBLOCK);

    mock_isr_context = false;
    CHECK(ctx.reported_mask == 0U && mock.feed_calls == 0U);
    dispose(&ctx);
    ++cases;
}

static void test_init_errors(void) {
    const int errors[] = {-ENOMEM, -EBUSY, -ENOTSUP, -EINVAL};

    for (size_t i = 0; i < sizeof(errors)/sizeof(errors[0]); ++i) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	mock.install_result = errors[i];
	CHECK(watchdog_init(&ctx, &bus) == errors[i]);
	CHECK(mock.setup_calls == 0U && mock.feed_calls == 0U);
	CHECK(ctx.state == WATCHDOG_FAULTED && ctx.channel_id == -1);
	CHECK(watchdog_init(&ctx, &bus) == -EALREADY);
	CHECK(watchdog_report(&ctx, 0U) == -EIO);
	CHECK(watchdog_service(&ctx) == -EIO);
	dispose(&ctx);
	++cases;
    }

    for (size_t i = 0; i < sizeof(errors)/sizeof(errors[0]); ++i) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	mock.setup_result = errors[i];
	CHECK(watchdog_init(&ctx, &bus) == errors[i]);
	CHECK(mock.setup_calls == 1U && mock.feed_calls == 0U);
	CHECK(watchdog_init(&ctx, &bus) == -EALREADY);
	CHECK(mock.install_calls == 1U);
	CHECK(watchdog_service(&ctx) == -EIO);
	dispose(&ctx);
	++cases;
    }

    const int channels[] = {0, 1, 7, 42};
    for (size_t i = 0; i < sizeof(channels)/sizeof(channels[0]); ++i) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	mock.install_result = channels[i];
	CHECK(watchdog_init(&ctx, &bus) == 0);
	CHECK(watchdog_report(&ctx, 0U) == 0);
	CHECK(watchdog_report(&ctx, 1U) == 0);
	CHECK(watchdog_service(&ctx) == 0);
	CHECK(ctx.reported_mask == 0U);
	dispose(&ctx);
	++cases;
    }
}

static void test_feed_errors(void) {
    const int errors[] = {-EIO, -EAGAIN, -EINVAL, -ENODEV};

    for (size_t i = 0; i < sizeof(errors)/sizeof(errors[0]); ++i) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	CHECK(watchdog_init(&ctx, &bus) == 0);
	CHECK(watchdog_report(&ctx, 0U) == 0);
	CHECK(watchdog_report(&ctx, 1U) == 0);

	mock.feed_result = errors[i];
	CHECK(watchdog_service(&ctx) == errors[i]);
	CHECK(mock.feed_calls == 1U);
	CHECK(ctx.reported_mask == WATCHDOG_REQUIRED_MASK);
	CHECK(ctx.state == WATCHDOG_FAULTED);

	mock.feed_result = 0; /* a later healthy driver must not bypass latch */
	CHECK(watchdog_service(&ctx) == -EIO);
	CHECK(watchdog_report(&ctx, 0U) == -EIO);
        CHECK(watchdog_init(&ctx, &bus) == -EALREADY);
	CHECK(mock.feed_calls == 1U);
	dispose(&ctx);
	++cases;
    }
}

static void test_exhaustive_sequences(void) {
    unsigned int possibilities = 1U;

    for (unsigned int length = 0; length <= 7U; ++length) {
	for (unsigned int sequence = 0; sequence < possibilities; ++sequence) {
            struct watchdog_ctx ctx = {0};
	    reset_mock(&ctx);

	    CHECK(watchdog_init(&ctx, &bus) == 0);
	    unsigned int code = sequence, mask = 0U, expected_feeds = 0U;

	    for (unsigned int step = 0; step < length; ++step) {
                const unsigned int event = code % 4U;
	        code /= 4U;

	        if (event < 2U) {
	            CHECK(watchdog_report(&ctx, event) == 0);
	            mask |= 1U << event;
	        } else if (event == 2U) {
	            const int expected = mask == 3U ? 0 : WATCHDOG_SERVICE_WAITING;
	    	    CHECK(watchdog_service(&ctx) == expected);
	    	
	    	    if (mask == 3U) {
	    	        ++expected_feeds;
	    	        mask = 0U;
	    	    }
	        } else {
	            CHECK(watchdog_report(&ctx, UINT_MAX) == -EINVAL);
	        }

	        CHECK(ctx.reported_mask == mask);
	        CHECK(ctx.state == WATCHDOG_RUNNING);
	        CHECK(mock.feed_calls == expected_feeds);
	    }

	    dispose(&ctx);
	    ++cases;
        }
        possibilities *= 4U;
    }
}

struct thread_call {
    struct watchdog_ctx *ctx;
    unsigned int id;
    int result;
};

static void *report_thread(void *arg) {
    struct thread_call *call = arg;
    call->result = watchdog_report(call->ctx, call->id);
    return NULL;
}

static void *service_thread(void *arg) {
    struct thread_call *call = arg;
    call->result = watchdog_service(call->ctx);
    return NULL;
}

static void *gated_report_thread(void *arg) {
    CHECK(pthread_mutex_lock(&gate) == 0);

    reporter_started  = true;
    CHECK(pthread_cond_broadcast(&condition) == 0);

    CHECK(pthread_mutex_unlock(&gate) == 0);

    return report_thread(arg);
}

static void test_concurrent_reports(void) {
    for (unsigned int round = 0; round < 128U; ++round) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	CHECK(watchdog_init(&ctx, &bus) == 0);

	struct thread_call a = {&ctx, 0, -1}, b = {&ctx, 1, -1};
	pthread_t t0, t1;
	
	CHECK(pthread_create(&t0, NULL, report_thread, &a) == 0);
	CHECK(pthread_create(&t1, NULL, report_thread, &b) == 0);

	CHECK(pthread_join(t0, NULL) == 0);
	CHECK(pthread_join(t1, NULL) == 0);

	CHECK(a.result == 0 && b.result == 0);
	CHECK(ctx.reported_mask == 3U && mock.feed_calls == 0U);
	CHECK(watchdog_service(&ctx) == 0);
	CHECK(watchdog_service(&ctx) == WATCHDOG_SERVICE_WAITING);
	CHECK(mock.feed_calls == 1U && ctx.reported_mask == 0U);
	dispose(&ctx);
	++cases;
    }
}

static void test_report_during_feed(void) {
    for (unsigned int iteration = 0; iteration < 32U; ++iteration) {
        struct watchdog_ctx ctx = {0};
	reset_mock(&ctx);

	CHECK(watchdog_init(&ctx, &bus) == 0);
	CHECK(watchdog_report(&ctx, 0U) == 0);
	CHECK(watchdog_report(&ctx, 1U) == 0);

	pause_feed = true;
	struct thread_call feed = {&ctx, 0, -1}, report = {&ctx, 0, -1};
	pthread_t feeder, reporter;

	CHECK(pthread_create(&feeder, NULL, service_thread, &feed) == 0);
	CHECK(pthread_mutex_lock(&gate) == 0);

	while (!feed_entered)
	    CHECK(pthread_cond_wait(&condition, &gate) == 0);

	CHECK(pthread_mutex_unlock(&gate) == 0);

	/* The service must hold the context mutex during the driver call */
	CHECK(pthread_mutex_trylock(&ctx.lock.native) == EBUSY);

	CHECK(pthread_create(&reporter, NULL, gated_report_thread, &report) == 0);
	CHECK(pthread_mutex_lock(&gate) == 0);

	while (!reporter_started)
	    CHECK(pthread_cond_wait(&condition, &gate) == 0);

	release_feed = true;

	CHECK(pthread_cond_broadcast(&condition) == 0);

	CHECK(pthread_mutex_unlock(&gate) == 0);

	CHECK(pthread_join(feeder, NULL) == 0);
	CHECK(pthread_join(reporter, NULL) == 0);

	CHECK(feed.result == 0 && report.result == 0);
        CHECK(ctx.reported_mask == 1U);
        CHECK(watchdog_service(&ctx) == WATCHDOG_SERVICE_WAITING);
        CHECK(mock.feed_calls == 1U);
        CHECK(watchdog_report(&ctx, 1U) == 0);
        CHECK(watchdog_service(&ctx) == 0);
        CHECK(mock.feed_calls == 2U);
        dispose(&ctx);
        ++cases;	
    }
}

int main(void) {
    test_validation();
    test_init_errors();
    test_feed_errors();
    test_exhaustive_sequences();
    test_concurrent_reports();
    test_report_during_feed();
    printf("PASS: %lu cases; %lu assertions\n", cases, atomic_load(&checks));
    puts("Host API/mutex contract only; not a Zephyr firmware or peripheral reset test");
    return 0;
}
