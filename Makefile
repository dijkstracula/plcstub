CC=gcc
CFLAGS=-Wall -fsanitize=address -g -I="./include" -std=c11
SRCS=$(wildcard src/*.c)
TARGET=libplctag.a

.PHONY: clean all

all: libplctag.a
    
libplctag.a: $(patsubst %.c,%.o,$(SRCS))
	ar rcs $(TARGET) $?

%.o: $(SRCS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm src/*.o libplctag.a || true
	make -C test clean
