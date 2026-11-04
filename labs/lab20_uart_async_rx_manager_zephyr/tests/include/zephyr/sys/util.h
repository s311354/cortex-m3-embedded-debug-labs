#ifndef HOST_ZEPHYR_SYS_UTIL_H
#define HOST_ZEPHYR_SYS_UTIL_H

#define CONFIG_SMP 0
#define BUILD_ASSERT(cond, msg) _Static_assert((cond), msg)
#define IS_ENABLED(config) (config)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif
