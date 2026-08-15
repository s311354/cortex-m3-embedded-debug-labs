#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "device.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
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
