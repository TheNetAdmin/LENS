CC=gcc
CFLAGS=-Wall -I/mnt/pmem/include -L/mnt/pmem/lib
LIBS=-lwolfssl -lpthread

rsa_keytest: rsa_keytest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -Wl,-rpath=/mnt/pmem

.PHONY: clean

clean:
	rm -f *.o rsa_keytest
