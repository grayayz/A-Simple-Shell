CC = gcc
CFLAGS = -g -std=c99 -Wall -fsanitize=address,undefined -pthread
LDFLAGS = -pthread

chatd: chatd.c
	$(CC) $(CFLAGS) chatd.c -o chatd $(LDFLAGS)
	@echo "./chatd and begin chatting!"

clean:
	rm -f chatd chatd.o