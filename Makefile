#Makefile#

CC = gcc
all: server

server: proxy_server.c
	$(CC) proxy_server.c -lcrypto -lm -o server

clean:
	rm -f server
