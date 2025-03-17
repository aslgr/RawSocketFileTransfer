server_name=server
client_name=client

server_objs=libServer.o server.o
client_objs=libServer.o client.o pilha.o

CFLAGS := -Wall
LDFLAGS := -lncurses

all: $(server_name) $(client_name)

$(server_name): $(server_objs)
	gcc -o $(server_name) $(server_objs) $(LDFLAGS)

$(client_name): $(client_objs)
	gcc -o $(client_name) $(client_objs) $(LDFLAGS)

libServer.o: libServer.h libServer.c
	gcc -c libServer.c $(CFLAGS)

server.o: server.c
	gcc -c server.c $(CFLAGS)

client.o: client.c pilha.h
	gcc -c client.c $(CFLAGS)

pilha.o: pilha.c pilha.h
	gcc -c pilha.c $(CFLAGS)

clean:
	rm -f $(server_objs) $(client_objs) pilha.o *~

purge: clean
	-rm -f $(server_name) $(client_name)
