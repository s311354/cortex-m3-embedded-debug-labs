#include <stdio.h>

#include "console.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    console_init();

    fputc('A', stdout);
    fputc('\n', stdout);

    printf("Lab12 parintf test\n");

    while (1) {
        int c;

	c = console_getc();

	if (c >= 0) {
	    console_putc((char)c);
	}
    }
}
