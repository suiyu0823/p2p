#;-*-Makefile-*-
CC = gcc
CFLAGS = -g -Wall

all: clean index_server peer

index_server:
	${CC} index_server.c -o index_server -lnsl  

peer:
	${CC} peer.c -o peer -lnsl -lpthread

clean: 
	rm {CFLAGS} -f *.o peer index_server
