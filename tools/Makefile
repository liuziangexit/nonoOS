CFLAGS=-O3 -Wall -Wextra -std=c11
LDFLAGS=-O3

.PHONY: all clean
.SUFFIXES: .o .c

all: sign mkfs idtgen

sign: src/sign/sign.c
	$(CC) -c $< -o $@.o $(CFLAGS)
	$(CC) $@.o -o $@ $(LDFLAGS)

mkfs: src/mkfs
	$(CC) -c $</mkfs.c -o $@.o $(CFLAGS)
	$(CC) $@.o -o $@ $(LDFLAGS)

idtgen: src/idtgen/idtgen.c
	$(CC) -c $< -o $@.o $(CFLAGS)
	$(CC) $@.o -o $@ $(LDFLAGS)

clean:
	rm -rf sign* mkfs* idtgen*
