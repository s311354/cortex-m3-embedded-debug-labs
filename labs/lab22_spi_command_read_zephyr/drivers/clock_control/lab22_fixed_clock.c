/* 
 *  PL022's Zephyr v4.2.0 driver expects a clk_id phandle cell. The MPS2 sysclk binding has zero clock cells.
 */
#define DT_DRV_COMPAT lab22_pl022_clock

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/util.h>

struct lab22_clock_config {
    uint32_t rate_hz;
};

static int lab22_clock_on(const struct device *dev, clock_control_subsys_t subsys) {

    /* Fixed clock: there is nothing to physically enable in this adapter */
    ARG_UNUSED(dev);
    ARG_UNUSED(subsys);

    return 0;
}

static int lab22_clock_off(const struct device *dev, clock_control_subsys_t subsys) {
    ARG_UNUSED(dev);
    ARG_UNUSED(subsys);

    return 0;
}

static int lab22_clock_get_rate(const struct device *dev, clock_control_subsys_t subsys, uint32_t *rate) {
    ARG_UNUSED(subsys);

    if (rate == NULL)
	return -EINVAL;

    const struct lab22_clock_config *config = dev->config;

    *rate = config->rate_hz;

    return 0;
}

static enum clock_control_status lab22_clock_get_status(const struct device *dev, clock_control_subsys_t subsys) {
    ARG_UNUSED(dev);
    ARG_UNUSED(subsys);

    return CLOCK_CONTROL_STATUS_ON;
}

static int lab22_clock_init(const struct device *dev) {
    ARG_UNUSED(dev);

    return 0;
}

static DEVICE_API(clock_control, lab22_clock_api) = {
    .on = lab22_clock_on,
    .off = lab22_clock_off,
    .get_rate = lab22_clock_get_rate,
    .get_status = lab22_clock_get_status,
};

#define LAB22_CLOCK_DEFINE(inst)                                        \
        static const struct lab22_clock_config                          \
        lab22_clock_config_##inst = {                                   \
            .rate_hz = DT_INST_PROP(inst, clock_frequency),             \
        };                                                              \
	                                                                \
                                                                        \
        DEVICE_DT_INST_DEFINE(                                          \
            inst,                                                       \
            lab22_clock_init,                                           \
            NULL,                                                       \
            NULL,                                                       \
            &lab22_clock_config_##inst,                                 \
            PRE_KERNEL_1,                                               \
            CONFIG_CLOCK_CONTROL_INIT_PRIORITY,                         \
            &lab22_clock_api);

DT_INST_FOREACH_STATUS_OKAY(LAB22_CLOCK_DEFINE)
