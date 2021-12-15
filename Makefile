CC     := gcc
CFLAGS := -Wall -Werror -g
LDFLAGS:= -L. -lmfs

LIB	   := libmfs.c

DEPS   := udp.c

.PHONY: all
all: libmfs.so server

server: server.o ${DEPS}
	${CC} ${CFLAGS} -o server server.o ${DEPS}

client: client.o libmfs.so
	${CC} ${CFLAGS} -o client client.o ${LDFLAGS}

libmfs.so : libmfs.o ${DEPS}
	${CC} -fPIC -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o udp.c -lc

clean:
	rm -f ./client ./server *.o libmfs.so

libmfs.o : ${LIB} Makefile
	${CC} ${CFLAGS} -c -fPIC ${LIB}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<