CC = gcc
CFLAGS = -g -Wall -Werror -std=c99 -m64


cachesim: cachelab.c
	$(CC) $(CFLAGS) -o cachesim cachelab.c -lm

