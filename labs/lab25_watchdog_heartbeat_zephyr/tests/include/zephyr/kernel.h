#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H

#include <stdbool.h>
#include <pthread.h>

#define K_FOREVER (-1)

struct k_mutex {
    pthread_mutex_t native;
};

extern _Thread_local bool mock_isr_context;

static inline bool k_is_in_isr(void) {
    return mock_isr_context;
}

static inline int k_mutex_init(struct k_mutex *m) {
    return -pthread_mutex_init(&m->native, NULL);
}

static inline int k_mutex_lock(struct k_mutex *m, int timeout) {
    (void) timeout;
    return -pthread_mutex_lock(&m->native);
}

static inline int k_mutex_unlock(struct k_mutex *m) {
    return -pthread_mutex_unlock(&m->native);
}


#endif
