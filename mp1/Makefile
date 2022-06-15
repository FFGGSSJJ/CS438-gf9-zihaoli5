CC = g++
CFLAGS = -Wall -g
LDFLAGS = -lm

all: clean client.o client server.o server

client.o: 
	${CC} ${CFLAGS} -c src/client.cpp
server.o:
	${CC} ${CFLAGS} -c src/server.cpp
client : client.o
	${CC} ${CFLAGS} client.o ${LDFLAGS} -o http_client
server : server.o
	${CC} ${CFLAGS} server.o ${LDFLAGS} -o http_server
clean:
	rm -rf *.o
	rm -rf http_client http_server