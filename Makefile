CC=gcc
CFLAGS=-Wall -g -I"./include" -I"./" -std=c11
#CFLAGS+=-fsanitize=address
SRCS=$(wildcard src/*.c)
TARGET=libplctag.a

all: libplctag.a
	make -C test

debug : CFLAGS+= -DDEBUG
debug: all

libplctag.a: $(patsubst %.c,%.o,$(SRCS))
	ar rcs $(TARGET) $?

%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	make -C test clean
	rm src/*.o libplctag.a || true
