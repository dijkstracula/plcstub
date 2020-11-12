CC=clang
CFLAGS=-Wall -fsanitize=address -g -I../include -I../

TESTS=$(wildcard *.c)
EXES=$(patsubst %.c,%,$(TESTS))
all: $(EXES)

%: %.c
	$(CC) $(CFLAGS) $< ../libplctag.a -o test_$@

clean:
	rm -r *.dSYM || true 2>/dev/null
	rm test_* 2>/dev/null
