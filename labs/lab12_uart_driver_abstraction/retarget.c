#include <stdio.h>
#include "console.h"

int sendchar(int ch) {
    console_putc((char) ch);

    return ch;
}

int fputc(int ch, FILE *stream) {
    (void) stream;

    if (ch == '\n') {
        sendchar('\r');
    }

    sendchar(ch);

    return ch;
}

/*
 * newlib/newlib-nano write syscall
 */
int _write(int fd, char *buf, int len) {
    (void) fd;

    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
	    sendchar('\r');
	}

	sendchar((unsigned char)buf[i]);
    }

    return len;
}
