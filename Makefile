CC=gcc
CFLAGS=-Wall -fsanitize=address -g -I"./include" -std=c11 -DDEBUG
SRCS=$(wildcard src/*.c)
TARGET=libplctag.a

all: libplctag.a
	make -C test
    
libplctag.a: $(patsubst %.c,%.o,$(SRCS))
	ar rcs $(TARGET) $?

%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	make -C test clean
	rm src/*.o libplctag.a || true
