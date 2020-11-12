CC=clang
CFLAGS=-Wall -fsanitize=address -g -I../include -I../

TESTS=$(wildcard *.c)
EXES=$(patsubst %.c,%,$(TESTS))
all: $(EXES)

%: %.c
	$(CC) $(CFLAGS) $< ../libplctag.a -o test_$@

clean:
	rm -rf *.dSYM
	rm -f test_*
