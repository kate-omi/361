CC=gcc
LDFLAGS=-pthread

all: server client

server: server.o interface.o auth.o
	$(CC) $(LDFLAGS) server.o interface.o auth.o -o server

client: client.o interface.o
	$(CC) $(LDFLAGS) client.o interface.o -o client

interface.o: interface.c
	$(CC) -c interface.c -o interface.o

auth.o: auth.c
	$(CC) -c auth.c -o auth.o
clean:
	rm -f *.o server client
