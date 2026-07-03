# vim: ts=8 sw=8 noet :
CC=clang
LD=ld.lld

CFLAGS= -std=c23 -g -O0 -Werror -flto -mtune=native -march=native



main: bind.o cursor.o rltest.o
	$(CC) $(CFLAGS) bind.o cursor.o rltest.o -o main

bind.o : bind.c
	$(CC) $(CFLAGS) bind.c -c

cursor.o : cursor.c
	$(CC) $(CFLAGS) cursor.c -c

rltest.o : rltest.c
	$(CC) $(CFLAGS) rltest.c -c

clean:
	rm -f *.o
	rm -f main

.PHONY: clean
