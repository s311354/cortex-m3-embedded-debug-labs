#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "device.h"

/* Global variables to verify __libc_init_array functionality */
volatile uint32_t g_constructor_called = 0;
volatile uint32_t g_constructor_count = 0;

/* Constructor function - demonstrates __libc_init_array calling init_array */
__attribute__((constructor))
void test_constructor_1(void) {
    g_constructor_called = 0x12345678;
    g_constructor_count++;
}

__attribute__((constructor))
void test_constructor_2(void) {
    g_constructor_count++;
}

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    // Verify constructors were called by __libc_init_array
    if (g_constructor_called != 0x12345678 || g_constructor_count != 2) {
        // Constructor not called - __libc_init_array failed!
        while(1);
    }

    // Test dynamic allocation
    char *buffer = malloc(100);

    if (buffer == NULL) {
        // Handle error
	while(1);
    }
 
    // Use buffer

    free(buffer);

    while (1) {
    }
}
