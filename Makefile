CC=clang
CFLAGS=-Wall -g -I"./include" -I"./" -std=gnu11
#CFLAGS+=-fsanitize=address
SRCS=$(wildcard src/*.c)
TARGET=libplctag.a

all: libplctag.a
	$(MAKE) -C test

debug : CFLAGS+= -DDEBUG
debug: all

libplctag.a: $(patsubst %.c,%.o,$(SRCS))
	ar rcs $(TARGET) $?

%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(MAKE) -C test clean
	rm -f src/*.o libplctag.a
