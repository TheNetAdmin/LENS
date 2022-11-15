CC=gcc
CFLAGS=-Wall -I/mnt/pmem/include -L/mnt/pmem/lib
LIBS=-lwolfssl -lpthread

rsa_keytest: rsa_keytest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -Wl,-rpath=/mnt/pmem

cert:
	openssl genrsa -out private.pem 2048
	openssl rsa -in private.pem -out private.der -outform DER

.PHONY: clean
clean:
	rm -f *.o rsa_keytest
