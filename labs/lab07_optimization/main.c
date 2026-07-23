#include <stdint.h>
#include "device.h"

__attribute__((naked))
void SVC_Handler(void) {

}


/* =========================================================
 * Global Veriables
 * ========================================================= */
volatile uint32_t volatile_flag = 0;
uint32_t normal_flag = 0;

volatile uint32_t sink;
volatile uint32_t result;
volatile uint32_t cycles;

volatile uint32_t data;
volatile uint32_t ready;

volatile uint32_t cycle_normal;
volatile uint32_t cycle_inline;

volatile uint32_t global_var = 10;

/* =========================================================
 * Practice 1 - volatile vs non-volatile
 * ========================================================= */

void test_nonvolatile(void) {
    while (normal_flag == 0) {
    }

    sink = normal_flag;
}

void test_volatile(void) {
    while (volatile_flag == 0) {
    }

    sink = volatile_flag;
}

/* =========================================================
 * Practive 2 - Compiler Barrier vs DMB
 * ========================================================= */

void without_barrier(void) {
    data = 100;
    ready = 1;
}

void compiler_barrier(void) {
    data = 100;

    __asm volatile("" ::: "memory");

    ready = 1;
}

void dmb_barrier(void) {
    data = 100;

    __DMB();

    ready = 1;
}

/* =========================================================
 * Practice 3 - Optimization Level
 * ========================================================= */

uint32_t sum_loop(void) {
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 1000; ++i) {
        sum += i;
    }

    return sum;
}

/* =========================================================
 * Practice 4 - DWT Cycle Counter
 * ========================================================= */

void delay_loop(void) {
    for (volatile uint32_t i = 0; i < 1000; ++i) {
        __NOP();
    }
}


/* =========================================================
 * Practice 5 - inline
 * ========================================================= */

uint32_t add_normal(uint32_t a, uint32_t b) {
    return a + b;
}

static inline uint32_t add_inline(uint32_t a, uint32_t b) {
    return a + b;
}

/* =========================================================
 * Practice 6 - Memory Access
 * ========================================================= */

void register_access(void) {
    register uint32_t r = global_var;

    sink = r;
}

void stack_access(void) {
    uint32_t local = global_var;
    sink = local;
}

void global_access(void) {
    sink = global_var;
}

/* =========================================================
 * Practice 7 - CMSIS vs Intrinsic vs Assembly
 * ========================================================= */

void cmsis_disable_irq(void) {
    __disable_irq();
}

void intrinsic_disable_irq(void) {
    __asm volatile ("cpsid i");
}

void cmsis_dmb(void) {
    __DMB();
}

void asm_dmb(void) {
    __asm volatile ("dmb");
}

/* =========================================================
 * DWT Init
 * ========================================================= */

void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

int main(void) {
#ifdef PRACTICE1
    test_nonvolatile();
    test_volatile();
#endif

#ifdef PRACTICE2
    without_barrier();
    compiler_barrier();
    dmb_barrier();
#endif

#ifdef PRACTICE3
    result = sum_loop();
#endif
 
#ifdef PRACTICE4
    dwt_init();
    DWT->CYCCNT = 0;

    delay_loop();

    cycles = DWT->CYCCNT;

#endif

#ifdef PRACTICE5
    dwt_init();
    
    DWT->CYCCNT = 0;
    add_normal(1, 2);
    cycle_normal = DWT->CYCCNT;

    DWT->CYCCNT = 0;
    add_inline(1, 2);
    cycle_inline = DWT->CYCCNT;
#endif

#ifdef PRACTICE6
    register_access();
    stack_access();
    global_access();
#endif

#ifdef PRACTICE7
    cmsis_disable_irq();
    intrinsic_disable_irq();

    cmsis_dmb();
    asm_dmb();
#endif

    while (1) {
    }
}
