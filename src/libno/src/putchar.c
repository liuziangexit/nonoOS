#include <stdio.h>

#include <tty.h>

int putchar(int ic) {
	char c = (char) ic;
	terminal_write(&c, sizeof(c));
	return ic;
}
