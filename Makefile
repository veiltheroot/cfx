CC      ?= cc
CFLAGS  ?= -O0 -g -Wall -Wextra -std=c99
LDFLAGS ?=

LIB_OBJS = varint.o hexb64.o checksum.o bits.o fixed.o ring.o stats.o session.o frame.o

all: cfx cfx_test

cfx: $(LIB_OBJS) main.o
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) main.o $(LDFLAGS)

cfx_test: $(LIB_OBJS) tests/cfx_test.o
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) tests/cfx_test.o $(LDFLAGS)

%.o: %.c cfx.h cfx_internal.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/cfx_test.o: tests/cfx_test.c cfx.h
	$(CC) $(CFLAGS) -c -o $@ $<

check: cfx_test
	./cfx_test

clean:
	rm -f $(LIB_OBJS) main.o tests/cfx_test.o cfx cfx_test

.PHONY: all check clean
