CC=gcc
CFLAGS=-Wall -fsanitize=address -g -I../

TESTS=$(wildcard *.c)
EXES=$(patsubst %.c,%,$(TESTS))
all: $(EXES)

%: %.c
	$(CC) $(CFLAGS) $< -o test_$@

clean:
	rm -r *.dSYM || true 2>/dev/null >/dev/null
	rm test_*
