
.DEFAULT_GOAL := side_channel

CFLAGS += -march=native

headers = chasing_config.h chasing.h print_info.h utils.h

side_channel.o: ${headers}

chasing.o: ${headers}

side_channel: side_channel.o chasing.o

.PHONY: clean
clean:
	rm -f *.o
	rm -f side_channel
