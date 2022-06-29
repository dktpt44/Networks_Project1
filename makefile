
CC = gcc
CFLAGS = -Wall -g
# ****************************************************

all: server client

server: ftpServer.o
	$(CC) $(CFLAGS) ftpServer.o -o ftpServer -lpthread

client: client.o
	$(CC) $(CFLAGS) client.o -o client -lpthread

ftpServer.o: ftpServer.c
	$(CC) $(CFLAGS) -c ftpServer.c -lpthread

client.o: client.c
	$(CC) $(CFLAGS) -c client.c -lpthread

clean:
	rm -f ftpServer client


# Makefile for executable adjust

# *****************************************************
# Parameters to control Makefile operation

