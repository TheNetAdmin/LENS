CC=gcc
CFLAGS=-Wall
LIBS=-lwolfssl -lpthread 

dsa_sign: dsa_sign.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -Wl,-rpath=/usr/local/lib

.PHONY: clean

clean:
	rm -f *.o dsa_sign